# RootlessViPER4Android engine (Magisk module)

Installs this fork's DSP engine as a **system-wide audio effect**.

## Why this exists

In root mode the app doesn't use the engine bundled in the APK. It drives a
system effect through Android's `AudioEffect` API, which is a *different*
native library — the one shipped by whichever JamesDSP module is installed.

That means with the stock module, everything this fork added silently does
nothing in root mode: the extra reverb rooms, the ViPER classic limiter, the
rebuilt delay, the retuned speaker optimisation. `JamesDspRemoteEngine` returns
`true` for those calls, so they fail quietly rather than crashing.

This module ships the fork's own engine instead, exposing the same effect
descriptor the app looks up, so no app-side change is needed.

## Building

```
./gradlew :app:externalNativeBuildRootFdroidRelease   # or build the target directly
cmake --build <build-dir> --target jamesdsp_hal
```

The target emits `libjamesdsp_hal.so` (it cannot be called `libjamesdsp.so` in
the build tree without colliding with the APK's own library of that name).
Copy it to `magisk/libs/<abi>/libjamesdsp.so` — renaming as you go, since that
is the filename the audio server expects — then zip the `magisk/` directory.

## Installing

Flash in Magisk, then reboot. The installer copies the library into
`/system/lib*/soundfx/` and registers it in whichever audio effects config the
device uses — XML on Android 9+, the older `.conf` on some vendor images —
overlaying the file rather than editing the real one.

## Status

The library loads, initialises the engine and processes audio. Parameter
dispatch (`EFFECT_CMD_SET_PARAM`) is acknowledged but not yet mapped onto the
engine's setters, so effect *settings* don't cross over yet. That mapping is
the remaining work, and it is mechanical: the ids already exist in the JNI
wrapper.

**Do not install alongside the stock JamesDSP module** — two libraries claiming
the same effect UUID will conflict.
