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

import android.media.audio.common.AudioHalVolumeCurve;

/**
 * AudioHalVolumeGroup is a software volume control concept that associates
 * volume on the UI (where volume index is incremented or decremented by steps
 * of 1) to a set of volume curves.
 *
 * {@hide}
 */
@SuppressWarnings(value={"redundant-name"}) // for *AUDIO_SERVICE
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalVolumeGroup {
    /**
     * Used to indicate that the system shall not use the HAL's definition of a
     * volume group's minIndex or maxIndex, and will instead defer to audio
     * service for these values
     */
    const int INDEX_DEFERRED_TO_AUDIO_SERVICE = -1;
    /**
     * Name is used to associate an AudioHalVolumeGroup with an AudioStreamType
     * and some AudioAttributes by means of an AudioHalAttributesGroup
     */
    @utf8InCpp String name;
    /**
     * Defines the min volume index on the UI scale, which must be a
     * non-negative integer <= maxIndex. Setting this to
     * INDEX_DEFERRED_TO_AUDIO_SERVICE is only valid if the AudioHalVolumeGroup
     * is associated to a valid AudioStreamType.
     */
    int minIndex;
    /**
     * Defines the max volume index on the UI scale, which must be
     * a non-negative integer >= minIndex. Setting this to
     * INDEX_DEFERRED_TO_AUDIO_SERVICE is only valid if the AudioHalVolumeGroup
     * is associated to a valid AudioStreamType.
     */
    int maxIndex;
    /**
     * Defines the set of volume curves for this group, each of which
     * corresponds to a different device category. That is, each
     * AudioHalVolumeCurve in this list contains a field for a device category,
     * and no two curves in this list can have the same deviceCategory.
     */
    AudioHalVolumeCurve[] volumeCurves;
}
