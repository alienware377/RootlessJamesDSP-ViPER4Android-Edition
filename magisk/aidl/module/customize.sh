#!/system/bin/sh
# Installs the AIDL effect HAL service.
#
# Refuses to install where it cannot work, rather than leaving a device with a
# replaced audio service and nothing to show for it.

SKIPUNZIP=0
API=$(getprop ro.build.version.sdk)

case "$ARCH" in
  arm64) ABI_DIR="arm64-v8a" ;;
  arm)   ABI_DIR="armeabi-v7a" ;;
  x64)   ABI_DIR="x86_64" ;;
  x86)   ABI_DIR="x86" ;;
  *)     abort "! Unsupported architecture: $ARCH" ;;
esac

ui_print "- Architecture: $ARCH, Android API $API"

# The AIDL effect HAL exists from 14 onward. Below that the legacy module is
# the right one, and this would register a service nothing looks up.
if [ "$API" -lt 34 ]; then
  ui_print "! This device predates the AIDL effect HAL."
  abort "! Install the standard module instead"
fi

# There must be a stock factory to hide behind. Without one, taking the default
# instance name would leave the device with our effect and nothing else.
if ! (service list 2>/dev/null | grep -q "android.hardware.audio.effect.IFactory"); then
  ui_print "! No audio effect factory found on this device."
  ui_print "  Nothing to delegate to, so this would remove"
  ui_print "  effects rather than add one."
  abort "! Aborting"
fi
ui_print "- Stock effect factory found"

BIN="$MODPATH/libs/$ABI_DIR/rv4a-effect-service"
[ -f "$BIN" ] || abort "! No service build for $ABI_DIR"

mkdir -p "$MODPATH/system/bin" "$MODPATH/system/etc/init"
mv "$BIN" "$MODPATH/system/bin/rv4a-effect-service"
set_perm "$MODPATH/system/bin/rv4a-effect-service" 0 0 0755
mv "$MODPATH/rv4a-effect.rc" "$MODPATH/system/etc/init/rv4a-effect.rc"
set_perm "$MODPATH/system/etc/init/rv4a-effect.rc" 0 0 0644
rm -rf "$MODPATH/libs"

ui_print "- Installed. Reboot to load."
ui_print " "
ui_print "  This module replaces the audio effect service."
ui_print "  If audio misbehaves, disable it in your root"
ui_print "  manager and reboot; the stock service returns."
