#!/usr/bin/env bash
# Generates the NDK backend for the AIDL effect HAL and everything it depends on.
#
# The aidl tool emits code only for the files it is given; packages passed with
# -I are used to resolve names but produce no headers. The effect interface
# refers to types from five other packages, so each is generated in turn or the
# build fails looking for their headers.
#
# Each package is generated at its own frozen version, taken from the directory
# name, since they are not versioned in step with one another.
set -euo pipefail

SDK="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/Android/Sdk}}"
AIDL="$(ls "$SDK"/build-tools/*/aidl 2>/dev/null | sort -V | tail -1)"
[ -x "$AIDL" ] || { echo "No aidl in $SDK/build-tools"; exit 1; }

HERE="$(cd "$(dirname "$0")" && pwd)"
IFACES="$HERE/interfaces"
OUT="${1:-$HERE/generated}"

rm -rf "$OUT"; mkdir -p "$OUT/src" "$OUT/include"

INC=()
for d in "$IFACES"/*/; do INC+=(-I "$d"); done

for pkg in "$IFACES"/*/; do
  name="$(basename "$pkg")"
  ver="${name##*-V}"
  [ "$ver" = "$name" ] && continue          # skip anything not version-tagged
  files=$(find "$pkg" -name "*.aidl")
  [ -n "$files" ] || continue
  echo "generating ${name%-V*} at V$ver"
  # min_sdk_version 33: ParcelableHolder needs 31 at least, and the effect HAL
  # only exists from 14 onwards anyway.
  "$AIDL" --lang=ndk --structured --stability=vintf --version="$ver" \
          --min_sdk_version=33 "${INC[@]}" \
          -o "$OUT/src" -h "$OUT/include" $files
done

echo "generated $(find "$OUT/src" -type f | wc -l) sources, $(find "$OUT/include" -type f | wc -l) headers"
