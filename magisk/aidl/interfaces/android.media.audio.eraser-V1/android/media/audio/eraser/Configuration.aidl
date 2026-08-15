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

import android.media.audio.eraser.ClassificationConfig;
import android.media.audio.eraser.IEraserCallback;
import android.media.audio.eraser.Mode;

/**
 * Eraser configurations. Configuration for eraser operation mode, sound classification behaviors,
 * and an optional callback interface.
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable Configuration {
    /**
     * Work mode for the eraser, specifies the current operating mode of the eraser effect.
     */
    Mode mode = Mode.ERASER;

    /**
     * List of eraser configurations.
     * Each configuration defines the behavior for specific sound classifications, allowing
     * different gain factors and confidence thresholds to be applied based on classification
     * results.
     */
    ClassificationConfig[] classificationConfigs;

    /**
     * Maximum number of classification metadata generated from the sound classification.
     *
     * Default value set to 5.
     */
    int maxClassificationMetadata = 5;

    /**
     * Optional callback inerface to get the eraser effect results.
     */
    @nullable IEraserCallback callback;
}
