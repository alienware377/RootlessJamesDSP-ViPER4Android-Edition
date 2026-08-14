#!/system/bin/sh
# Registers the effect library with whichever audio effects config the device
# uses. Both formats are handled: the XML used from Android 9 onward, and the
# older .conf still present on some vendor images.

EFFECT_UUID="f27317f4-c984-4de6-9a90-545759495bf2"
EFFECT_TYPE="f98765f4-c321-5de6-9a45-123459495ab2"

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

patch_xml() {
  src="$1"; dst="$2"
  # Insert the library and the effect entry, skipping if already present so a
  # reinstall can't duplicate them.
  if grep -q "$EFFECT_UUID" "$src"; then
    cp "$src" "$dst"
    return
  fi
  awk -v lib="        <library name=\"jamesdsp\" path=\"libjamesdsp.so\"/>" \
      -v eff="        <effect name=\"jamesdsp\" library=\"jamesdsp\" uuid=\"$EFFECT_UUID\"/>" '
    { print }
    /<libraries>/ && !ldone { print lib; ldone=1 }
    /<effects>/   && !edone { print eff; edone=1 }
  ' "$src" > "$dst"
}

patch_conf() {
  src="$1"; dst="$2"
  if grep -q "$EFFECT_UUID" "$src"; then
    cp "$src" "$dst"
    return
  fi
  awk -v lib="  jamesdsp {\n    path /system/lib/soundfx/libjamesdsp.so\n  }" \
      -v eff="  jamesdsp {\n    library jamesdsp\n    uuid $EFFECT_UUID\n  }" '
    { print }
    /^libraries[[:space:]]*\{/ && !ldone { printf "%s\n", lib; ldone=1 }
    /^effects[[:space:]]*\{/   && !edone { printf "%s\n", eff; edone=1 }
  ' "$src" > "$dst"
}

patch_audio_config() {
  found=0
  for cfg in $(find_configs); do
    found=1
    # Mirror the file's real path inside the module so Magisk overlays it
    target="$MODPATH$cfg"
    mkdir -p "$(dirname "$target")"
    case "$cfg" in
      *.xml)  patch_xml  "$cfg" "$target" ;;
      *.conf) patch_conf "$cfg" "$target" ;;
    esac
    set_perm "$target" 0 0 0644
    ui_print "- Registered in $cfg"
  done
  [ "$found" = "1" ] || ui_print "! No audio_effects config found; effect not registered"
}
