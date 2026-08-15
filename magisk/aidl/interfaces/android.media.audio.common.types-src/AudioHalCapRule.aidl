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

import android.media.audio.common.AudioHalCapCriterionV2;

/**
 * AudioHalCapRule defines the Configurable Audio Policy (CAP) rules for a given configuration.
 *
 * Rule may be compounded using a logical operator "ALL" (aka "AND") and "ANY' (aka "OR").
 * Compounded rule can be nested.
 * Rules on criterion is made of:
 *      -type of criterion:
 *              -inclusive -> match rules are "Includes" or "Excludes"
 *              -exclusive -> match rules are "Is" or "IsNot" aka equal or different
 *      -Name of the criterion must match the provided name in AudioHalCapCriterionV2
 *      -Value of the criterion must match the provided list of literal values from
 *         associated AudioHalCapCriterionV2 values
 * Example of rule:
 *      ALL
 *          ANY
 *              ALL
 *                  CriterionRule1
 *                  CriterionRule2
 *              CriterionRule3
 *          CriterionRule4
 *
 * It will correspond to
 *      ALL
 *          nestedRule1
 *          CriterionRule4
 *
 * where nestedRule1 is
 *            ANY
 *              nestedRule2
 *              CriterionRule3
 * and nestedRule2 is
 *              ALL
 *                  CriterionRule1
 *                  CriterionRule2
 *
 * Translated into:
 *     { .compoundRule = CompoundRule::ALL, .nestedRule[0] =
 *         { .compoundRule = CompoundRule::ANY, .nestedRule[0] =
 *             { .compoundRule = CompoundRule::ALL, .criterionRules =
 *                  { CriterionRule1, CriterionRule2 }},
 *           .criterionRules = { CriterionRule3 },
 *         },
 *       .criterionRules = { CriterionRule4 }}
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioHalCapRule {
    @VintfStability
    enum CompoundRule {
        INVALID = 0,
        /*
         * OR'ed rules
         */
        ANY,
        /*
         * AND'ed rules
         */
        ALL,
    }

    @VintfStability
    enum MatchingRule {
        INVALID = -1,
        /*
         * Matching rule on exclusive criterion type.
         */
        IS = 0,
        /*
         * Exclusion rule on exclusive criterion type.
         */
        IS_NOT,
        /*
         * Matching rule on inclusive criterion type (aka bitfield type).
         */
        INCLUDES,
        /*
         * Exclusion rule on inclusive criterion type (aka bitfield type).
         */
        EXCLUDES,
    }

    @VintfStability
    parcelable CriterionRule {
        MatchingRule matchingRule = MatchingRule.INVALID;
        /*
         * Must be one of the names defined by {@see AudioHalCapCriterionV2}.
         * By convention, when AudioHalCapCriterionV2 is used as a rule, the rule uses
         * the first element of the 'values' field which must be a non-empty array.
         */
        AudioHalCapCriterionV2 criterionAndValue;
    }
    /*
     * Defines the AND or OR'ed logcal rule between provided criterion rules if any and provided
     * nested rule if any.
     * Even if no rules or nestedRule are provided, a compound is expected with ALL rule to set the
     * rule of the {@see AudioHalConfiguration} as "always applicable".
     */
    CompoundRule compoundRule = CompoundRule.INVALID;
    /*
     * An AudioHalCapRule may contain 0..n CriterionRules.
     */
    CriterionRule[] criterionRules;
    /*
     * An AudioHalCapRule may be nested with 0..n AudioHalCapRules.
     */
    AudioHalCapRule[] nestedRules;
}
