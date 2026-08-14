#!/system/bin/sh
# Boot watchdog and safety net. Runs before the audio server starts.
#
# Two jobs:
#  1. If the previous boot never finished, disable this module so the next boot
#     comes up clean. A bad audio override can crash the audio server on every
#     boot, which looks like a bootloop; this limits it to a single bad boot
#     instead of an endless one, without the user needing a PC.
#  2. Sanity-check our own audio configs and quarantine any that look wrong.

MODDIR=${0%/*}
UUID="f27317f4-c984-4de6-9a90-545759495bf2"
LOG="$MODDIR/boot.log"
MARK="$MODDIR/.boot_incomplete"

{
  echo "--- boot check $(date 2>/dev/null) ---"

  # --- 1. did the last boot finish? ------------------------------------
  if [ -f "$MARK" ]; then
    echo "previous boot did not complete: disabling module"
    # Magisk skips any module containing a file named 'disable'
    touch "$MODDIR/disable"
    # Also move our overrides aside, so even a stale mount can't be used
    find "$MODDIR/system" \( -name "audio_effects*.xml" -o -name "audio_effects*.conf" \) 2>/dev/null |
      while read -r f; do
        mv "$f" "$f.disabled" 2>/dev/null && echo "quarantined $f"
      done
    rm -f "$MARK"
    echo "module disabled; re-enable it in the Magisk app once the cause is known"
    exit 0
  fi

  # Mark the boot as in progress. service.sh clears this once Android is up.
  touch "$MARK"

  # --- 2. validate our own configs -------------------------------------
  find "$MODDIR/system" \( -name "audio_effects*.xml" -o -name "audio_effects*.conf" \) 2>/dev/null |
    while read -r f; do
      bad=0
      grep -q "$UUID" "$f" || bad=1
      case "$f" in
        *.xml)
          grep -q "</audio_effects_conf>" "$f" || bad=1
          # our entries must sit inside the sections they belong to
          grep -q "</libraries>" "$f" || bad=1
          grep -q "</effects>" "$f" || bad=1
          ;;
      esac
      [ "$(wc -c < "$f" 2>/dev/null || echo 0)" -gt 100 ] || bad=1

      if [ "$bad" = "1" ]; then
        mv "$f" "$f.disabled" 2>/dev/null
        echo "quarantined $f (failed validation)"
      else
        echo "ok $f"
      fi
    done

  for d in "$MODDIR"/system/*/soundfx "$MODDIR"/system/vendor/*/soundfx; do
    [ -f "$d/libjamesdsp.so" ] &&
      echo "engine present: $d ($(wc -c < "$d/libjamesdsp.so") bytes)"
  done
} > "$LOG" 2>&1
