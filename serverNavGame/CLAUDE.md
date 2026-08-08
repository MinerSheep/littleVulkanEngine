# serverNavGame

A Vulkan project split into a reusable **engine** static library and a **game** that links it.
Derived from Brendan Galea's "Little Vulkan Engine" tutorial series, restructured so the engine
is built as `libvulkan_engine.a` and the game consumes it.

## Comment style

Write comments in plain, simple language — describe what the thing *does* in the game, not what
the API contract is.

- Use the game's own words (fireball, collider, light), not qualified C++ names
  (`PlayerAbilityComponent::onImpact`).
- One idea per line. Two short lines beat one long sentence that wraps.
- No clause-stacking ("which both consumes X and leaves Y") — just say the effect.
- No period at the end of a comment.

```cpp
// bad
// Answers PlayerAbilityComponent::onImpact: true once a shot has hit something
// solid, which both consumes the projectile and leaves its light behind

// good
// Returns true when the fireball hits a collider
// onImpact makes fireball disappear and leaves a light behind
```

## File size — keep files short

**Prefer files under 100 lines.** A file that has grown past that is usually doing more than one
job, and the job that got added last is normally the one that wants its own file.

**If an implementation could sensibly be split into another file, ask before writing it.** Do not
split unprompted, and do not pile onto a long file without mentioning it either. Say which pieces
would move, roughly how many lines each new file would be, and what the original drops to — then
let the choice be made. The answer is often "yes, but not that far", so offer a couple of
granularities rather than one.

New game files go beside the ones they belong with (`game/src/petscop/` for the room map), not in
a new folder.

Worked example — `room_scene.cpp` reached 619 lines carrying three separate jobs, and split into:

| file | lines | job |
|---|---|---|
| `petscop/prop.hpp` / `.cpp` | 85 / 76 | what a prop *is*, and how it moves itself |
| `petscop/interactions.hpp` / `.cpp` | 57 / 162 | reading E and running a prop's actions |
| `petscop/room_scene.cpp` | 405 | standing a room up, and the per-frame draw lists |

The seam that made it work was **data away from behaviour**: `Prop` owns its own `Motion` and knows
how to `refreshBox()`, so `InteractionRunner` only has to decide *where* things go, never how they
get there. `InteractionRunner` holds a `std::vector<Prop>*`, not pointers into it, so a room swap
that refills the vector cannot strand it — unlike `CollisionSystem`, which does hold element
pointers and must be cleared first.

## Toolchain / environment

- **Built and run under WSL2 (Ubuntu), GCC 9** (`/usr/include/c++/9`). Source lives on the Windows
  side (OneDrive path) but compilation/execution happen in WSL — ASan traces show `/mnt/c/...` paths.
- **Vulkan SDK** via `VULKAN_SDK_PATH` (defined in `engine/.env`), **GLFW3** via `pkg-config`, links `-lvulkan`.
- Game is compiled with **AddressSanitizer**: `-std=c++17 -g3 -O0 -fno-omit-frame-pointer -fsanitize=address`.
- Engine is compiled **without** ASan / `-g` / `-O` flags (`-std=c++17 -I. -Ilibs -Iinclude ...`).

## Build & run

```sh
# 1. Build the engine static lib first
cd engine && make          # -> engine/libvulkan_engine.a

# 2. Build the game (links ../engine/libvulkan_engine.a)
cd ../game && make          # -> game/game

# 3. Run FROM THE REPO ROOT, not from game/
./game/game
```

Scenes load models with CWD-relative paths like `"models/hilt1.obj"`, and there is no `game/models/`.
Running `./game` from inside `game/` throws `Cannot open file [models/...]` in the OBJ scene's
`loadModels()`, which `main` catches and turns into exit code 1 *before the skinned scene ever loads*.
Shaders resolve either way, because they go through `getExecutableDir() + "/../shaders"`.

## Layout

- `engine/include`, `engine/src` — `LveWindow` (GLFW), `LveDevice`, `LveRenderer`, `LveSwapChain`,
  descriptors/pools (`LveDescriptors`), `LveBuffer`, `LveModel`, `LveCamera`, render systems
  (`systems/simple_render_system`, `systems/point_light_system`), all orchestrated by **`LveEngine`**
  (a function-local singleton: `lve::LveEngine::instance()`).
- `game/src` — `main.cpp` builds a `LevelScene` (a `GameObject` collection), plus
  `keyboard_movement_controller` and `servernav_sim`.
- `game/dep` — old `first_app.*`, kept for reference (not part of the current build).

### Game object / component split

`lve_game_object.hpp` holds only the core: `Component`, `GameObject`, `TransformComponent`.
Two things that used to live there now have their own headers, so a file that doesn't need them
doesn't rebuild when they change:

- `ui_object.hpp` — `RectTransformComponent` (screen-space placement + `UIAnchor`)
- `light_component.hpp` — `PointLightComponent`

`collision_system.hpp` is the physics resolver. `ColliderComponent` owns a box and
`RigidbodyComponent` owns velocity, but neither one resolves a hit — `CollisionSystem` does.
A scene registers its colliders once (`addStatic` / `addDynamic`) and calls `settleAll()` each frame
after `updateComponents`. It stores raw pointers, so register only after the vectors holding the
props and bodies have stopped growing (see `SkinnedDemoScene::registerColliders`).

## Engine ↔ game data flow

The game/engine boundary is **`engine/include/lve_scene.hpp`** + **`engine/include/lve_frame_info.hpp`**.

- `LevelScene : public lve::LveScene` fills, each frame, the base-class members
  `ubo` (`GlobalUbo`), `renderItems`, `UIrenderItems`, `lightItems`, `skinnedRenderItems`.
- `main` points `engine.activeScene` at a scene, then calls `engine.render()` — **no arguments**; it
  reads `activeScene` itself. `render()` packs the scene's lists into a `FrameInfo`, hands that to the
  render systems, and writes `scene.ubo` into the per-frame UBO buffer.
- `RenderItem` / `UIRenderItem` / `LightRenderItem` reference `LveModel*` (non-owning).

## Build: both Makefiles track header dependencies (was a critical gotcha)

Both `game/Makefile` and `engine/Makefile` now have `-MMD -MP` and `-include $(DEPS)`, so editing a
header rebuilds every `.o` that includes it. **Editing an engine header no longer needs a manual
clean rebuild.**

`engine/Makefile` used to be the odd one out, and it caused real memory corruption. Its only rule was
`$(TARGET): $(SOURCES)` with the `$(HEADERS)` prerequisite commented out — and that prerequisite
would not have helped anyway, because it globbed `find src -name "*.hpp"` while the engine keeps its
headers in `include/`, so it always expanded to nothing. It also compiled every source into one flat
pile in `engine/` and archived with `ar rcs $(TARGET) *.o`, which swept up orphaned `.o` files left
behind by renamed or deleted sources.

So editing an engine header rebuilt nothing, and the engine kept using the old struct layouts while
the game (which did track headers) rebuilt against the new ones. The two halves then **disagreed on
struct layout** (an ODR / ABI mismatch), and the symptom was memory corruption nowhere near the code
you were looking at.

It now builds one `.o` per source into `engine/obj/`, keeping the `src/` subdirectory structure so
two sources may share a basename, and archives an explicit file list rather than a glob.

**Worked example of what this used to cause (the July 2026 `gameObjects` FPE):** `lve_scene.hpp` gained a
`std::vector<UIRenderItem> UIrenderItems` member, which grew `sizeof(lve::LveScene)`. Because
`LevelScene::gameObjects` sits right after the `LveScene` base subobject, its offset shifted. With a
stale object file linked in, `gameObjects` was read from the wrong bytes → `bucket_count() == 0` →
`emplace` computes `hash % 0` → **AddressSanitizer: FPE** in `std::_Hashtable::_M_bucket_index`.
The bug is *not* in `loadModels`; the map is corrupt the moment `LevelScene` is constructed.
`std::cout << gameObjects.bucket_count()` printing `0` is the tell — a freshly default-constructed
libstdc++ `unordered_map` always reports `1`, never `0`.

Legacy `engine/*.o` files from the old flat build are still committed to the repo. They are no longer
read by anything — `ar` now takes an explicit list from `engine/obj/` — so they can be
`git rm --cached`'d whenever you get round to gitignoring build outputs.

## The room map (`game/src/petscop`)

A map is several rooms and the doors between them. One room is live at a time; walking into a doorway
fades out, swaps the room, and fades back in.

- **`maps/*.mapsrc`** — hand-written. Rooms are declared as boxes (`size 12 8`, `height 3`,
  `door front north 0`) and joined with `link foyer.front hall.back`.
- **`tools/build_map.py`** — compiles that into `maps/*.map`, filling in the floor and the wall
  pieces around each doorway, resolving links, and baking each door's arrival spawn point. It
  validates at build time (every link names real doors, every preset has a `models/<name>.obj`) and
  exits non-zero on failure, so a broken map never reaches the game. Re-run it after editing a
  `.mapsrc` or a props layout.
- **`rooms/*.layout`** — optional per-room decoration, in the same format `LveSceneEditor` saves.
  The generator handles floors and walls; the editor still handles props.
- **`RoomScene`** plays the compiled map. It fits the existing `LveScene` contract with no engine
  header change.

Two things in `RoomScene` are load-bearing and easy to break:

1. **`enterRoom` is the only place the room is rebuilt**, and it runs only at the bottom of the fade —
   after `settleAll()` has returned, never mid-frame. It calls `collisions.clear()` *first*, because
   `CollisionSystem` holds raw pointers into `props`, then refills and registers last.
2. **The door you arrive through starts disarmed** and re-arms once you step clear of it. Without
   that you bounce straight back through the door you just came out of.

Door triggers are never registered with `CollisionSystem` — you are meant to walk through them.

## Other known issues / footguns

- **Engine built without ASan, game with ASan.** Layout is unaffected, but ASan can't instrument
  engine code, so overflows inside the engine go undetected. Prefer building the engine with matching
  flags for debug builds.
- **Low-level engine code reaches *up* into the `LveEngine` singleton.** `LveModel::createModelFromFile`,
  `LveSkinnedModel`, and `LveTextRenderer` all call `LveEngine::instance()` to get the device or the
  aspect ratio. That inversion is why they can't be built or tested without a live engine. Passing a
  `LveDevice&` in would free a dozen files.
- **`getExecutableDir()` is declared in `lve_frame_info.hpp`** at global scope, outside `namespace lve`
  — a filesystem helper living in the render-data contract header. Defined in `lve_frame_info.cpp`.
- **`LveEngine::getRenderPass()` returns a default-constructed `VkRenderPass()`** — a stub handing back
  a null handle to anyone who trusts it.
- **`LveEngine::cleanup()` runs twice**, once from `shouldClose()` and again from the destructor.
- **`LveBuffer::getAlignmentSize()` returns `instanceSize`, not `alignmentSize`.** Nothing calls it
  today, so it's latent — but it will lie to the first caller.
- **Compiled artifacts (`.o`, `.a`, the `game` binary) are committed to the repo**, so `git status`
  churns with binaries and it's easy to run a stale binary. Consider `.gitignore`-ing build outputs.

### Fixed (was previously listed here)

- `engine/Makefile`'s `clean` now does `rm -f *.o libvulkan_engine.a`.
- `LveEngine::getAspectRatio()` is one non-const method; the `0.0f` `const` overload is gone.
- `LveEngine::init()` uses `globalDescriptorSets.resize(N)`. It used to `reserve(N)` and then write
  `globalDescriptorSets[i]`, which was out of bounds on every startup.
- `PointLightSystem::update` drops lights past `MAX_LIGHTS` instead of asserting. The list is sorted
  by distance first, so the ones dropped are the furthest away.

## Where the project stands

`ARCHITECTURE.md` rates every `.cpp`/`.hpp` in `engine/` and `game/` on coupling, cohesion,
architectural fit, and testing. Read it before a refactor — it names the two units worth worrying
about (`lve_engine` and `scenes/skinneddemoscene`) and lists what is unit-testable today.

**There are no tests.** No test target, no test directory, no framework.
