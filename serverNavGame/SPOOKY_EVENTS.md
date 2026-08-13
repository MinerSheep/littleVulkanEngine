# Spooky events — building one

A pool of events to draw from, and a progression spine that decides when building two opens.

Sixteen of these are built and marked **✔** — those entries describe what actually shipped, which
in a couple of cases is not what the entry originally proposed. Everything unmarked is new and
nothing here is committed to. Pick what you like, rename anything, drop the rest.

Everything is written against the map as it stands in `maps/petscop.mapsrc`: **foyer, closet,
hall, hall_main, hall_west, yard, bathroom, billiard_room, ballroom, greenhouse, field, field_red,
shed, terrace, building_two**.

---

## The rule these follow

Petscop is not frightening because things jump out. It is frightening because the game keeps
behaving *correctly* while doing something it should not be able to do. A prop that remembers
you. A room that is the wrong size but still has its own light and its own name in the corner.
A save file with a line in it you did not write.

So the good events here are all built out of the machinery already in the repo — flags, prop
memories, `do move`, the fade, the room stretch — used one step past what it was for.

Three pacing rules worth holding to:

1. **Most rooms are quiet, most of the time.** If every room does something, none of them do.
2. **The first hour is nearly clean.** One or two events, both deniable.
3. **An event fires once and is remembered.** Set a flag, gate it with `when noflag`. A scare on
   a loop is a mechanic.

---

## Where the events live now

The original version of this file guessed the events would arrive as new `do` verbs in the map
file. They did not. They live in **`game/src/petscop/events.cpp`**, as `EventDirector`, and the
map file has learnt nothing new. Read the ideas below with that in mind: "needs" almost always
means a few lines of C++, not a change to `build_map.py`.

The director is handed a `Stage` — the map, the save, the live prop list, the model cache, the
player transform — and events come in two shapes:

- **Put props right as the room stands up** (`onEnterRoom`). E01, E15, E36. Safe to touch the real
  props here, because collision registration has not happened yet.
- **Set an override, worked out again from nothing every frame** (`update`). E05, E07, E30, E32.
  Nothing has to be undone when the event stops, because everything resets at the top of the frame.

`dress()` sits ahead of both: it gets the `MapRoom` before a single prop is built, and can change
anything on it — size, camera, lights, objects, doors, spawn points. E11 is the only thing using it
today and it is by far the most under-used hook in the file.

**Counters cost nothing.** `@visits.<room>` and `@edge.field` are items with an `@` in front, so
they ride in the save with every other item and need no `saveVersion` bump. Any "Nth time" idea
below is one `addItem` and one `itemCount`.

## What you can trigger on today

| Trigger | State |
|---|---|
| Pressed E on a prop | **have** — `interact` + `do` lines |
| A flag is / is not set | **have** — `when flag`, `when noflag` |
| Carrying / not carrying an item | **have** — `when item`, `when noitem` |
| How a prop was left last time | **have** — `PropMemory`, restored on re-entry |
| Nth time entering a room | **have** — `@visits.<room>`, read by `visits()` |
| Pressed E on a prop N times | **have** — `startedProp` into `update`, plus an `@` counter |
| Stood still for N seconds | **have** — `idle`, and it watches keys as well as position |
| Standing at a spot, or leaning on a wall | **have** — `stage.player->translation`, see `fieldEdge` |
| Re-entered a room after seeing X | **have** — flag set by X |
| Doors taken back to back | **have** — `mash`, off `sinceEntry` |
| Quit and came back | need — a session count in the save |
| Total distance walked | need — accumulate in `update` |

| Thing the director can do | Where |
|---|---|
| Resize a room, move its camera, edit its objects and lights | `dress()` — E11 only, so far |
| Stand something up that has no collision | `conjure()` — E21, E30, E32 |
| Replace what a prop says | `rewrite()` — E36 |
| Kill one light, or dim every light | `gain` / `killedLight` — E05, E07, E30 |
| Wash the room a colour | `tint` — E30, E32 |
| Stop the backdrop | `bgScale` — E29 |
| Hold the screen black, take the character off you | `black` / `frozen` — E13 |
| Lock the doors | `locked` — E07 |
| Move you to another room | `takeWarp` — E32 |
| Send a door somewhere else | `reroute()` — E33 |
| Play or cut a sound | `LveAudio::play` / `stopAll` |

Still missing, and named in the ideas below: **per-light gains** (there is one global `gain` and one
killed index), **naming props from `dress()`** so unnamed layout scenery can be remembered,
**the room-name overlay text**, and **a session count**.

---

# The ideas

Read `→ Piece n` as "this feeds the progression spine at the bottom".

## Foyer — the room you wake up in

**E01 · The tree is gone. ✔**
Third time you walk back into the foyer, the tree is not there. A bare slab is scaled flat where it
stood, and pressing E on it says `SOMEBODY TOOK IT.` Sets `tree_taken`, so it never comes back.

**E02 · The gate comes back down.**
The lever lifts the slab off the closet doorway and prop memory holds it up between visits. From
the third visit the house puts it back down while you are away — the memory says up, the room says
down. The lever's second page changes from `IT HAS BEEN PULLED BEFORE.` to `YOU HAVE PULLED IT
BEFORE.` and nothing else about it changes. → Piece 1
*Have:* `PropMemory`, `rewrite()`.

**E03 · The camera steps back.**
The foyer camera is fixed. Every fourth visit `dress()` pushes `cameraEye` one step further from
`cameraLook`, so the room reads smaller and more of the ceiling is in shot. It never comes back in,
and by the last step the character is small enough to be hard to pick out.
*Have:* `dress()` already moves a camera — E11 does it to keep the hall on screen.

**E04 · The door that is not one.**
A door-shaped panel on the foyer's south wall, where the front of the house should be. Press E:
`IT IS NOT A DOOR.` Once you hold three pieces it says `IT IS NOT A DOOR YET.` and nothing else
about it has changed.
*Have:* all of it — an `interact` with a `when item piece` split, straight in the mapsrc.

**E05 · The blue light stays. ✔**
Come back from the yard and the warm key light over the foyer is out for good, leaving the cold
blue fill on its own. Sets `saw_dark_foyer`, which is what arms E15.

## Closet — the dead end off the foyer

**E06 · The rock is in your way.**
The rock turns 90° a press and never toggles back. Every fourth quarter turn it also travels one
step toward the doorway, between visits, never while you are looking. Eventually you have to walk
around it to get out, and one more step puts it in the foyer. → Piece 1
*Have:* `PropMemory`, `startedProp` for the press count.

**E07 · Shut in. ✔**
Roughly one press of the rock in three shuts the closet. Doors lock for twenty seconds, the orange
light fades to nothing over the first eight, and then it lets you out with no comment.

**E08 · The closet is deeper.**
Every time E07 shuts you in, `dress()` adds two to the closet's depth. The doorway stays where it
is and the one orange light stays in the middle, so each time there is more room behind you that
the light does not reach. Nothing is ever in it.
*Needs:* the E11 stretch, applied on one axis and keyed to a counter instead of visits.

## Hall and hall_main — the spine of the house

**E09 · The light behind you.**
`hall_main` has three lights. Walk past the middle and the one nearest the door you came in through
goes out; walk back toward it and it lifts again as you approach. It is smooth enough to read as
falloff and steep enough to be wrong. → Piece 2
*Needs:* per-light gains, driven off `player.x`.

**E10 · Footprints.**
Once you have walked the length of `hall_main`, flat dark slabs stand up behind you at the spots you
stopped at — the whole path, laid down at once, while you are facing away from it. They are gone
next visit. Late game only.
*Have:* `conjure()`. *Needs:* a short ring of positions kept in `update`.

**E11 · The hall changes length. ✔**
From the fourth visit `dress()` scales `hall_main` and everything in it — walls, doors, lights,
spawn points — and pulls the camera in to match. Shipped at `stretch = 0.5`, so the hall comes back
*half* the length rather than half as long again; the comment in `events.cpp` says the opposite of
what the constant does, so pick the one you meant.

**E12 · The hall gives you back.**
One time, after you hold two pieces, taking any north door out of `hall_main` fades out and fades
back into `hall_main` — at the far end, facing in. The door works normally on the second try and
every try after.
*Have:* `reroute()`, which already does this for the shed.

**E13 · The hall you cannot leave. ✔**
Six doors taken inside 1.2 seconds of each other. The next fade goes to black and stays for ten
seconds of footsteps, and when it comes up the character is facing the other way.

## Yard — outdoors, southwest, wide and open

**E14 · The path you wore.**
The yard grass is `pass`, so you walk straight through it. Every tuft you walk over is flattened
out of sight and stays flattened, in the save, so the yard slowly grows a path shaped like the way
you always cross it. Nobody is told this is happening.
*Needs:* `dress()` naming the layout props, because only named props are remembered. That one
change unlocks four of the ideas here.

**E15 · The tuft that has a name. ✔**
Once the foyer has gone dark, exactly one tuft answers E with `ELEANOR.` It sets `tuft_named` and
never says it again.

**E16 · Dig here. ✔**
Two stones either side of a patch of turned earth. Without the spade it says the earth has been
turned before, not by hand. With the spade it gives you a **piece** and takes the spade back.
→ Piece 4

**E17 · Somebody else has been digging.**
After E16, every later visit to the yard has one more patch of turned earth in it, somewhere you
did not dig, and the count matches how many pieces you hold. All of them are already empty:
`THIS ONE IS ALREADY EMPTY.`
*Have:* `conjure()`, `itemCount("piece")`.

## Bathroom — small, off the main hall

**E18 · The tap you found.**
Hear the water three times (E19) and the fourth visit has a sink on the wall that was never there.
The water never plays again. Press E on it: `IT IS DRY.`
*Have:* `dress()` adding one object, plus a counter on E19 firing.

**E19 · Running water. ✔**
There is no sink. Ten seconds standing still starts the water, and any input cuts it mid-sample.

**E20 · The switch is not for this room.**
The cue throws the switch above the bathroom mirror and nothing in the bathroom responds. What it
actually does is kill the middle light of `hall_main`, permanently, and there is something to press
E on in the dark stretch it leaves. → Piece 2
*Have:* `switch_thrown` is already set. *Needs:* per-light gains.

## Billiard room — long, off the north side

**E21 · The balls spell something. ✔**
The balls on the table lay out one more letter every visit, and by the sixth they read **BYGONE**.

**E22 · The table is set for two.**
After BYGONE, the balls stop spelling and lay out as a game already in progress. It has moved on
every time you come back — always a legal shot on from where you last saw it, never while you are
in the room.
*Have:* `conjure()`, visit count. The layouts can be a hand-written table of six positions.

**E23 · The cue. ✔**
A cue stick against the wall you can take. It is the reach-tool for the bathroom switch, and there
is nowhere to stand it back. → Piece 2

## Ballroom — the big empty room at the east end

**E24 · The piano plays itself. ✔**
The piano is in the middle of the floor the first time and gone every time after. Walking through
the space where it stood plays it anyway, once per visit.

**E25 · The piano comes back.**
Hold three pieces and the piano is standing there again. Pressing E gives one line — `IT HAS BEEN
MOVED.` — and no sound, and one of the ballroom's two lights is out, leaving it lit from one side.
*Have:* show/hide, `rewrite()`. *Needs:* per-light gains.

**E26 · Six notes.**
One floor slab in the ballroom is a shade off the rest. Stand on it for three seconds and a single
piano note plays and the slab is somewhere else. Six notes in and it is a tune you have already
heard come out of the piano. → Piece 3
*Have:* `conjure()`, position test, audio.

**E27 · The second walker.**
While you are in the ballroom your own footsteps play back a beat late. It reads as the room being
large until you stop walking, because the late set carries on for one more step.
*Needs:* a delay line on the footstep sound. Nothing else.

## Greenhouse — glass, between the hall and the field

**E28 · The panes stack up.**
After the shape passes over (E30), one pane of glass is leaning against the north wall. Every visit
after there is another, stacked against the last, until half the room is glass you have to walk
around. Press E on the stack: `SOMETHING HAS TO BE REPLACED.`
*Have:* `conjure()` by visit count. Solid ones want `dress()` instead, so they get boxes.

**E29 · The bars stop. ✔**
The moving backdrop is behind every room. In the greenhouse, and only the greenhouse, it holds
still.

**E30 · Something over the glass. ✔**
Third visit, three seconds in: every light dies, a flat blue wash comes up, and something far too
large drifts over the roof for two seconds with a drone under it. Sets `saw_shape`.

## Field — outdoors, the widest place on the map

**E31 · The shed gets further away.**
Every time you come into the field from the greenhouse, `dress()` stretches it westward a little —
the stairs and the terrace stay exactly where they were, and the shed door and the north-west
corner do not. The walk to the tools gets longer all game and never gets shorter. → Piece 4
*Have:* the E11 stretch, one axis, one direction.

**E32 · The edge that gives way. ✔**
Lean on the north-west corner of the field five separate times and the fifth one puts you in
`field_red` — the field again, washed red, slabs laid out in a spiral where the grass was. Walk far
enough into it and you come out in the greenhouse. → Piece 4

**E33 · The shed leads to the closet. ✔**
Read the note in the closet (`DLEIF WN`) and the next time you walk out of the shed you are in the
closet instead, on the other side of the house. It happens once.

## Shed — small outbuilding on the west edge of the field

**E34 · The hooks fill up.**
The shed wall is mostly empty hooks. There is a tool on one more hook every time you set a flag, so
the wall is the same list the board reads (E36) in a different form. None of them can be taken:
`NOT THAT ONE.`
*Have:* `conjure()` against `state.flags.size()`.

**E35 · Under the shed.**
The shed floor has a seam in it. Before E33 it does nothing. After E33 has put you out into the
closet, pressing E on the seam says `THE CLOSET IS UNDER HERE.` — which cannot be true of anywhere
on this map. It never opens.
*Have:* all of it, gated on `shed_closet`.

**E36 · The list. ✔**
The board reads your own save back at you — every flag, in the order the set holds them, three to
a line, in caps. The last line is always `SAW YOU READING THIS`, which is not a flag you have.

## Terrace — low walls, looks over the field, and the way on

**E37 · The terrace looks the wrong way.**
The terrace walls are waist height and there is nothing standing beyond them. Late game there is:
the yard's trees and rocks, in the yard's arrangement, laid out below a terrace that looks over the
field from the other side of the house.
*Have:* `conjure()`, and the yard's layout is already a file you can read positions out of.

**E38 · The doors ask for things.**
The building two doors say one line per piece you hold, and they are instructions rather than
descriptions — `BRING THE ROCK.` `LEAVE THE CUE.` Each one is a thing you can actually do. Doing it
sets a flag and is never mentioned again, by the doors or by anything else. → the fifth requirement
*Have:* `rewrite()`, `itemCount`. The rock is already portable if E06 walks it out of the closet.

## Anywhere — system-level

**E39 · You wake up further along.**
The save writes down which room you quit in and puts you back there. Late game it puts you back one
room further on — always the next room toward the terrace, never backward, and only when you have
been away long enough that you might not be sure.
*Have:* `state.room` is already the whole mechanism.

**E40 · He turns to face you.**
Stand still for forty seconds anywhere and the character turns to face the camera, rather than the
way he was walking. Any input and he snaps back mid-turn.
*Have:* `idle`, and E13 already writes `player->rotation`.

**E41 · The lights come up late.**
From the third piece on, every room's lights come up a quarter of a second after the fade has
finished, so every room in the house begins dark for a moment. It is the same everywhere, which is
what makes it read as the game rather than the room.
*Have:* `sinceEntry`. *Needs:* per-light gains only if you want it uneven.

**E42 · Flags for rooms you have not been in.**
The save quietly gains flags named after rooms in building two. They do nothing, they are never
checked, and the first place you read them is the board in the shed (E36) — which prints whatever
is in the save without asking where it came from.
*Have:* `setFlag` from the director, and E36 does the rest. Keep it inside `saves/` — a game that
writes outside its own save folder is a different kind of scary than the one you are going for.

---

# Progression — what building two costs

The terrace door to `building_two` is a plain link today. Make it the gate.

## Four pieces, one per wing

| Piece | Wing | Thread | Events |
|---|---|---|---|
| 1 | Foyer / closet | Lift the gate and keep it lifted, then turn the rock until it walks itself out of the closet and press E on it in the foyer | E02, E06 |
| 2 | Bathroom / billiard | Take the cue, throw the switch, and find what the switch put in the dark at the far end of the hall | E09, E20, E23 |
| 3 | Ballroom | Stand on the odd slab six times and finish the tune | E26 |
| 4 | Greenhouse / field / shed / yard | Take the spade, dig the marked patch, and lean on the corner of the field until it gives | E16, E31, E32 |

Each thread ends in a `do give piece`, so the gate is `when item piece 4` — which is one line in the
mapsrc once doors take a `when`, and that is the only piece of map syntax the whole spine needs.

Deliberate shape here: pieces 1 and 3 are puzzles you can solve on purpose, and 2 and 4 both end in
something the game never asked for (a switch with no visible effect, walking off the edge). That is
the point. The player who treats it as a normal game gets halfway.

## The hidden fifth requirement

Count events witnessed the same way visits are counted — one `@seen.<event>` item, set by the event
itself at the moment it actually happens in front of the player. No save version bump.

- **Fewer than ~8 seen:** the door opens. Building two's entrance is a copy of the terrace, one room
  deep, and the door back out leads to the terrace as well. Not a locked door — a loop.
- **8 or more:** the door opens onto building two proper.

The door is never refused. The game just does not have anywhere to put a player who has not been
paying attention. That reads as far worse than a lock, and it costs one branch in `enterRoom`.

## Pacing it across the four pieces

| Pieces held | What the house does |
|---|---|
| 0 | One event total, and it is deniable. E01 — something moved, maybe you misremembered |
| 1 | Lights start behaving. E05, E09, E29. Still nothing addresses you |
| 2 | The house responds to you specifically. E12, E14, E40 |
| 3 | Rooms stop being the size they were. E03, E08, E11, E31 |
| 4 | The save talks. E36, E39, E42 |

Gate it on `piece` count rather than on room visits — it keeps the escalation tied to progress
instead of to how thorough the player is being.

---

# If you build three things first

1. **Per-light gains.** `lightGain` is one global multiplier plus a single killed index, so no event
   can dim one light and lift another. A `std::vector<float>` sized to the room's lights, reset each
   frame like everything else, unlocks E09, E20, E25 and E41 — the whole "lights behave" tier of the
   pacing table.
2. **Names from `dress()`.** Only named props are remembered, and the scenery in `rooms/*.layout`
   has no names, so nothing the player does to grass or trees can survive a room change. Naming them
   as the room is dressed makes the outdoors rooms rememberable and unlocks E14, E17 and E28.
3. **`dress()` doing more than E11.** It is the only hook that runs before a room exists, and one
   event uses it. Adding and removing objects through it — rather than `conjure()`, which cannot
   give anything a collision box — is what E08, E18, E28 and E31 all need.

The overlay text hook and a session count are each worth one idea apiece and no more, so they can
wait until something needs them twice.
