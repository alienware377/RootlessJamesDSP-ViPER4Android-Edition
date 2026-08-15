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
 * Represents the capabilities of a sound separator component.
 *
 * This parcelable includes the maximum number of sound sources that can be separated
 * simultaneously.
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable SeparatorCapability {
    /**
     * Indicates whether sound separation is supported.
     *
     * Note: If sound separation is supported, the effect must also support sound remixing to
     * handle the separated audio streams, and produce remixed audio as output.
     */
    boolean supported;

    /**
     * The minimum number of sound sources a sound separator must support.
     */
    const int MIN_SOUND_SOURCE_SUPPORTED = 2;

    /**
     * Maximum number of sound sources that can be separated.
     *
     * Specifies the maximum number of individual sound sources that the separator can process
     * simultaneously.
     *
     * Each separated sound source have an soundSourceId, range in [0, maxSoundSources -1]. In
     * ERASER mode, each sound source will be classified with a classifier, identified by the
     * soundSourceId.
     *
     * The minimum value of `maxSoundSources` is 2 as defined by `MIN_SOUND_SOURCE_SUPPORTED`, the
     * default value is 4.
     */
    int maxSoundSources = 4;
}
