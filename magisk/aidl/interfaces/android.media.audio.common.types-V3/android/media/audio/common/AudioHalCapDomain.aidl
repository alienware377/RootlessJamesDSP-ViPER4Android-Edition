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

import android.media.audio.common.AudioHalCapConfiguration;

/**
 * AudioHalCapDomain defines the Configurable Audio Policy (CAP) groups of parameters that belong
 * to a given domain and manage the runtime behavior (aka values of the parameter).
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalCapDomain {
    /**
     * Name of the configurable domain. It must be unique within the CAP configuration.
     */
    @utf8InCpp String name;
    /**
     * A non-empty list of configurations aka different runtime conditions that may affect
     * the value of the parameters controlled by this domain and the values of the parameters.
     * All the settings within a domain shall define values for  the same parameters belonging
     * to this domain.
     */
    AudioHalCapConfiguration[] configurations;
}
