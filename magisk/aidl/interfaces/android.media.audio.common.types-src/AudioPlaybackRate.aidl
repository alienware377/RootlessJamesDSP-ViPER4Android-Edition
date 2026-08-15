/*
 * Copyright (C) 2022 The Android Open Source Project
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
 * Parameters determining playback behavior. They are used to speed up or slow
 * down playback and / or change the tonal frequency of the audio content (pitch).
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioPlaybackRate {
    /**
     * Speed factor (multiplier). Normal speed has the value of 1.0f.
     * Values less than 1.0f slow down playback, value greater than 1.0f
     * speed it up.
     */
    float speed;
    /**
     * Pitch factor (multiplier). Setting pitch value to 1.0f together
     * with changing playback speed preserves the pitch, this is often
     * called "timestretching." Setting the pitch value equal to speed produces
     * the same effect as playing audio content at different sampling rate.
     */
    float pitch;

    /**
     * Algorithms used for timestretching (preserving pitch while playing audio
     * content at different speed).
     */
    @VintfStability
    @Backing(type="int")
    enum TimestretchMode {
        // Needs to be in sync with AUDIO_STRETCH_MODE_* constants in
        // frameworks/base/media/java/android/media/PlaybackParams.java
        DEFAULT = 0,
        /** Selects timestretch algorithm best suitable for voice (speech) content. */
        VOICE = 1,
    }
    /**
     * Selects the algorithm used for timestretching (preserving pitch while
     * playing audio at different speed).
     */
    TimestretchMode timestretchMode = TimestretchMode.DEFAULT;

    /**
     * Behavior when the values for speed and / or pitch are out of the
     * applicable range.
     */
    @VintfStability
    @Backing(type="int")
    enum TimestretchFallbackMode {
        // Needs to be in sync with AUDIO_FALLBACK_MODE_* constants in
        // frameworks/base/media/java/android/media/PlaybackParams.java,
        /** Reserved for use by the framework. */
        SYS_RESERVED_CUT_REPEAT = -1,
        /** Reserved for use by the framework. */
        SYS_RESERVED_DEFAULT = 0,
        /**
         * If possible, play silence for parameter values that are out of range,
         * otherwise return an error (same as 'FAIL' would return).
         */
        MUTE = 1,
        /**
         * Always return an error while trying to set the parameters that are
         * out of range.
         */
        FAIL = 2,
    }
    /**
     * Selects the behavior when the specified values for speed and / or pitch
     * are out of applicable range.
     */
    TimestretchFallbackMode fallbackMode = TimestretchFallbackMode.SYS_RESERVED_DEFAULT;
}
