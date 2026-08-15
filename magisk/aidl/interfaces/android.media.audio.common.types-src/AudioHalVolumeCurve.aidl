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
 * AudioHalVolumeCurve defines a set of curve points mapping a value from [0,100]
 * to an attenuation in millibels, and associates them with a device category.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalVolumeCurve {
    @Backing(type="byte")
    @VintfStability
    enum DeviceCategory {
        HEADSET = 0,
        SPEAKER = 1,
        EARPIECE = 2,
        EXT_MEDIA = 3,
        HEARING_AID = 4
    }
    DeviceCategory deviceCategory = DeviceCategory.SPEAKER;
    @VintfStability
    parcelable CurvePoint {
        const byte MIN_INDEX = 0;
        const byte MAX_INDEX = 100;
        /**
         * Must be a value in the range [MIN_INDEX, MAX_INDEX].
         */
        byte index;
        /**
         * Attenuation in millibels
         */
        int attenuationMb;
    }
    /**
     * Each curve point maps a value in the range [CurvePoint.MIN_INDEX, CurvePoint.MAX_INDEX]
     * to an attenuation in millibels. This is used to create a volume curve using linear
     * interpolation.
     */
    CurvePoint[] curvePoints;
}
