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

import android.media.audio.common.AudioDeviceType;

/**
 * AudioHalCapCriterionType contains a criterion's possible numerical values
 * and their corresponding string literal representations. This is to be used
 * exclusively for the Configurable Audio Policy (CAP) engine configuration.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalCapCriterionType {
    /**
     * Name is used to associate an AudioHalCapCriterionType with an
     * AudioHalCapCriterion.
     */
    @utf8InCpp String name;
    /**
     * If true, indicates that the criterion type supports more than one value
     * at a time (e.g. a criterion type that represents a mask/bitfield).
     * Otherwise, the criterion type can only have one value at a time.
     */
    boolean isInclusive;
    /**
     * List of all possible criterion values represented as human-readable
     * string literals. These strings must only contain alphanumeric characters,
     * and the client must never attempt to parse them.
     */
    @utf8InCpp String[] values;
}
