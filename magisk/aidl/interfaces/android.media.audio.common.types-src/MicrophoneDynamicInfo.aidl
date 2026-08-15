/*
 * Copyright (C) 2023 The Android Open Source Project
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
 * Structure providing dynamic information on a microphone. This information
 * changes between recording sessions.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable MicrophoneDynamicInfo {
    /**
     * Unique alphanumeric id for the microphone. It must match the id of one of
     * the 'MicrophoneInfo' entries returned by audio HAL method
     * 'IModule.getMicrophones'.
     */
    @utf8InCpp String id;

    @VintfStability
    @Backing(type="int")
    enum ChannelMapping {
        /** Channel not used. */
        UNUSED = 0,
        /** Channel is used and the signal is not processed. */
        DIRECT = 1,
        /** Channel is used and the signal has some processing. */
        PROCESSED = 2,
    }
    /**
     * The vector is indexes by zero-based channels of the microphone, thus the
     * element '0' corresponds to the first channel, '1' is the second, etc. The
     * vector must contain at least 1 element.
     */
    ChannelMapping[] channelMapping;
}
