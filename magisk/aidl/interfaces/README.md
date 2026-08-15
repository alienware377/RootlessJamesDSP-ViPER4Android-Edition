# Vendored AIDL interfaces

Copied from AOSP at commit `1a56e38`, from the **frozen** `aidl_api` snapshots
rather than the live tree, so the definitions cannot shift under us:

- `android.hardware.audio.effect-V3` — the effect HAL
- `android.media.audio.common.types-V3` — the types it references
- `android.media.audio.eraser-V1` — needed because `Parameter.aidl` references
  `Eraser`, so it cannot simply be left out of the generation

**V3 is the version to target.** Its `queryEffects` takes three parameters
(type, implementation, proxy); V1 and V2 took fewer, and a device expecting one
signature will not talk to a service built against another. Frozen versions are
immutable in AOSP, which is exactly the property we want here.

Generated with the SDK's own `aidl` tool, which supports the NDK backend:

```
aidl --lang=ndk --structured --stability=vintf --version=3 \
     -I android.hardware.audio.effect-V3 \
     -I android.media.audio.common.types-V3 \
     -I android.media.audio.eraser-V1 \
     -o out/src -h out/include <files>
```

No AOSP build is needed for this step.
