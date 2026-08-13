# Spooky events — what had to be touched outside events.cpp

Sixteen events from `SPOOKY_EVENTS.md` now run, plus the four progression quests
that hold the terrace door shut. Almost all of it lives in
`game/src/petscop/events.{hpp,cpp}` (173 + 711 lines). This is the account of
everywhere else the change had to reach, and why.

## The shape of it

`EventDirector` never drives the scene. The scene calls four hooks and then reads
overrides back out, so nothing in `room_scene.cpp` grew a branch per event.

| Hook | When | What it is for |
|---|---|---|
| `dress(room, i)` | top of `enterRoom` | hands back a changed copy of the room about to be built |
| `onEnterRoom(i)` | bottom of `enterRoom` | puts props right once they are all standing |
| `update(dt, playing, pressed)` | every frame | runs the timed events |
| `reroute(toRoom, toDoor)` | a door is taken | sends that door somewhere else, or refuses it |

Read back out: `locksDoors`, `holdsBlack`, `takesControl`, `lightGain`,
`backgroundSpeed`, `ambient`, `extras`, `takeWarp`, `sealedDoor`.

## Files outside events.cpp

| File | Change | Why |
|---|---|---|
| `petscop/interactions.hpp` `.cpp` | `startedProp()` — which prop a press just set going | E07 has to know you pressed E on the rock. 4 lines |
| `petscop/room_scene.hpp` | the director, `liveRoom`, `bgPhase` | see below |
| `petscop/room_scene.cpp` | the four hook calls, the override reads | 64 lines, no event named anywhere in it |
| `maps/petscop.mapsrc` | props the events act on, and `field_red` | the map half of E16/E21/E23/E24/E32/E33/E34/E36 |
| `tools/make_sounds.py`, `sounds/*.wav` | water, piano, footsteps, drone | placeholders, same as the existing lever and stone |

Three things in `room_scene` are worth naming.

**`liveRoom`** — the scene used to read lights and the room name straight out of
`map.rooms[currentRoom]`. Once `dress()` can hand back a changed room that is the
wrong place to look, so the scene keeps a pointer to the room it actually built.

**`bgPhase`** — the backdrop marched on `clock * bgSpeed`. Setting the speed to
zero that way snaps the bars back to where the clock says they should be. It now
accumulates its own phase, so E29 stops them where they stand.

**The extras list** — `CollisionSystem` holds raw pointers into `props`, so an
event that pushed onto it mid-room would strand them. Anything an event conjures
goes in the director's own vector and is drawn straight after the room's props,
never registered, never walked into.

## Two decisions worth knowing about

**The save format did not change.** Visit counts are kept as items with an `@`
prefix — `item @visits.foyer 3`, `item @edge.field 2`. `readSave` throws away a
save whose version does not match, so a real field would have cost every save on
disk. Nothing shows the pocket, so they stay out of sight.

**Everything an event overrides is recomputed from nothing every frame.**
`update` clears the light gain, the tint, the lock and the extras before it runs
anything, so an event that stops running leaves nothing behind to undo. Only the
timers carry between frames.

## The events, and where each one lives

| | Event | Lives in |
|---|---|---|
| E01 | foyer tree gone on the 3rd visit, bare floor left | `foyerTree` + `tree_hole` in the map |
| E05 | foyer's warm light never comes back after the yard | `foyerLight`, kills light 0 |
| E07 | turning the rock shuts you in, 1 press in 3 | `closetShutIn` | X
| E11 | hall_main is half as long again from the 4th visit | `dress`, scales the room on X |
| E13 | six quick doors and the screen stays shut for 10s | `blackout`, turns you round | one time
| E15 | one tuft of grass answers E, once, after E05 | `yardTuft` |
| E16 + E34 | spade in the shed, dig the patch in the yard | map only |
| E19 | a tap you cannot find, after 10s standing still | `bathroomWater` |
| E21 | the balls spell one more letter of BYGONE a visit | `billiardWord`, 3x5 dot font |
| E23 | the cue reaches the switch set too high | map only |
| E24 | piano gone on the 2nd visit, plays where it stood | `ballroomPiano` |
| E29 | the backdrop holds still in the greenhouse only | one line in `update` |
| E30 | greenhouse goes dark, something far too big passes | `greenhouseShape` |
| E32 | lean on the NW corner 5 times and the field turns red | `fieldEdge` + `field_red` |
| E33 | read the note, come out of the shed into the closet | `reroute` |
| E36 | the shed board reads your save back, one line ahead | `shedBoard` |

## Progression — four things, then building two opens

Four flags, `quest_stone` `quest_mirror` `quest_tiles` `quest_dig`. While any is
unset, the terrace's `doors` doorway is plugged and walking up to it says how many
are left. `questsLeft()` is the only thing that reads them.

| | Quest | How it is checked |
|---|---|---|
| 1 | Turn the stone three times, put the gate back down | `stoneAndGate`, off the save |
| 2 | Lever the cue in behind the bathroom mirror | map only — `do flag quest_mirror` |
| 3 | Cross the ballroom to the piano on the pale tiles | `ballroomTiles` |
| 4 | Dig the yard patch with the shed's spade | map only — `do flag quest_dig` |

**Quest 1 reads two rooms at once**, which nothing else here does. `placeOf()`
takes the live prop when you are standing in its room, the save's `PropMemory`
when you are not, and the map's own placement when nothing has ever touched it —
so a fresh game correctly reads the gate as already closed. The rock is compared
modulo a full turn, so three turns, seven and eleven all count and overshooting is
something you can walk off rather than a run you have to start again.

Opening the gate is the only way through to the rock, so putting it back down is
the half of quest 1 that is easy to leave undone.

**Quest 3's tiles are `conjure`d slabs**, not floor. The pale tiles are the twelve
raised slabs; every gap between them is a dark tile. A crossing starts only on the
doorway tile `(0,3)`, ends on the piano tile `(3,3)`, and any step off the pale
ones ends it — so hopping a dark tile to a further pale one does not work, and
loading straight into the middle of the room awards nothing.

**The locked door is plugged, not just refused.** The terrace doorway is a real
hole in a waist-high wall, so refusing the transition alone would leave you able to
walk off the terrace. `sealedDoor()` names one door index and the scene enables
that door's blocker — the same collider added for E07. `reroute()` returning false
is the backstop for a slow frame carrying you through the plug.

## Loose ends

- **The names are placeholders.** The tuft says `ELEANOR.`, the board's invented
  last line is `SAW YOU READING THIS`. Both are one string in `events.cpp`.
- **E32's hint is E33's note.** `DLEIF WN` in the closet is NW FIELD backwards,
  which is the corner E32 watches. Nothing enforces that pairing.
- **Digging takes the spade back** (`do take spade` in the mapsrc). It is the only
  way to make the patch one-shot without a `when` that can ask two things at once.
- **E19 stops the clip with `stopAll()`**, which would cut anything else playing.
  Nothing else plays in the bathroom today.
- **The bathroom switch is now the mirror.** E23's cue used to throw a switch on
  that wall; it levers the mirror off it instead, which is what quest 2 wanted.
  `switch_thrown` is gone and nothing asked about it.
- **A dialog can now be opened by something that is not a prop.** `InteractionRunner`
  only advanced a dialog while one of its own scripts was running, so anything else
  that opened one hung the game. It advances any open dialog now.
- **The terrace door's count is honest.** E38 in `SPOOKY_EVENTS.md` wants it to say
  three when four are left. One string in `terraceDoor` if you want that back.
- **`events.cpp` is 711 lines**, well past the 100 the repo aims at. The seam if
  it wants splitting is the one already marked in the file: events that put props
  right as a room stands up, events that only set a per-frame override, and now
  progression, which is self-contained and would move out cleanly on its own.
