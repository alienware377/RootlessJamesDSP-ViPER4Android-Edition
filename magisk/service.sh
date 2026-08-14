#!/system/bin/sh
# Runs late in boot, once Android is up.
#
# Clearing the marker tells the next boot that this one succeeded. If the device
# never gets this far, post-fs-data.sh sees the marker still present and
# disables the module, so a bad install costs one failed boot rather than an
# unrecoverable loop.

MODDIR=${0%/*}

# Wait for boot to be genuinely complete rather than just for this script to run
i=0
while [ "$(getprop sys.boot_completed)" != "1" ] && [ $i -lt 120 ]; do
  sleep 1
  i=$((i + 1))
done

rm -f "$MODDIR/.boot_incomplete"
echo "boot completed, watchdog cleared ($(date 2>/dev/null))" >> "$MODDIR/boot.log"
