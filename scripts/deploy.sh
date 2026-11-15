#!/usr/bin/env bash
set -euo pipefail

# Deploy the cross-compiled plugin DLL into the GW2 arcdps addons folder.
# Mirrors arcdps-axipulse/scripts/deploy.sh: atomic tmp+rename so a live
# GW2 never sees a truncated inode mid-copy.

# --dev deploys the spike/scanner build instead. It is a separate addon (own
# file name, arcdps sig and ini) so it coexists with an installed release DLL.
NAME="arcdps_player_outline"
BUILD="build-win"
if [[ "${1:-}" == "--dev" ]]; then
    NAME="arcdps_player_outline_dev"
    BUILD="build-dev"
fi

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$REPO_ROOT/$BUILD/$NAME.dll"
DEFAULT_DIR="/var/mnt/data/SteamLibrary/steamapps/common/Guild Wars 2/addons"
DEST="${PLAYER_OUTLINE_DEPLOY_DEST:-$DEFAULT_DIR/$NAME.dll}"
# The env override names the release DLL; keep its folder but never let a --dev
# build be written over the installed release file.
if [[ "$BUILD" == "build-dev" ]]; then
    DEST="${DEST%/*}/$NAME.dll"
fi

if [[ ! -f "$SRC" ]]; then
    echo "build artifact missing: $SRC — cross-compile first:" >&2
    if [[ "$BUILD" == "build-dev" ]]; then
        echo "  cmake -S . -B build-dev -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake -DPLAYER_OUTLINE_SPIKE=ON && cmake --build build-dev" >&2
    else
        echo "  cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake && cmake --build build-win" >&2
    fi
    exit 1
fi

DEST_DIR="${DEST%/*}"
if [[ ! -d "$DEST_DIR" ]]; then
    echo "destination folder missing: $DEST_DIR" >&2
    echo "set PLAYER_OUTLINE_DEPLOY_DEST to your GW2 addons path" >&2
    exit 1
fi

TMP="${DEST}.new"
cp "$SRC" "$TMP"
mv "$TMP" "$DEST"
ls -lh "$DEST"
echo "deployed → $DEST"
