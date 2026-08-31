# Privacy Policy for RootlessViPER4Android

**Last updated: 9 August 2026**

RootlessViPER4Android ("the app") is a free, open-source audio processing app for
Android. This policy explains, in plain language, exactly what the app does and does
not do with your information.

**In short: the app collects nothing, sends no personal data anywhere, and has no
analytics, advertising, or tracking of any kind.** There is no account, no login, and
no server operated by the developer.

---

## 1. Information the app collects

**None.**

The app does not collect, store, transmit, or share any personal information. There is
no registration, no user account, no advertising identifier, no device fingerprinting,
and no usage analytics.

The official release builds published on GitHub and APKPure are built from the
`fdroid` product flavour, which **excludes all proprietary analytics and crash
reporting libraries at build time**. No Firebase, Crashlytics, or comparable SDK is
present in these builds. This can be verified in the app's public source code and
build configuration.

## 2. Audio processing

The app applies audio effects (equalization, reverb, compression, and similar) to
sound already playing on your device.

- Audio is processed **entirely on your device, in real time, in memory**.
- Audio is **never recorded to storage**, never uploaded, and never transmitted
  anywhere.
- The app does not listen to, analyse, or store the content of your audio for any
  purpose other than applying the effects you have enabled, moment to moment.

## 3. Permissions and why they are needed

| Permission | Why the app needs it |
| --- | --- |
| **Record audio** (`RECORD_AUDIO`) | Required by Android to receive the audio stream for processing. Despite the name, this permission is used together with the media projection system to capture *playback* audio, not your microphone. Nothing is saved or sent. |
| **Media projection** (`FOREGROUND_SERVICE_MEDIA_PROJECTION`) | The Android mechanism that allows a rootless app to capture and process device playback audio. This is what makes the app work without root. |
| **Foreground service** and **data sync** | Keeps audio processing running reliably while you use other apps, with the required ongoing notification. |
| **Post notifications** | Shows the processing status notification that Android requires for a foreground service. |
| **Internet** | Used **only** for optional features you explicitly trigger: downloading impulse responses and DDC profiles from public repositories, downloading AutoEq profiles, and opening links you tap. The app does not contact any server on its own for tracking or telemetry. |
| **Modify audio settings** | Lets the app configure audio output while processing. |
| **Receive boot completed** | Optionally restarts processing after a reboot, if you have enabled that setting. |
| **Wake lock** | Prevents the processing service from being suspended while audio is playing. |
| **System alert window** | Used for optional on-screen elements such as the effect overlay, where enabled. |
| **Dump** | Used to read audio session information on supported devices so effects can be applied to the correct output. |

You can revoke any permission at any time through Android's system settings. Doing so
may stop audio processing from working.

## 4. Data stored on your device

Your settings, presets, effect layouts, imported impulse responses, DDC profiles, and
Liveprog scripts are stored **locally on your device only**. They are never uploaded.

If a crash occurs, a technical crash log may be written to local storage so you can
view it under **Settings → Troubleshooting** and, if you choose, share it manually
when reporting a bug. These logs stay on your device unless you deliberately send
them. They contain technical diagnostic information, not personal data.

Uninstalling the app removes its settings and locally stored data.

## 5. Optional network features

Some features fetch files from public third-party servers **only when you actively ask
them to**:

- Downloading convolver impulse responses and ViPER DDC profiles from public GitHub
  repositories.
- Downloading AutoEq headphone correction profiles.
- Opening project, documentation, or translation links you tap.

When you use these features, your device makes an ordinary web request to that
third-party service (for example GitHub), which will see your IP address as any
website would. The developer does not operate, control, or receive data from those
services. Their own privacy policies apply — for example,
[GitHub's Privacy Statement](https://docs.github.com/site-policy/privacy-policies/github-privacy-statement).

The app performs no background network activity for any other purpose.

## 6. Children's privacy

The app collects no data from anyone, including children under 13. It is a general
purpose audio utility and is not directed at children.

## 7. Third-party distribution

The app is distributed through GitHub and APKPure. Those platforms may collect their
own data (such as download statistics) under their own privacy policies, which are
outside the developer's control.

## 8. Open source and verification

The app is open source and licensed under the GNU General Public License v3.0. Because
the complete source code and build configuration are public, every claim in this policy
can be independently verified:

**https://github.com/alienware377/RootlessViPER4Android**

RootlessViPER4Android is a fork of RootlessJamesDSP by Tim Schneeberger.

## 9. Changes to this policy

If this policy changes, the updated version will be published in the project
repository with a new "Last updated" date. Because the app collects no data, changes
are expected to be rare.

## 10. Contact

Questions about this policy or the app's privacy practices can be raised as an issue
in the project repository:

**https://github.com/alienware377/RootlessViPER4Android/issues**
