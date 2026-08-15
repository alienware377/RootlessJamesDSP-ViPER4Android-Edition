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

import android.media.audio.common.AudioDevice;

/**
 * Structure providing static information on a microphone. This information
 * never changes during the lifetime of the HAL IModule which owns the microphone.
 * The information presented in this structure indicates the location and
 * orientation of the microphone on the device as well as useful information
 * like frequency response and sensitivity.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable MicrophoneInfo {
    /**
     * Unique alphanumeric id for the microphone. It must remain the same across
     * device reboots. The client must never attempt to parse the value of this
     * field.
     */
    @utf8InCpp String id;
    /**
     * Describes the location of the microphone in terms of managed audio devices.
     */
    AudioDevice device;

    @VintfStability
    @Backing(type="int")
    enum Location {
        /** Microphone location is unknown. */
        UNKNOWN = 0,
        /** The microphone is located on the main body of the device. */
        MAINBODY = 1,
        /** The microphone is located on a movable main body of the device. */
        MAINBODY_MOVABLE = 2,
        /** The microphone is located on a peripheral. */
        PERIPHERAL = 3,
    }
    /** Location of the microphone in regard to the body of the device */
    Location location = Location.UNKNOWN;

    /**
     * This value is used when the group of the microphone is unknown.
     */
    const int GROUP_UNKNOWN = -1;
    /**
     * An identifier to group related microphones together, for example,
     * microphones of a microphone array should all belong to the same group.
     * Note that microphones assigned to 'GROUP_UNKNOWN' do not form a group.
     */
    int group = GROUP_UNKNOWN;
    /**
     * This value is used when the index in the group of the microphone is
     * unknown.
     */
    const int INDEX_IN_THE_GROUP_UNKNOWN = -1;
    /**
     * Index of this microphone within the group. The pair (group, index) must
     * be unique within the same HAL module, except the pair
     * (GROUP_UNKNOWN, INDEX_IN_THE_GROUP_UNKNOWN).
     */
    int indexInTheGroup = INDEX_IN_THE_GROUP_UNKNOWN;

    @VintfStability
    parcelable Sensitivity {
        /** Level in dBFS produced by a 1000 Hz tone at 94 dB SPL. */
        float leveldBFS;
        /** Level in dB of the max SPL supported at 1000 Hz */
        float maxSpldB;
        /** Level in dB of the min SPL supported at 1000 Hz */
        float minSpldB;
    }
    /**
     * If provided, must describe acceptable sound pressure levels (SPL)
     * for a 1 kHz sine wave, and the resulting level in dBFS.
     */
    @nullable Sensitivity sensitivity;

    @VintfStability
    @Backing(type="int")
    enum Directionality {
        UNKNOWN = 0,
        OMNI = 1,
        BI_DIRECTIONAL = 2,
        CARDIOID = 3,
        HYPER_CARDIOID = 4,
        SUPER_CARDIOID = 5,
    }
    /**
     * The standard polar pattern of the microphone.
     */
    Directionality directionality = Directionality.UNKNOWN;

    /**
     * A (frequency, level) pair. Used to represent frequency response.
     */
    @VintfStability
    parcelable FrequencyResponsePoint {
        float frequencyHz;
        float leveldB;
    }
    /**
     * Vector with ordered frequency responses (from low to high frequencies)
     * with the frequency response of the microphone. Levels are in dB,
     * relative to level at 1000 Hz.
     */
    FrequencyResponsePoint[] frequencyResponse;

    /**
     * A 3D point used to represent position or orientation of a microphone.
     */
    @VintfStability
    parcelable Coordinate {
        float x;
        float y;
        float z;
    }
    /**
     * If provided, must specify distances of the microphone's capsule, in
     * meters, from the bottom-left-back corner of the bounding box of device in
     * its natural orientation (PORTRAIT for phones, LANDSCAPE for tablets, TVs,
     * etc).
     */
    @nullable Coordinate position;
    /**
     * If provided, describes the normalized point which defines the main
     * orientation of the microphone's capsule.
     * Magnitude = sqrt(x^2 + y^2 + z^2) = 1.
     */
    @nullable Coordinate orientation;
}
