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

package android.media.audio.common;

import android.media.audio.common.AudioDeviceAddress;
import android.media.audio.common.AudioDeviceDescription;
import android.media.audio.common.AudioProductStrategyType;
import android.media.audio.common.AudioSource;
import android.media.audio.common.AudioStreamType;

/**
 * Defines the audio CAP Engine Parameters expected to be controlled by the configurable engine.
 * These parameters deal with:
 *    Output/Input device selection for a given strategy based on:
 *        -the type (each device will be a bit in a bitfield, allowing to select multiple devices);
 *        -the address.
 *    Volume Profile: for volume curve selection (e.g. dtmf follows call curves during call).
 *
 * {@hide}
 */
@VintfStability
union AudioHalCapParameter {
    @VintfStability
    parcelable StrategyDevice {
        AudioDeviceDescription device;
        // AudioHalProductStrategy.id
        int id = AudioProductStrategyType.SYS_RESERVED_NONE;
        /**
         * Specifies whether the device is selected or not selected in the configuration.
         */
        boolean isSelected;
    }
    @VintfStability
    parcelable InputSourceDevice {
        AudioDeviceDescription device;
        AudioSource inputSource = AudioSource.DEFAULT;
        /**
         * Specifies whether the device is selected or not selected in the configuration.
         */
        boolean isSelected;
    }
    @VintfStability
    parcelable StrategyDeviceAddress {
        AudioDeviceAddress deviceAddress;
        // AudioHalProductStrategy.id
        int id = AudioProductStrategyType.SYS_RESERVED_NONE;
    }
    @VintfStability
    parcelable StreamVolumeProfile {
        AudioStreamType stream = AudioStreamType.INVALID;
        AudioStreamType profile = AudioStreamType.INVALID;
    }

    /**
     * Parameter allowing to choose a device by its type and associate
     * the choice with a product strategy.
     */
    StrategyDevice selectedStrategyDevice;
    /**
     * Parameter allowing to choose an input device by and source type.
     */
    InputSourceDevice selectedInputSourceDevice;
    /**
     * Parameter allowing to choose a particular device by its address and
     * associate the choice with a product strategy.
     */
    StrategyDeviceAddress strategyDeviceAddress;
    /**
     * Parameter dealing with volume curve selection.
     */
    StreamVolumeProfile streamVolumeProfile;
}
