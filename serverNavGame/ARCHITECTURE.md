# ARCHITECTURE — health check

A file-by-file rating of every `.cpp`/`.hpp` in `engine/` and `game/`, measured 2026-07-27.

A `.hpp` and its `.cpp` are rated together as one **unit**, because they succeed or fail together.
38 units, ~10,000 lines.

## How to read the scores

Every column runs **1 (bad) to 5 (good)**, so a low number is always the thing to look at.

| Metric                      | 5 means                                             | 1 means                                                             |
| --------------------------- | --------------------------------------------------- | ------------------------------------------------------------------- |
| **Coup** (coupling)         | Depends on almost nothing, and little depends on it | Tangled both ways — it pulls in the world and the world pulls it in |
| **Coh** (cohesion)          | Does one job, and the name says which job           | Does five jobs that only share a file                               |
| **Arch** (architecture)     | Its place in the project is obvious                 | You have to read it to find out why it exists                       |
| **Test** (unit tests)       | Has tests that prove it works                       | Has none                                                            |
| **Fail** (fails gracefully) | Bad input gets a documented, survivable answer      | Bad input is undefined behaviour or a hard crash                    |

**Test is 0 for all 38 units.** There is no test target, no test directory, and no test framework
anywhere in the repo. That column is not a per-file judgement — it is one project-wide fact, repeated
38 times. The useful signal is in **Fail**, and in the "What can be tested today" section below.

---

## Engine

| Unit                            |  LoC | Coup  |  Coh  | Arch  | Test  | Fail  | Notes                                                                                                                                           |
| ------------------------------- | ---: | :---: | :---: | :---: | :---: | :---: | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `lve_scene`                     |   29 |   5   |   5   |   5   |   0   |   5   | The game/engine contract. Smallest and best-designed file here                                                                                  |
| `lve_event`                     |   60 |   5   |   5   |   5   |   0   |   5   | Plain tagged struct + makers. Nothing to break                                                                                                  |
| `lve_utils`                     |   13 |   5   |   5   |   5   |   0   |   5   | One hash helper, borrowed from StackOverflow                                                                                                    |
| `lve_camera`                    |  145 |   5   |   5   |   5   |   0   |   4   | Pure glm math, zero Vulkan. Trivially testable                                                                                                  |
| `lve_window`                    |  103 |   5   |   5   |   5   |   0   |   4   | Clean GLFW wrapper. Throws on surface failure                                                                                                   |
| `lve_event_dispatcher`          |   78 |   5   |   5   |   4   |   0   |   3   | Header-only pub/sub. `unsubscribe` inside a handler would invalidate the loop in `emit` — nothing calls it yet, so latent                       |
| `lve_renderer`                  |  224 |   4   |   5   |   5   |   0   |   4   | Frame begin/end + swapchain recreate. One job, done well                                                                                        |
| `lve_pipeline`                  |  337 |   4   |   5   |   5   |   0   |   4   | Move ctor is `= delete` with a comment calling itself wrong                                                                                     |
| `lve_descriptors`               |  317 |   4   |   5   |   5   |   0   |   4   | Three related classes, builder pattern, consistent                                                                                              |
| `lve_buffer`                    |  263 |   4   |   5   |   5   |   0   |   3   | `getAlignmentSize()` returns `instanceSize`, not `alignmentSize`. Nothing calls it, so latent                                                   |
| `keyboard_movement_controller`  |   87 |   4   |   5   |   3   |   0   |   4   | Fly-camera input. Lives in the engine but is game input policy                                                                                  |
| `systems/skinned_render_system` |  185 |   4   |   5   |   4   |   0   |   3   | Narrowest of the three render systems                                                                                                           |
| `systems/point_light_system`    |  183 |   4   |   4   |   4   |   0   |   2   | `assert(lightIndex < MAX_LIGHTS)` — a hard crash on too much data. Scenes have to defend it themselves                                          |
| `systems/simple_render_system`  |  234 |   4   |   3   |   4   |   0   |   3   | Two pipelines, two jobs: 3D `render()` and 2D `renderUI()` in one class                                                                         |
| `lve_swap_chain`                |  521 |   3   |   4   |   4   |   0   |   4   | Big but focused. 7 throws with real messages                                                                                                    |
| `lve_text`                      |  260 |   3   |   5   |   4   |   0   |   4   | Glyphs to UIRenderItems. Missing glyph draws nothing, on purpose. Reaches into `LveEngine::instance()` for aspect ratio                         |
| `lve_model`                     |  327 |   3   |   4   |   3   |   0   |   3   | `createModelFromFile` reaches back into `LveEngine::instance()` for the device                                                                  |
| `lve_device`                    |  653 |   2   |   3   |   4   |   0   |   4   | 13 files depend on it. Device + queues + buffer helpers + image helpers bundled together                                                        |
| `lve_frame_info`                |   81 |   3   |   2   |   2   |   0   |   3   | Render structs, `GlobalUbo`, a `#define MAX_LIGHTS`, **and** `getExecutableDir()` — a filesystem helper at global scope in a render-data header |
| `lve_skinned_model`             |  785 |   2   |   2   |   3   |   0   |   3   | Five jobs: glTF parsing, vertex building, skeleton, animation sampling, bone SSBO                                                               |
| `lve_scene_editor`              |  362 |   2   |   2   |   2   |   0   |   4   | An editor tool inside the library every game links. Six jobs. `save()` does check the stream                                                    |
| `lve_engine`                    |  269 | **1** |   2   |   2   |   0   | **1** | See below                                                                                                                                       |

### `lve_engine` — the one to worry about

It scores lowest on both coupling and failure, and it is the file everything else bends around.

- Its header pulls in **9 project headers**; 12 files include it back.
- Five engine files and six game files call `LveEngine::instance()` to reach the device, the GLFW
  window, or the aspect ratio. Low-level code (`lve_model`, `lve_skinned_model`, `lve_text`) reaches
  *up* into the top-level singleton to get what it needs. That is the project's main architectural
  inversion, and it is why `lve_model` and `lve_text` cannot be built or tested without a live engine.
- `init()` does `globalDescriptorSets.reserve(N)` then writes `globalDescriptorSets[i]`
  (`lve_engine.cpp:52`). `reserve` leaves `size() == 0`, so every write is out of bounds — undefined
  behaviour on the happy path. This is the bug CLAUDE.md flagged and it is still there.
- `getRenderPass()` returns a default-constructed `VkRenderPass()` — a stub that hands back a null
  handle to anyone who trusts it.
- `cleanup()` runs twice: once from `shouldClose()` and again from the destructor.

---

## Game

| Unit                          |  LoC | Coup  |  Coh  | Arch  | Test  | Fail  | Notes                                                                                                                                                                                                                      |
| ----------------------------- | ---: | :---: | :---: | :---: | :---: | :---: | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `http_client`                 |  253 |   5   |   5   |   5   |   0   |   5   | RAII curl wrapper, `valid()`, response size cap, no throws. Best failure design in the repo                                                                                                                                |
| `pathfinding`                 |  137 |   5   |   5   |   5   |   0   |   5   | BFS behind a `blocked()` predicate. Knows nothing about the sim. Every failure returns an empty path, documented                                                                                                           |
| `rigidbody_component`         |  203 |   5   |   5   |   5   |   0   |   4   | One job, stated plainly. Collision deliberately left to the caller. Best unit in `game/`                                                                                                                                   |
| `landmask`                    |  102 |   4   |   5   |   4   |   0   |   5   | Forward-declares `HttpClient` instead of including it. A failed lookup means "open water", on purpose                                                                                                                      |
| `skinned_model_component`     |  127 |   3   |   4   |   4   |   0   |   5   | Null model and `-1` joints are both documented no-ops                                                                                                                                                                      |
| `player_ability_component`    |  109 |   3   |   5   |   4   |   0   |   4   | The `onImpact` callback is a clean inversion — the component owns motion, the scene owns the world                                                                                                                         |
| `keyboard_movement_component` |   66 |   3   |   5   |   4   |   0   |   4   | Small and clear. Reaches the singleton for the GLFW window                                                                                                                                                                 |
| `collider_component`          |  200 |   4   |   4   |   4   |   0   |   4   | AABB math + the component wrapper. The math half is pure and testable                                                                                                                                                      |
| `fetch_weather`               |  225 |   4   |   4   |   4   |   0   |   3   | Two providers behind one struct. Hand-rolled JSON scraping                                                                                                                                                                 |
| `scenes/levelscene`           |  152 |   3   |   4   |   4   |   0   |   3   | Small, conventional scene                                                                                                                                                                                                  |
| `scenes/reforgescene`         |  234 |   3   |   3   |   4   |   0   |   3   | Same shape as levelscene                                                                                                                                                                                                   |
| `main`                        |  151 |   2   |   3   |   3   |   0   |   3   | Loop + key polling + FPS readout + event wiring + scene registry. Catches and cleans up on throw, but derefs `activeScene` unguarded                                                                                       |
| `bench/servernav_benchmark`   |  220 |   4   |   5   |   3   |   0   |   3   | The closest thing to a test harness — **and the Makefile does not build it** (`SOURCES` only globs `src/`)                                                                                                                 |
| `lve_game_object`             |  198 |   3   |   2   |   2   |   0   |   3   | 10 files depend on it. Holds `Component`, `GameObject`, `TransformComponent`, `RectTransformComponent` **and** `PointLightComponent`. `getComponent` is a `dynamic_cast` linear scan. Engine-shaped code living in `game/` |
| `scenes/servernavscene`       |  366 |   2   |   2   |   3   |   0   |   3   | Sim driving + HUD text + camera + vessel-to-GameObject mapping                                                                                                                                                             |
| `servernav_sim`               | 1009 |   3   |   2   |   4   |   0   |   3   | Largest unit. Sim step + target picking + routing + scenario builders + nav readouts. Redeemed by its header being genuinely standalone — no engine, no Vulkan                                                             |
| `scenes/skinneddemoscene`     |  714 | **1** | **1** |   2   |   0   |   3   | See below                                                                                                                                                                                                                  |

Not rated: `game/dep/first_app.{cpp,hpp}` — dead code, excluded from the build.

### `scenes/skinneddemoscene` — the other one to worry about

It scores lowest on both coupling and cohesion, for the same underlying reason: it is where every
game system meets, and it has quietly absorbed jobs that belong elsewhere.

Eight jobs in one class: model loading, layout file parsing, collision resolution, orbit camera,
jump input, the light budget, render-item building, and fireball impact tests.

The load-bearing part is `settleBody()` — the project's **only** collision resolver. Rigidbodies and
colliders are both clean, well-scoped components, but neither resolves anything; the two-pass push-out
and the ground probe live here, in a demo scene. Any second scene that wants physics has to copy it.
The commit message `collision needs some work - skinneddemo is main resolver` already says this.

---

## What can be tested today, with no refactoring

Twelve units are pure logic with no Vulkan, no GLFW, and no engine singleton. They can be compiled
into a test binary as-is:

- `pathfinding` — its own header says "so it can be reused and unit-tested on its own"
- `rigidbody_component` — integration, drag, bounce, freeze axes
- `collider_component` — `overlaps`, `pushOut`, `pushOutXZ`, `expanded`, `fitToModel`
- `lve_camera` — projection and view matrices
- `servernav_sim` — already driven by the unbuilt benchmark
- `lve_utils`, `lve_event`, `lve_event_dispatcher`, `landmask`, `http_client` (against a local URL),
  `fetch_weather` (parsing, given a canned body), `lve_game_object` (transform math)

That is roughly 2,500 lines of testable logic with zero coverage. Everything else needs a live
`LveDevice`, and mostly needs it because of the singleton reach-back, not because the logic is
inherently graphical.

## Ranked by what it would cost to fix

1. **`lve_engine.cpp:52` — `reserve` should be `resize`.** One word. It is undefined behaviour on
   every startup.
2. **Build the benchmark.** `game/bench/servernav_benchmark.cpp` exists, drives the sim, and is not
   in `SOURCES`. Adding a `bench` target is the cheapest possible first step toward a test target.
3. **Pass the device in, stop reaching for the singleton.** `LveModel::createModelFromFile` and
   `LveTextRenderer` taking a `LveDevice&` would let a dozen files build without an engine.
4. **Move `getExecutableDir()` out of `lve_frame_info.hpp`** into its own small utility header. It is
   a filesystem call declared at global scope in the render-data contract.
5. **Lift `settleBody()` out of the demo scene** into a `collision_system` next to the collider and
   rigidbody components, so a second scene can have physics.
6. **Split `lve_game_object.hpp`.** The UI anchor type and the point light have nothing to do with
   the ECS core, and 10 files rebuild when any of them changes.
7. **Give `PointLightSystem` a soft cap** instead of `assert`, so an over-budget scene dims rather
   than dies.

## Done since this was measured (2026-07-27, same day)

Items 1, 5, 6 and 7 are implemented. The scores above are the **before** picture; these are the deltas.

- **1 — `lve_engine.cpp` now uses `resize`.** The out-of-bounds writes on every startup are gone.
  `lve_engine` **Fail 1 → 3**. Coupling and cohesion are unchanged, so it is still the file to worry about.
- **5 — `settleBody()` is now `CollisionSystem`** (`game/src/collision_system.{hpp,cpp}`). A scene calls
  `addStatic` / `addDynamic` once, then `settleAll()` each frame. The two-pass push-out and the ground
  probe moved over unchanged. `scenes/skinneddemoscene` **Coh 1 → 2, Coup 1 → 2**: it lost the physics
  job, but still owns model loading, layout parsing, camera, input, lights and draw building.
  The new unit rates **Coup 4, Coh 5, Arch 5, Test 0, Fail 4**.
  - *Behaviour change:* dynamic bodies now push each other **both** ways. Before, the man was pushed
    out of the falling boxes but the boxes ignored him, because the scene's lists happened to be
    asymmetric. The player can now nudge the boxes around.
- **6 — `lve_game_object.hpp` is split.** `RectTransformComponent` moved to `ui_object.hpp` and
  `PointLightComponent` to `light_component.hpp`. The core header is down to `Component`,
  `GameObject`, `TransformComponent`. `lve_game_object` **Coh 2 → 4, Arch 2 → 3**. The remaining
  wart is `getComponent`'s `dynamic_cast` linear scan.
- **7 — `PointLightSystem` drops overflow lights instead of asserting.** The list is already sorted by
  distance to camera, so the ones dropped are the furthest away. `systems/point_light_system`
  **Fail 2 → 4**.

Verified by a clean rebuild of both engine and game, a 20-second run of the skinned demo with no
sanitizer report, and a standalone ASan check of the lifted physics (bodies land on the ground, stack
on props, report grounded, and stop falling).

Still open from the list above: **2** (build the benchmark), **3** (pass the device in instead of
reaching for the singleton), **4** (move `getExecutableDir()` out of the render-data header).

## Notes on CLAUDE.md drift

Three things CLAUDE.md warns about have already been fixed, and it should be updated:

- `game/Makefile` **does** have `-MMD -MP` and `-include $(DEPS)` now. The header-tracking gotcha
  applies only to `engine/Makefile`, which still has `$(TARGET): $(SOURCES)` and `ar rcs $(TARGET) *.o`.
- `engine/Makefile`'s `clean` **does** now remove `*.o` and `libvulkan_engine.a`.
- `getAspectRatio()` has one overload, not two — the `0.0f` const version is gone.

Still accurate: build artifacts being committed to the repo.

One doc error worth fixing: CLAUDE.md describes `engine.render(scene)`. The real signature is
`render()` with no arguments — it reads the public `activeScene` pointer instead.

**All of the above has now been applied to CLAUDE.md**, along with the corrected run instruction
(run `./game/game` from the repo root, not `./game` from inside `game/`).
