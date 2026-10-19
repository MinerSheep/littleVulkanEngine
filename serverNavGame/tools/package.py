"""Zips the release build and everything it loads into dist/, ready to upload

Build first with RELEASE=1 (package.sh builds both and runs this twice), then:
  python3 tools/package.py linux      -> dist/serverNavGame-linux.zip
  python3 tools/package.py windows    -> dist/serverNavGame-windows.zip
"""

import glob
import os
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOP = "serverNavGame"

# What the game reads at run time, as globs from the repo root
ASSETS = [
    "shaders/*.spv",
    "models/*",
    "maps/*.map",
    "rooms/*.layout",
    "textures/*.tex",
    "sounds/*.wav",
    "sounds/ui/*.wav",
    "sounds/stingers/*.wav",
    "sounds/ambience/*.wav",
]

PLAY_SH = """#!/bin/sh
# Starts the game, and names any library this machine is missing
cd "$(dirname "$0")"
missing=$(ldd game/game | grep "not found")
if [ -n "$missing" ]; then
  echo "$missing"
  echo "On Ubuntu/Debian:  sudo apt install libvulkan1 libcurl4t64 mesa-vulkan-drivers"
  exit 1
fi
exec ./game/game "$@"
"""

PLAY_BAT = '@echo off\r\nstart "" "%~dp0game\\game.exe"\r\n'

README = """serverNavGame

Start it with {launch}.
It needs a graphics driver with Vulkan support.{extra}

Progress is saved on its own as you walk through doors, and when you quit.
Saves live in {saves}
Delete that folder to start over. Photos are kept in saves/photos beside the game.
"""

PLATFORMS = {
    "linux": dict(binary="game/game_release", name="game/game", launcher=("play.sh", PLAY_SH),
                  launch="./play.sh (or ./game/game)", saves="~/.local/share/petscop/saves",
                  extra="\nOn Ubuntu/Debian it also wants: libvulkan1 libcurl4t64"),
    "windows": dict(binary="game/game_release.exe", name="game/game.exe",
                    launcher=("Play.bat", PLAY_BAT), launch="Play.bat (or game\\game.exe)",
                    saves="%APPDATA%\\petscop\\saves", extra=""),
}


def add(zf, name, source=None, text=None, executable=False):
    """Adds a file from disk, or a piece of text, under serverNavGame/ in the zip"""
    if source:
        info = zipfile.ZipInfo.from_file(source, f"{TOP}/{name}")
    else:
        info = zipfile.ZipInfo(f"{TOP}/{name}", date_time=(2026, 1, 1, 0, 0, 0))
    info.create_system = 3  # unix permission bits, which unzip honours
    info.external_attr = (0o755 if executable else 0o644) << 16
    info.compress_type = zipfile.ZIP_DEFLATED
    if text is not None:
        zf.writestr(info, text)
        return
    with open(source, "rb") as src, zf.open(info, "w") as dst:
        while chunk := src.read(1 << 20):
            dst.write(chunk)


def main():
    if len(sys.argv) != 2 or sys.argv[1] not in PLATFORMS:
        sys.exit("usage: package.py linux|windows")
    platform = sys.argv[1]
    plan = PLATFORMS[platform]

    binary = os.path.join(REPO, plan["binary"])
    if not os.path.isfile(binary):
        sys.exit(f"package.py: {plan['binary']} is missing, build with RELEASE=1 first")

    os.makedirs(os.path.join(REPO, "dist"), exist_ok=True)
    out = os.path.join(REPO, "dist", f"{TOP}-{platform}.zip")

    with zipfile.ZipFile(out, "w", compresslevel=6) as zf:
        add(zf, plan["name"], source=binary, executable=True)
        for pattern in ASSETS:
            for path in sorted(glob.glob(os.path.join(REPO, pattern))):
                if os.path.isfile(path):
                    add(zf, os.path.relpath(path, REPO).replace(os.sep, "/"), source=path)

        # GLFW rides along on Linux, found through the binary's rpath
        if platform == "linux":
            glfw = os.path.realpath("/usr/lib/x86_64-linux-gnu/libglfw.so.3")
            add(zf, "lib/libglfw.so.3", source=glfw)

        launcher, text = plan["launcher"]
        add(zf, launcher, text=text, executable=True)
        add(zf, "README.txt", text=README.format(**plan))

    print(f"wrote {out} ({os.path.getsize(out) / 1e6:.0f} MB)")


if __name__ == "__main__":
    main()
