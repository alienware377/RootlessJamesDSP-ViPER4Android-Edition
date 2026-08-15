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

import android.media.audio.eraser.Classification;

/**
 * Configuration for the eraser to apply specific gain adjustments to certain sound classifications.
 *
 * Gain is applied to the audio signal by scaling the amplitude of the output audio based on the
 * classification of the input sound.
 * If a classification exists in the configuration list, the remixer applies the specified gain to
 * the output audio when the confidence score is higher than `confidenceThreshold`. If a
 * classification is not present in the configuration, it is considered to have a gain of 1.0
 * (no gain adjustment).
 * If a ClassificationConfig contains an empty classification list, the same threshold and gain
 * specified in the ClassificationConfig will be applied to all classifications not explicitly
 * configured.
 *
 * Examples:
 *
 * 1. {classifications = [{classification = SoundClassification.NATURE},
 *                        {classification = SoundClassification.ENVIRONMENT}],
 *     confidenceThreshold = 0.8,
 *     gainFactor = 0.0}
 *
 *    - If the input audio is classified as NATURE or ENVIRONMENT, with a confidence score higher
 *      than 0.8, the output audio will be muted.
 *    - If the classification confidence score is 0.8 or lower, or if the audio is classified
 *      differently, the output audio remains unchanged.
 *
 * 2.  {classifications = [{classification = SoundClassification.MUSIC}],
 *      confidenceThreshold = 0.6,
 *      gainFactor = 0.5}
 *
 *    - If the input audio is classified as MUSIC with a confidence score higher than 0.6, the
 *      output audio should have a gain factor of 0.5 (reduced by half).
 *    - If the classification confidence score is 0.6 or lower, or if the audio is classified
 *      differently, the output audio remains unchanged.
 *
 * 3. When combined as a list, the eraser can be configured to apply different gainFactor to
 *    a classifications when confideence score is higher than the corresponding threshold.
 *    [{classifications = [{classification = SoundClassification.NATURE}],
 *      confidenceThreshold = 0.8,
 *      gainFactor = 0.0},
 *     {classifications = [{classification = SoundClassification.MUSIC}],
 *      confidenceThreshold = 0.8,
 *      gainFactor = 0.6},
 *     {classifications = [{classification = SoundClassification.MUSIC}],
 *      confidenceThreshold = 0.5,
 *      gainFactor = 0.5}]
 *
 *    - If the input audio is classified as NATURE, and the confidence score is higher than 0.8,
 *      the output audio classification will be muted (gainFactor = 0.0).
 *
 *    - If the input audio is classified as MUSIC with a confidence score higher than 0.8, the
 *      output audio classification will have a gain factor of 0.6. If the input audio is
 *      classified as MUSIC with a confidence score higher than 0.5, the output audio
 *      classification will have a gain factor of 0.5.
 *
 *    - For all other sound classifications, the audio signal remains unchanged (gainFactor = 1.0).
 *
 * 4. [{classifications = [{classification = SoundClassification.HUMAN}],
 *      confidenceThreshold = 0.8,
 *      gainFactor = 1.0},
 *     {classifications = [],
 *      confidenceThreshold = 0.0,
 *      gainFactor = 0.5}]
 *
 *    - If the input audio is classified as HUMAN, and the confidence score is higher than 0.8, the
 *      output audio classification will remains unchanged.
 *
 *    - For all other sound classifications, the audio signal will have a gain factor of 0.5.
 *
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable ClassificationConfig {
    /**
     * List of sound classifications to which this configuration applies.
     *
     * Each entry specifies a sound classification (e.g., MUSIC, NATURE) targeted by this
     * configuration.
     */
    Classification[] classifications;

    /**
     * Confidence threshold in the range of [0.0, 1.0], only apply the gainFactor when the
     * classifier's confidence score for the specified classifications exceeds this threshold.
     *
     * Default Value is 0.0 which means apply gain regardless of confidence score.
     */
    float confidenceThreshold = 0f;

    /**
     * Gain factor to apply to the output audio when the specified classifications are detected.
     * Gain factor is applied by multiplying the amplitude of the audio signal by the `gainFactor`.
     *
     * - A `gainFactor` of `1.0` means no gain adjustment (the original volume is preserved).
     * - A `gainFactor` of `0.5` reduces the amplitude of the audio by half.
     * - A `gainFactor` of `0.0` mutes the audio.
     * - A `gainFactor` > `1.0` amplifies the audio signal, increasing its volume (useful for
     *   compressor and amplification cases).
     * - A `gainFactor` < `0.0` inverts the phase of the audio signal (useful for phase
     *   cancellation or specific spatial audio manipulation).
     *
     * The `gainFactor` must be within the `gainFactorRange` defined in `RemixerCapability`, the
     * default value is `1.0`.
     */
    float gainFactor = 1f;
}
