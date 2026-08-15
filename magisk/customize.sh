#!/system/bin/sh
# Installs the effect library and registers it with the audio HAL.
#
# Only one copy of audio_effects.xml can be mounted at a time, so a module that
# overlays it directly fights with every other audio mod on the device. When
# Audio Modification Library is present we hand it our fragment and let it do
# the merging; only without AML do we overlay the file ourselves.

SKIPUNZIP=0

# --- what are we installing under, and onto? ------------------------------
API=$(getprop ro.build.version.sdk)

if [ -d /data/adb/ksu ] || [ -n "$KSU" ]; then
  ROOT_IMPL="KernelSU"
elif [ -d /data/adb/ap ] || [ -n "$APATCH" ]; then
  ROOT_IMPL="APatch"
else
  ROOT_IMPL="Magisk"
fi
ui_print "- Root: $ROOT_IMPL, Android API $API"

# The effect is a legacy audio-effect library: the audio server discovers it
# through audio_effects.xml or .conf. Android 15 moved effects to an AIDL HAL,
# and devices that ship only the AIDL path never read those files, so nothing
# would load the engine no matter where it is placed.
HAS_LEGACY_CONFIG=0
for f in /vendor/etc/audio_effects.xml /vendor/etc/audio_effects.conf \
         /system/vendor/etc/audio_effects.xml /system/vendor/etc/audio_effects.conf \
         /system/etc/audio_effects.xml /system/etc/audio_effects.conf; do
  [ -f "$f" ] && HAS_LEGACY_CONFIG=1
done

if [ "$HAS_LEGACY_CONFIG" = "0" ]; then
  ui_print " "
  ui_print "! This device has no legacy audio effects config."
  ui_print "  Its audio HAL is AIDL-only, which does not load"
  ui_print "  effect libraries like this one. Android 15 moved"
  ui_print "  effects to AIDL and this engine implements the"
  ui_print "  older interface, so it cannot be found here."
  ui_print "  Rootless mode still works and is unaffected."
  abort "! Aborting rather than installing something inert"
fi

case "$ARCH" in
  arm64) ABI_DIR="arm64-v8a";   LIBDIR="lib64" ;;
  arm)   ABI_DIR="armeabi-v7a"; LIBDIR="lib"   ;;
  x64)   ABI_DIR="x86_64";      LIBDIR="lib64" ;;
  x86)   ABI_DIR="x86";         LIBDIR="lib"   ;;
  *)     abort "! Unsupported architecture: $ARCH" ;;
esac

ui_print "- Architecture: $ARCH ($ABI_DIR)"

SO="$MODPATH/libs/$ABI_DIR/libjamesdsp.so"
[ -f "$SO" ] || abort "! No engine build for $ABI_DIR in this package"

# Install to both system and vendor soundfx: which one the audio server loads
# from depends on where the device's config lives, and this costs one file.
for D in "$MODPATH/system/$LIBDIR/soundfx" "$MODPATH/system/vendor/$LIBDIR/soundfx"; do
  mkdir -p "$D"
  cp "$SO" "$D/libjamesdsp.so"
  # No explicit SELinux label: the same file is installed under both system
  # and vendor, and those take different contexts. Magisk applies the correct
  # one for each mount point; forcing system_lib_file onto the vendor copy
  # risks a denial when the audio server tries to load it.
  set_perm "$D/libjamesdsp.so" 0 0 0644
done
rm -rf "$MODPATH/libs"
ui_print "- Engine installed to $LIBDIR/soundfx"

. "$MODPATH/common/patch_audio_config.sh"

if [ -d /data/adb/modules/aml ] || [ -d /data/adb/modules/AML ]; then
  # AML sweeps modules for audio config files and merges them into one set,
  # so leaving ours in place is exactly what it expects. Patching /vendor
  # ourselves as well would give the device two competing mounts.
  ui_print "- Audio Modification Library detected"
  ui_print "  Leaving the merge to AML; both mods can coexist"
  write_effect_fragment
else
  patch_audio_config
fi

ui_print "- Reboot to load the engine"
ui_print " "
ui_print "  If the device fails to boot, connect it and run:"
ui_print "    adb wait-for-device shell magisk --remove-modules"
ui_print "  or boot into Safe Mode, which disables all modules."
