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

import android.media.audio.eraser.ClassificationMetadataList;

/**
 * Callback interface for delivering results from the eraser effect.
 */
@VintfStability
interface IEraserCallback {
    /**
     * Provides classifier updates, including sound classifications and their confidence scores,
     * along with the associated timestamp for a given `soundSourceId`.
     *
     * The callback is invoked when there is a change in the list of active classification metadata
     * for each sound source. Changes include the addition and removal of a classification, or
     * a change in the condidence score.
     *
     * The number of metadata elements in the `ClassificationMetadataList.metadatas` list will not
     * exceed the `maxClassificationMetadata` set in `android.media.audio.eraser.Configuration`.
     *
     * Different classifiers may have varying window sizes, regardless of the window size, the
     * classifier updates occur at most once per window per sound source.
     *
     * @param soundSourceId The identifier for the sound source being classified. In ERASER mode,
     *                      this identifies the separated sound source.
     *        - In CLASSIFIER mode, the `soundSourceId` is always `0` as there is only one sound
     *          source for the eraser effect.
     *        - In ERASER mode, the `soundSourceId` range is [0, `maxSoundSources - 1`], where
     *          `maxSoundSources` is defined in the eraser capability through `SeparatorCapability`.
     *
     * @param metadata The classification metadata list for the current sound source.
     */
    oneway void onClassifierUpdate(in int soundSourceId, in ClassificationMetadataList metadata);
}
