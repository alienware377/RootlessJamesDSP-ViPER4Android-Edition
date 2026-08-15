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

import android.media.audio.common.AudioAttributes;
import android.media.audio.common.AudioStreamType;

/**
 * AudioHalAttributesGroup associates an AudioHalVolumeGroup with an
 * AudioStreamType and a collection of AudioAttributes. This is used in the
 * context of AudioHalProductStrategy.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalAttributesGroup {
    /**
     * StreamType may only be set to AudioStreamType.INVALID when using the
     * Configurable Audio Policy (CAP) engine. An AudioHalAttributesGroup with
     * AudioStreamType.INVALID is used when the volume group and attributes are
     * not associated to any AudioStreamType.
     */
    AudioStreamType streamType = AudioStreamType.INVALID;
    /**
     * Used to map the AudioHalAttributesGroup to an AudioHalVolumeGroup with
     * the same name
     */
    @utf8InCpp String volumeGroupName;
    AudioAttributes[] attributes;
}
