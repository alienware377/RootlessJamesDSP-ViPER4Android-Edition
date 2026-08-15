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

import android.media.audio.common.AudioChannelLayout;
import android.media.audio.eraser.ClassifierCapability;
import android.media.audio.eraser.Mode;
import android.media.audio.eraser.RemixerCapability;
import android.media.audio.eraser.SeparatorCapability;

/**
 * Represents the capability of an audio eraser.
 *
 * This parcelable defines the supported input/output data formats, available work modes, and the
 * specific capabilities of the sound classifier, separator, and remixer components within the
 * eraser effect.
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable Capability {
    /**
     * List of supported sample rates for the eraser.
     *
     * The output audio sample rate will be the same as the input.
     */
    int[] sampleRates;

    /**
     * List of supported channel layouts for the eraser.
     *
     * The output audio channel layout will be the same as the input.
     */
    AudioChannelLayout[] channelLayouts;

    /**
     * List of supported work modes.
     *
     * Defines the different operational modes (e.g., `ERASER`, `CLASSIFIER`) that the eraser can
     * work in.
     */
    Mode[] modes;

    /**
     * Separator capability.
     *
     * Specifies the capabilities of the sound separator component within the eraser effect,
     * including the maximum number of sound sources it can separate.
     */
    SeparatorCapability separator;

    /**
     * Classifier capability.
     *
     * Specifies the capabilities of the sound classifier component within the eraser effect,
     * including the sound classifications it can detect.
     */
    ClassifierCapability classifier;

    /**
     * Remixer capability.
     *
     * Specifies the capabilities of the sound remixer component within the eraser effect,
     * including the gainFactor range supported.
     */
    RemixerCapability remixer;
}
