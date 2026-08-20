<h1 align="center">
  <img alt="Icon" width="75" src="img/icons/web/icon-192.png?raw=true">
  <br>
  RootlessViPER4Android
  <br>
</h1>
<h4 align="center">RootlessJamesDSP — ViPER4Android Edition (V4A)<br>ViPER4Android-style audio effects on Android — <b>no root required</b>.</h4>

<p align="center">
  <a href="https://github.com/alienware377/RootlessJamesDSP-ViPER4Android-Edition/actions/workflows/build-fork.yml">
      <img alt="Build status" src="https://img.shields.io/github/actions/workflow/status/alienware377/RootlessJamesDSP-ViPER4Android-Edition/build-fork.yml?branch=viper-extras">
  </a>
  <a href="https://github.com/alienware377/RootlessJamesDSP-ViPER4Android-Edition/releases">
      <img alt="Release" src="https://img.shields.io/github/v/release/alienware377/RootlessJamesDSP-ViPER4Android-Edition?include_prereleases">
  </a>
  <a href="LICENSE">
      <img alt="License" src="https://img.shields.io/github/license/alienware377/RootlessJamesDSP-ViPER4Android-Edition">
  </a>
</p>

<p align="center">
  <a href="#-features">Features</a> •
  <a href="#-download--install">Download</a> •
  <a href="#-limitations">Limitations</a> •
  <a href="#-faq">FAQ</a> •
  <a href="#-credits--thanks">Credits</a>
</p>

---

**RootlessViPER4Android** (RootlessJamesDSP — ViPER4Android Edition) is a system-wide **Android equalizer and audio effects app** that brings the beloved **ViPER4Android (V4A)** sound experience to **non-rooted phones**. It combines the powerful open-source **JamesDSP** engine with native re-implementations of ViPER's classic effects — Dynamic System, ViPER Bass, ViPER Clarity, Spectrum Extension, Field Surround, Tube Simulator (6N1J) and many more — all running **without root, without Magisk, and without custom ROMs**.

Beyond the ViPER suite, it adds a **fully reorderable DSP chain**, so you decide the order every effect runs in, with the output limiter always kept last. An **interactive parametric equalizer** lets you drag bands directly on the response curve, add or remove them with a long press, and fine-tune frequency, Q and gain on rotary controls with full undo. A **studio-grade echo/delay** offers mono, stereo and ping-pong modes with a filtered feedback path, sample-rate and bit-depth crushing, tape-style modulation, diffusion and feedback distortion — all laid out on a knob panel that reflows to fit any screen. **Up to four Liveprog (EEL2) scripts** can be chained together and picked in one multi-select pass, and **446 official ViPER DDC profiles** are bundled and ready to load.

The interface is yours to arrange: **reorder, hide and group the effect cards**, search across every effect, and save the whole setup — including your layout and chain order — into portable presets. Effect cards load as you scroll so the app opens instantly, and large effect buffers are allocated only while an effect is switched on, keeping memory use modest even with many apps playing audio. Processing also stands down on its own once playback goes quiet, so nothing is spent grinding through silence while your music is paused.

The **Reverberation** effect offers four rooms — the classic ViPER model plus a smooth studio Plate, a large warm Concert hall, and a natural Room with early reflections — and the output stage adds a **ViPER classic limiter**, a faithful re-creation of the original V4A limiter with the lookahead and transient bite that made it distinctive. The **convolver and DDC libraries** have their own search, custom sorting, hiding and groups, and can pull fresh impulse responses and DDC profiles straight from public repositories, skipping anything you already own.

Prefer the original experience? **ViPER4Android-only mode** narrows the app to the effects V4A shipped, using its naming, its processing order and its limiter, and switches everything else off so it costs nothing. A matching **ViPER4Android classic theme** is available on its own too, if you want the classic look without giving up any of the extras.

## 📸 Screenshots

| ViPER effects & FIR EQ | Output control & Bass exciter | Surround, Dynamic system & Clarity |
|:---:|:---:|:---:|
| ![ViPER effects and FIR equalizer](img/screenshots/viper_effects_eq.jpg) | ![Output control and dual-band bass exciter](img/screenshots/output_bass_exciter.jpg) | ![Surround, dynamic system and clarity sections](img/screenshots/viper_sections.jpg) |

## ✨ Features

### 🐍 ViPER4Android effect suite (native ports)
Arranged in the classic V4A order:

| Effect | What it does |
|---|---|
| **Playback gain control (AGC)** | Evens out quiet/loud tracks automatically |
| **FET compressor** | Punchy studio-style compression |
| **ViPER-DDC** | Headphone correction profiles — **446 official DDC files bundled!** |
| **Spectrum extension** | Restores sparkle & air to compressed music |
| **FIR equalizer** | Precision fixed-band EQ |
| **Convolver** | Impulse response processing |
| **Field surround** | Widens the stereo field naturally |
| **Differential surround** | Classic V4A channel-delay surround |
| **Headphone surround +** | Speakers-in-a-room simulation for headphones |
| **Reverberation** | Full room reverb (size, damping, width, wet/dry) |
| **Dynamic system** | ViPER Dynamic Bass with all **19 original device presets** + custom mode |
| **Tube simulator (6N1J)** | Warm analog tube saturation (up to 24 dB drive) |
| **ViPER bass** | Natural / Pure Bass+ / Subwoofer modes |
| **ViPER clarity** | Natural / OZone+ / XHiFi treble enhancement |
| **Auditory system protection (Cure+)** | Anti-fatigue crossfeed for long sessions |
| **Speaker optimization** | Corrective tuning for phone speakers |

### 🎚 Plus the full JamesDSP toolkit
Output limiter (with selectable peak / soft-saturation modes), auto-loudness compander, bass boost, **dual-band psychoacoustic bass exciter**, stereo widener, crossfeed, virtual room reverb, arbitrary-response graphic EQ (AutoEQ import), and a scriptable **Liveprog (EEL2)** engine.

### 💜 Quality-of-life
- Plain-language "ℹ️ what is this?" explainers on every section
- Smart UI: irrelevant sliders hide automatically
- Crash-safe DSP dispatch with built-in diagnostics
- Works per-app, session-based — no audio HAL patching

## 🆕 Added & changed vs. upstream RootlessJamesDSP

**Added in this fork:**
- 🐍 **16 native ViPER4Android effect sections** (see table above) — including brand-new engine code for Dynamic System, ViPER Bass, Clarity, FET Compressor, Cure+, Field/Differential/Headphone Surround, Reverberation, Spectrum Extension, AGC & Speaker Optimization
- 🔊 **Dual-band psychoacoustic bass exciter** — two independently tunable harmonic bands (the second band hides until you enable it)
- 🛡 **Selectable output limiter modes**: classic peak limiter, smooth **soft-saturation** (tanh) mode, or fully off
- 🎧 **446 official ViPER-DDC correction files bundled** and auto-installed on first launch
- ℹ️ Collapsible plain-language explainers on every effect section
- 🧰 Built-in crash diagnostics: native + JVM crash reports auto-copied to clipboard on next launch

**Changed & fixed:**
- 🔥 **Tube simulator actually works now** — upstream divided the drive value by 100 before it reached the engine (max ≈ 0.12 dB, inaudible); fixed and range extended to **24 dB**
- 🐛 Fixed a crash when seekbars use a `%` unit label
- 🪄 Smart UI: Dynamic System custom sliders only appear in Custom mode; irrelevant controls stay hidden
- 🗂 All ViPER-equivalent sections renamed to their **authentic V4A titles** and reordered into the classic V4A layout under a dedicated header
- ➖ Removed the redundant Parametric EQ card (Arbitrary-response/graphic EQ covers it)
- 📛 App renamed to **RootlessViPER4Android** (installs alongside the original RootlessJamesDSP)

## 🗺 Roadmap

### ✅ Shipped

- ~~**Reorderable DSP chain**~~ — put the effects in any order you like
- ~~**Interactive parametric equalizer**~~ — drag bands on the curve, rotary fine-tuning, live apply
- ~~**Full echo / delay unit**~~ — ping-pong, filtered feedback, bit crushing, modulation, on its own knob panel
- ~~**Chained Liveprog scripts**~~ — multi-select scripts, one card per script, chained in order
- ~~**Custom effect groups**~~ — create, rename and delete groups, move cards between them, hide what you don't use
- ~~**More reverb room types**~~ — Plate, Concert hall and Room alongside the classic ViPER model ([#1](https://github.com/alienware377/RootlessJamesDSP-ViPER4Android-Edition/issues/1))
- ~~**ViPER classic limiter**~~ — a faithful re-creation of V4A's own limiter, lookahead and all
- ~~**ViPER4Android-only mode**~~ — the original effect set, naming, processing order and limiter, with everything else switched off
- ~~**ViPER4Android classic theme**~~ — the classic look, available with or without the mode
- ~~**Library management for Convolver & DDC**~~ — search, sorting, hiding, groups, and downloading more profiles from public repositories
- ~~**Idle battery fix**~~ — processing stands down when playback is silent, not just when apps release the audio session
- ~~**Multiband distortion**~~ — pick frequency ranges to drive, with eight distortion characters, chorus, and a global mix, built on the interactive EQ editor with cutoff-type handles
- ~~**Full maximiser**~~ — four limiting algorithms, with character, transient emphasis, true-peak detection, stereo linking and oversampling
- ~~**Search, sorting & hiding on the presets screen**~~ — the same pull-down search, custom order, hiding and groups the convolver and DDC libraries have
- ~~**Linear / minimum phase toggle**~~ — on both equaliser cards, since the two are merged into one filter before the engine sees them
- ~~**Dynamic EQ**~~ — bands that only act while their own slice of the spectrum crosses a threshold, so taming a boom that happens on four notes no longer costs body on everything else; ships tuned for low boom, harshness and sibilance
- ~~**Multiband stereo imaging**~~ — width chosen per frequency range rather than all at once, with a mono-below control, so the low end can be folded to the centre while the top opens up; mono recordings come through untouched
- ~~**Impact (attack & sustain)**~~ — shapes how a note starts and how long it takes to die rather than how loud it is, per frequency range, so a held tone is left completely untouched while a drum hit can be sharpened or softened
- ~~**Low end**~~ — a steep subsonic filter to stop inaudible rumble eating headroom, one knob for body, and a wide dip where recordings turn thick (mono-below lives in the imaging card and punch in Impact, so neither is duplicated)
- ~~**Soft clipper**~~ — three curves for the maximiser's Character control, from one that bends immediately and adds density to one that stays linear until the very top and only clips peaks; a shape inside the maximiser rather than a second thing doing the limiter's job
- ~~**Mid/side modes**~~ — the dynamic EQ can be pointed at the centre of the image or at its edges, so a de-esser can catch a vocal without dulling the reverb around it; the untouched half comes through bit-for-bit
- ~~**Exciter**~~ — four ranges, each with its own amount, and five characters: three symmetrical ones that add odd harmonics and two lopsided ones that add even harmonics, which is the difference between a transistor sound and a valve one. The older two-band Bass exciter card is left in place so existing setups keep working
- ~~**Vintage tape**~~ — wow and flutter as a genuinely modulated delay rather than a phase trick, gentle tape saturation, a bias control that runs the right way round (down is brighter and grittier, up is duller), and the head bump that makes tape masters sound weightier

### 🔜 Planned

- **Global "Poweramp-style" mode** — process the whole output mix with Android's built-in effect API instead of audio capture: no capture permission, no persistent notification, and apps that block capture (Spotify) would work. Trade-off: only EQ/bass/limiter-class effects can run there — the ViPER effects need the capture engine. Deprecated by Google and device-dependent (works on many Samsungs, often not Pixels), so it'll ship clearly labelled as experimental.
- **Spectral shaper** — dynamic taming of harshness and resonances, especially handy on bright IEMs
- **Match EQ** — match playback to a target curve, pairing naturally with the AutoEq and DDC profiles already supported
- **Binaural upmix** — build a surround bed from stereo, then fold it back to headphones through HRTF/BRIR convolution so sources can sit behind and above you. Headphones only; two speakers can't do this without crosstalk cancellation
- **AIDL effect HAL** — a second module for Android 15 and newer, where the effect framework moved from the legacy plugin interface to a binder service. Not a port of the existing one: the engine has to be exposed behind a new interface and driven from fast message queues rather than a process callback
- **Rooted mode** — for devices with the JamesDSP magisk module (testers welcome!)
- **GitHub search for new profile sources** — built but parked while its edge cases are ironed out; adding your own repository link already works
- **Built-in media player** *(after everything above)* — a library browser in the spirit of jetAudio, with independent tempo and pitch control as well as combined speed, and a now-playing screen modelled on vintage turntables: platter wow and flutter, motor speed drift, surface noise and other period defects as deliberate, adjustable colour. Visual and behavioural reference: the iPad app *Vinyl* ([demo](https://www.youtube.com/watch?v=8leOYocOGSs))
- **Remember effect on/off states** across a ViPER4Android-only mode round trip — your settings already survive, but effects switched off by the mode stay off when you leave it

## 📲 Download & Install
1. Grab the latest APK from the [**Releases**](https://github.com/alienware377/RootlessJamesDSP-ViPER4Android-Edition/releases) page (or fresh builds from [Actions](https://github.com/alienware377/RootlessJamesDSP-ViPER4Android-Edition/actions)).
2. Install and follow the in-app onboarding — it walks you through the permission setup step-by-step.
3. Play music, open the app, and start flipping switches 🎶

Installs **alongside** the original RootlessJamesDSP as a separate app — keep both or remove the original, your choice.

## ⚠️ Limitations
Rootless audio capture on Android has inherent restrictions (inherited from upstream RootlessJamesDSP):
* Apps that block internal audio capture (some streaming apps, e.g. Spotify) are unaffected unless patched
* Calls and protected media may bypass processing
* A persistent capture notification is required by Android

See the [upstream documentation](https://github.com/timschneeberger/RootlessJamesDSP) for details and workarounds.

## ❓ FAQ
**Is this really ViPER4Android?**
The original V4A is closed-source and requires root. This project re-implements its most-loved effects natively on top of the JamesDSP engine — same spirit, same workflow, zero root.

**Do I need Magisk / a custom kernel / a driver install?**
Nope. Nothing to flash, no "driver" status screen, no SELinux tricks.

**Will it work on Android 13/14/15?**
It targets modern Android and is tested on current versions. If something breaks, open an issue with the in-app crash report.

## 🙏 Credits & Thanks
This fork stands entirely on the shoulders of giants — thank you!

* **[James Fung (@james34602)](https://github.com/james34602)** — creator of **[JamesDSP](https://github.com/james34602/JamesDSPManager)**, the brilliant open-source audio engine powering every effect here
* **[Tim Schneeberger (@timschneeberger)](https://github.com/timschneeberger)** — creator of **[RootlessJamesDSP](https://github.com/timschneeberger/RootlessJamesDSP)**, which made system-wide DSP without root a reality and is the direct base of this fork
* **The ViPER's Audio team (ViPER520 & zhuhang)** — creators of the legendary **[ViPER4Android](https://github.com/vipersaudio)**, whose effect designs inspired every port in this edition
* All upstream contributors and translators of both projects 💜

## Privacy

The app collects no personal data, contains no analytics or tracking, and processes all audio locally on your device. See [PRIVACY.md](PRIVACY.md) for the full policy.

## 📄 License
This project is licensed under **GPL-3.0**, same as upstream RootlessJamesDSP. See [LICENSE](LICENSE). ViPER4Android is a trademark of its respective owners; this project is an independent, unaffiliated re-implementation.

---
<sub>Keywords: ViPER4Android no root, V4A without root, rootless equalizer Android, system-wide equalizer, JamesDSP fork, Android audio effects, bass boost, DDC headphone correction, AutoEQ, spectrum extension, tube amp simulator</sub>
