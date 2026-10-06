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
| `sounds/ambience/forest.wav` | `DSGNSynth_Dark Loop Mystic Forest Tonal Steady Synth_ESM_SGA3.wav` |
| `sounds/stingers/yard_bang.wav` | `METLImpt_Metal Bangs, Metal Hits, Banging On Doors_344 Audio_Haunting Ambiences Vol 3.wav` |
| `sounds/ui/menu_toggle.wav` | `Interface Percussion Snap.wav` |
| `sounds/ui/menu_accept.wav` | `Interface Accept Glassy Snap.wav` |
| `sounds/stingers/quest_done.wav` | `Transition Braam Slow Dark Creepy.wav` |

Nothing else was renamed. `drone`, `footsteps`, `lever`, `piano`, `step`, `stone` and `water`
predate the bundle and are still loose in `sounds/`.

### The two that are cuts, not copies

`forest.wav` and `yard_bang.wav` are not the bundle files under a new name — both sources are
enormous and neither is usable whole.

- **`forest.wav`** — 40 s taken from 5 s into the source, resampled 96 kHz/24-bit down to
  48 kHz/16-bit, with the tail equal-power crossfaded back into the head so the loop has no
  seam. 34 MB becomes 7.3 MB.
- **`yard_bang.wav`** — 2.5 s. The source is 208 MB of door bangs; the cut is the loudest
  transient in it (91.5 s in), started 0.15 s early and given a 250 ms fade-out. 0.46 MB.

Both sources are untouched in `_raw/`, so either cut can be redone.

## What plays them

| Clip | When |
|---|---|
| `clock_tick` | runs under **Hall_Main** the whole time you are in it, for the grandfather clock |
| `wind` | runs under **Yard** and **Field**, the two open rooms |
| `forest_bed` | runs under every room of the **forest** map, quiet, keyed on the map rather than the room |
| `yard_bang` | a chance each second of a bang carrying in from the Yard while you stand in **Hall_West**, once a visit |
| `menu_toggle` | the pause menu going up or away |
| `menu_accept` | moving about inside it -- map, options, and backing out of either |
| `quest_done` | one of the four steps of the poem landing, the first time each one lands |

`quest_done` is hung off the flag rather than the prop, so it fires wherever a `quest_` flag is
first set -- two of the four are set by `do flag` lines in the mapsrc and two in
`eventspetscop.cpp`, and all four go through it.

`RoomScene::keepRoomBed` picks the bed each frame and runs at most one. Walking from Hall_Main
into the Yard swaps the clock for the wind; walking into a room with no bed stops it.

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

The split is not stylistic, it is about size. `clock_tick.wav` is **34 MB** on disk; decoding
it would mean holding the whole recording in memory as float samples, which is several times
the file size. So beds are registered by hand in `main.cpp` with `loadLoop(name, path)` rather
than swept up by `loadFolder`, and the folder they live in is deliberately one that nothing
scans. `wind.wav` and `forest.wav` are small enough to decode now that they are cut down, but
they stay beds because they loop, and looping is what `loadLoop` is for.

To add a bed: drop the file in `sounds/ambience/`, add a `loadLoop` line, and call
`LveAudio::loop(name, volume)` while you want it running. `loop` is safe to call every frame --
it leaves a running bed alone rather than restarting it, which is how `RoomScene::keepRoomBed`
keeps one alive.

### `sounds/_raw/`

Bundle files that are in the repo but not in the game. Nothing loads this folder, so a file
parked here costs nothing at startup:

- `DSGNSynth_Dark Loop Mystic Forest Tonal Steady Synth_ESM_SGA3.wav` (34 MB) — source for `forest.wav`
- `METLImpt_Metal Bangs, Metal Hits, Banging On Doors_344 Audio_Haunting Ambiences Vol 3.wav` (208 MB) — source for `yard_bang.wav`
- `Ting Coins.wav`

They were sitting loose in `sounds/`, where `loadFolder` was decoding all 242 MB of them at
every launch for nothing.

---

## Two things worth watching

**The `wind.wav` source is gone.** `sounds/ambience/wind.wav` is a 40 s loop, but the 199 MB
bundle original it was cut from was overwritten in place by the script that made the loop
rather than being read and left alone. `_raw/` never got a copy. The loop in the game is fine;
what is lost is the ability to re-cut it differently. OneDrive version history on the original
path, or a re-download of the bundle, are the two ways back. Every cut since then writes to a
new path and never to its own input.

**Anything loose in `sounds/` is decoded at startup.** `FFG002.wav` (23 MB) and `FFW003.wav`
(19 MB) are there now and are being decoded every launch. If they are meant for something,
rename them and file them; if they are not, move them to `_raw/`.
