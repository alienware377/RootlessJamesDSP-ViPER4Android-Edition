/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package android.media.audio.eraser;

/**
 * Represents the capabilities of a sound remixer component.
 *
 * This parcelable defines the supported range of gainFactors that the remixer can apply to the
 * audio signal.
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable RemixerCapability {
    /**
     * Indicates whether sound remixer is supported.
     *
     * If `supported` is true, the sound remixer can adjust the gain of the audio signal based on
     * the provided classifications and gainFactors, and remix the separated sounds into one.
     */
    boolean supported;

    /**
     * Minimum gainFactor supported by the sound remixer.
     *
     * Specifies the lowest gainFactor that the remixer can apply. A gainFactor of `0.0`
     * typically mutes the sound. In some less common cases, a remixer can support a negative
     * `gainFactor`, which enables some use cases like phase inversion and noise cancellation.
     *
     * The minimum gainFactor must be at least `0.0`. The default minimum gainFactor for a remixer
     * is `0.0` (the sound is muted).
     */
    float minGainFactor = 0f;

    /**
     * Maximum gainFactor supported by the sound remixer.
     *
     * Specifies the highest gainFactor that the remixer can apply. A gainFactor of `1.0` means no
     * adjustment to the sound's original volume. In the case of gainFactor greater than `1.0`, the
     * remixer may apply amplification to the audio signal.
     *
     * The maximum gainFactor must be at least `1.0`, and the default maximum gainFactor for a
     * remixer is `1.0` (no gain adjustment to the sound).
     */
    float maxGainFactor = 1f;
}
