# AIDL effect HAL — design notes

## Why a second module is needed

The existing module ships a **legacy** effect library: a `.so` exporting
`AELI` / `EffectCreate` / `EffectGetDescriptor`, found by the audio server
through `audio_effects.xml`. That interface is what Android used up to 14.

From Android 14 the effect HAL is defined in **stable AIDL**, and from 15 it is
the only supported path on devices that ship it. The framework no longer walks
a config into `libaudioeffect`-style plugins; it queries a **vendor HAL
service** over binder:

- `IFactory.queryEffects(type, implementation)` — enumerate
- `IFactory.queryProcessing(type)` — what the effect applies to
- `IFactory.createEffect(implUuid)` → `IEffect`
- `IEffect` — open, close, command, parameter get/set, and an **FMQ** pair for
  audio rather than a `process()` callback

Two consequences worth stating plainly:

1. This is **not a relocation of a file**. The legacy `.so` cannot be loaded by
   the AIDL service at all; the engine has to be exposed behind a new
   interface, in a process that registers with `servicemanager`.
2. Audio no longer arrives as `audio_buffer_t` in a callback. The AIDL effect
   reads and writes **fast message queues**, so the processing loop is ours to
   drive.

## What can be reused

Everything below the interface. `hal/JamesDspHalEffect.cpp` already contains the
parts that matter — engine init from a negotiated config, block-bounded
processing, the parameter dispatch mapping our ids onto the engine's setters.
That code moves largely intact; the wrapper around it changes.

## Build problem to solve first

The AIDL interface headers (`android.hardware.audio.effect-V*-ndk`) come from
AOSP, not the NDK. Options, cheapest first:

1. Vendor the `.aidl` files and generate the NDK backend with the `aidl`
   compiler, pinning a version (`V2`/`V3`) and checking `queryProcessing`
   exists, since it was added after the first release.
2. Extract prebuilt headers from an AOSP build for the target version.
3. Hand-write the binder marshalling — smallest dependency, largest risk, and
   it breaks whenever the interface version moves.

Option 1 is the one to try: it keeps us honest about which interface version we
target, and the versioned `aidl_api` directory in AOSP tells us exactly what
each version contains.

## Device caveat: Pixels

Reported repeatedly for Pixel 8 and 9: no `audio_effects_config.xml` present to
patch at all, which is what other vendors' devices expose for injecting
effects. If the file genuinely does not exist on the target device, even a
correct AIDL implementation has nothing to register through, and the effect
service will never load it.

**Check before building for a device:**

```
adb shell ls -l /vendor/etc/audio_effects_config.xml
adb shell ls -l /vendor/etc/audio_effects.xml
adb shell dumpsys media.audio_flinger | head -40
adb shell service list | grep -i audio.effect
```

The fourth line is the important one: it shows whether an AIDL effect service
is registered and under what instance name. If there is a service but no
config, the next question is whether that service loads anything beyond what
the vendor built in.

## Status

Design only. Nothing here is built yet.
