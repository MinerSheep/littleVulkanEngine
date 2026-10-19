#!/usr/bin/env bash
# package.sh — release builds for Linux and Windows, zipped into dist/ for players
#
# Usage:
#   ./package.sh       build engine+game with RELEASE=1 for both, then zip both
#   ./package.sh -n    skip the builds, just zip
#
# Run it in WSL. Windows is cross-built with MinGW-w64 (apt install mingw-w64),
# against the Windows Vulkan SDK under /mnt/c/VulkanSDK unless VK_SDK_PATH says otherwise
set -eu

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VK_SDK_PATH="${VK_SDK_PATH:-$(ls -d /mnt/c/VulkanSDK/*/ 2>/dev/null | tail -1)}"
WIN=(WINDOWS=1 RELEASE=1 CXX=x86_64-w64-mingw32-g++-posix AR=x86_64-w64-mingw32-ar
     "VK_SDK_PATH=${VK_SDK_PATH%/}")

if [ "${1:-}" != "-n" ]; then
  echo ">> building linux release…"
  make -C "$REPO/engine" RELEASE=1 -j"$(nproc)"
  make -C "$REPO/game" RELEASE=1 -j"$(nproc)"

  echo ">> building windows release…"
  make -C "$REPO/engine" "${WIN[@]}" -j"$(nproc)"
  make -C "$REPO/game" "${WIN[@]}" -j"$(nproc)"
fi

python3 "$REPO/tools/package.py" linux
python3 "$REPO/tools/package.py" windows
