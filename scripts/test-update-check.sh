#!/usr/bin/env bash
set -euo pipefail
# Builds tests/manual/update_check_probe.cpp with MinGW and runs it under Wine
# against github.com, for a version older than, equal to and newer than any
# release. Needs x86_64-w64-mingw32-g++, wine and network access.
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$(mktemp -d)"
trap 'rm -rf "$OUT"' EXIT
x86_64-w64-mingw32-g++ -std=c++17 -O1 -I"$REPO_ROOT/src/plugin" -I"$REPO_ROOT/src/core" \
    "$REPO_ROOT/tests/manual/update_check_probe.cpp" "$REPO_ROOT/src/plugin/update_check.cpp" \
    -o "$OUT/probe.exe" -lwinhttp -static
fail=0
check() {   # version, expected state
    local got
    got="$(WINEDEBUG=-all wine "$OUT/probe.exe" "$1" 2>/dev/null | tr -d '\r')"
    echo "$1 -> $got"
    [[ "$got" == state=$2* ]] || { echo "  expected state=$2" >&2; fail=1; }
}
check 0.0.1 3      # older than any release: Available
check 999.0.0 2    # newer than any release: UpToDate
check 0.0.0-dev 2  # unversioned build: no notice
exit $fail
