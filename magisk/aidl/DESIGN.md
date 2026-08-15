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

## Field data: Pixel 9 Pro XL, Android 17

```
ls /vendor/etc/audio_effects_config.xml   -> No such file or directory
ls /vendor/etc/audio_effects.xml          -> No such file or directory
service list | grep audio.effect
  -> android.hardware.audio.effect.IFactory/default
find /system -name "*audio_effects*.xml"
  -> /system/etc/audio_effects.xml
```

Three things follow, and they decide the design.

**The AIDL effect service is running**, registered under the instance name
`default`. So the device does host effects through the modern interface.

**There is no vendor config to patch.** The AOSP reference service learns which
libraries to load by reading `audio_effects_config.xml`; this device has none,
so there is no file into which our effect could be declared. Adding a library
to the filesystem would leave it unreferenced and unloaded.

**The `/system/etc/audio_effects.xml` that does exist is the framework-side
file**, not the HAL's library list. Under AIDL the framework asks the service
via `queryEffects` and `queryProcessing` instead, so patching it registers
nothing.

## Consequence: a config patch cannot work here

On vendors that ship `audio_effects_config.xml`, an AIDL module can declare
itself in that file and be loaded by the stock service. That is the cheap path,
and it is what other AIDL audio mods rely on.

This device offers no such hook. The only remaining way in is to **become the
service**: publish our own implementation of
`android.hardware.audio.effect.IFactory/default`, hold a handle to the vendor
implementation behind it, and answer:

- `queryEffects` — the vendor's list plus ours
- `queryProcessing` — the vendor's answer, unchanged
- `createEffect` — ours for our UUID, delegated for everything else

Effectively an effect proxy. It keeps every stock effect working while adding
one, which also means a failure in our path degrades to "our effect missing"
rather than "no audio effects at all".

Costs to be honest about, since this is a large step up from the legacy module:

- Only one process may hold an interface instance name, so the vendor service
  has to be stopped and ours started in its place, via an init script the
  module installs.
- A new service needs an SELinux domain permitted to talk to audioserver and
  to hold that interface. Root can add policy live, but it has to be right.
- If our service fails to start, the device has **no** effect HAL. That is a
  worse failure than the legacy module could produce, so the boot watchdog
  matters more here, not less.

## Order of work

1. Generate the NDK backend from vendored `.aidl` files, pinned to a version.
2. Implement `IEffect` over FMQ around the existing engine wrapper.
3. Implement the proxying `IFactory`.
4. Package as a separate module, refusing to install where the stock service
   cannot be located.

Vendors that *do* ship `audio_effects_config.xml` can be served by steps 1-2
alone with a config patch, so those devices are reachable well before Pixel is.

## Progress

**Done**

- Interfaces vendored and pinned: `android.hardware.audio.effect-V3` and
  `android.media.audio.common.types-V3`, taken from AOSP's frozen `aidl_api`
  snapshots (commit `1a56e38`) rather than the live tree, so they cannot shift.
  98 files, laid out under their package paths so the include roots resolve.
- Version settled: **V3**, whose `queryEffects` takes three parameters. V1 and
  V2 differ, and a service built against the wrong one will not be talked to.
- Toolchain settled: the **SDK's own `aidl`** supports `--lang=ndk`, so the
  NDK backend can be generated without an AOSP build. This was the main
  unknown, and it removes the worst of the three options in the notes above.
- The interface is small: `IFactory` is four methods.

- **Backend generates cleanly**: 29 sources and 87 headers, including
  `BnFactory.h` and `BnEffect.h`, the base classes to implement. Run
  `magisk/aidl/generate.sh`. Five further packages had to be vendored to get
  there, each discovered by generation failing in turn: the eraser types
  (reached through `Parameter`), the FMQ descriptors (how `IEffect` carries
  audio), and the audio common metadata types.

**Next**

1. ~~Generate the NDK backend~~ — done, see `generate.sh`. Remaining:
   ```
   aidl --lang=ndk --structured --stability=vintf --version=3 \
        -I magisk/aidl/interfaces/android.hardware.audio.effect-V3 \
        -I magisk/aidl/interfaces/android.media.audio.common.types-V3 \
        -o out/src -h out/include <effect aidl files>
   ```
2. Implement `IEffect` over FMQ, reusing the engine setup, block-bounded
   processing and parameter dispatch already written in
   `app/src/main/cpp/hal/JamesDspHalEffect.cpp`.
3. Implement the proxying `IFactory` for devices with no config to patch.
4. Package separately, refusing to install where the stock service is absent.
