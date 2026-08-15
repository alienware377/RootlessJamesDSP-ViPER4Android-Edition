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
