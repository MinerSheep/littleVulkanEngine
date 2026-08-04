#!/usr/bin/env python3
"""Turns a readable map source into the .map file the game loads.

Rooms are written as boxes -- a size, a height and some doors -- and this fills in
the floor and the wall pieces around each doorway. It also works out where you
stand when you come through a door, and which door on the far side you came out of.

Run it from the repo root:

    python3 tools/build_map.py maps/petscop.mapsrc -o maps/petscop.map

Paths inside the source file (props, models) are relative to the repo root too.

Everything the game needs is resolved here, so the loader on the other side only
has to read numbers. Anything wrong with the map -- a door linked to nothing, a
mesh that is not on disk -- stops the build instead of reaching the game.
"""

import argparse
import math
import os
import sys

# -Y is up in this engine, and the floor sits here
GROUND_Y = 0.5

# cube.obj spans -1..1, so a cube's half size is its scale, not half of it
FLOOR_HALF_THICK = 0.1

# Walls are chunky on purpose
# You move 3 units a second and a frame can stretch to 1/5 s, so a thin wall is
# something you walk straight through on a slow frame
WALL_THICK = 0.5

# A doorway you can miss is worse than one you trigger early, so the trigger box
# reaches further through the wall than the wall itself is thick
MIN_TRIGGER_DEPTH = 1.0

DOOR_WIDTH = 1.6
DOOR_HEIGHT = 2.2

# How far inside the room you appear after walking through a door
# Far enough that you are clear of the trigger you just came out of
SPAWN_INSET = 1.5

# Wall pieces thinner than this are dropped, they would just be z-fighting slivers
MIN_SEGMENT = 0.05

FLOOR_PRESET = "cube"
WALL_PRESET = "cube"

# Which way each wall faces, and which way is into the room from it
# 'along' is the axis the wall runs down, the other horizontal axis is pinned
WALLS = {
    "north": {"along": "x", "sign": +1.0, "inward": (0.0, 0.0, -1.0)},
    "south": {"along": "x", "sign": -1.0, "inward": (0.0, 0.0, +1.0)},
    "east":  {"along": "z", "sign": +1.0, "inward": (-1.0, 0.0, 0.0)},
    "west":  {"along": "z", "sign": -1.0, "inward": (+1.0, 0.0, 0.0)},
}


class MapError(Exception):
    """Something in the map source is wrong and the build should stop"""


def fail(where, message):
    raise MapError("{}: {}".format(where, message))


# --- source file ------------------------------------------------------------

class Door:
    def __init__(self, ident, wall, offset, width, height, line):
        self.ident = ident
        self.wall = wall
        self.offset = offset
        self.width = width
        self.height = height
        self.line = line
        # filled in once links are resolved
        self.to_room = -1
        self.to_door = -1
        # filled in by the geometry pass
        self.translation = (0.0, 0.0, 0.0)
        self.scale = (1.0, 1.0, 1.0)
        self.spawn = (0.0, 0.0, 0.0)
        self.spawn_yaw = 0.0


class Room:
    def __init__(self, ident, line):
        self.ident = ident
        self.line = line
        self.width = None
        self.depth = None
        self.height = 3.0
        self.cam = None          # (eye, look) once set
        self.lights = []         # (pos, colour, intensity)
        self.doors = []
        self.prop_files = []
        self.interactions = []
        # (preset, translation, rotation, scale, face, dialog)
        # face is the outward normal for a wall, and all zeroes for anything the
        # game should never hide, like the floor or a prop
        self.objects = []

    def door_index(self, ident):
        for i, d in enumerate(self.doors):
            if d.ident == ident:
                return i
        return -1


def parse_floats(where, parts, count, what):
    if len(parts) < count:
        fail(where, "{} needs {} numbers, got {}".format(what, count, len(parts)))
    out = []
    for p in parts[:count]:
        try:
            out.append(float(p))
        except ValueError:
            fail(where, "{} is not a number in {}".format(p, what))
    return out


def parse_mapsrc(path):
    """Reads the hand written source into rooms and links"""
    try:
        with open(path, "r") as handle:
            raw = handle.readlines()
    except OSError as exc:
        raise MapError("could not open {}: {}".format(path, exc))

    name = os.path.splitext(os.path.basename(path))[0]
    start = None
    rooms = []
    by_ident = {}
    links = []
    room = None

    for number, text in enumerate(raw, start=1):
        line = text.split("#", 1)[0].strip()
        if not line:
            continue
        where = "{}:{}".format(path, number)
        parts = line.split()
        key = parts[0].lower()
        rest = parts[1:]

        if key == "map":
            if not rest:
                fail(where, "map needs a name")
            name = rest[0]
            room = None

        elif key == "start":
            if not rest:
                fail(where, "start needs a room name")
            start = rest[0]
            room = None

        elif key == "room":
            if not rest:
                fail(where, "room needs a name")
            if rest[0] in by_ident:
                fail(where, "room {} is declared twice".format(rest[0]))
            room = Room(rest[0], where)
            rooms.append(room)
            by_ident[room.ident] = room

        elif key == "link":
            room = None
            if len(rest) != 2:
                fail(where, "link needs exactly two door names, like: link foyer.front hall.back")
            links.append((rest[0], rest[1], where))

        elif key == "size":
            if room is None:
                fail(where, "size outside a room")
            room.width, room.depth = parse_floats(where, rest, 2, "size")
            if room.width <= 0 or room.depth <= 0:
                fail(where, "size must be positive")

        elif key == "height":
            if room is None:
                fail(where, "height outside a room")
            room.height = parse_floats(where, rest, 1, "height")[0]
            if room.height <= 0:
                fail(where, "height must be positive")

        elif key == "cam":
            if room is None:
                fail(where, "cam outside a room")
            # cam at X Y Z look X Y Z
            words = [w.lower() for w in rest]
            if "at" not in words or "look" not in words:
                fail(where, "cam reads: cam at X Y Z look X Y Z")
            a = words.index("at")
            l = words.index("look")
            eye = parse_floats(where, rest[a + 1:a + 4], 3, "cam at")
            look = parse_floats(where, rest[l + 1:l + 4], 3, "cam look")
            room.cam = (tuple(eye), tuple(look))

        elif key == "light":
            if room is None:
                fail(where, "light outside a room")
            values = parse_floats(where, rest, 7, "light")
            room.lights.append((tuple(values[0:3]), tuple(values[3:6]), values[6]))

        elif key == "door":
            if room is None:
                fail(where, "door outside a room")
            if len(rest) < 3:
                fail(where, "door reads: door <name> <wall> <offset> [width W] [height H]")
            ident, wall = rest[0], rest[1].lower()
            if wall not in WALLS:
                fail(where, "unknown wall {}, expected north south east or west".format(rest[1]))
            if room.door_index(ident) >= 0:
                fail(where, "door {} is declared twice in room {}".format(ident, room.ident))
            offset = parse_floats(where, rest[2:3], 1, "door offset")[0]
            width, height = DOOR_WIDTH, DOOR_HEIGHT
            extra = rest[3:]
            i = 0
            while i < len(extra):
                word = extra[i].lower()
                if word == "width":
                    width = parse_floats(where, extra[i + 1:i + 2], 1, "door width")[0]
                elif word == "height":
                    height = parse_floats(where, extra[i + 1:i + 2], 1, "door height")[0]
                else:
                    fail(where, "unexpected {} after door, expected width or height".format(extra[i]))
                i += 2
            if width <= 0 or height <= 0:
                fail(where, "door width and height must be positive")
            room.doors.append(Door(ident, wall, offset, width, height, where))

        elif key == "props":
            if room is None:
                fail(where, "props outside a room")
            if not rest:
                fail(where, "props needs a file path")
            room.prop_files.append((rest[0], where))

        elif key == "interact":
            if room is None:
                fail(where, "interact outside a room")
            if "#" in text:
                fail(where, "an interact line cannot use #, not even as a trailing comment: "
                            "the game reads # as a comment and would cut the text short")
            words = [w.lower() for w in rest]
            if "say" not in words:
                fail(where, "interact reads: interact <preset>  tx ty tz  rx ry rz  sx sy sz  "
                            "say <words>")
            s = words.index("say")
            if s != 10:
                fail(where, "interact needs a preset and 9 numbers before its 'say', got {}"
                            .format(s))
            preset = rest[0]
            values = parse_floats(where, rest[1:10], 9, "interact")
            dialog = " ".join(rest[s + 1:]).strip()
            if not dialog:
                fail(where, "interact has nothing to say")
            room.interactions.append(
                (preset, tuple(values[0:3]), tuple(values[3:6]), tuple(values[6:9]), dialog))

        else:
            fail(where, "unknown directive {}".format(parts[0]))

    if not rooms:
        raise MapError("{}: no rooms declared".format(path))
    for room in rooms:
        if room.width is None:
            fail(room.line, "room {} has no size".format(room.ident))
    if start is None:
        start = rooms[0].ident
    if start not in by_ident:
        raise MapError("{}: start names room {}, which does not exist".format(path, start))

    return name, start, rooms, by_ident, links


def parse_layout(path, where):
    """Reads an editor saved layout, the same 10 field lines the game already reads"""
    try:
        with open(path, "r") as handle:
            raw = handle.readlines()
    except OSError as exc:
        fail(where, "could not open props file {}: {}".format(path, exc))

    out = []
    for number, text in enumerate(raw, start=1):
        line = text.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) < 10:
            print("[build_map] {}:{}: skipping malformed layout line: {}".format(path, number, line),
                  file=sys.stderr)
            continue
        spot = "{}:{}".format(path, number)
        values = parse_floats(spot, parts[1:10], 9, "layout line")
        out.append((parts[0], tuple(values[0:3]), tuple(values[3:6]), tuple(values[6:9])))
    return out


# --- geometry ---------------------------------------------------------------

def subtract(span, holes):
    """Takes the door openings out of a wall and returns the pieces that are left"""
    pieces = [span]
    for hole in sorted(holes):
        nxt = []
        for lo, hi in pieces:
            if hole[1] <= lo or hole[0] >= hi:
                nxt.append((lo, hi))
                continue
            if lo < hole[0]:
                nxt.append((lo, hole[0]))
            if hole[1] < hi:
                nxt.append((hole[1], hi))
        pieces = nxt
    return [p for p in pieces if p[1] - p[0] > MIN_SEGMENT]


def place(along, c_along, c_fixed, s_along, s_fixed, c_y, s_y):
    """Builds a translation and a scale from wall local numbers"""
    if along == "x":
        return (c_along, c_y, c_fixed), (s_along, s_y, s_fixed)
    return (c_fixed, c_y, c_along), (s_fixed, s_y, s_along)


def build_room(room):
    """Fills in the floor, the wall pieces and each door's box and spawn point"""
    half_w = room.width * 0.5
    half_d = room.depth * 0.5
    wall_half = WALL_THICK * 0.5
    ceiling_y = GROUND_Y - room.height  # -Y is up, so the top of the room is the smaller number

    # Floor, sitting so that you walk on exactly GROUND_Y
    # No face, because the floor must never be hidden
    room.objects.append((
        FLOOR_PRESET,
        (0.0, GROUND_Y + FLOOR_HALF_THICK, 0.0),
        (0.0, 0.0, 0.0),
        (half_w, FLOOR_HALF_THICK, half_d),
        (0.0, 0.0, 0.0),
        "",
    ))

    for wall, info in WALLS.items():
        along = info["along"]
        along_half = half_w if along == "x" else half_d
        fixed = info["sign"] * (half_d if along == "x" else half_w)
        doors = [d for d in room.doors if d.wall == wall]

        # Which way this wall faces out of the room. The game hides a wall whose
        # outside is turned toward the camera, because it stands in the way
        outward = tuple(-c for c in info["inward"])

        for door in doors:
            lo = door.offset - door.width * 0.5
            hi = door.offset + door.width * 0.5
            if lo < -along_half - 1e-4 or hi > along_half + 1e-4:
                fail(door.line, "door {} runs off the end of the {} wall of room {} "
                                "(wall spans {:.2f} to {:.2f}, door spans {:.2f} to {:.2f})"
                                .format(door.ident, wall, room.ident, -along_half, along_half, lo, hi))
            if door.height > room.height + 1e-4:
                fail(door.line, "door {} is {:.2f} tall but room {} is only {:.2f} tall"
                                .format(door.ident, door.height, room.ident, room.height))

        holes = [(d.offset - d.width * 0.5, d.offset + d.width * 0.5) for d in doors]

        # Wall pieces either side of every doorway
        for lo, hi in subtract((-along_half, along_half), holes):
            translation, scale = place(
                along,
                (lo + hi) * 0.5, fixed,
                (hi - lo) * 0.5, wall_half,
                GROUND_Y - room.height * 0.5, room.height * 0.5)
            room.objects.append((WALL_PRESET, translation, (0.0, 0.0, 0.0), scale, outward, ""))

        for door in doors:
            # The bit of wall above the opening
            if room.height - door.height > MIN_SEGMENT:
                lintel_half = (room.height - door.height) * 0.5
                translation, scale = place(
                    along,
                    door.offset, fixed,
                    door.width * 0.5, wall_half,
                    ceiling_y + lintel_half, lintel_half)
                # Part of the wall, so it goes when the wall goes
                room.objects.append((WALL_PRESET, translation, (0.0, 0.0, 0.0), scale, outward, ""))

            # The trigger, reaching further through the wall than the wall is thick
            depth_half = max(wall_half, MIN_TRIGGER_DEPTH * 0.5)
            translation, scale = place(
                along,
                door.offset, fixed,
                door.width * 0.5, depth_half,
                GROUND_Y - door.height * 0.5, door.height * 0.5)
            door.translation = translation
            door.scale = scale

            # Where you stand coming through, and which way you face
            inward = info["inward"]
            door.spawn = (translation[0] + inward[0] * SPAWN_INSET,
                          GROUND_Y,
                          translation[2] + inward[2] * SPAWN_INSET)
            door.spawn_yaw = math.atan2(inward[0], inward[2])

    if room.cam is None:
        room.cam = auto_camera(room)


def auto_camera(room):
    """A camera that looks down into the room from the south, when none was given"""
    span = max(room.width, room.depth)
    eye = (0.0,
           GROUND_Y - span * 0.55 - room.height * 0.4,
           -(room.depth * 0.5 + span * 0.55))
    look = (0.0, GROUND_Y - room.height * 0.25, 0.0)
    return (eye, look)


# --- linking and validation -------------------------------------------------

def resolve_links(rooms, by_ident, links, room_index):
    for left, right, where in links:
        ends = []
        for side in (left, right):
            if "." not in side:
                fail(where, "{} should read <room>.<door>".format(side))
            room_id, door_id = side.split(".", 1)
            if room_id not in by_ident:
                fail(where, "no room called {}".format(room_id))
            room = by_ident[room_id]
            index = room.door_index(door_id)
            if index < 0:
                known = ", ".join(d.ident for d in room.doors) or "none"
                fail(where, "room {} has no door called {} (it has: {})".format(room_id, door_id, known))
            ends.append((room, index))

        (room_a, door_a), (room_b, door_b) = ends
        if room_a is room_b and door_a == door_b:
            fail(where, "a door cannot link to itself")

        for room, index in ends:
            if room.doors[index].to_room >= 0:
                fail(where, "door {}.{} is already linked".format(room.ident, room.doors[index].ident))

        room_a.doors[door_a].to_room = room_index[room_b.ident]
        room_a.doors[door_a].to_door = door_b
        room_b.doors[door_b].to_room = room_index[room_a.ident]
        room_b.doors[door_b].to_door = door_a


def check_presets(presets, models_dir):
    missing = []
    for name in presets:
        if not os.path.isfile(os.path.join(models_dir, name + ".obj")):
            missing.append(name)
    if missing:
        raise MapError("no mesh on disk for preset(s): {}\nlooked for {}"
                       .format(", ".join(sorted(missing)),
                               ", ".join(os.path.join(models_dir, m + ".obj") for m in sorted(missing))))


# --- output -----------------------------------------------------------------

def f3(value):
    return "{:.3f}".format(value)


def vec(values):
    return " ".join(f3(v) for v in values)


def emit(name, start_index, rooms, presets, out_path):
    lines = []
    lines.append("# generated by tools/build_map.py -- do not edit, edit the .mapsrc instead")
    lines.append("map 1")
    lines.append("name {}".format(name))
    lines.append("start {}".format(start_index))
    lines.append("")
    for i, preset in enumerate(presets):
        lines.append("preset {} {}".format(i, preset))
    lines.append("")

    for index, room in enumerate(rooms):
        lines.append("room {} {}".format(index, room.ident))
        lines.append("size {} {} {}".format(f3(room.width), f3(room.depth), f3(room.height)))
        eye, look = room.cam
        lines.append("cam {}  {}".format(vec(eye), vec(look)))
        for position, colour, intensity in room.lights:
            lines.append("light {}  {}  {}".format(vec(position), vec(colour), f3(intensity)))
        for preset, translation, rotation, scale, face, dialog in room.objects:
            body = "{}  {}  {}  {}".format(
                presets.index(preset), vec(translation), vec(rotation), vec(scale))
            if dialog:
                lines.append("interact {}  say {}".format(body, dialog))
            elif face == (0.0, 0.0, 0.0):
                lines.append("obj {}".format(body))
            else:
                # A wall knows which way it faces, so the game can drop it when it
                # stands between the camera and the room
                lines.append("wall {}  face {}".format(body, vec(face)))
        for door in room.doors:
            lines.append("door {}  {}  {}  {}  to {} {}  spawn {} {}".format(
                door.ident,
                vec(door.translation), vec((0.0, 0.0, 0.0)), vec(door.scale),
                door.to_room, door.to_door,
                vec(door.spawn), f3(door.spawn_yaw)))
        lines.append("endroom")
        lines.append("")

    text = "\n".join(lines).rstrip() + "\n"
    directory = os.path.dirname(out_path)
    if directory:
        os.makedirs(directory, exist_ok=True)
    with open(out_path, "w") as handle:
        handle.write(text)


def build(src_path, out_path, models_dir):
    name, start, rooms, by_ident, links = parse_mapsrc(src_path)
    room_index = {room.ident: i for i, room in enumerate(rooms)}

    for room in rooms:
        build_room(room)
        for rel, where in room.prop_files:
            for preset, translation, rotation, scale in parse_layout(rel, where):
                # No face: a prop is never hidden, only walls are
                room.objects.append((preset, translation, rotation, scale, (0.0, 0.0, 0.0), ""))
        for preset, translation, rotation, scale, dialog in room.interactions:
            room.objects.append((preset, translation, rotation, scale, (0.0, 0.0, 0.0), dialog))

    resolve_links(rooms, by_ident, links, room_index)

    # Doors that go nowhere are allowed while a map is half built, they just do
    # nothing when you walk into them
    dangling = 0
    for room in rooms:
        for door in room.doors:
            if door.to_room < 0:
                dangling += 1
                print("[build_map] warning: {}.{} is not linked to anything"
                      .format(room.ident, door.ident), file=sys.stderr)

    presets = []
    for room in rooms:
        for preset, _, _, _, _, _ in room.objects:
            if preset not in presets:
                presets.append(preset)
    check_presets(presets, models_dir)

    emit(name, room_index[start], rooms, presets, out_path)

    doors = sum(len(r.doors) for r in rooms)
    objects = sum(len(r.objects) for r in rooms)
    print("[build_map] {} -> {}".format(src_path, out_path))
    print("[build_map] {} room(s), {} object(s), {} door(s), {} preset(s), starting in {}"
          .format(len(rooms), objects, doors, len(presets), start))
    if dangling:
        print("[build_map] {} door(s) lead nowhere".format(dangling))


def main():
    parser = argparse.ArgumentParser(description="Compile a .mapsrc into the .map the game loads")
    parser.add_argument("source", help="the .mapsrc to read")
    parser.add_argument("-o", "--out", help="where to write the .map (default: alongside the source)")
    parser.add_argument("--models-dir", default="models",
                        help="where the preset meshes live (default: models)")
    args = parser.parse_args()

    out = args.out or os.path.splitext(args.source)[0] + ".map"
    try:
        build(args.source, out, args.models_dir)
    except MapError as exc:
        print("[build_map] error: {}".format(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
