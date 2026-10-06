#!/usr/bin/env bash
# play.sh — build (incrementally) and run serverNavGame on the dzn GPU driver.
#
# Usage:
#   ./play.sh              build engine+game, then run on dzn (Direct3D12 GPU)
#   ./play.sh -n           skip the build, just run
#   ./play.sh -c           clean rebuild of engine+game first (use after header edits)
#   ./play.sh --cpu        run on llvmpipe (software Vulkan) instead of dzn
#   ./play.sh -t 20        auto-stop after 20s (default: run until you close the window)
#   ./play.sh -h           show this help
set -eu

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD=1
CLEAN=0
USE_DZN=1
TIMEOUT=0

while [ $# -gt 0 ]; do
  case "$1" in
    -n|--no-build)     BUILD=0 ;;
    -c|--clean)        CLEAN=1 ;;
    --cpu|--llvmpipe)  USE_DZN=0 ;;
    -t|--timeout)      shift; TIMEOUT="${1:-0}" ;;
    -h|--help)         sed -n '2,10p' "$0" | sed 's/^# \?//'; exit 0 ;;
    *) echo "play.sh: unknown option '$1' (try -h)" >&2; exit 2 ;;
  esac
  shift
done

if [ "$CLEAN" = 1 ]; then
  echo ">> clean…"; make -C "$REPO/engine" clean; make -C "$REPO/game" clean
fi
if [ "$BUILD" = 1 ] || [ "$CLEAN" = 1 ]; then
  echo ">> building engine…"; make -C "$REPO/engine"
  echo ">> building game…";   make -C "$REPO/game"
fi

if [ "$USE_DZN" = 1 ]; then
  if [ -f "$HOME/use-dzn.sh" ]; then
    # shellcheck disable=SC1091
    source "$HOME/use-dzn.sh"
  else
    icd="$HOME/mesa-dzn/share/vulkan/icd.d/dzn_icd.x86_64.json"
    [ -f "$icd" ] || { echo "play.sh: dzn ICD not found ($icd)" >&2; exit 1; }
    export VK_ICD_FILENAMES="$icd" VK_DRIVER_FILES="$icd"
    echo "dzn active -> $icd"
  fi
else
  echo ">> using default (llvmpipe/software) Vulkan"
fi

export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
cd "$REPO"   # run from repo root: scenes load models/ via CWD-relative paths
echo ">> launching ./game/game  (Ctrl+C or close the window to quit)"
if [ "$TIMEOUT" -gt 0 ] 2>/dev/null; then
  exec timeout --signal=TERM --kill-after=5 "$TIMEOUT" ./game/game
else
  exec ./game/game
fi
