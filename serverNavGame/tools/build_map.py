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

# How much wall an open room keeps either side of a doorway
# Outdoors there is no wall to walk into, only the frame you step through
JAMB_WIDTH = 0.7

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


class Action:
    """One step of what pressing E on a prop does"""

    def __init__(self, kind, target="", text="", amount=(0.0, 0.0, 0.0), seconds=0.0,
                 toggle=False, count=1, line=""):
        self.kind = kind
        self.target = target
        self.text = text
        self.amount = amount
        self.seconds = seconds
        self.toggle = toggle
        self.count = count
        self.line = line
        # What the save has to say before this line runs, set by the 'when' above it
        self.when = "always"
        self.when_name = ""


class Obj:
    """One thing standing in a room

    face is the outward normal for a wall, and all zeroes for anything the game
    should never hide, like the floor or a prop. name is what an action calls it,
    and is empty for the generated floor and walls that nothing points at
    """

    def __init__(self, preset, translation, rotation, scale, face=(0.0, 0.0, 0.0), name="",
                 solid=True):
        self.preset = preset
        self.translation = translation
        self.rotation = rotation
        self.scale = scale
        self.face = face
        self.name = name
        self.solid = solid
        self.actions = []


class Room:
    def __init__(self, ident, line):
        self.ident = ident
        self.line = line
        self.width = None
        self.depth = None
        self.height = 3.0
        self.wall_height = None  # walls run the full height unless told otherwise
        self.open = False        # outdoors: door frames only, no walls between them
        self.cam = None          # (eye, look) once set
        self.lights = []         # (pos, colour, intensity)
        self.doors = []
        self.prop_files = []
        self.placed = []
        self.last = None
        self.objects = []
        self.when = ("always", "")  # the latch every 'do' under it inherits

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


def parse_when(where, rest):
    """Reads a 'when ...' latch, which gates every 'do' line under it"""
    if not rest:
        fail(where, "when reads: when <always|flag|noflag|item|noitem> <name>")

    kind = rest[0].lower()
    if kind == "always":
        if len(rest) != 1:
            fail(where, "when always takes nothing after it")
        return ("always", "")

    if kind not in ("flag", "noflag", "item", "noitem"):
        fail(where, "when does not know about {}".format(rest[0]))
    if len(rest) != 2:
        fail(where, "when {0} reads: when {0} <name>".format(kind))
    return (kind, rest[1])


def parse_action(where, text, rest):
    """Reads one 'do ...' line

    Angles are written in degrees here because that is what a person wants to
    type, and go out to the .map in radians like every other rotation the game
    reads. Scale is a multiplier, so 2 2 2 means twice the size it was built at
    """
    kind = rest[0].lower()

    if kind == "say":
        if "#" in text:
            fail(where, "a do say line cannot use #, not even as a trailing comment: "
                        "the game reads # as a comment and would cut the text short")
        words = " ".join(rest[1:]).strip()
        if not words:
            fail(where, "do say has nothing to say")
        return Action("say", text=words, line=where)

    if kind == "sound":
        if len(rest) != 2:
            fail(where, "do sound reads: do sound <clip>")
        return Action("sound", text=rest[1], line=where)

    if kind in ("show", "hide"):
        if len(rest) != 2:
            fail(where, "do {0} reads: do {0} <prop|self>".format(kind))
        return Action(kind, target=rest[1], line=where)

    if kind in ("flag", "unflag"):
        if len(rest) != 2:
            fail(where, "do {0} reads: do {0} <name>".format(kind))
        return Action(kind, text=rest[1], line=where)

    if kind in ("give", "take"):
        if len(rest) not in (2, 3):
            fail(where, "do {0} reads: do {0} <item> [count]".format(kind))
        count = 1
        if len(rest) == 3:
            count = int(parse_floats(where, rest[2:3], 1, "do " + kind)[0])
        if count < 1:
            fail(where, "do {} needs a count of one or more".format(kind))
        return Action(kind, text=rest[1], count=count, line=where)

    if kind not in ("move", "rotate", "scale"):
        fail(where, "do does not know how to {}".format(rest[0]))

    usage = ("do {0} reads: do {0} <prop|self>  x y z  over <seconds>  [toggle]".format(kind))
    words = [w.lower() for w in rest]
    if "over" not in words:
        fail(where, usage)
    o = words.index("over")
    if o != 5:
        fail(where, "do {} needs a target and three numbers before its 'over'".format(kind))

    amount = parse_floats(where, rest[2:5], 3, "do " + kind)
    seconds = parse_floats(where, rest[o + 1:o + 2], 1, "do {} over".format(kind))[0]
    if seconds < 0:
        fail(where, "do {} cannot take negative time".format(kind))

    toggle = False
    extra = rest[o + 2:]
    if extra:
        if len(extra) != 1 or extra[0].lower() != "toggle":
            fail(where, "unexpected {} after {}".format(" ".join(extra), usage))
        toggle = True

    if kind == "rotate":
        amount = [math.radians(v) for v in amount]
    if kind == "scale" and any(v == 0.0 for v in amount):
        fail(where, "do scale by zero would flatten the prop, use hide instead")

    return Action(kind, target=rest[1], amount=tuple(amount), seconds=seconds, toggle=toggle,
                  line=where)


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

        elif key == "wallheight":
            if room is None:
                fail(where, "wallheight outside a room")
            room.wall_height = parse_floats(where, rest, 1, "wallheight")[0]
            if room.wall_height <= 0:
                fail(where, "wallheight must be positive")

        elif key == "open":
            if room is None:
                fail(where, "open outside a room")
            if rest:
                fail(where, "open takes nothing after it")
            room.open = True

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
            solid = True
            if len(rest) > 1:
                if len(rest) != 2 or rest[1].lower() != "pass":
                    fail(where, "props reads: props <file> [pass]")
                solid = False
            room.prop_files.append((rest[0], solid, where))

        elif key in ("prop", "interact"):
            if room is None:
                fail(where, "{} outside a room".format(key))
            if "#" in text:
                fail(where, "a {} line cannot use #, not even as a trailing comment: "
                            "the game reads # as a comment and would cut the text short".format(key))
            if len(rest) < 10:
                fail(where, "{} reads: {} <preset>  tx ty tz  rx ry rz  sx sy sz  "
                            "[name <id>]  [say <words>]".format(key, key))
            values = parse_floats(where, rest[1:10], 9, key)
            obj = Obj(rest[0], tuple(values[0:3]), tuple(values[3:6]), tuple(values[6:9]))

            tail = rest[10:]
            i = 0
            while i < len(tail):
                word = tail[i].lower()
                if word == "name":
                    if i + 1 >= len(tail):
                        fail(where, "name needs a word after it")
                    obj.name = tail[i + 1]
                    i += 2
                elif word == "say":
                    words = " ".join(tail[i + 1:]).strip()
                    if not words:
                        fail(where, "say has nothing to say")
                    obj.actions.append(Action("say", text=words, line=where))
                    i = len(tail)
                else:
                    fail(where, "unexpected {} after {}".format(tail[i], key))

            room.placed.append(obj)
            room.last = obj
            room.when = ("always", "")

        elif key == "when":
            if room is None:
                fail(where, "when outside a room")
            if room.last is None:
                fail(where, "when comes after the prop it belongs to, and there is none yet")
            room.when = parse_when(where, rest)

        elif key == "do":
            if room is None:
                fail(where, "do outside a room")
            if room.last is None:
                fail(where, "do comes after the prop it belongs to, and there is none yet")
            if not rest:
                fail(where, "do reads: "
                            "do <say|move|rotate|scale|show|hide|sound|give|take|flag|unflag> ...")
            action = parse_action(where, text, rest)
            action.when, action.when_name = room.when
            room.last.actions.append(action)

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


def jambs(doors, along_half):
    """The short posts either side of each doorway, for a wall that isn't there"""
    spans = []
    for door in doors:
        lo = door.offset - door.width * 0.5
        hi = door.offset + door.width * 0.5
        spans.append((max(-along_half, lo - JAMB_WIDTH), lo))
        spans.append((hi, min(along_half, hi + JAMB_WIDTH)))

    # Two doors side by side share the post between them
    merged = []
    for lo, hi in sorted(spans):
        if merged and lo <= merged[-1][1] + 1e-4:
            merged[-1] = (merged[-1][0], max(merged[-1][1], hi))
        else:
            merged.append((lo, hi))
    return [p for p in merged if p[1] - p[0] > MIN_SEGMENT]


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

    # A room can stand walls shorter than itself, like a terrace you see over
    wall_h = room.height if room.wall_height is None else room.wall_height
    if wall_h > room.height + 1e-4:
        fail(room.line, "room {} has walls {:.2f} tall but is only {:.2f} tall"
                        .format(room.ident, wall_h, room.height))

    # Floor, sitting so that you walk on exactly GROUND_Y
    # No face, because the floor must never be hidden
    room.objects.append(Obj(
        FLOOR_PRESET,
        (0.0, GROUND_Y + FLOOR_HALF_THICK, 0.0),
        (0.0, 0.0, 0.0),
        (half_w, FLOOR_HALF_THICK, half_d),
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

        if room.open:
            pieces = jambs(doors, along_half)
        else:
            pieces = subtract((-along_half, along_half), holes)

        # Wall pieces either side of every doorway
        for lo, hi in pieces:
            translation, scale = place(
                along,
                (lo + hi) * 0.5, fixed,
                (hi - lo) * 0.5, wall_half,
                GROUND_Y - wall_h * 0.5, wall_h * 0.5)
            room.objects.append(Obj(WALL_PRESET, translation, (0.0, 0.0, 0.0), scale, outward))

        for door in doors:
            # The bit of wall above the opening
            if wall_h - door.height > MIN_SEGMENT:
                lintel_half = (wall_h - door.height) * 0.5
                translation, scale = place(
                    along,
                    door.offset, fixed,
                    door.width * 0.5, wall_half,
                    GROUND_Y - wall_h + lintel_half, lintel_half)
                # Part of the wall, so it goes when the wall goes
                room.objects.append(Obj(WALL_PRESET, translation, (0.0, 0.0, 0.0), scale, outward))

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


def check_actions(rooms):
    """Every action has to point at something that is actually in the same room"""
    for room in rooms:
        names = set()
        for obj in room.objects:
            if not obj.name:
                continue
            if obj.name in names:
                fail(room.line, "room {} has two things called {}".format(room.ident, obj.name))
            names.add(obj.name)

        for obj in room.objects:
            for action in obj.actions:
                if action.target in ("", "self"):
                    continue
                if action.target not in names:
                    known = ", ".join(sorted(names)) or "nothing is named"
                    fail(action.line or room.line,
                         "do {} points at {}, which is not in room {} (it has: {})"
                         .format(action.kind, action.target, room.ident, known))


def check_state(rooms):
    """A 'when' that asks about something nothing ever sets is nearly always a typo"""
    flags = set()
    items = set()
    asked = set()

    for room in rooms:
        for obj in room.objects:
            for action in obj.actions:
                if action.kind in ("flag", "unflag"):
                    flags.add(action.text)
                elif action.kind in ("give", "take"):
                    items.add(action.text)
                if action.when != "always":
                    asked.add((room.ident, action.when, action.when_name))

    for ident, when, name in sorted(asked):
        known = flags if when in ("flag", "noflag") else items
        if name not in known:
            print("[build_map] warning: room {} asks 'when {} {}', and no map line ever sets it"
                  .format(ident, when, name), file=sys.stderr)


def check_sounds(rooms, sounds_dir):
    """A missing clip is only a warning, the game just stays quiet"""
    wanted = set()
    for room in rooms:
        for obj in room.objects:
            for action in obj.actions:
                if action.kind == "sound":
                    wanted.add(action.text)

    for name in sorted(wanted):
        found = any(os.path.isfile(os.path.join(sounds_dir, name + ext))
                    for ext in (".wav", ".ogg", ".mp3", ".flac"))
        if not found:
            print("[build_map] warning: no clip on disk for sound '{}', looked in {}"
                  .format(name, sounds_dir), file=sys.stderr)


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


def emit_action(action):
    """Writes one action out as the 'do ...' line the game reads"""
    if action.kind in ("say", "sound"):
        return "do {} {}".format(action.kind, action.text)

    if action.kind in ("flag", "unflag"):
        return "do {} {}".format(action.kind, action.text)

    if action.kind in ("give", "take"):
        return "do {} {} {}".format(action.kind, action.text, action.count)

    target = action.target or "self"
    if action.kind in ("show", "hide"):
        return "do {} {}".format(action.kind, target)

    line = "do {} {}  {}  over {}".format(
        action.kind, target, vec(action.amount), f3(action.seconds))
    if action.toggle:
        line += "  toggle"
    return line


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
        for obj in room.objects:
            body = "{}  {}  {}  {}".format(
                presets.index(obj.preset), vec(obj.translation), vec(obj.rotation), vec(obj.scale))
            if obj.face != (0.0, 0.0, 0.0):
                # A wall knows which way it faces, so the game can drop it when it
                # stands between the camera and the room
                head = "wall {}  face {}".format(body, vec(obj.face))
            elif obj.actions:
                head = "interact {}".format(body)
            else:
                head = "obj {}".format(body)
            if not obj.solid:
                head += "  pass"
            if obj.name:
                head += "  name {}".format(obj.name)
            lines.append(head)

            # One 'when' line whenever the latch changes, not one per action
            latch = ("always", "")
            for action in obj.actions:
                if (action.when, action.when_name) != latch:
                    latch = (action.when, action.when_name)
                    lines.append("when {} {}".format(*latch).rstrip())
                lines.append(emit_action(action))
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


def build(src_path, out_path, models_dir, sounds_dir):
    name, start, rooms, by_ident, links = parse_mapsrc(src_path)
    room_index = {room.ident: i for i, room in enumerate(rooms)}

    for room in rooms:
        build_room(room)
        for rel, solid, where in room.prop_files:
            for preset, translation, rotation, scale in parse_layout(rel, where):
                # No face: a prop is never hidden, only walls are
                room.objects.append(Obj(preset, translation, rotation, scale, solid=solid))
        room.objects.extend(room.placed)

    check_actions(rooms)
    check_state(rooms)
    check_sounds(rooms, sounds_dir)

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
        for obj in room.objects:
            if obj.preset not in presets:
                presets.append(obj.preset)
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
    parser.add_argument("--sounds-dir", default="sounds",
                        help="where the sound clips live (default: sounds)")
    args = parser.parse_args()

    out = args.out or os.path.splitext(args.source)[0] + ".map"
    try:
        build(args.source, out, args.models_dir, args.sounds_dir)
    except MapError as exc:
        print("[build_map] error: {}".format(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
