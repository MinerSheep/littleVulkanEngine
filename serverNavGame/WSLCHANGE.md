# WSLCHANGE.md — Ubuntu-24.04 + GPU (dzn) setup for serverNavGame

**Date:** 2026-07-18
**Goal:** Run serverNavGame in a **WSL2 Ubuntu-24.04** distro on the **real GPU**
(AMD Radeon via Direct3D12) using Mesa's **dzn / "Dozen"** Vulkan-on-D3D12 driver,
instead of the software `llvmpipe` fallback.

**Result:** ~140–155 FPS (6–7 ms/frame) on dzn vs ~11–12 FPS (86 ms/frame) on llvmpipe (~12×).

This file records everything that was done so it can be reproduced or debugged later.

---

## 0. Starting point (already true before these changes)

- Ubuntu-24.04 was **already installed** by you as a second WSL2 distro. The older default
  distro was `Ubuntu` (GCC 9, ~Ubuntu 20.04) — still present, untouched.
- The Mesa source was already cloned at `~/mesa` (**Mesa 25.2.8**, `git-766909b321`).
- WSL GPU passthrough libs were present: `/usr/lib/wsl/lib/{libd3d12.so, libd3d12core.so, libdxcore.so}`.
- Toolchain in 24.04: **g++ 13.3.0**, meson 1.3.2 (apt), ninja, plus most Mesa/Vulkan dev
  packages. GPU: AMD Radeon integrated.
- Problem reported: `ls /usr/share/vulkan/icd.d/` had no `dzn` entry — a prior `meson setup`
  in `~/mesa/build-dzn` had failed and left only config scaffolding.

> Everything below runs **inside the Ubuntu-24.04 distro**. From Windows: `wsl -d Ubuntu-24.04`.
> `sudo` needs a password in this distro, but WSL gives **passwordless root** via
> `wsl -d Ubuntu-24.04 -u root <cmd>` — that's how apt installs were done.

---

## PART A — Build the dzn (Dozen) Vulkan-on-D3D12 driver

### A.1 The blocker and the fix (meson too old)

Root cause of the original failure:

```
meson.build:9:18: ERROR: Meson version is 1.3.2 but project requires >= 1.4.0
```

Mesa 25.2.8 needs **meson ≥ 1.4.0**; Ubuntu 24.04's apt ships **1.3.2**. Fix **without sudo/pip**
(meson is pure Python and runs straight from a source checkout):

```sh
git clone --filter=blob:none https://github.com/mesonbuild/meson ~/meson-src
cd ~/meson-src
git checkout 1.11.2          # the version we used; any >= 1.4.0 stable tag works
python3 ~/meson-src/meson.py --version   # -> 1.11.2
```

From here on, **meson = `python3 ~/meson-src/meson.py`** (do NOT use the apt `meson`).

### A.2 Configure the dzn-only build

Build deps were **all already present** in 24.04 (directx-headers-dev, spirv-tools, the
xcb/wayland WSI libs, build-essential, flex, bison, python3-mako). Nothing extra was needed.

```sh
cd ~/mesa
rm -rf build-dzn            # wipe the stale, half-configured dir
python3 ~/meson-src/meson.py setup build-dzn \
  -Dbuildtype=release \
  -Dgallium-drivers= \
  -Dvulkan-drivers=microsoft-experimental \
  -Dplatforms=x11,wayland \
  -Dllvm=disabled \
  -Dglx=disabled \
  -Degl=disabled \
  -Dgbm=disabled \
  -Dgles1=disabled \
  -Dgles2=disabled \
  -Dopengl=false \
  -Dprefix=$HOME/mesa-dzn \
  -Dlibdir=lib
```

`-Dvulkan-drivers=microsoft-experimental` is what enables **dzn**. Successful configure prints
`Vulkan Drivers: microsoft-experimental` and `ICD dir: share/vulkan/icd.d`.

### A.3 Build and install (no sudo — installs into $HOME)

```sh
ninja -C ~/mesa/build-dzn
python3 ~/meson-src/meson.py install -C ~/mesa/build-dzn
```

Installs into the `~/mesa-dzn` prefix:
- `~/mesa-dzn/lib/libvulkan_dzn.so`
- `~/mesa-dzn/share/vulkan/icd.d/dzn_icd.x86_64.json`  (ICD manifest; points at the .so via an
  absolute `library_path`, so pointing the Vulkan loader at this file is all that's needed)

### A.4 Enable dzn — `~/use-dzn.sh`

We did **not** copy the ICD into the root-owned `/usr/share/vulkan/icd.d/`. Instead there's a
per-shell toggle at `~/use-dzn.sh` that forces the loader to use **only** dzn:

```sh
# ~/use-dzn.sh  (source it: `source ~/use-dzn.sh`)
export VK_ICD_FILENAMES="$HOME/mesa-dzn/share/vulkan/icd.d/dzn_icd.x86_64.json"
export VK_DRIVER_FILES="$HOME/mesa-dzn/share/vulkan/icd.d/dzn_icd.x86_64.json"
```

Open a fresh shell (or unset those vars) to go back to llvmpipe.

**Optional — make dzn visible system-wide instead** (needs root; enumerates dzn *alongside*
lavapipe for all apps):

```sh
wsl -d Ubuntu-24.04 -u root bash -c \
  'ln -sf /home/ryder/mesa-dzn/share/vulkan/icd.d/dzn_icd.x86_64.json /usr/share/vulkan/icd.d/'
```

### A.5 Verify the driver

```sh
source ~/use-dzn.sh
vulkaninfo --summary | grep -E 'deviceName|driverName|driverID'
# Expect:
#   deviceName = Microsoft Direct3D12 (AMD Radeon(TM) Graphics)
#   driverName = Dozen
#   driverID   = DRIVER_ID_MESA_DOZEN
vkcube --c 100        # renders 100 frames on the GPU then exits (exit 0)
```

The banner `dzn is not a conformant Vulkan implementation, testing use only` is **normal**.

---

## PART B — Build and run the game in Ubuntu-24.04

### B.1 Extra packages the fresh distro was missing

Installed as passwordless root:

```sh
wsl -d Ubuntu-24.04 -u root bash -c 'apt-get install -y libcurl4-openssl-dev nlohmann-json3-dev'
```

- `libcurl4-openssl-dev` — the game links libcurl (`src/http_client.cpp`, `src/fetch_weather.cpp`).
- `nlohmann-json3-dev` — provides `<nlohmann/json.hpp>` (used by `fetch_weather.cpp`, `landmask.cpp`).

Already present (no action): `libglfw3-dev`, `libglm-dev` (GLM is a **system** header, not vendored),
`vulkan-validationlayers`, `libvulkan-dev`, `glslang-tools`, `build-essential`.

### B.2 Clean rebuild (important)

The repo has **committed `.o`/`.a`/`game` artifacts built with the old distro's GCC 9.** Mixing
those with GCC 13 output is an ABI landmine, so do a full clean rebuild — engine first, then game:

```sh
cd ~/mesa   # (not needed; just illustrating)
SG=/mnt/c/Users/ryder/OneDrive/Desktop/Coding_Practice/littleVulkanEngine/serverNavGame
cd "$SG/engine" && make clean && make          # -> engine/libvulkan_engine.a (no ASan)
cd "$SG/game"   && make clean && make          # -> game/game (ASan-instrumented)
```

Shaders: the `.spv` files under the repo-root `shaders/` are committed and are portable SPIR-V,
so **no shader recompile is needed**. (If ever needed: `glslangValidator -V shaders/x.vert -o shaders/x.vert.spv`.)

### B.3 Run

Run **from the repo root** (scenes load `models/…` via CWD-relative paths):

```sh
cd "$SG"
source ~/use-dzn.sh
ASAN_OPTIONS=detect_leaks=0 ./game/game
```

Expected startup log includes `physical device: Microsoft Direct3D12 (AMD Radeon(TM) Graphics)`,
`Present mode: Mailbox`, then `FPS: ~140–155 (6–7 ms/frame)`. The only validation warnings are the
pre-existing "vertex attribute at location 2/3 not consumed" ones.

### B.4 `play.sh` (the one-shot; lives at the repo root)

```sh
./play.sh          # build (incremental) -> source dzn -> run, from repo root
./play.sh -n       # skip build, just run
./play.sh -c       # clean rebuild first (use after editing a header)
./play.sh --cpu    # run on llvmpipe (software) instead of dzn
./play.sh -t 20    # auto-stop after 20 s (default: run until window closed / Ctrl+C)
./play.sh -h       # help
```

---

## PART C — Environment changes

### C.1 Ubuntu-24.04 is now the DEFAULT WSL distro

```powershell
wsl --set-default Ubuntu-24.04
wsl -l -v          # '*' should be on Ubuntu-24.04
```

A bare `wsl` now launches Ubuntu-24.04. The old distro is still reachable with `wsl -d Ubuntu`.

### C.2 VS Code

`code .` works in Ubuntu-24.04: the `code` CLI resolves through Windows PATH interop
(`/mnt/c/Users/ryder/AppData/Local/Programs/Microsoft VS Code/bin/code`), and the VS Code
**Server** auto-installs into the distro on first use (needs the "WSL" VS Code extension + network).

---

## Inventory — what was created / changed

| Path | What |
|------|------|
| `~/meson-src` | meson 1.11.2 git checkout (run via `python3 ~/meson-src/meson.py`) |
| `~/mesa/build-dzn` | dzn meson/ninja build directory |
| `~/mesa-dzn/` | dzn **install prefix** (`lib/libvulkan_dzn.so`, `share/vulkan/icd.d/dzn_icd.x86_64.json`) |
| `~/use-dzn.sh` | per-shell toggle: exports `VK_ICD_FILENAMES`/`VK_DRIVER_FILES` to the dzn ICD |
| `<repo>/play.sh` | one-shot build+run-on-dzn wrapper |
| `<repo>/WSLCHANGE.md` | this file |

**apt packages installed (as root):** `libcurl4-openssl-dev`, `nlohmann-json3-dev`.

**Windows/WSL config changed:** default WSL distro → `Ubuntu-24.04`.

**Not changed:** the old `Ubuntu` (GCC 9) distro; nothing was installed with `sudo` needing a
password; `/usr/share/vulkan/icd.d/` was left untouched (dzn is enabled via env var, not a system ICD).

---

## Verification checklist (quick "is it still working?")

```sh
wsl -l -v                                   # * on Ubuntu-24.04
wsl -d Ubuntu-24.04 -- python3 ~/meson-src/meson.py --version   # >= 1.4.0
wsl -d Ubuntu-24.04 -- ls -l ~/mesa-dzn/share/vulkan/icd.d/dzn_icd.x86_64.json  # exists
# inside the distro:
source ~/use-dzn.sh && vulkaninfo --summary | grep driverName   # -> Dozen
cd <repo> && ./play.sh -t 8                 # builds (no-op) + runs on dzn ~8s, exit 124 = OK
```

---

## Troubleshooting / recovery

- **`meson.build: ... requires >= 1.4.0`** → you used the apt meson. Use `python3 ~/meson-src/meson.py`.
  If `~/meson-src` is gone, re-clone it (Part A.1).
- **Game/`vulkaninfo` shows `llvmpipe`/`lavapipe`, not `Dozen`** → you didn't `source ~/use-dzn.sh`
  in that shell, or `VK_ICD_FILENAMES` is unset. Re-source it. Confirm the ICD json still exists.
- **`dzn_icd.x86_64.json` / `libvulkan_dzn.so` missing** → rebuild the driver: Part A.2 → A.3.
  (After a `git pull` in `~/mesa`, re-run all of A.2/A.3.)
- **Game link error `cannot find -lcurl` / `nlohmann/json.hpp: No such file`** → reinstall the
  Part B.1 packages.
- **Weird memory corruption / FPE right after startup, or `bucket_count()==0`** → a stale object
  file with a mismatched struct layout (the repo has no header-dependency tracking). Do a full
  clean rebuild of **both** engine and game (`./play.sh -c`), or `make clean` in each. See CLAUDE.md.
- **`Cannot open file [models/...]` then exit code 1** → you ran from `game/` instead of the repo
  root. Run from the repo root (or just use `./play.sh`).
- **Driver loads but rendering is wrong/crashes** → confirm `/usr/lib/wsl/lib/libd3d12*.so` still
  exist (WSL provides them); a Windows GPU-driver update can change them. `vkcube --c 100` isolates
  driver vs. game issues.
- **`code .` fails** → ensure the "WSL" extension is installed in VS Code; first run needs network
  to download the server. `wsl -d Ubuntu-24.04 -- command -v code` should print the `/mnt/c/...` path.
- **Automated/headless capture hangs** → don't wrap the game in `script(1)`; its pty can hang on
  teardown under WSLg. Use plain redirection + `timeout`/`play.sh -t N`.

---

## Notes / footguns

- After these builds, the repo's committed `.o`/`.a`/`game` are now GCC-13 artifacts, so
  `git status` shows them changed. Consider `.gitignore`-ing build outputs.
- The engine builds **without** ASan, the game **with** ASan — intentional and fine to link.
- dzn is marked experimental/non-conformant; the "testing use only" banner is expected.
- Keep using `python3 ~/meson-src/meson.py` for any Mesa rebuild until the apt meson is ≥ 1.4.0.
