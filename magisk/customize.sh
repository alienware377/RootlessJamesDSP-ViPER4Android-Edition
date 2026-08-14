#!/system/bin/sh
# Installs the effect library and registers it with the audio HAL.

SKIPUNZIP=0

ARCH_DIR=""
case "$ARCH" in
  arm64) ARCH_DIR="arm64-v8a"; LIBDIR="lib64" ;;
  arm)   ARCH_DIR="armeabi-v7a"; LIBDIR="lib" ;;
  x64)   ARCH_DIR="x86_64"; LIBDIR="lib64" ;;
  x86)   ARCH_DIR="x86"; LIBDIR="lib" ;;
  *)     abort "! Unsupported architecture: $ARCH" ;;
esac

ui_print "- Architecture: $ARCH ($ARCH_DIR)"

# Android 10+ requires the effect to live where the audio server can load it.
mkdir -p "$MODPATH/system/$LIBDIR/soundfx"
if [ ! -f "$MODPATH/libs/$ARCH_DIR/libjamesdsp.so" ]; then
  abort "! No engine build for $ARCH_DIR in this package"
fi
mv "$MODPATH/libs/$ARCH_DIR/libjamesdsp.so" "$MODPATH/system/$LIBDIR/soundfx/libjamesdsp.so"
set_perm "$MODPATH/system/$LIBDIR/soundfx/libjamesdsp.so" 0 0 0644 u:object_r:system_lib_file:s0

# Drop the architectures we don't need, so the module stays small on device
rm -rf "$MODPATH/libs"

# The audio config lives in different places depending on the device, and may
# be XML (newer) or conf (older). Patch whichever this device actually uses.
. "$MODPATH/common/patch_audio_config.sh"
patch_audio_config

ui_print "- Reboot to load the engine"
