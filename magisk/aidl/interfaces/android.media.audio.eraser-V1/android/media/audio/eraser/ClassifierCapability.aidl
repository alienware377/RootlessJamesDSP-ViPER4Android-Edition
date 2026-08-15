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
 * Represents the capabilities of a sound classifier component.
 *
 * This parcelable contains a list of supported sound classifications that the classifier can
 * recognize and process.
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable ClassifierCapability {
    /**
     * The window size of the classifier model in milliseconds.
     *
     * Indicates the duration over which the classifier processes audio data to output a
     * classification result.
     *
     * Clients can expect to receive updates at most once per window.
     */
    int windowSizeMs;

    /**
     * List of supported sound classifications.
     *
     * Each entry specifies a sound classification category that the classifier can recognize, such
     * as `HUMAN`, `MUSIC`, `ANIMAL`, etc. This defines the types of sounds the classifier is
     * capable of identifying in the input audio.
     */
    Classification[] supportedClassifications;
}
