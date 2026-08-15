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
 * AudioHalCapCriterion is a wrapper for a CriterionType and its default value.
 * This is to be used exclusively for the Configurable Audio Policy (CAP) engine
 * configuration.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalCapCriterion {
    @utf8InCpp String name;
    /**
     * Used to map the AudioHalCapCriterion to an AudioHalCapCriterionType with
     * the same name.
     */
    @utf8InCpp String criterionTypeName;
    /**
     * This is the default value of a criterion represented as a string literal.
     * An empty string must be used for inclusive criteria (i.e. criteria that
     * are used to represent a bitfield). The client must never attempt to parse
     * the value of this field.
     */
    @utf8InCpp String defaultLiteralValue;
}
