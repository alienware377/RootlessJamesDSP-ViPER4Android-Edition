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

import android.media.audio.eraser.ClassificationMetadata;

/**
 * List of active `ClassificationMetadata` aligned to a specific timestamp.
 *
 * A `ClassificationMetadata` is considered active when the `confidenceScore` exceeds the
 * `ClassificationConfig.confidenceThreshold`.
 *
 * The classifier component in the eraser must maintain the active metadata list when an
 * `IEraserCallback` is configured and send the list via `onClassifierUpdate` whenever a change
 * occurs.
 */

@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable ClassificationMetadataList {
    /**
     * Timestamp in milliseconds within the audio stream that this classification result is aligned
     * with, the timestamp is calculated with audio frames the eraser effect received, starting
     * from the first frame processed by the eraser effect.
     *
     * The `timeMs` indicates the starting point in the audio stream that the classification results
     * in this metadata list apply to.
     * Each classifier process window produces a list of `ClassificationMetadata`. The `timeMs` in
     * the metadata list always aligns with the start of the window (the starting point of the audio
     * segment processed by the classifier).
     * In rare cases where the classifier produces an identical list of classifications for
     * consecutive windows (including confidence scores), the `onClassifierUpdate` callback will
     * only be triggered once for the first process window, with a `timeMs` indicating the start of
     * that window. No further `onClassifierUpdate` callbacks will be made for the subsequent
     * windows, as there is no meaningful change in the classification results. This avoids
     * redundant updates when the classification remains the same across windows.
     *
     * Client Usage:
     * The `timeMs` allows clients to map the classification results back to a specific portion of
     * the audio stream. Clients can use this information to synchronize classification results
     * with the audio data or other events. Each metadata list update corresponds to one window of
     * classified audio, and the `timeMs` will always point to the start of that window.
     *
     * For an example, below is an audio stream timeline with a 1 second classifier window.
     * Audio stream:
     * |==========>=========|============>=========|===========>==========|===========>=========|
     * 0                   1000                  2000                   3000                   4000
     *                       |                     |                      |                     |
     *                       V                     V                      V                     V
     *                [{HUMAN, 0.8}]        [{HUMAN, 0.8},        [{HUMAN, 0.8},      [{HUMAN, 0.8}]
     *                       |               {NATURE, 0.4}]        {NATURE, 0.4}]               |
     *                       |                     |                                            |
     *                       V                     V                                            V
     *             onClassifierUpdate      onClassifierUpdate                     onClassifierUpdate
     *                  timeMs: 0             timeMs: 1000                           timeMs: 3000
     *                [{HUMAN, 0.8}]        [{HUMAN, 0.8},                          [{HUMAN, 0.8}]
     *                                       {NATURE, 0.4}]
     */
    int timeMs;

    /**
     * List of classification metadata, including the sound classification, confidence score, and
     * a duration since when the sound class was considered active.
     *
     * Metadatas in the list should be ranked in descending order based on the confidence score.
     */
    ClassificationMetadata[] metadatas;
}
