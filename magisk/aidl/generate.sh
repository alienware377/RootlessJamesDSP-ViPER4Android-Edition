#!/usr/bin/env bash
# Generates the NDK backend for the AIDL effect HAL.
#
# Uses the Android SDK's own aidl, which supports the NDK backend - no AOSP
# build is needed. Every include root under interfaces/ is passed, because the
# effect interface reaches into five other packages: the common audio types,
# the eraser types (via Parameter), the fast message queue descriptors (how
# IEffect carries audio), and the audio common metadata types.
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

# min_sdk_version 33: ParcelableHolder needs at least 31, and the effect HAL
# only exists on 14 and above anyway.
"$AIDL" --lang=ndk --structured --stability=vintf --version=3 --min_sdk_version=33 \
        "${INC[@]}" -o "$OUT/src" -h "$OUT/include" \
        "$IFACES"/android.hardware.audio.effect-V3/android/hardware/audio/effect/*.aidl

echo "generated $(find "$OUT/src" -type f | wc -l) sources, $(find "$OUT/include" -type f | wc -l) headers"
echo "implement: BnFactory (4 methods) and BnEffect"
