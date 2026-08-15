/*
 * Copyright 2023 The Android Open Source Project
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

package android.media.audio.common;

/**
 * Audio playback spatialization settings.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable Spatialization {
    /**
     * The spatialization level supported by the spatializer stage effect implementation.
     * Used by methods of the ISpatializer interface.
     * {@hide}
     */
    @Backing(type="byte")
    enum Level {
        /** Spatialization is disabled. */
        NONE = 0,
        /** The spatializer accepts audio with positional multichannel masks (e.g 5.1). */
        MULTICHANNEL = 1,
        /**
         * The spatializer accepts audio made of a channel bed of positional multichannels
         * (e.g 5.1) and audio objects positioned independently via meta data.
         */
        BED_PLUS_OBJECTS = 2,
    }

    /**
     * The spatialization mode supported by the spatializer stage effect implementation.
     * Used by methods of the ISpatializer interface.
     * {@hide}
     */
    @Backing(type="byte")
    enum Mode {
        /** The spatializer supports binaural mode (over headphones type devices). */
        BINAURAL = 0,
        /** The spatializer supports transaural mode (over speaker type devices). */
        TRANSAURAL = 1,
    }
}
