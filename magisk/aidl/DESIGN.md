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

## Blocker: the proxy design does not work as specified

Read from AOSP's `EffectMain.cpp`, which is what the stock service runs:

```cpp
std::string serviceName = std::string() + effectFactory->descriptor + "/default";
binder_status_t status = AServiceManager_addService(..., serviceName.c_str());
CHECK_EQ(STATUS_OK, status);
```

The instance name is **hardcoded**. The service takes no argument for it, so the
plan of starting the stock binary under `vendor_original` and delegating to it
cannot work: it would register `/default` regardless, race ours for the name,
and `CHECK_EQ` would abort whichever lost. A crash loop in the audio HAL is a
worse outcome than not installing.

The same file shows it exits if no config file is found. So the service on a
device with no `audio_effects_config.xml` is **not** this AOSP one - it is the
vendor's own, whose binary name and behaviour we cannot assume either.

### What this rules out

Proxying by re-registering the stock service under another name. That was the
only route identified for devices with no config to patch, which is exactly the
Pixel case this was aimed at.

### What remains viable

For devices that *do* ship `audio_effects_config.xml` - the majority of AIDL
devices - no proxy is needed at all. The stock service loads libraries listed in
that config, so shipping our effect as a library and adding an entry is both
simpler and far safer: nothing takes over a service, and a failure means our
effect is missing rather than the device having no effect HAL.

That is the design to build next, and it serves everyone except Pixel.

### Pixel

**Superseded — see PIXEL-ROUTE.md.** The premise below was wrong: the config is
not absent, it lives inside the audio APEX, which is where the service looks
first and where nobody thought to check.

Previous conclusion, kept for the reasoning: Not a packaging problem to solve with more care: there is no
config to declare an effect in, and the one interposition point available is
closed by a hardcoded name. Worth revisiting only with new information about how
Google's own effect service discovers effects.

## Original reasoning, kept for context: why a config patch cannot work on Pixel

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

- **`IFactory` implemented** in `src/FactoryImpl.cpp`, proxying the vendor's.
  The rule it follows throughout: never leave the device worse than it was.
  Everything that is not ours is delegated verbatim; a vendor failure is logged
  and tolerated rather than propagated, because a device with only our effect
  is poor while a device the framework believes has no effect HAL is broken.
  `queryProcessing` passes straight through - we add an effect, we do not
  change routing rules.

  Instance naming is settled: the init script starts the stock binary as
  `IFactory/vendor_original` and this service as `/default`, so the delegation
  target is explicit rather than trying to hold a handle to a service we are in
  the middle of replacing.

- **Service entry point and CMake wiring** in `src/service.cpp` and
  `CMakeLists.txt`. Generation is clean: **488 files, zero errors**, using
  build-tools **37**. Build-tools 34 must not be used - it does not recognise a
  suppression annotation in `AudioChannelLayout.aidl`, reports it as an error,
  and silently emits only part of the package, which then fails much later as
  missing headers.

**Build state (latest)**

The detached build pattern works and produced a full log. Progress since:

- Binder service symbols now resolve — the soname stub does its job.
- The convolver symbols resolved once the engine glob was made recursive.
- Still failing: `AgcProcess`, `DDCProcess` and friends are undefined at link.

That last one needs a careful look rather than another guess. Those symbols live
in `jdsp/Effects/viperextras.c`, which the recursive glob *should* already
match, and the app build compiles the identical pattern successfully. Two
possibilities worth separating before changing anything:

1. The log read may predate the glob fix landing, in which case it is stale.
2. Or the file compiles with an error earlier in the log and only shows up as a
   missing symbol at link, which would mean the tail is misleading.

Check the head of the log for compile failures before touching the glob again -
the tail alone does not distinguish these.

Note: `Tee-Object` writes UTF-16, which makes the log awkward to read through
the filesystem connector. Worth switching the detached script to
`Out-File -Encoding utf8`.

**Earlier build progress**

Every external dependency now resolves — interfaces, fmq, binder, native_handle
and logging. The compiler reached our own sources and found three real faults,
all fixed:

- The effect UUIDs have the high bit set, and `AudioUuid.timeLow` is a signed
  32-bit field, so they must be written as the signed pattern rather than
  narrowed implicitly.
- The effect class became a header, so the factory can construct it without a
  second translation unit defining the same symbols.
- The parameter hook was declared but never defined, which would have failed at
  link time.

**Worth knowing about parameters**

Nothing reaches an AIDL effect from the app yet: `JamesDspRemoteEngine` drives
the legacy `AudioEffect` API, which has no path to a binder service. So an AIDL
build will process audio but ignore settings until that half is written. The
hook logs whatever arrives, so the moment the app side exists it will be
obvious.

**Earlier issues, resolved**

`cutils/native_handle.h` is vendored, and the full current `binder_ndk` header
set was taken from AOSP rather than patching individual gaps, so the generator
and the declarations come from the same platform level. The build was
interrupted before it could be rerun, so the next step is simply:

```
magisk/aidl/generate.sh          # build-tools 37, not 34
cmake -S magisk/aidl -B <dir> -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-33
cmake --build <dir>
```

**For the record, the issues that were resolved**

1. `cutils/native_handle.h` — one more platform header to vendor or shim,
   same category as the four already handled for libfmq.

2. **Binder API mismatch, the one that matters.** The code generated by
   build-tools 37 calls into a newer `libbinder_ndk` than **NDK r27** declares:
   `asBinderReference` is undeclared and one call has a two-argument form
   against four supplied. The generator and the NDK are from different platform
   levels.

   Three ways out, in order of preference:

   - Vendor the current `binder_ndk` headers from AOSP, as already done for
     `binder_manager.h`. The symbols exist in `libbinder_ndk.so` on any device
     new enough to have an AIDL effect HAL, so only declarations are missing.
     Consistent with everything else here.
   - Use a newer NDK whose binder headers match the generator.
   - Generate with an older `aidl`, which means first dealing with the
     truncation problem above - the worst option, since it trades a clear
     compile error for silently incomplete code.

**Next**

- **`IEffect` implemented** in `src/EffectImpl.cpp`: all eight methods, the
  three message queues created and sized from the negotiated frame count, and a
  worker thread that deinterleaves, processes in engine-sized chunks and
  reports consumed/produced back through the status queue.

**Remaining dependency: libfmq**

`AidlMessageQueue` lives in `system/libfmq`, which does not ship in the NDK
either. Same problem as the interfaces, and the same shape of answer: vendor
it, or hand-roll the ring buffer from the grantor descriptors. Vendoring is
preferable - the descriptor layout is not something to reimplement from
guesswork, and getting it subtly wrong would surface as audio corruption rather
than a clean failure.

**Next**
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

---

# Field research: how the existing Pixel mods get in

Written after searching for how anyone else solves this, rather than assuming
we are first. Two projects already do, and between them they answer the
question the notes above left open.

- **PIXAML** (`ShadoV90/PIXAML`, authored with `anonymix007`) - a Magisk module
  whose stated purpose is exactly ours: "allow Pixels with AIDL audio effects
  use JamesDSP and (potentially) Viper4Android".
- **`anonymix007/vendor_aml_effects`** - the source of the service PIXAML
  ships, derived from AOSP's reference effect factory.

## The config file: there isn't one, so they make one

The direct answer to "where does the file live on a Pixel" is that it does not
live anywhere - and the ecosystem's response is not to find it but to **create**
it. PIXAML's `aml.sh` writes an empty `/vendor/etc/audio_effects_config.xml`
through the module's `system/vendor/etc` overlay when the path is absent:

```xml
<audio_effects_conf version="2.0" ...>
    <libraries>
    </libraries>
    <effects>
    </effects>
</audio_effects_conf>
```

Each audio mod then appends its own `<library>` and `<effect>` line with `sed`
(see their `v4a.sh`). So the file becomes a shared registry that mods edit,
rather than something the vendor provides.

Creating the file is necessary but **not sufficient**: nothing on the device
reads it. That is what the rest of the module is for.

## How they make something read it

Three pieces, and the third is the clever one.

**1. A second factory, not a replacement.** `vendor.aml.effect-service` is
AOSP's reference `Factory` with one change: it registers as
`IFactory/aml`, and takes the *vendor's* `IFactory/default` as its delegate,
resolved at startup with `waitForService`. The stock service is never
displaced, never renamed, never stopped.

**2. It reads the config we just created.** Being AOSP's factory, it calls
`audio_find_readable_configuration_file("audio_effects_config.xml")`, which
searches `/odm/etc`, `/vendor/etc`, `/system/etc` - so the file PIXAML creates
is found - and `dlopen`s every library named in it.

**3. The framework is bent toward it by patching its client library.** This is
the part that has no equivalent in our notes. `patch.sh` copies
`/system/lib64/libaudiohal@aidl.so` into the module and runs `patchelf` on the
copy:

```
patchelf --add-needed aml.so                                libaudiohal@aidl.so
patchelf --clear-symbol-version AServiceManager_waitForService  libaudiohal@aidl.so
patchelf --rename-dynamic-symbols aml_map.txt               libaudiohal@aidl.so
```

with `aml_map.txt` containing a single line:

```
AServiceManager_waitForService AMLServiceManager_waitForService
```

`aml.so` then defines that renamed symbol, and its whole body is:

```c
if (strcmp(instance, "android.hardware.audio.effect.IFactory/default") == 0)
    return AServiceManager_waitForService(".../IFactory/aml");
return AServiceManager_waitForService(instance);
```

So the framework's own lookup of the effect factory is redirected, inside the
framework's own library, to the proxy - which then answers with the vendor's
effects plus whatever the config file listed. Patching a copy rather than the
original is deliberate on their part: an OTA that bumps a dependency cannot
bootloop the device, because the stock file is still underneath.

## What this changes for us

Our plan was to **become** `IFactory/default`: rename the vendor service via an
init script and take its name. That works, but it is the most dangerous
arrangement available - it is the one case where our failure leaves the
framework with no effect factory at all, which is why the notes above argue the
boot watchdog matters more here than in the legacy module.

Their arrangement avoids that entirely. Nothing about the vendor service
changes; the redirection lives in a library that is itself a patched copy.

But the more useful realisation is a step further back. The factory - ours or
theirs - loads **effect libraries**, and the contract for one is small
(`EffectFactory.cpp`, `openEffectLibrary`): a `.so` exporting three C symbols.

```c
extern "C" binder_exception_t createEffect (const AudioUuid*, std::shared_ptr<IEffect>*);
extern "C" binder_exception_t queryEffect  (const AudioUuid*, Descriptor*);
extern "C" binder_exception_t destroyEffect(const std::shared_ptr<IEffect>&);
```

That is the same `IEffect` we have already implemented in `src/EffectImpl.cpp`,
wrapped in three functions. Their bundled config shows both major mods taking
this shape already:

```xml
<library name="jdsp"   path="libjamesdspaidl.so"/>
<library name="v4a_re" path="libv4aidl_re.so"/>
```

So the natural product of this work is an **effect library**, not a service.
That is worth stating plainly because it deletes a large amount of what remains:

| | Factory service (our plan) | Effect library |
|---|---|---|
| `IFactory` implementation | needed | not needed |
| Service registration, init script, SELinux domain | needed | not needed |
| Instance-name contention with the vendor | yes | none |
| Worst-case failure | device has **no** effect HAL | our effect fails to load |
| Works on vendors that ship a config | via a config patch | same |
| Works on Pixels | on its own | needs PIXAML present |

The last row is the only column where the service wins, and it wins by taking
on every risk in the rows above it.

## Decision

Target the **effect library** shape.

- `src/EffectImpl.cpp` is kept as-is; it is the bulk of the work and it is
  already the right thing.
- Add a small `library.cpp` exporting the three entry points over it.
- Declare ourselves in `audio_effects_config.xml` the way every other mod does,
  by shipping an `.aml.sh` fragment, so we register through AML on devices that
  have it and through the vendor's own config on devices that ship one.
- On a Pixel, state the PIXAML dependency in the installer rather than
  reimplementing it. Their approach is sound, it is maintained, and duplicating
  the `libaudiohal` patch would mean two modules fighting over the same file.

`src/FactoryImpl.cpp` and `src/service.cpp` stay in the tree but stop being the
target. They remain the fallback if a device turns up with an AIDL HAL, no
config, and no PIXAML support - and having built them, we know that path works.

## Licensing note

`vendor_aml_effects` carries no licence, so **none of it is copied**. What is
taken is the shape of the mechanism, which is observable from the module's
shell scripts and from AOSP's own factory (Apache-2.0) that it derives from.
Our `IEffect` is already written and owes it nothing.
