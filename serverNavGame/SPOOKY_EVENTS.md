# Spooky events — building one

A pool of events to draw from, and a progression spine that decides when building two opens.
Nothing here is committed to. Pick what you like, rename anything, drop the rest.

Everything is written against the map as it stands in `maps/petscop.mapsrc`: **foyer, closet,
hall, hall_main, hall_west, yard, bathroom, billiard_room, ballroom, greenhouse, field, shed,
terrace, building_two**.

---

## The rule these follow

Petscop is not frightening because things jump out. It is frightening because the game keeps
behaving *correctly* while doing something it should not be able to do. A prop that remembers
you. A room that is the wrong size but still has its own light and its own name in the corner.
A save file with a line in it you did not write.

So the good events here are all built out of the machinery already in the repo — flags, prop
memories, `do move`, the fade, the room-name overlay — used one step past what it was for.

Three pacing rules worth holding to:

1. **Most rooms are quiet, most of the time.** If every room does something, none of them do.
2. **The first hour is nearly clean.** One or two events, both deniable.
3. **An event fires once and is remembered.** Set a flag, gate it with `when noflag`. A scare on
   a loop is a mechanic.

---

## What you can trigger on today, and what needs building

| Trigger | State today |
|---|---|
| Pressed E on a prop | **have** — `interact` + `do` lines |
| A flag is / is not set | **have** — `when flag`, `when noflag` |
| Carrying / not carrying an item | **have** — `when item`, `when noitem` |
| How a prop was left last time | **have** — `PropMemory`, restored on re-entry |
| Entered a room | **have** — `enterRoom()`, but nothing hangs off it |
| **Nth** time entering a room | need — `std::map<std::string,int> visits` in `GameState` |
| Stood still for N seconds | need — idle timer in `RoomScene::update` |
| Walked into a wall, at a spot | need — bump test off `CollisionSystem`, plus a position box |
| Pressed E on a prop N times | need — a counter per prop, or reuse `flipped` |
| Quit and came back | need — `sessions` count written by `writeSave` |
| Total distance walked | need — accumulate in `update` |
| Re-entered a room *after* seeing X | **have** — flag set by X, `when flag` on re-entry |

| New action | Unlocks |
|---|---|
| `do light <r g b> <intensity> over <s>` | ~9 of the ideas below. Highest value single item |
| `do wait <seconds>` | lets a script breathe instead of firing all at once |
| `do warp <room> [door]` | rooms that lead somewhere other than their door |
| `do spawn <mesh> <x y z>` / `do despawn` | anything appearing while you watch |
| `do freeze` / `do release` | takes the character off your hands for a beat |
| `do camera <eye> <look> over <s>` | the camera is fixed per room today, so any move reads as wrong |
| `do fade <seconds>` / `do title <text>` | reuses the fade quad and the room-name overlay |
| `do rename <room-name>` | the corner overlay is a great liar |
| a `when` clause on a **door** | the whole progression gate below depends on this |

Two notes on the save. `readSave` throws away a save whose version does not match, so any new
field means bumping `saveVersion` and losing existing saves — do the additions in one go rather
than three times. And `GameState` is the natural home for a `seen` set: the count of events the
player has actually witnessed is what the ending below keys off.

---

# The ideas

Read `→ Piece n` as "this feeds the progression spine at the bottom".

## Foyer — the room you wake up in

**E01 · The tree is gone.**
Third time you walk back into the foyer, the tree is not there. The floor where it stood is
scaled flat and slightly wrong-coloured. Press E on the empty spot: `SOMEBODY TOOK IT.`
*Needs:* visit count. *Have:* `do hide`, prop memory.

**E02 · The lever that goes one step too far.**
The lever is `toggle` today, so it rocks between two positions. Pull it eight times in one visit
and the toggle stops working — the gate keeps going down, through the floor, and does not come
back. The lever is now stuck at an angle no toggle can produce.
*Needs:* press counter. *Have:* `do move`, `do rotate`, `do flag`.

**E03 · The second lever.**
Once E02 has happened, a second lever stands where the first one was, and the first one is gone.
It has no dialog at all — pressing E does nothing but play `lever.wav`. It moves nothing you can
see. It sets a flag. → Piece 1

**E04 · The count in the corner.**
The room-name overlay reads `FOYER` normally. After enough visits it reads `FOYER (12)`. The
number is your visit count plus one, always, and you can check.
*Needs:* visit count, overlay text hook.

**E05 · The blue light stays.**
The foyer has a warm key light and a cold blue fill. Come back from the yard after dark-flag and
the warm one is out. Nothing else changes — the room is just blue now, permanently.
*Needs:* `do light`.

## Closet — the dead end off the foyer

**E06 · The worn side.**
The rock turns 90° per press with no toggle, so it has four faces. Turn the worn side to face the
doorway and leave the room. Come back: it has been turned back. Do it again and it stays, and the
worn side has a shape in it, about the size of a hand. → Piece 1
*Have:* prop memory does exactly this already.

**E07 · Shut in.**
On one specific entry the closet door is disarmed. Twenty seconds, the light drops to nothing over
about eight of them, and then it lets you out with no comment.
*Needs:* `do light`, door arm/disarm from a script.

**E08 · The closet is the ballroom.**
After you have been in the ballroom, one later visit to the closet loads a room 14×12 instead of
6×6 — the ballroom's dimensions, with the closet's one orange light stranded in the middle and
the closet's name in the corner. Walk to the far wall. There is nothing there. Leave and it is a
closet again.
*Needs:* per-room size override, or a duplicate room the door points at conditionally.

## Hall and hall_main — the spine of the house

**E09 · Something walking the other way.**
Halfway up the long hall, a prop moves past you on the opposite side and out the far end, at
walking pace. It is a `man.glb`-shaped silhouette with no light on it. It does not stop.
*Needs:* `do spawn`, a path move. *Have:* `do move` over seconds.

**E10 · The knock.**
Walk into the north wall of `hall_main` at the gap between the bathroom and billiard doors.
Three bumps at that exact spot, three knocks answering back, and the fourth one gets:
`THERE IS A ROOM HERE.` A door appears in the wall that `build_map.py` never generated. → Piece 2
*Needs:* wall-bump detector, `do spawn`, a door added at runtime.

**E11 · The hall got longer.**
`hall_main` is 32 long. Leave by the west end and come back and it is 48, with the same three
lights spread thinner and a stretch in the middle that is unlit. The doors are all still where
they should be, which means walking further between them.
*Needs:* room size override.

**E12 · Rooms closing behind you.**
Every time you finish a thread, one door on `hall_main` you have already used quietly stops
working. No dialog, no locked sound. It just does not fade any more.
*Have:* door arming. *Needs:* a `when` on a door.

**E13 · The hall you cannot leave.**
Mash a door back and forth — in and out within a second of arriving, six times. The seventh
fade goes to black and stays. Ten seconds of footsteps in the dark, then it fades up in the hall,
facing the way you were not.
*Needs:* re-entry timing, `do fade`, `do freeze`.

## Yard — outdoors, southwest, wide and open

**E14 · Someone at the tree line.**
Stand still in the yard for ninety seconds. A figure is standing at the west edge, in front of
the far tree. Walk at it and it is gone once you are within about six metres — not fading, just
absent on the next frame.
*Needs:* idle timer, `do spawn`/`do despawn`.

**E15 · The tuft that has a name.**
The yard grass is `pass`, so you walk through it and it is not interactable. After E14, exactly
one tuft takes an E press. It says a name — a person's, first name only, in caps like everything
else. It never says it again.
*Have:* all of it, gated on a flag.

**E16 · Dig here.**
Two rocks in the yard sit either side of a patch. With the spade from the shed (**E30**) you can
dig it. What comes out is a piece. What is under the piece is a second, smaller hole.
→ Piece 4
*Needs:* an item-gated interact — `when item spade` already does this.

**E17 · The yard at night.**
One entry, late in the game, the yard's two cool lights are at intensity 2 instead of 14, and the
moving background bars stop moving. Everything else is identical.
*Needs:* `do light`, a background freeze toggle.

## Bathroom — small, off the main hall

**E18 · The mirror does not jump.**
A `plane` on the wall with a second copy of the character mirrored behind it, matched to your
position each frame. It matches walking. It does not match jumping — when you leave the ground it
stays down, and picks you back up when you land.
*Needs:* a mirrored prop driven by player transform. Cheap, and the single best idea in this file.

**E19 · Running water.**
There is no sink. Standing still for ten seconds starts a water sound. Any input stops it
instantly, mid-sample.
*Needs:* idle timer, `do sound` with a stop.

**E20 · The bathroom is the yard.**
Late-game entry loads a room 30×12 with the bathroom's one pale light and the bathroom's name.
The yard's trees and rocks are all there, indoors, lit like a washroom.
*Needs:* room size override, same machinery as E08 and E11.

## Billiard room — long, off the north side

**E21 · The balls spell something.**
Balls (`sphere`) on the table are arranged differently every visit. Over six visits the
arrangement resolves into letters — one letter per visit, and the sixth is the whole word.
*Have:* per-visit layouts via prop memory. *Needs:* visit count.

**E22 · The ball that does not come back.**
Press E on the table and a ball rolls to the far cushion and back. Every time. On the twelfth it
rolls off and does not come back, and the table is one ball short from then on, in the save.
*Needs:* press counter. *Have:* `do move`, prop memory.

**E23 · The cue.**
A cue stick on the wall you can take. It is the reach-tool — the switch behind the bathroom
mirror is not reachable without it. → Piece 2
*Have:* `do give`, `when item`.

## Ballroom — the big empty room at the east end

**E24 · Music you can only hear by not playing.**
Idle sixty seconds and a waltz starts, quiet and far off. Any input cuts it. The clip is about
three minutes long, so the only way to hear it end is to stop playing entirely.

**E25 · The audience.**
Thirty identical props in a grid, all facing the far wall. Each time you re-enter, one more has
turned to face the door. At thirty, the room is full of them looking at you, and the next entry
they are all gone and the room is empty. → Piece 3
*Have:* prop memory, `do rotate`. *Needs:* visit count.

**E26 · Do not touch the dark squares.**
The ballroom floor is a checker of two presets. Cross it start to finish on the light squares only
and a flag sets, with no acknowledgement of any kind. Nothing tells you this is a thing.
→ Piece 3
*Needs:* a floor-tile position test.

**E27 · The room is being used.**
Enter and the fade comes up half a second late. In that half second there is sound of a room full
of people. It is over before the picture arrives.
*Needs:* `do wait`, `do fade`.

## Greenhouse — glass, between the hall and the field

**E28 · The plant.**
One plant grows a little every visit, scaled off the visit count, until it is through the roof.
Then it is a stump, and the pot has soil turned over in it.
*Needs:* visit count. *Have:* `do scale`.

**E29 · The bars stop.**
The moving background is behind every room. In the greenhouse, and only the greenhouse, it holds
still. Nobody will consciously notice, and everybody will feel it.
*Needs:* per-room `bgSpeed`.

**E30 · Something over the glass.**
Kill the greenhouse's two lights for two seconds. In the dark there is a shape above the roof,
lit from behind, much too large. Lights come back and it is gone.
*Needs:* `do light`, `do spawn`.

## Field — outdoors, the widest place on the map

**E31 · One tree fewer.**
Cross the field east to west and the tree count is what it was. Cross west to east and it is one
lower. It never goes back up.
*Needs:* crossing direction test. *Have:* prop memory, `do hide`.

**E32 · The edge that gives way.**
Walk into the far west edge of the field — not the shed door, the blank stretch north of it.
Five times, and on the fifth the collision is not there. Beyond is the field again, unlit, with no
grass and no doors, and walking far enough in fades you back to the greenhouse. → Piece 4
*Needs:* wall-bump counter, a collider that can be turned off, `do warp`.

**E33 · The shed leads to the closet.**
Go into the shed from the field and it is the shed. Come out and you are in the closet, in the
foyer wing, on the other side of the house. Going back through the closet door puts you in the
foyer, normally, and the shed is a shed again next time.
*Needs:* `do warp`, or a conditional door target.

## Shed — small outbuilding on the west edge of the field

**E34 · The spade.**
A spade on a wall of tools. One hook is empty. Take the spade and use it in the yard (**E16**).
→ Piece 4

**E35 · The empty hook.**
After you have dug with it, the empty hook has something on it. It is an item you are carrying.
You still have yours.

**E36 · The list.**
A board in the shed that reads your own save back at you — flags, in the order they were set, in
plain caps. It is one line ahead of you. The bottom line is a flag you have not earned.
*Needs:* text from `GameState` into a dialog. Small job, big payoff.

## Terrace — low walls, looks over the field, and the way on

**E37 · The field is empty.**
The terrace walls are waist height so the field is visible from it. On one visit the field below
has no grass, no trees, no rocks. One figure, standing in the middle of it, facing the terrace.
Walk down the stairs and the field is normal and nobody is in it.
*Needs:* a second field variant, or `do hide` across a room boundary.

**E38 · The count is wrong.**
Press E on the building two doors without the requirements and it tells you how many you are
missing. It says three when you need four. It says this consistently, every time, and it is
wrong every time.
*Needs:* the door `when`, and a dialog that reads item counts.

## Anywhere — system-level

**E39 · It knows you left.**
Quit and come back and the first room-name overlay of the session reads `YOU LEFT.` for two
seconds before it settles into the room's real name.
*Needs:* session count in the save.

**E40 · The character walks off.**
Five minutes with no input, anywhere. The character starts walking on his own, taking doors, all
the way to one specific room, and stands there. Input takes him back at any point.
*Needs:* idle timer, a scripted path, `do freeze`.

**E41 · A room that is not on the map.**
Rarely, the corner overlay shows a room name that does not appear in `petscop.mapsrc`. The room
around you is unchanged. It corrects itself on the next room change.
*Needs:* `do rename`.

**E42 · The line you did not write.**
The save gains a `flag` line nobody set — named like a sentence rather than an identifier, so it
reads as English in the middle of a config file. It has no effect on anything. It survives
deleting other flags.
*Needs:* one line in `writeSave`. Keep it inside `saves/`, and keep it honest — a game that
writes outside its own save folder is a different kind of scary than the one you are going for.

---

# Progression — what building two costs

The terrace door to `building_two` is a plain link today. Make it the gate.

## Four pieces, one per wing

| Piece | Wing | Thread | Events |
|---|---|---|---|
| 1 | Foyer / closet | Turn the rock's worn side to the door twice, then pull the lever past its toggle | E02, E03, E06 |
| 2 | Bathroom / billiard | Take the cue, reach what is behind the mirror, and knock through the hall wall | E10, E18, E23 |
| 3 | Ballroom | Cross the floor on light squares only, and be there when the audience is full | E25, E26 |
| 4 | Greenhouse / field / shed / yard | Take the spade, dig the marked patch, and walk through the west edge | E16, E32, E34 |

Each thread ends in a `do give piece`, so the gate is `when item piece 4` — which is one line in
the mapsrc once doors take a `when`.

Deliberate shape here: pieces 1 and 3 are puzzles you can solve on purpose, and 2 and 4 both
require you to have already done something the game never asked for (walking into a wall, walking
off the edge). That is the point. The player who treats it as a normal game gets halfway.

## The hidden fifth requirement

Count events witnessed in `GameState` — a `std::set<std::string> seen`, one entry per event that
actually fired in front of the player.

- **Fewer than ~8 seen:** the door opens. Building two's entrance is a copy of the terrace, one
  room deep, and the door back out leads to the terrace as well. Not a locked door — a loop.
- **8 or more:** the door opens onto building two proper.

The door is never refused. The game just does not have anywhere to put a player who has not been
paying attention. That reads as far worse than a lock, and it costs one branch in `enterRoom`.

## Pacing it across the four pieces

| Pieces held | What the house does |
|---|---|
| 0 | One event total, and it is deniable. E01 or E28 — something moved, maybe you misremembered |
| 1 | Lights start behaving. E05, E29. Still nothing addresses you |
| 2 | The house responds to you specifically. E14, E18, E25 |
| 3 | Rooms stop being the size they were. E08, E11, E20 |
| 4 | The save talks. E36, E39, E42 |

Gate it on `piece` count rather than on room visits — it keeps the escalation tied to progress
instead of to how thorough the player is being.

---

# If you build three things first

1. **Visit counts in `GameState`.** A `std::map<std::string,int>`, incremented in `enterRoom`,
   written by `writeSave`, plus `when visits <room> >= n`. Eleven of the ideas above need only
   this. Bundle it with the other save additions and bump `saveVersion` once.
2. **`do light`.** The rooms already carry their own lights and nothing can touch them at runtime.
   Nine ideas, and it is the cheapest atmosphere lever in the whole engine.
3. **The mirror (E18).** It needs no new action kinds — a prop whose transform is written from the
   player's each frame, with the jump left out. It is the one event here that is unsettling on the
   first viewing and worse on the second.

Everything else can wait for whichever thread you feel like writing.
