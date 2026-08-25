## ⚠️ Read this first — v3.35.0 does not update your existing install

The package name has changed to this project's own, so Android treats this as a **new app**. It installs **alongside** your current copy rather than updating it, and your settings do not follow on their own.

**Back up before you switch.** In your existing app: *Settings → Backup & restore → Create backup*. Install v3.35.0, restore that file, and your presets, card layout and every effect setting come across intact. Then uninstall the old one.

| | old | new |
|---|---|---|
| Rootless | `me.timschneeberger.rootlessjamesdsp` | `com.alienware377.viper4android.rootless` |
| Rooted | `james.dsp` | `com.alienware377.viper4android.rootful` |

The rooted build is now called **RootfulViPER4Android**, because shipping the *rooted* app with "Rootless" in its name was never anything but confusing.

---

## 🎛 A full mastering suite

Six new effect cards, plus substantial additions to two existing ones.

- **Dynamic EQ** — bands that act only while their own slice of the spectrum crosses a threshold, so taming a boom that happens on four notes no longer costs body on everything else. Bands are fully editable, and each can be pointed at the **centre of the stereo image or its edges** — a de-esser can catch a vocal without dulling the reverb around it, and the half you did not aim at comes through bit-for-bit unchanged.
- **Multiband stereo imaging** — width chosen per frequency range instead of all at once, with a mono-below control so the low end folds to the centre while the top opens up. Mono recordings come through untouched.
- **Impact** — shapes how a note *starts* and how long it takes to die, rather than how loud it is, per frequency range. A held tone is left completely alone while a drum hit can be sharpened or softened.
- **Low end** — a steep subsonic filter so inaudible rumble stops eating headroom, one knob for body, and a wide dip where recordings turn thick.
- **Exciter** — four ranges each with its own amount, and five characters: three symmetrical ones adding odd harmonics, two lopsided ones adding even harmonics. That difference is what separates a transistor sound from a valve one. The older two-band Bass exciter stays put so existing setups keep working.
- **Vintage tape** — wow and flutter as a genuinely modulated delay line rather than a phase trick, gentle saturation, a bias control that runs the right way round (down is brighter and grittier, up is duller), and the head bump that makes tape masters sound weightier.
- **Maximiser** — four limiting algorithms with character, transient emphasis, true-peak detection, stereo linking and oversampling, plus a **soft clipper** offering three curves: from one that bends immediately and adds density to one that stays linear until the very top and only catches peaks.
- **Multiband distortion** — pick the ranges to drive, with eight distortion characters, chorus and a global mix, built on the interactive EQ editor.

## 🔓 Rooted mode

A Magisk module carrying this fork's **own engine** as a system-wide audio effect, so rooted devices run these effects rather than the stock JamesDSP set. Ships as a separate app, **RootfulViPER4Android**, alongside the rootless one. Legacy effect HAL, so Android 14 and below — Android 15 moved to a binder service and that work is still in progress.

## 🎨 Theme builder

Design a scheme from a single base colour with hue, saturation and brightness sliders or a hex value, with dark and pure-black switches and a live preview of the derived roles. Hand-set Secondary and Tertiary now actually reach the things they should, including the equaliser graphs.

## 🎚 Equaliser

A **linear / minimum phase** toggle for the FIR equaliser — the coefficient generator always supported both, but the mode was pinned to minimum phase. It appears on both equaliser cards, since the two are merged into one filter before the engine sees them, and the setting stays in step across them.

## 🐛 Notable fixes

- **Presets now actually load.** Loading a preset applied nothing at all in some cases ([#3](https://github.com/alienware377/RootlessJamesDSP-ViPER4Android-Edition/issues/3)).
- **Your card layout is yours.** Loading a preset no longer replaces your arrangement. The one thing it may still do is reveal a card it switches on, so an effect can never come on with nothing on screen to show for it. Take a preset's layout deliberately with long-press → *Apply card layout*. Backups now carry the layout too.
- **The card order stopped decaying as it saved.**
- **Cutoffs actually cut off** — the control drew a gentle slope across the whole spectrum rather than a corner.
- **Six controls that looked live and did nothing** now do what they say.
- **Distortion was inaudible**, and the overdrive curve behind it was wrong.
- **The tube simulator's drive** was divided by 100 before reaching the engine, capping it around 0.12 dB.
- **Effects added after you had reordered your chain were silently dropped** from it.
- **Every new effect was missing from the default chain**, so on a fresh install the six cards above did nothing at all. They were switched on, configured and receiving parameters — simply never dispatched.
- **A crash when switching an effect off** freed its buffers under the audio thread.
- Several **layout and preset crashes**, and a crash re-applying a layout whose groups had changed.
- **Cold-open scrolling** no longer stutters while cards install in the background.
- Effects designed at one sample rate **now follow when the rate changes** — previously a filter asked to sit at 30 Hz could sit at 27.6 Hz for a whole session.
- **Vintage tape no longer drops out** for 12 ms when switched on, or pops when switched off.

## 🙏 Credits

Built on [JamesDSP](https://github.com/james34602/JamesDSPManager) by james34602 and [RootlessJamesDSP](https://github.com/timschneeberger/RootlessJamesDSP) by Tim Schneeberger, which made system-wide DSP without root a reality. ViPER4Android effects are native re-implementations.

**138 commits since v2.8.2.**
