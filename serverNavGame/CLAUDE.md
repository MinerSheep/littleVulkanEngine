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

## ⚠️ Critical build gotcha: the ENGINE has no header-dependency tracking (ODR/layout crashes)

`game/Makefile` is **fixed** — it has `-MMD -MP` and `-include $(DEPS)`, so editing a header rebuilds
every game `.o` that includes it.

`engine/Makefile` is **not**. Its only rule is `$(TARGET): $(SOURCES)` (the `$(HEADERS)` prerequisite
is still commented out), and it archives with `ar rcs $(TARGET) *.o`, which globs whatever `.o` files
happen to be sitting there.

So **editing an engine header rebuilds nothing.** Some `.o` files stay compiled against the old header
while others get rebuilt against the new one, producing translation units that **disagree on struct
layout** (an ODR / ABI mismatch). Symptoms are memory corruption that has nothing to do with the code
you're looking at.

**Worked example (the July 2026 `gameObjects` FPE):** `lve_scene.hpp` gained a
`std::vector<UIRenderItem> UIrenderItems` member, which grew `sizeof(lve::LveScene)`. Because
`LevelScene::gameObjects` sits right after the `LveScene` base subobject, its offset shifted. With a
stale object file linked in, `gameObjects` was read from the wrong bytes → `bucket_count() == 0` →
`emplace` computes `hash % 0` → **AddressSanitizer: FPE** in `std::_Hashtable::_M_bucket_index`.
The bug is *not* in `loadModels`; the map is corrupt the moment `LevelScene` is constructed.
`std::cout << gameObjects.bucket_count()` printing `0` is the tell — a freshly default-constructed
libstdc++ `unordered_map` always reports `1`, never `0`.

**Fix (port what `game/Makefile` already does over to `engine/Makefile`):**

```make
CXXFLAGS += -MMD -MP
DEPS = $(OBJECTS:.o=.d)
-include $(DEPS)
```

Until that's in, after **any engine header change** do a full clean rebuild of **both** engine and
game so every TU shares one layout:

```sh
cd engine && make clean && make && cd ../game && make clean && make
```

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
