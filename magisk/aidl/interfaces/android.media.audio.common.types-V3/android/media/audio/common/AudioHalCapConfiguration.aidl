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

import android.media.audio.common.AudioHalCapParameter;
import android.media.audio.common.AudioHalCapRule;

/**
 * AudioHalCapConfiguration defines the Configurable Audio Policy (CAP) groups of runtime
 * configurations that may affect a group of parameter within a {@see AudioHalCapDomain}.
 * The configuration is defined by its name and associated rule based on provided criteria.
 * The configuration is referred in {@see AudioHalCapSetting} with its name, that must hence be
 * unique within the belonging domain.
 *
 *                            ┌──────────────────────────┐
 *                            │   AudioHalCapParameter   │
 *                            │        (union)           ◄────────────────────────────────────────┐
 *                            │                          │                                        │
 *                            │selectedStrateguDevice    │                                        │
 *                            │strateguDeviceAddress     │                                        │
 *                            │selectedInputSourceDevice │     ┌───────────────────────────────┐  │
 *                            │streamVolumeProfile       │     │ AudioHalCapConfiguration      │  │
 *                            └──────────────────────────┘ ┌───►                               │  │
 *                                                         │   │ name                          │  │
 *                            ┌──────────────────────────┐ │  ┌┼─rule                          │  │
 *                            │    AudioHalCapDomain     │ │  ││ parameterSettings[]───────────┼──┘
 *                            │                          │ │  │└───────────────────────────────┘
 *                         ┌──►name                      │ │  │
 *                         │  │configurations[]──────────┼─┘  │┌───────────────────────────────┐
 *                         │  └──────────────────────────┘    └► AudioHalCapRule               │
 *                         │                                   │                               │
 * ┌──────────────────────┐│                               ┌───┤ criterionRules[]──────────────┼──┐
 * │ AudioHalEngineConfig ││                               └───┼─nestedRules[]                 │  │
 * │                      ││                                   └───────────────────────────────┘  │
 * │ domains[]────────────┼┘ ┌────────────────────────────────┐                                   │
 * │ criteriaV2[]─────────┼┐ │   AudioHalCapCriterionV2       │┌───────────────────────────────┐  │
 * └──────────────────────┘│ │           (union)              ││ CriterionRule                 │  │
 *                         │ │ type                           ││                               ◄──┘
 *                         │ │                                ││ matchingRule                  │
 *                         │ │ availableInputDevices          ││ audioHalCapCriterionV2        │
 *                         │ │ availableOutputDevices         ││ audioHalCapCriterionV2.type   │
 *                         └─► availableInputDevicesAddresses │└───────────────────────────────┘
 *                           │ availableOutputDevicesAddresses│
 *                           │ telephonyMode                  │
 *                           │ forcConfigForUse               │
 *                           └────────────────────────────────┘
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalCapConfiguration {
    /**
     * Unique name of the configuration within a {@see AudioHalCapDomain}.
     */
    @utf8InCpp String name;
    /**
     * Rule to be verified to apply this configuration.
     */
    AudioHalCapRule rule;
    /**
     * A non-empty list of parameter settings, aka a couple of parameter and values.
     */
    AudioHalCapParameter[] parameterSettings;
}
