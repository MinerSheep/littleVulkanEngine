# Events — what the game actually does

Every event that is **built and running**. Nothing here is a proposal; if it is on this sheet you
can walk into it. Ideas that are not built live in `GAME_IDEAS.md`.

The house events are in `game/src/petscop/eventspetscop.cpp`, the forest ones in
`eventsforest.cpp`, and both hang off `EventDirector`. Events come in two shapes: some put props
right as a room stands up (`dress`, `onEnterRoom`), the rest set an override that is worked out
again from nothing every frame (`update`), so nothing has to be undone when one stops.

> **The `When` column is built.** A daytime event does not happen at night and a night one does
> not happen by day. The hour comes off the machine unless the settings say otherwise — see
> **Day and night** at the bottom.

---

## Seeing one without waiting for it

| Want | Do |
|---|---|
| A particular hour, in game | pause, OPTIONS, and press E on **TIME** to cycle AUTO / DAY / NIGHT |
| A particular hour, from the shell | `PETSCOP_HOUR=2 ./play.sh -n` — `2` is night, `13` is full daylight |
| The game to shut itself down | `PETSCOP_QUIT_AFTER=10 ./play.sh -n -t 40` |
| A photograph as a picture | `python3 tools/photo2png.py saves/photos/photo_0001.txt out.png 8` |
| Most forest events | they gate on `@runs`, so relaunch two or three times |
| A specific event | its flag is in `saves/*.save` — add or remove the line and relaunch |

Three harnesses, each with its build line in its own header comment: `tools/test_daylight.cpp`,
`tools/test_photo.cpp`, `tools/test_other.cpp`.

---

# The house · area one

## Foyer

| | When | What happens |
|---|---|---|
| **E01** | Day | The third time you walk in, the tree is gone. A bare slab is left where it stood and pressing E on it says somebody took it — and once it is taken it stays gone at any hour. |
| **E03** | Both | The camera pulls one step further back every fourth visit, by day. After dark it stops holding still and drifts along behind you instead. |
| **E05** | Night | Come back from the yard and the warm key light over the foyer is out for good, leaving only the cold blue fill. |
| **E39** | Night | Late on, the save stops starting you where you left off — you wake on the terrace, stood in front of the doors, looking straight at them. |

## Closet

| | When | What happens |
|---|---|---|
| **E07** | Night | Now and then, turning the rock shuts the closet. The doors lock for twenty seconds and the orange light fades out over the first eight, then it lets you go with no comment. Once per save. |

## Hall and Hall_Main

| | When | What happens |
|---|---|---|
| **E09** | Night | The light nearest the door you came in by goes out behind you, and lifts again as you walk back toward it. |
| **E10** | Day | Walk the whole length of the hall and every place you stopped on the way stands up behind you at once, as flat dark slabs. |
| **E11** | Day | From the fourth visit the hall is a different length, and stays that way. Walls, doors, spawn points and lights all come out of the stretched room data, so everything is further apart. |
| **E12** | Night | One time only, a north door out of the hall does not lead out of the hall — you come back in at the far end, facing the way you were going. |
| **E13** | Day | Six doors taken inside 1.2 seconds of each other and the next fade holds black for ten seconds of footsteps. When it lifts he is facing the other way. |
| **X03** | Night | One frame of a face, in the middle of an ordinary walk down the hall. Once in the life of a save, nothing leads up to it, no sound on it. |

## Hall_West

| | When | What happens |
|---|---|---|
| **—** | Night | The yard is through the door at the end of the corridor, and now and then something out there gets hit. Quiet and flat, so it reads as being outside. Once a visit. |

## Yard

| | When | What happens |
|---|---|---|
| **E14** | Night | Every tuft you walk over is flattened out of sight and stays flattened in the save, so the yard slowly wears a path shaped like the way you always cross it. |
| **E15** | Day | Once the foyer has gone dark, exactly one tuft of grass answers E with a name, and never says it again. |
| **E17** | Night | After you dig, every later visit has one more patch of turned earth in it, somewhere you did not dig, and all of them are already empty. |

## Bathroom

| | When | What happens |
|---|---|---|
| **E18** | Day | Hear the water three times and the fourth visit has a sink on a wall the bathroom has never had. Finding it stops the water for good. |
| **E19** | Day | There is no sink and no tap. Ten seconds standing still starts water running somewhere, and any input cuts it mid-sample. |

## Billiard room

| | When | What happens |
|---|---|---|
| **E21** | Day | The balls on the table lay out one more letter every visit, and by the sixth they read BYGONE. |
| **E22** | Night | Once it is spelt they stop spelling and lay out a game already in progress instead, moved on every time you come back. |

## Ballroom

| | When | What happens |
|---|---|---|
| **E24** | Night | The piano is in the middle of the floor the first time and gone every time after — and walking through the space where it stood plays it anyway. |
| **E25** | Night | Three steps of the poem done and the piano is standing there again, moved, with the room lit from one side only. |
| **E27** | Night | Somebody walks the ballroom a beat behind you. The late set takes one more step after you have already stopped. |

## Greenhouse

| | When | What happens |
|---|---|---|
| **E28** | Night | After the shape goes over, a pane of glass is leaning on the north wall, and one more every visit until you are walking around the stack. |
| **E29** | Night | The moving backdrop is behind every room in the game. In the greenhouse, and only there, it holds still. |
| **E30** | Night | Third visit, three seconds in: every light dies, a flat blue wash comes up, and something far too large drifts over the roof. |

## Field

| | When | What happens |
|---|---|---|
| **E32** | Night | Lean on the north-west corner of the field enough times and it gives way, and you are in the field again, washed red, with slabs laid in a spiral where the grass was. |

## Shed

| | When | What happens |
|---|---|---|
| **E33** | Day | Read the note in the closet and the next time you walk out of the shed you come out in the closet instead, on the other side of the house. Once only. |
| **E35** | Night | A seam in the shed floor that does nothing at all, until the shed has put you out into the closet — then it tells you the closet is under here. |
| **E36** | Day | The board reads your own save back at you, every flag in caps, three to a line, and the last line is always one you do not have. |

## Anywhere in the house

| | When | What happens |
|---|---|---|
| **E40** | Night | Forty seconds stood still and he stops facing the way he was walking and turns to look at the camera. Any input and he snaps back mid-turn. |
| **E41** | Night | From the third step of the poem on, every room comes up dark for a quarter of a second after the fade has already finished. |
| **—** | Always | His own feet, in every room of both maps — one step per stride of walking, nothing at all while stood still. The ballroom answers a step late off the same beat. |

## The four steps of the poem

The terrace door into building two is plugged until all four are done.

| Step | What it takes |
|---|---|
| **Stone and gate** | The rock left three quarter-turns off where it started, and the slab back down across the closet doorway. Opening the gate is the only way in to the rock, so putting it back is the half people forget. |
| **The mirror** | Take the cue from the billiard room and use it on what it reaches. |
| **The tiles** | Cross the ballroom to the piano on the pale tiles only. Step off and the crossing is over, and it starts again from the doorway. |
| **The dig** | Take the spade from the shed and dig the marked patch in the yard. |

---

# The forest · area two

Most of these gate on how many times the game has been run (`@runs`), so they arrive over several
launches rather than all at once.

| | When | What happens |
|---|---|---|
| **F01** | Night | One way east out of the trees does not come out where it should. The room the other side says Foyer, it is not the one you remember, and the lever is the only thing in it that answers. |
| **F02** | Day | A man stands across the room and walks out of it. The screen goes dark as he appears, so even in daylight he is never more than a shape. |
| **F03** | Night | Once the man has been seen the hollow stops being lit by anything but you, and there is nothing behind the room at all. |
| **F04** | Night | A sign at the end of the line west, counting off how long it waited for you. |
| **F05** | Night | A tree in the ring answers E and does nothing. From then on it walks after you, room to room, at a pace you can always outrun. |
| **F06** | Night | Something goes missing from your pockets on the way into a room, and it is lying on the ground in the one you just walked out of. Pressing E on it puts it back. |
| **F07** | Night | One way out is shut because you do not want to take it, and the other one has an opinion about that. |
| **F08** | Night | The doorway you came in by fills itself in once you are a few steps off it. A different doorway takes you anyway and you come out grey, walking through everything and held up by nothing — the settings are the way back into your body. |
| **F09** | Night | The room is stood on its head: everything in it mirrored across the middle and turned over, with the doorways left where they were. |
| **F10** | Night | Some runs do not start where the last one stopped. One of the places you can wake up is a room with no name on the map, and it has your controls the wrong way round. |
| **F11** | Day | Two trees lie across the clearing, one over each door, and there is no way on. Eight seconds stood still in the room is enough for neither of them to be there any more. |
| **F12** | Day | Somebody the other side of the room walks your walk back at you, step for step, with the camera pulled back so you can see you both. Meeting him in the middle ends the run — and the next one opens on him up against the camera, with a bar across his eyes, before he is not there. |
| **F13** | Always | The name in the corner of the screen stops being a place. In the forest this never goes back. |
| **F14** | Night | The camera lets you walk away and stays looking at the door you came in by, catching up only once you are nearly out of the room. |
| **F15** | Night | A doll at the top of the room. Press E and it says nothing, and it has turned a little further every time you look away. |
| **F16** | Night | Somebody is standing at the north wall with their back to the room, and there is nothing to press E on. |
| **F17** | Night | Come back to the game and it greets you, and there is a note by the shrine that was not there, addressed to you. |
| **F18** | Both | The bridge in the north is blocked all day and open after dark. |
| **F19** | Day | The pause menu has one button left, and it does not say exit. |

## The forest after dark

Night is not only when most of these fire — it changes how the forest is lit and shot, in every
room, with no event needed:

- **The camera comes off its peg** and follows the player, the way it does for F03 today.
- **The room's own lights are off**, and the only light in the clearing is on the player.
- **The key in Tent_Camp is not there.** The forest withholds at night, where the house withholds
  by day — the cue and the spade are the daytime half of the same idea.

---

# The systems

Five of the thirteen in `GAME_IDEAS.md` are built. These are not single events — they are things
the director can now do, and events are written against them.

| | What it gives |
|---|---|
| **X03 · the canvas** | Pictures with no texture pipeline, one UI quad per pixel. Drives the map you carry in the pause menu and X03's one-frame insert. |
| **X04 · the post pass** | The room is drawn into an image of its own and put on screen through one shader, so the whole picture can be wobbled, drained, inverted, grained, vignetted, colour-split or held still. |
| **X07 · the other one** | Somebody plays your save while the game is closed — one action per real hour you were away, resolved before the first frame. He walks, presses things, and takes what he finds, and puts it back down in a room you have been in. |
| **X11 · day and night** | The machine's clock lights the house. Bright and quiet by day, dark and busy at night, with dawn and dusk as ramps rather than switches. |
| **X12 · photographs** | Six camera stands take a real screenshot of the room, filed in `saves/photos/` and read back from a pause-menu page. One picture has him standing in it, because the game drew him for that one frame. |

---

# Day and night

## How it works

**One test, used everywhere.** `byDay()` and `byNight()` on the director read `sky.day`, which is
true above three-quarters sun — roughly 06:30 to 18:30, the same threshold that hides the quest
props. There is no band in between, so every hour belongs to one half or the other.

**The hour can be chosen.** The pause menu's OPTIONS page carries **TIME**, cycling AUTO / DAY /
NIGHT. The choice rides in the save as `@timeofday` and the director reads it in `readSky`, so it
survives a relaunch. `PETSCOP_HOUR` still overrides the machine clock for a shell run, and the
setting overrides both.

**The change is walked into, not switched to.** `readSky` keeps a `sunShown` that moves toward
the hour at a fixed rate, and the whole of `Daylight` is rebuilt from it by `daylightFromSun`, so
picking DAY at two in the morning takes about a second and a half of lights coming up, the
backdrop going pale and the ambient warming. A scene that has just come up snaps instead of
fading in from the wrong sky.

**Nothing is ever standing there dead.** A prop is either in the room and answers when you press
E, or it is not in the room. By day `daylightHides` takes the **cue, spade, lever, mirror, dig
patch and piano** out of the room entirely, and the forest's **key** goes the same way at night.
The **gate** is never touched — it is the slab over the closet doorway, and hiding it would open
the closet.

**The rock is the exception.** It stays in the closet at every hour and always answers. By day it
says it seems stuck in place and does not turn; after dark it offers a spin and turns. That is
the one prop whose *answer* changes with the hour rather than its presence.

## What was decided along the way

- **Footsteps and F13 are `Always`.** Feedback, not events.
- **E10 keeps counting, and only stands up by day.** The places you stopped are recorded whatever
  the hour, or switching to day mid-room would show nothing.
- **E14 keeps its names.** `yardEarth` names every tuft at any hour, because the names are what
  make them rememberable; only E17's patches wait for dark.
- **Nothing can strand you.** E32 only gives the corner way at night, but Field Red always draws
  and always lets you out. F01's false foyer is never gated — only being sent there is — because
  its lever is the one door. Leaving NOT HERE NOT ANYWHERE works at any hour; only waking up
  there is a night thing.
- **F12's stare is ungated.** The mirror is a daytime event, but the face on the run after the
  faked crash plays whatever the hour is. Gating it would leave `crash_seen` set with nothing to
  clear it.
- **F03 is now the black behind the room.** Camera follow and a light carried on the player is
  what every clearing does after dark, so that is all F03 still owns.
- **F18 stacks its two rules.** Three runs before the bridge comes back at all, and then only
  after dark. There is no first-night crossing.
- **Two doors can be plugged at once.** F11 needs both ways out blocked, so `sealedDoor()` became
  `sealedDoor(index)` and the director keeps a second slot.
