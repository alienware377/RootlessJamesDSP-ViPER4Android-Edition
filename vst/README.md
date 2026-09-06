# RV4A Bass Exciter (VST3)

The psychoacoustic bass exciter from **RootlessViPER4Android**, as a plugin.

The DSP in `src/BassExciterDsp.h` is a verbatim port of the app's
`jdsp/Effects/bassex.c`, so a given setting sounds the same in both.

## What it does

Small speakers cannot reproduce a low fundamental, but the ear will infer one
from its harmonics. So rather than boosting bass that the driver cannot produce,
this generates the harmonics that imply it:

1. **Lowpass** at the cutoff, isolating the sub content
2. **Rectify** it — which creates harmonics at multiples of the fundamental
3. **Drive and soft-clip**, with a slow DC tracker keeping it centred
4. **Bandpass at 2.5× the cutoff**, keeping only the useful harmonics — this
   step is what stops it sounding like distortion on the bass
5. **Mix back** with the dry signal

A second band with its own cutoff, intensity and mix can be layered on top.

## Parameters

| Parameter | Range | Notes |
|---|---|---|
| Cutoff | 40–200 Hz | Where the sub content is taken from |
| Intensity | 0–100 % | Drive into the rectifier |
| Mix | 0–100 % | How much harmonic content is added |
| Band 2 | on/off | A second, independent band |
| Cutoff 2 | 30–200 Hz | Usually set below band 1 |
| Intensity 2 / Mix 2 | 0–100 % | As above |

Starting point: cutoff 100 Hz, intensity 40 %, mix 50 % — the app's defaults.

## Installing (FL Studio 20)

Copy `RV4ABassExciter.vst3` into `C:\Program Files\Common Files\VST3\`, then in
FL Studio: **Options → Manage plugins → Find installed plugins**.

FL Studio 20 supports VST3. There is no VST2 build because the VST2 SDK has not
been distributable for years.

## Building

CMake fetches the Steinberg VST3 SDK itself:

```
cmake -S vst -B build/vst -DCMAKE_BUILD_TYPE=Release
cmake --build build/vst --config Release
```

CI builds the Windows binary on every change — see
`.github/workflows/build-vst.yml`.

## Licence

Plugin code follows the repository's licence. The Steinberg VST3 SDK it links
against is licensed separately by Steinberg (GPLv3 or a proprietary licence);
its terms are yours to observe if you distribute a build.
