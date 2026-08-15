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

import android.media.audio.common.AudioHalAttributesGroup;
import android.media.audio.common.AudioProductStrategyType;

/**
 * AudioHalProductStrategy is a grouping of AudioHalAttributesGroups that will
 * share the same audio routing and volume policy.
 *
 * {@hide}
 */
@SuppressWarnings(value={"redundant-name"}) // for VENDOR_STRATEGY*
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalProductStrategy {
    @VintfStability
    @Backing(type="int")
    enum ZoneId {
        /**
         * Value indicating that there is no explicit zone associated to the product strategy
         * It is the case for non-automotive products or for default zone for automotive.
         */
        DEFAULT = 0,
    }
    /**
     * Defines the start of the vendor-defined product strategies
     */
    const int VENDOR_STRATEGY_ID_START = 1000;
    /**
     * Identifies the product strategy with a predefined constant. Vendors
     * using the default audio policy engine must use AudioProductStrategyType.
     * Vendors using the Configurable Audio Policy (CAP) engine must number
     * their strategies starting at VENDOR_STRATEGY_ID_START.
     */
    int id = AudioProductStrategyType.SYS_RESERVED_NONE;
    /**
     * This is the list of use cases that follow the same routing strategy.
     */
    AudioHalAttributesGroup[] attributesGroups;
    /**
     * Name of the strategy. Nullable for backward compatibility. If null, id
     * are assigned by the audio policy engine, ensuring uniqueness.
     *
     * With complete engine configuration AIDL migration, strategy ids are
     * preallocated (from `VENDOR_STRATEGY_ID_START` to
     * `VENDOR_STRATEGY_ID_START+39`). A human readable name must be
     * defined uniquely (to make dump/debug easier) for all strategies and
     * must not be null for any.
     */
    @nullable @utf8InCpp String name;
     /**
      * Audio zone id can be used to independently manage audio routing and volume for different
      * audio device configurations.
      * In automotive for example, audio zone id can be used to route different user id to different
      * audio zones. Thus providing independent audio routing and volume management for each user
      * in the car.
      * Note:
      * 1/ Audio zone id must begin at DEFAULT and increment respectively from DEFAULT
      * (i.e. DEFAULT + 1...).
      * 2/ Audio zone id can be held by one or more product strategy(ies).
      */
    int zoneId = ZoneId.DEFAULT;
}
