#!/system/bin/sh
# Boot safety net.
#
# A bad audio_effects override can leave the audio server crashing on every
# boot, which on some devices is indistinguishable from a bootloop. This runs
# before the audio server starts: if our own override looks wrong, it is moved
# aside so the device boots with the stock configuration rather than ours.

MODDIR=${0%/*}
UUID="f27317f4-c984-4de6-9a90-545759495bf2"
LOG="$MODDIR/boot.log"

echo "--- $(date) boot check ---" > "$LOG"

for f in $(find "$MODDIR/system" -name "audio_effects*.xml" -o -name "audio_effects*.conf" 2>/dev/null); do
  bad=0
  grep -q "$UUID" "$f" || bad=1
  case "$f" in
    *.xml) grep -q "</audio_effects_conf>" "$f" || bad=1 ;;
  esac
  [ "$(wc -c < "$f")" -gt 100 ] || bad=1

  if [ "$bad" = "1" ]; then
    mv "$f" "$f.disabled"
    echo "quarantined $f (failed validation)" >> "$LOG"
  else
    echo "ok $f" >> "$LOG"
  fi
done

# Record what the engine has to work with, so a failure to load can be
# diagnosed from the module folder without a logcat.
for d in "$MODDIR"/system/*/soundfx "$MODDIR"/system/vendor/*/soundfx; do
  [ -d "$d" ] && echo "lib present: $d/libjamesdsp.so ($(wc -c < "$d/libjamesdsp.so" 2>/dev/null) bytes)" >> "$LOG"
done
