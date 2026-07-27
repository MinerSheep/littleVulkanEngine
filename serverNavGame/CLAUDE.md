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

# 3. Run (from game/, so relative asset paths resolve; loads models/*.obj)
./game
```

## Layout

- `engine/include`, `engine/src` — `LveWindow` (GLFW), `LveDevice`, `LveRenderer`, `LveSwapChain`,
  descriptors/pools (`LveDescriptors`), `LveBuffer`, `LveModel`, `LveCamera`, render systems
  (`systems/simple_render_system`, `systems/point_light_system`), all orchestrated by **`LveEngine`**
  (a function-local singleton: `lve::LveEngine::instance()`).
- `game/src` — `main.cpp` builds a `LevelScene` (a `GameObject` collection), plus
  `keyboard_movement_controller` and `servernav_sim`.
- `game/dep` — old `first_app.*`, kept for reference (not part of the current build).

## Engine ↔ game data flow

The game/engine boundary is **`engine/include/lve_scene.hpp`** + **`engine/include/lve_frame_info.hpp`**.

- `LevelScene : public lve::LveScene` fills, each frame, the base-class members
  `ubo` (`GlobalUbo`), `renderItems`, `UIrenderItems`, `lightItems`.
- `main` calls `engine.render(scene)`, which packs those into a `FrameInfo` and hands it to the
  render systems. `LveEngine::render` also writes `scene.ubo` into the per-frame UBO buffer.
- `RenderItem` / `UIRenderItem` / `LightRenderItem` reference `LveModel*` (non-owning).

## ⚠️ Critical build gotcha: no header-dependency tracking (causes ODR/layout crashes)

**Neither Makefile tracks header dependencies.** Object rules depend only on their `.cpp`:

- `game/Makefile`: `$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp`
- `engine/Makefile`: `$(TARGET): $(SOURCES)` (the `$(HEADERS)` prerequisite is commented out)

So **editing a header rebuilds nothing.** Some `.o` files stay compiled against the old header while
others get rebuilt against the new one, producing translation units that **disagree on struct layout**
(an ODR / ABI mismatch). Symptoms are memory corruption that has nothing to do with the code you're
looking at.

**Worked example (the July 2026 `gameObjects` FPE):** `lve_scene.hpp` gained a
`std::vector<UIRenderItem> UIrenderItems` member, which grew `sizeof(lve::LveScene)`. Because
`LevelScene::gameObjects` sits right after the `LveScene` base subobject, its offset shifted. With a
stale object file linked in, `gameObjects` was read from the wrong bytes → `bucket_count() == 0` →
`emplace` computes `hash % 0` → **AddressSanitizer: FPE** in `std::_Hashtable::_M_bucket_index`.
The bug is *not* in `loadModels`; the map is corrupt the moment `LevelScene` is constructed.
`std::cout << gameObjects.bucket_count()` printing `0` is the tell — a freshly default-constructed
libstdc++ `unordered_map` always reports `1`, never `0`.

**Fix (add auto dependency tracking to both Makefiles):**

```make
CXXFLAGS += -MMD -MP
DEPS = $(OBJECTS:.o=.d)
-include $(DEPS)
```

Until that's in, after **any** header change do a full clean rebuild of **both** engine and game so
every TU shares one layout.

## Other known issues / footguns

- **`engine/Makefile`'s `clean` does not remove `*.o` or `libvulkan_engine.a`** (only `a.out` and
  `shaders/*.spv`). Combined with `ar rcs $(TARGET) *.o` (globs whatever `.o` are present), stale
  engine objects survive a `make clean` and get re-archived. Fix `clean` to `rm -f *.o $(TARGET)`.
- **Engine built without ASan, game with ASan.** Layout is unaffected, but ASan can't instrument
  engine code, so overflows inside the engine go undetected. Prefer building the engine with matching
  flags for debug builds.
- **`LveEngine::init()` uses `globalDescriptorSets.reserve(N)` then writes `globalDescriptorSets[i]`**
  (`lve_engine.cpp`). `reserve` leaves `size() == 0`; `operator[]` is out of bounds (UB). Should be
  `resize(N)`.
- **`LveEngine::getAspectRatio()` has two overloads** — a `const` one returning `0.0f` and a non-const
  one returning the real value. Callers on the non-const singleton get the right value, but the `0.0f`
  `const` overload is a landmine. Consolidate to one method.
- **Compiled artifacts (`.o`, `.a`, the `game` binary) are committed to the repo**, so `git status`
  churns with binaries and it's easy to run a stale binary. Consider `.gitignore`-ing build outputs.
