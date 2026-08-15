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
 * Defines the product strategies that are used by the default audio policy
 * engine to determine audio routing.
 *
 * {@hide}
 */
@Backing(type="byte")
@VintfStability
enum AudioProductStrategyType {
    /**
     * Strategy indicating that there is no corresponding product strategy
     * defined.
     *
     * Value reserved for system use only. HALs must never return this value to
     * the system or accept it from the system.
     */
    SYS_RESERVED_NONE = -1,
    /**
     * Strategy corresponding to media usages (this is normally media playback,
     * gaming, and assistant usages).
     */
    MEDIA = 0,
    /**
     * Strategy corresponding to voice communication usages.
     */
    PHONE = 1,
    /**
     * Strategy corresponding to ringtone and alarm usages.
     */
    SONIFICATION = 2,
    /**
     * Strategy corresponding to notification usages.
     */
    SONIFICATION_RESPECTFUL = 3,
    /**
     * Dual-Tone Multi-Frequency (DTMF a.k.a. touch tone) is the strategy
     * corresponding to voice communication signalling usages.
     */
    DTMF = 4,
    /**
     * Strategy corresponding to the use case where sound must be externally
     * audible (i.e. for sounds subject to regulatory behaviors in some
     * countries, such as camera shutter sound).
     */
    ENFORCED_AUDIBLE = 5,
    /**
     * Strategy corresponding to the usages where audio streams are exclusively
     * transmitted through the speaker of the device.
     */
    TRANSMITTED_THROUGH_SPEAKER = 6,
    /**
     * Strategy corresponding to accessibility usages.
     */
    ACCESSIBILITY = 7,
    /**
     * Strategy corresponding to rerouting usages.
     *
     * Value reserved for system use only. HALs must never return this value to
     * the system or accept it from the system.
     */
    SYS_RESERVED_REROUTING = 8,
    /**
     * Strategy corresponding to call assistant usages.
     *
     * Value reserved for system use only. HALs must never return this value to
     * the system or accept it from the system.
     */
    SYS_RESERVED_CALL_ASSISTANT = 9,
}
