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
 * Defines the operational mode of the Eraser effect.
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
@Backing(type="byte")
enum Mode {
    /**
     * ERASER mode: The effect operates as an automatic sound eraser.
     *
     * In this mode, the Eraser effect processes the input audio using the Sound Separator,
     * Sound Classifier, and Remixer components. The sound to be erased or retained is determined
     * by the classifications and gain adjustments specified in eraser configuration.
     *
     * - The Sound Separator separates the input audio into multiple sound sources.
     * - The Sound Classifier analyzes each separated sound to determine its sound category.
     * - The Remixer applies gain adjustments based on the classifications and configurations, and
     *   re-mix the processed sounds back into a single output audio stream.
     *
     * Requirements: To operate in this mode, the effect must support the classifier, separator,
     * and remixer capabilities.
     *
     * Use Cases: Selectively suppressing or enhancing specific sounds in the audio stream,
     * such as removing background noise or isolating desired sound sources.
     */
    ERASER,

    /**
     * CLASSIFIER mode: The effect operates as a sound classifier.
     *
     * In this mode, the Sound Classifier analyzes the input audio in real-time and emits
     * classification results based on the sound categories detected. The input audio is directly
     * passed through to the output without any modification.
     *
     * Use Cases: Useful for applications that need to detect specific sound events, monitor audio
     * content, or provide real-time visual feedback on audio classifications, without altering the
     * original audio stream.
     */
    CLASSIFIER,
}
