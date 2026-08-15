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

/**
 * The sound classification is based on the top-level categories of the "AudioSet ontology".
 *
 * The AudioSet ontology is a hierarchical collection of sound event classes.
 * For more details, refer to the ICASSP 2017 paper: "AudioSet: An ontology and human-labeled
 * dataset for audio events".
 * https://research.google/pubs/audio-set-an-ontology-and-human-labeled-dataset-for-audio-events/
 */
@JavaDerive(equals=true, toString=true)
@Backing(type="int")
@VintfStability
enum SoundClassification {
    /**
     * Sounds produced by the human body through the actions of the individual.
     */
    HUMAN,

    /**
     * All sound produced by the bodies and actions of nonhuman animals.
     */
    ANIMAL,

    /**
     * Sounds produced by natural sources in their normal soundscape, excluding animal and human
     * sounds.
     */
    NATURE,

    /**
     * Music is an art form and cultural activity whose medium is sound and silence. The common
     * elements of music are pitch, rhythm, dynamics, and the sonic qualities of timbre and texture.
     */
    MUSIC,

    /**
     * Set of sound classes referring to sounds that are immediately understood by listeners as
     * arising from specific objects (rather than being heard more literally as "sounds").
     */
    THINGS,

    /**
     * Portmanteau class for sounds that do not immediately suggest specific source objects, but
     * which are more likely to be perceived and described according to their acoustic properties.
     */
    AMBIGUOUS,

    /**
     * A class for sound categories that suggest information about attributes other than the
     * foreground or target objects.
     */
    ENVIRONMENT,

    /**
     * Vendor customizable extension, for possible classifications not listed above.
     */
    VENDOR_EXTENSION,
}
