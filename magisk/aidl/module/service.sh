#!/system/bin/sh
# Late boot: confirm the device is actually healthy, not merely booted.
#
# A device can boot perfectly well while audio is broken, which the legacy
# module could not really cause but this one can - so booting is not enough of a
# test on its own. If our factory did not register, or the audio server is not
# running, we step aside rather than leave the user with silence.

MODDIR=${0%/*}
BACKUP="/data/adb/rv4a-aidl-backup"

i=0
while [ "$(getprop sys.boot_completed)" != "1" ] && [ $i -lt 120 ]; do
  sleep 1
  i=$((i + 1))
done

# Give the audio server a moment to settle after boot completes
sleep 5

{
  echo "boot completed $(date 2>/dev/null)"

  FACTORY=$(service list 2>/dev/null | grep -i "audio.effect.IFactory")
  AUDIOSERVER=$(pidof audioserver 2>/dev/null)

  echo "effect factory: ${FACTORY:-MISSING}"
  echo "audioserver pid: ${AUDIOSERVER:-NOT RUNNING}"

  if [ -z "$FACTORY" ] || [ -z "$AUDIOSERVER" ]; then
    # Booting is not the same as working. Leaving a device with no effect
    # factory or no audio server is worse than not installing at all.
    echo "audio is not healthy: disabling module"
    touch "$MODDIR/disable"
    [ -f "$MODDIR/system/etc/init/rv4a-effect.rc" ] &&
      mv "$MODDIR/system/etc/init/rv4a-effect.rc" \
         "$MODDIR/system/etc/init/rv4a-effect.rc.disabled"
    echo "reboot to return to the stock effect service"
  else
    echo "audio healthy; clearing watchdog"
    rm -f "$MODDIR/.boot_incomplete"
    rm -f "$BACKUP/consecutive_failures"
  fi

  echo "--- comparison with pre-install state ---"
  echo "before: $(cat "$BACKUP/services_before" 2>/dev/null || echo unknown)"
  echo "now:    ${FACTORY:-none}"
} >> "$MODDIR/boot.log"
