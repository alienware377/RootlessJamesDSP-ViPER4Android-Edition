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

/**
 * "Force Use" specifies high-level routing policies which are used
 * in order to override the usual routing behavior.
 *
 * {@hide}
 */
@SuppressWarnings(value={"redundant-name"})
@VintfStability
union AudioPolicyForceUse {
    @Backing(type="byte")
    @VintfStability
    enum CommunicationDeviceCategory {
        NONE = 0,
        SPEAKER,
        BT_SCO,
        BT_BLE,
        WIRED_ACCESSORY,
    }
    @Backing(type="byte")
    @VintfStability
    enum MediaDeviceCategory {
        NONE = 0,
        SPEAKER,
        HEADPHONES,
        BT_A2DP,
        ANALOG_DOCK,
        DIGITAL_DOCK,
        WIRED_ACCESSORY,
        NO_BT_A2DP,
    }
    @Backing(type="byte")
    @VintfStability
    enum DockType {
        NONE = 0,
        BT_CAR_DOCK,
        BT_DESK_DOCK,
        ANALOG_DOCK,
        DIGITAL_DOCK,
        WIRED_ACCESSORY,
    }
    @Backing(type="byte")
    @VintfStability
    enum EncodedSurroundConfig {
        UNSPECIFIED = 0,
        NEVER,
        ALWAYS,
        MANUAL,
    }

    /**
     * Configures the audio device used for media playback.
     * This is also the default value.
     */
    MediaDeviceCategory forMedia = MediaDeviceCategory.NONE;
    /**
     * Configures the audio device used for "communication" (telephony, VoIP) use cases.
     * Note that 'BT_BLE' and 'WIRED_ACCESSORY' can not be used in this case.
     */
    CommunicationDeviceCategory forCommunication = CommunicationDeviceCategory.NONE;
    /**
     * Configures the audio device used for recording.
     * Note that 'SPEAKER' and 'BT_BLE' can not be used in this case.
     */
    CommunicationDeviceCategory forRecord = CommunicationDeviceCategory.NONE;
    /**
     * Configures whether in muted audio mode ringing should also be sent to a BT device.
     * Note that 'SPEAKER' and 'WIRED_ACCESSORY' can not be used in this case.
     */
    CommunicationDeviceCategory forVibrateRinging = CommunicationDeviceCategory.NONE;
    /**
     * Specifies whether the phone is currently placed into a dock. The value of
     * specifies the kind of the dock. This field may also be used that sending
     * of audio to the dock is overridden by another device.
     */
    DockType dock = DockType.NONE;
    /**
     * Specifies whether enforcing of certain sounds is enabled, for example,
     * enforcing of the camera shutter sound.
     */
    boolean systemSounds = false;
    /**
     * Specifies whether sending of system audio via HDMI is enabled.
     */
    boolean hdmiSystemAudio = false;
    /**
     * Configures whether support for encoded surround formats is enabled for
     * applications.
     */
    EncodedSurroundConfig encodedSurround = EncodedSurroundConfig.UNSPECIFIED;
}
