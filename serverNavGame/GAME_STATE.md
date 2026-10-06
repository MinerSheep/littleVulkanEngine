# Game state

Items, flags, and how you left each room. Lives in `saves/petscop.save`.
Delete that file to start over.

## What persists on its own

Every **named** prop remembers its position, rotation, scale, hidden state, and
which way each toggle goes next. Walls and floors have no name and are skipped.

Pull the lever, walk to the hall, walk back — the gate stays open. It used to snap
shut, because `enterRoom` rebuilds every prop from the map.

The save also holds which room you were in, and the game starts you back there.

## Map vocabulary

New `do` lines:

    do give   <item> [count]     put an item in your pocket
    do take   <item> [count]     take one back out
    do flag   <name>             remember that something happened
    do unflag <name>

A `when` line gates every `do` under it, up to the next `when` or the next prop:

    when flag <name>      when noflag <name>
    when item <name>      when noitem <name>
    when always           back to ungated

### Example

    interact cube  0 0 0  0 0 0  1 1 1  name chest
      when noflag chest_open
      do say THE LID GIVES.
      do give red_key
      do flag chest_open
      do rotate self  0 0 -70  over 0.4
      when flag chest_open
      do say IT IS EMPTY NOW.

    interact cube  ...  name gate
      when noitem red_key
      do say LOCKED.
      when item red_key
      do say THE KEY TURNS.
      do take red_key
      do move gate  0 -2.6 0  over 1.1

Rebuild after editing:

    python3 tools/build_map.py maps/petscop.mapsrc -o maps/petscop.map

It warns when a `when` names a flag or item that no line anywhere ever sets.

## When it saves

- Every time you walk through a door
- On `RoomScene::cleanup()`

Not continuous. A crash loses whatever you did since the last door.

## From C++

`RoomScene` owns one `petscop::GameState state`.

    state.hasFlag("chest_open");
    state.setFlag("chest_open", true);
    state.hasItem("red_key");
    state.itemCount("coin");
    state.addItem("coin", -1);        // negative takes away

    petscop::readSave(path, state);   // false means no file yet, a new game
    petscop::writeSave(path, state);

## Files

| file | lines | job |
|---|---|---|
| `petscop/game_state.hpp` / `.cpp` | 51 / 86 | flags, items, prop memory, gating |
| `petscop/save_file.hpp` / `.cpp` | 16 / 139 | the text format, nothing else |

Edited in place: `map_loader` (parses the new lines), `interactions` (runs them,
skips gated ones), `room_scene` (loads, restores, autosaves), `build_map.py`.

## Worth knowing

- Only named props are remembered. Give a prop `name <id>` if it should be.
- A prop whose lines are all gated off draws no floating E.
- An item stack that hits zero is dropped, not held at zero.
- A save from an older version is ignored and you start fresh.
- `do give` with no `when` around it gives again on every press.
- A prop caught mid-move is written down where it was heading, not mid-slide.
- There is no `.gitignore`, so `saves/` will show up in `git status`.
- `do show` turns a collider back on even for a `pass` prop — pre-existing, in
  `interactions.cpp`. Restoring a save respects `solid`.
