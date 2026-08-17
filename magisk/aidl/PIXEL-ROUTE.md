# A route for Pixel

The earlier conclusion — "no route" — was based on a wrong assumption. Onur
looked for `/vendor/etc/audio_effects_config.xml` and found nothing, and I took
that to mean the device has no effect config at all. It doesn't mean that.

## Where the config actually is

From AOSP's `EffectMain.cpp`, the service looks for its config in this order:

```cpp
candidatePath = "/apex/" + apexName + "/etc/audio_effects_config.xml";
if (access(candidatePath, R_OK) == 0) return candidatePath;
return audio_find_readable_configuration_file("audio_effects_config.xml");
```

**It looks inside its own APEX first.** Modern Pixels ship the audio HAL in an
APEX, so the config is almost certainly at
`/apex/<audio-apex-name>/etc/audio_effects_config.xml` — a path nobody would
find by checking `/vendor/etc`. The fallback only searches `/odm`, `/vendor`
and `/system` when the APEX copy is absent.

So the config exists. It was just never where we looked.

## Why this changes the design completely

The service `dlopen`s each library named in that config and resolves three
symbols (`EffectFactory.cpp`):

```
createEffect    queryEffect    destroyEffect
```

That is a **plugin interface**. Nothing needs to take over a service name, so
the proxy design — and the hardcoded-instance-name blocker that killed it — is
irrelevant. We ship a library, add an entry, and the stock service loads us
alongside everything else.

Failure mode improves accordingly: a broken library means **our effect is
missing**, not **the device has no effect HAL**.

## The two real obstacles

**1. Writing into an APEX.** APEXes are mounted read-only from signed images
and Magisk cannot overlay them the ordinary way; this is a long-standing
limitation. But bind-mounting over a file inside `/apex` from an early boot
script has been demonstrated to work — the mount lands, and the substituted file
is what is on disk at that path.

Our case is more forgiving than the usual attempt, which is why it is worth
trying: the target is a **config file read once when the service starts**, not a
library that may already be loaded. A bind-mount performed in `post-fs-data`,
well before `class hal` services start, should be what the service reads.

**2. Linker namespace.** A vendor or APEX process cannot load libraries from
arbitrary paths. Our library has to sit somewhere that process is permitted to
load from — most likely the APEX's own `lib64`, or the vendor library path the
existing entries use. The config's existing entries tell us which, since they
are already being loaded successfully.

## What to ask Onur for

These four answer everything still open:

```
ls -l /apex/*/etc/audio_effects_config.xml
cat /apex/*/etc/audio_effects_config.xml | head -40
pgrep -f audio.effect | head -1 | xargs -I{} readlink /proc/{}/exe
pgrep -f audio.effect | head -1 | xargs -I{} cat /proc/{}/maps | grep -o '/[^ ]*lib64[^ ]*' | sort -u | head
```

In order: whether the config is where the source says; what libraries it already
declares and by what path form; which binary is actually serving the interface;
and which library directories that process really loads from — which settles
the linker namespace question by observation rather than assumption.

## Honest status

This is a credible route, not a finished one. The config location is
established from source; the plugin interface is established from source; the
bind-mount is reported to work but is unproven here; the linker namespace is
unknown until we see the device's own answer.

Materially better than the proxy design regardless: no service is displaced, and
the worst case is a missing effect rather than broken audio.

---

# It is already being done: PIXAML

Written after searching for prior art rather than assuming we are first. Two
projects already ship this on Pixel 8 and 9, and between them they settle both
"real obstacles" above — one by solving it, one by sidestepping it entirely.

- **PIXAML** — `ShadoV90/PIXAML`, authored with `anonymix007`. Its `module.prop`
  states the purpose exactly: *"allow Pixels with AIDL audio effects use
  JamesDSP and (potentially) Viper4Android"*.
- **`anonymix007/vendor_aml_effects`** — the source of the service it ships,
  derived from AOSP's reference effect factory.

That JamesDSP already works on a Pixel through this is the most useful fact
here: the route is proven, not theoretical.

## They do not write into the APEX at all

Obstacle 1 above is the one I was least sure of, and their answer is to avoid
it. Their `EffectMain.cpp` is AOSP's, **minus the APEX branch**:

```cpp
if (argc > 1 && access(argv[1], R_OK) == 0) configFile = argv[1];
else configFile = android::audio_find_readable_configuration_file(kDefaultConfigName);
```

With no APEX lookup it falls straight through to the `/odm`, `/vendor`,
`/system` search — and their `aml.sh` creates the file it will find, when the
path is absent, through the module's `system/vendor/etc` overlay:

```xml
<audio_effects_conf version="2.0" ...>
    <libraries>
    </libraries>
    <effects>
    </effects>
</audio_effects_conf>
```

Each mod then appends its own `<library>` and `<effect>` line with `sed` — their
`v4a.sh` does exactly this. The file becomes a registry the mods share, living
somewhere writable, rather than something the vendor supplies.

Note what this implies about the plan above: if the APEX copy **does** exist,
the stock service reads it and never consults `/vendor/etc` — so creating a
config there achieves nothing on its own. That is precisely why they needed the
next piece.

## How they get the framework to talk to that service

Their service registers as `IFactory/aml`, **not** `/default`, and holds the
vendor's `/default` as a delegate resolved at startup. The stock service is
never renamed, stopped or displaced — which is what our proxy design foundered
on, since AOSP's binary hardcodes its instance name.

The framework is bent toward it by patching its *client* library. `patch.sh`
copies `/system/lib64/libaudiohal@aidl.so` into the module and runs `patchelf`
on the copy:

```
patchelf --add-needed aml.so                                   <copy>
patchelf --clear-symbol-version AServiceManager_waitForService <copy>
patchelf --rename-dynamic-symbols aml_map.txt                  <copy>
```

`aml_map.txt` is one line:

```
AServiceManager_waitForService AMLServiceManager_waitForService
```

and `aml.so` supplies that renamed symbol. Its entire body:

```c
if (strcmp(instance, "android.hardware.audio.effect.IFactory/default") == 0)
    return AServiceManager_waitForService(".../IFactory/aml");
return AServiceManager_waitForService(instance);
```

So the framework's own lookup of the effect factory is redirected, inside the
framework's own library, to the proxy. Patching a copy rather than the original
is deliberate: an OTA that bumps a dependency cannot bootloop the device,
because the stock file is still underneath the overlay.

## Obstacle 2, answered by observation

Their service is installed to `/vendor/bin/hw/vendor.aml.effect-service` with
`u:object_r:vendor_file:s0`, and `patch.sh` copies the versioned NDK
dependencies it needs into `/vendor/lib64` — resolving each by searching
`/system/lib64 /apex/*/lib64 /vendor/lib64` for the newest `<name>-V*-ndk.so`
and `--replace-needed`ing the binary to match what the device actually has.

So the answer to "which library directories is the process permitted to load
from" is **`/vendor/lib64`**, for a `/vendor/bin/hw` binary. Their sepolicy
needs only:

```
allow audioserver default_android_service service_manager find
```

because a name absent from `service_contexts` gets the default service type,
and audioserver has to be allowed to look it up.

## What this means for us

The structural point is that **both routes want the same artefact**. Whether the
stock service reads a bind-mounted APEX config, or a second factory reads a
config in `/vendor/etc`, what gets loaded either way is a library exporting
`createEffect`, `queryEffect`, `destroyEffect`. Their bundled config shows both
major mods already in that shape:

```xml
<library name="jdsp"   path="libjamesdspaidl.so"/>
<library name="v4a_re" path="libv4aidl_re.so"/>
```

So the routing question does not block the build. Build the library; decide the
route on device.

Three ways to package it, and I would not pick between them from here:

1. **Bind-mount the APEX config** — this document's original plan. Cleanest if
   it works: no patched framework library, no second service, no dependency on
   another module. Unproven, and the one that needs a device to settle.
2. **Depend on PIXAML.** Ship an `.aml.sh` fragment the way every other mod
   does and let it register us. Proven today, nothing for us to maintain, and
   duplicating their `libaudiohal` patch ourselves would put two modules in
   contention over the same file — an actively bad outcome.
3. **Reimplement their approach ourselves.** Most control, most risk, and no
   advantage over (2) that I can identify.

(2) is the sane default for Pixel, with (1) preferred if the device says it
works. Either way the library is identical, which is why it is worth writing
before the question is settled.

## Licensing

`vendor_aml_effects` carries no licence, so **nothing is copied from it**. What
is taken here is the shape of a mechanism, observable from the module's shell
scripts and from AOSP's own factory (Apache-2.0) that it derives from. Our
`IEffect` was written before any of this was found and owes it nothing.
