#!/system/bin/sh
# Boot watchdog and restore.
#
# This module takes over the name the framework uses to find audio effects, so a
# failure is heavier than the legacy module's: the framework may end up with no
# effect factory at all. The failsafes therefore go further than that module's,
# not less far.
#
# Nothing of the stock HAL is ever deleted - its binary is untouched and Magisk
# overlays rather than modifies - so "restoring" means getting out of the way,
# and the backup exists for the one file we displace.

MODDIR=${0%/*}
BACKUP="/data/adb/rv4a-aidl-backup"
LOG="$MODDIR/boot.log"
MARK="$MODDIR/.boot_incomplete"
FAILS="$BACKUP/consecutive_failures"

step_aside() {
  # Everything that could start our service, neutralised in one place.
  touch "$MODDIR/disable"
  [ -f "$MODDIR/system/etc/init/rv4a-effect.rc" ] &&
    mv "$MODDIR/system/etc/init/rv4a-effect.rc" \
       "$MODDIR/system/etc/init/rv4a-effect.rc.disabled"
  # Drop any overlay of the stock init entry, so the untouched original applies
  find "$MODDIR/system" -name "*audio*effect*.rc" 2>/dev/null |
    while read -r f; do mv "$f" "$f.disabled"; done
}

{
  echo "--- boot check $(date 2>/dev/null) ---"

  if [ -f "$MARK" ]; then
    n=$(cat "$FAILS" 2>/dev/null || echo 0)
    n=$((n + 1))
    echo "$n" > "$FAILS"
    echo "previous boot did not complete (failure $n)"
    step_aside
    rm -f "$MARK"
    echo "module disabled; stock effect service will be used"
    echo "backup of displaced init entry is at $BACKUP"
    exit 0
  fi

  touch "$MARK"
  echo "effect services before our start:"
  service list 2>/dev/null | grep -i "audio.effect" || echo "  none"
} > "$LOG" 2>&1
