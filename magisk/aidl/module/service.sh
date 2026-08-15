#!/system/bin/sh
# Runs late in boot. Clearing the marker tells the next boot this one worked.
#
# Also records whether our factory actually took the default name, which is the
# single most useful line in the log when someone reports the effect missing.

MODDIR=${0%/*}

i=0
while [ "$(getprop sys.boot_completed)" != "1" ] && [ $i -lt 120 ]; do
  sleep 1
  i=$((i + 1))
done

rm -f "$MODDIR/.boot_incomplete"
{
  echo "boot completed $(date 2>/dev/null)"
  echo "effect services now:"
  service list 2>/dev/null | grep -i "audio.effect" || echo "  none - our service did not register"
} >> "$MODDIR/boot.log"
