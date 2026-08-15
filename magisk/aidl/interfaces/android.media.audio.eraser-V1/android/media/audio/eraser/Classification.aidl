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

package android.media.audio.eraser;

import android.media.audio.eraser.SoundClassification;

/**
 * Represents a sound classification category.
 *
 * The classification includes the top-level sound category based on the AudioSet ontology.
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable Classification {
    /**
     * The top-level sounds classification supported.
     *
     * This field specifies the primary sound category that this classification represents,
     * as defined in the AudioSet ontology. It helps identify the general type of sound,
     * such as HUMAN, ANIMAL, MUSIC, etc.
     */
    SoundClassification classification = SoundClassification.HUMAN;
}
