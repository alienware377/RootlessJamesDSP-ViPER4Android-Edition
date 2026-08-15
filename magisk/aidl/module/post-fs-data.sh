#!/system/bin/sh
# Boot watchdog.
#
# This module replaces the service the framework asks for audio effects, so a
# failure here is heavier than the legacy module's: if our service does not come
# up, the framework may find no effect factory at all. The watchdog therefore
# matters more, not less.
#
# The rule is the same as the legacy module's: if the previous boot never
# finished, disable ourselves so the next one comes up on the stock service,
# without the user needing a PC.

MODDIR=${0%/*}
LOG="$MODDIR/boot.log"
MARK="$MODDIR/.boot_incomplete"

{
  echo "--- boot check $(date 2>/dev/null) ---"

  if [ -f "$MARK" ]; then
    echo "previous boot did not complete: disabling"
    touch "$MODDIR/disable"
    # Also neutralise the init script directly, so even a stale mount cannot
    # start our service on the way past.
    [ -f "$MODDIR/system/etc/init/rv4a-effect.rc" ] &&
      mv "$MODDIR/system/etc/init/rv4a-effect.rc" \
         "$MODDIR/system/etc/init/rv4a-effect.rc.disabled" &&
      echo "init script disabled"
    rm -f "$MARK"
    echo "stock effect service will be used on this boot"
    exit 0
  fi

  touch "$MARK"

  # Record what the stock service looked like before we take its name, so a
  # later failure can be compared against a known-good starting point.
  echo "effect services before start:"
  service list 2>/dev/null | grep -i "audio.effect" || echo "  none found"

  [ -f "$MODDIR/system/bin/rv4a-effect-service" ] &&
    echo "service binary present ($(wc -c < "$MODDIR/system/bin/rv4a-effect-service") bytes)"
} > "$LOG" 2>&1
