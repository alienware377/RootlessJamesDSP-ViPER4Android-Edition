#!/system/bin/sh
# Installs the AIDL effect HAL service.
#
# Refuses to install where it cannot work, rather than leaving a device with a
# replaced audio service and nothing to show for it.

SKIPUNZIP=0

# --- not ready ------------------------------------------------------------
# The stock effect service hardcodes its instance name and takes no argument
# for it (AOSP EffectMain.cpp), so starting it under a second name to delegate
# to is impossible: both services would claim the same name and the loser
# aborts. Installing this would risk an audio HAL crash loop, which is worse
# than not installing at all.
ui_print "! This module is not ready to install."
ui_print "  The stock effect service cannot be run under a"
ui_print "  second name, so taking over its name would leave"
ui_print "  the device with no working effect HAL."
ui_print "  See DESIGN.md for what replaces this approach."
abort "! Aborting deliberately"

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

# --- back up what we displace -------------------------------------------
#
# Nothing of the stock HAL is deleted or overwritten: its binary stays where it
# is, and Magisk overlays rather than modifies. What we do displace is the init
# entry that starts it under the default instance name, since two services
# cannot hold one name. That file is backed up before anything is overlaid, to
# a location outside the module so it survives the module being removed.
BACKUP="/data/adb/rv4a-aidl-backup"
mkdir -p "$BACKUP"

STOCK_RC=""
for d in /vendor/etc/init /system/etc/init /odm/etc/init; do
  [ -d "$d" ] || continue
  f=$(grep -rl "audio.effect" "$d" 2>/dev/null | head -1)
  [ -n "$f" ] && { STOCK_RC="$f"; break; }
done

if [ -n "$STOCK_RC" ]; then
  cp "$STOCK_RC" "$BACKUP/$(basename "$STOCK_RC").orig"
  echo "$STOCK_RC" > "$BACKUP/stock_rc_path"
  ui_print "- Backed up $(basename "$STOCK_RC")"
else
  # Without it we cannot free the default name, and our service would lose the
  # race to register. Better to stop than to install something that cannot work.
  ui_print "! Could not find the stock effect service init entry."
  ui_print "  Without it the default instance name cannot be freed."
  abort "! Aborting"
fi

# Record the pre-install state, so a later comparison has something to compare to
service list 2>/dev/null | grep -i "audio.effect" > "$BACKUP/services_before" 2>/dev/null
getprop | grep -i "audio" > "$BACKUP/props_before" 2>/dev/null

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
