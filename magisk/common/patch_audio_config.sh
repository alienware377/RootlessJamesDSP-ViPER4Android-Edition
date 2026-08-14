#!/system/bin/sh
# Registers the effect with the device's audio configuration.
#
# A malformed audio_effects file can crash the audio server on every boot, so
# each generated file is checked before it is left in place; anything that
# fails validation is discarded rather than shipped.

EFFECT_UUID="f27317f4-c984-4de6-9a90-545759495bf2"

find_configs() {
  for f in \
    /vendor/etc/audio_effects.xml \
    /vendor/etc/audio_effects.conf \
    /system/vendor/etc/audio_effects.xml \
    /system/vendor/etc/audio_effects.conf \
    /system/etc/audio_effects.xml \
    /system/etc/audio_effects.conf
  do
    [ -f "$f" ] && echo "$f"
  done
}

# Magisk mirrors module files from $MODPATH/system, so /vendor/etc/x has to be
# written to $MODPATH/system/vendor/etc/x. Writing to $MODPATH/vendor/etc
# silently produces a module that mounts nothing.
module_path_for() {
  case "$1" in
    /system/*) echo "$MODPATH$1" ;;
    /vendor/*) echo "$MODPATH/system$1" ;;
    *)         echo "$MODPATH/system$1" ;;
  esac
}

validate_xml() {
  grep -q "$EFFECT_UUID" "$1" &&
  grep -q "</audio_effects_conf>" "$1" &&
  [ "$(wc -c < "$1")" -gt 200 ]
}

validate_conf() {
  grep -q "$EFFECT_UUID" "$1" && [ "$(wc -c < "$1")" -gt 100 ]
}

patch_xml() {
  awk -v lib="        <library name=\"jamesdsp\" path=\"libjamesdsp.so\"/>" \
      -v eff="        <effect name=\"jamesdsp\" library=\"jamesdsp\" uuid=\"$EFFECT_UUID\"/>" '
    { print }
    /<libraries>/ && !l { print lib; l=1 }
    /<effects>/   && !e { print eff; e=1 }
  ' "$1" > "$2"
}

patch_conf() {
  awk -v lib="  jamesdsp {\n    path /system/lib/soundfx/libjamesdsp.so\n  }" \
      -v eff="  jamesdsp {\n    library jamesdsp\n    uuid $EFFECT_UUID\n  }" '
    { print }
    /^libraries[[:space:]]*\{/ && !l { printf "%s\n", lib; l=1 }
    /^effects[[:space:]]*\{/   && !e { printf "%s\n", eff; e=1 }
  ' "$1" > "$2"
}

patch_audio_config() {
  found=0
  for cfg in $(find_configs); do
    # Already registered by another copy of this module: nothing to do.
    grep -q "$EFFECT_UUID" "$cfg" && { ui_print "- Already present in $cfg"; found=1; continue; }

    target="$(module_path_for "$cfg")"
    mkdir -p "$(dirname "$target")"

    case "$cfg" in
      *.xml) patch_xml  "$cfg" "$target"; ok=$(validate_xml  "$target" && echo 1) ;;
      *.conf) patch_conf "$cfg" "$target"; ok=$(validate_conf "$target" && echo 1) ;;
    esac

    if [ "$ok" = "1" ]; then
      set_perm "$target" 0 0 0644
      ui_print "- Registered in $cfg"
      found=1
    else
      # Better to ship no override than one that stops the audio server booting
      rm -f "$target"
      ui_print "! Generated config for $cfg failed validation, skipped"
    fi
    ok=
  done
  [ "$found" = "1" ] || ui_print "! No audio config patched; the effect will not load"
}

# For AML: drop our entries where it looks for them, and leave the merge to it.
write_effect_fragment() {
  mkdir -p "$MODPATH/system/vendor/etc"
  cat > "$MODPATH/aml.sh" << AMLEOF
# Consumed by Audio Modification Library when it merges audio mods.
AML_LIB_NAME="jamesdsp"
AML_LIB_PATH="libjamesdsp.so"
AML_EFFECT_NAME="jamesdsp"
AML_EFFECT_UUID="$EFFECT_UUID"
AMLEOF
  ui_print "- Wrote aml.sh for the library to pick up"
}
