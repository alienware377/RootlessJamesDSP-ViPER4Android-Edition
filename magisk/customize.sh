#!/system/bin/sh
# Installs the effect library and registers it with the audio HAL.
#
# Only one copy of audio_effects.xml can be mounted at a time, so a module that
# overlays it directly fights with every other audio mod on the device. When
# Audio Modification Library is present we hand it our fragment and let it do
# the merging; only without AML do we overlay the file ourselves.

SKIPUNZIP=0

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
