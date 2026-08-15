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

import android.media.audio.common.AudioHalCapCriterion;
import android.media.audio.common.AudioHalCapCriterionV2;
import android.media.audio.common.AudioHalCapCriterionType;
import android.media.audio.common.AudioHalCapDomain;
import android.media.audio.common.AudioHalProductStrategy;
import android.media.audio.common.AudioHalVolumeGroup;
import android.media.audio.common.AudioProductStrategyType;

/**
 * AudioHalEngineConfig defines the configuration items that are used upon
 * initial engine loading. This includes the choice of engine flavor: default
 * audio policy engine or Configurable Audio Policy (CAP) engine. The flavor
 * is determined only once, and it is done by checking presence of
 * capSpecificConfig during client initialization.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalEngineConfig {
    /**
     * Used to identify the default product strategy, which will be used for any
     * attributes not already associated to any other product strategy. Vendors
     * configuring their own product strategies must identify one of their
     * strategies as the default.
     */
    int defaultProductStrategyId = AudioProductStrategyType.SYS_RESERVED_NONE;
    /**
     * A non-empty list of product strategies is mandatory for the configurable
     * audio policy (CAP) engine. If no product strategies are provided
     * when using the default audio policy engine, however, the engine shall use
     * the default product strategies.
     */
    AudioHalProductStrategy[] productStrategies;
    /**
     * A non-empty list of volume groups is mandatory for both flavors of the
     * audio policy engine.
     */
    AudioHalVolumeGroup[] volumeGroups;
    @VintfStability
    parcelable CapSpecificConfig {
        AudioHalCapCriterion[] criteria;
        AudioHalCapCriterionType[] criterionTypes;

        @nullable AudioHalCapCriterionV2[] criteriaV2;
        /**
         * AudioHalCapDomains defines the Configurable Audio Policy (CAP) engine configurable
         * domains that are used by the parameter-framework to define a dynamic management of
         * policy engine parameters (aka defining the values of the policy parameters according to
         * the values of provided criteria).
         */
        @nullable AudioHalCapDomain[] domains;
    }
    /**
     * Specifies the configuration items that are specific to the Configurable
     * Audio Policy (CAP) engine. Clients normally use the default audio policy
     * engine. The client will use the CAP engine only when this field has a
     * non-null value.
     */
    @nullable CapSpecificConfig capSpecificConfig;
}
