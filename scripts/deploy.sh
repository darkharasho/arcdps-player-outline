#!/usr/bin/env bash
set -euo pipefail

# Deploy the cross-compiled plugin DLL into the GW2 arcdps addons folder.
# Mirrors arcdps-axipulse/scripts/deploy.sh: atomic tmp+rename so a live
# GW2 never sees a truncated inode mid-copy.

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$REPO_ROOT/build-win/arcdps_player_outline.dll"
DEST="${PLAYER_OUTLINE_DEPLOY_DEST:-/var/mnt/data/SteamLibrary/steamapps/common/Guild Wars 2/addons/arcdps_player_outline.dll}"

if [[ ! -f "$SRC" ]]; then
    echo "build artifact missing: $SRC — cross-compile first:" >&2
    echo "  cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake && cmake --build build-win" >&2
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
