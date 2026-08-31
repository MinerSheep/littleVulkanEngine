# Sounds — what came from where

The audio in `sounds/` is cut from the **Sonniss GDC 2026 Game Audio Bundle**. The bundle
ships everything under long descriptive names, which are useful when you are picking through
ten thousand files and useless once one is in the game. Everything the game plays has been
renamed to what it *does*.

This file is the record of which is which, so a clip can always be traced back to the file it
came out of.

---

## The renames

| Now | Was |
|---|---|
| `sounds/ambience/clock_tick.wav` | `CLOCKTick_Crooked Antique Clock_344 Audio_Antique Clocks.wav` |
| `sounds/ambience/wind.wav` | `WINDDsgn_EXT, Eye Of The Storm_344 Audio_Extreme Winds Vol 1.wav` |
| `sounds/ui/menu_toggle.wav` | `Interface Percussion Snap.wav` |
| `sounds/ui/menu_accept.wav` | `Interface Accept Glassy Snap.wav` |
| `sounds/stingers/quest_done.wav` | `Transition Braam Slow Dark Creepy.wav` |

Nothing else was renamed. `drone`, `footsteps`, `lever`, `piano`, `step`, `stone` and `water`
predate the bundle and are still loose in `sounds/`.

## What plays them

| Clip | When |
|---|---|
| `clock_tick` | runs under **Hall_Main** the whole time you are in it, for the grandfather clock |
| `wind` | runs under **Yard**, **Field** and **Field_Red**, the three rooms with no roof |
| `menu_toggle` | the pause menu going up or away |
| `menu_accept` | moving about inside it -- map, options, and backing out of either |
| `quest_done` | one of the four steps of the poem landing, the first time each one lands |

`quest_done` is hung off the flag rather than the prop, so it fires wherever a `quest_` flag is
first set -- two of the four are set by `do flag` lines in the mapsrc and two in
`eventspetscop.cpp`, and all four go through it.

---

## How the folders work

    sounds/            one-shots, loaded and decoded at startup
    sounds/ui/         one-shots, the same
    sounds/stingers/   one-shots, the same
    sounds/ambience/   beds, streamed off the disk and never decoded
    sounds/_raw/       bundle files nothing plays yet, kept out of the way

**A clip's name is its file name without the extension.** `menu_accept.wav` is played as
`play("menu_accept")`. Rename a file and every reference to it breaks, which is the reason for
the table above.

**`loadFolder` does not walk into subfolders.** Each folder is asked for by name in
`main.cpp`, so adding a folder means adding a line there. That is deliberate -- see below.

### One-shots vs beds

A one-shot is decoded into memory up front and can play several times at once. A bed is
**streamed** off the disk, runs on a single voice, and loops.

The split is not stylistic, it is about size. `wind.wav` is **200 MB** on disk and
`clock_tick.wav` is **34 MB**; decoding either would mean holding the whole recording in
memory as float samples, which is several times the file size. So beds are registered by hand
in `main.cpp` with `loadLoop(name, path)` rather than swept up by `loadFolder`, and the folder
they live in is deliberately one that nothing scans.

To add a bed: drop the file in `sounds/ambience/`, add a `loadLoop` line, and call
`LveAudio::loop(name, volume)` while you want it running. `loop` is safe to call every frame --
it leaves a running bed alone rather than restarting it, which is how `RoomScene::keepRoomBed`
keeps one alive.

### `sounds/_raw/`

Bundle files that are in the repo but not in the game. Nothing loads this folder, so a file
parked here costs nothing at startup:

- `DSGNSynth_Dark Loop Mystic Forest Tonal Steady Synth_ESM_SGA3.wav` (34 MB)
- `METLImpt_Metal Bangs, Metal Hits, Banging On Doors_344 Audio_Haunting Ambiences Vol 3.wav` (208 MB)
- `Ting Coins.wav`

They were sitting loose in `sounds/`, where `loadFolder` was decoding all 242 MB of them at
every launch for nothing.

---

## Two things worth watching

**Anything loose in `sounds/` is decoded at startup.** `FFG002.wav` (23 MB) and `FFW003.wav`
(19 MB) are there now and are being decoded every launch. If they are meant for something,
rename them and file them; if they are not, move them to `_raw/`.

**The bundle zips are still in `sounds/`.** Three of them, 3.7 GB, untracked and not
gitignored. They cost nothing at runtime -- `loadFolder` only looks at `.wav`, `.ogg`, `.mp3`
and `.flac` -- but they are worth moving out of the assets folder or adding to `.gitignore`
before anything tries to commit them.
