/*
 * Copyright 2024 The Android Open Source Project
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
 * Audio Volume Group Change Event.
 *
 * {@hide}
 */
@SuppressWarnings(value={"redundant-name"}) // for VOLUME_FLAG_*
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable AudioVolumeGroupChangeEvent {
    /**
     * Shall show a toast containing the current volume.
     */
    const int VOLUME_FLAG_SHOW_UI = 1 << 0;
    /**
     * Whether to include ringer modes as possible options when changing volume..
     */
    const int VOLUME_FLAG_ALLOW_RINGER_MODES = 1 << 1;
    /**
     * Whether to play a sound when changing the volume.
     */
    const int VOLUME_FLAG_PLAY_SOUND = 1 << 2;
    /**
     * Removes any sounds/vibrate that may be in the queue, or are playing.
     */
    const int VOLUME_FLAG_REMOVE_SOUND_AND_VIBRATE = 1 << 3;
    /**
     * Whether to vibrate if going into the vibrate ringer mode.
     */
    const int VOLUME_FLAG_VIBRATE = 1 << 4;
    /**
     * Indicates to VolumePanel that the volume slider should be disabled as user cannot
     * change the volume.
     */
    const int VOLUME_FLAG_FIXED_VOLUME = 1 << 5;
    /**
     * Indicates the volume set/adjust call is for Bluetooth absolute volume.
     */
    const int VOLUME_FLAG_BLUETOOTH_ABS_VOLUME = 1 << 6;
    /**
     * Adjusting the volume was prevented due to silent mode, display a hint in the UI.
     */
    const int VOLUME_FLAG_SHOW_SILENT_HINT = 1 << 7;
    /**
     * Indicates the volume call is for Hdmi Cec system audio volume.
     */
    const int VOLUME_FLAG_HDMI_SYSTEM_AUDIO_VOLUME = 1 << 8;
    /**
     * Indicates that this should only be handled if media is actively playing.
     */
    const int VOLUME_FLAG_ACTIVE_MEDIA_ONLY = 1 << 9;
    /**
     * Like FLAG_SHOW_UI, but only dialog warnings and confirmations, no sliders.
     */
    const int VOLUME_FLAG_SHOW_UI_WARNINGS = 1 << 10;
    /**
     * Adjusting the volume down from vibrated was prevented, display a hint in the UI.
     */
    const int VOLUME_FLAG_SHOW_VIBRATE_HINT = 1 << 11;
    /**
     * Adjusting the volume due to a hardware key press.
     */
    const int VOLUME_FLAG_FROM_KEY = 1 << 12;
    /**
     * Indicates that an absolute volume controller is notifying AudioService of a change in the
     * volume or mute status of an external audio system..
     */
    const int VOLUME_FLAG_ABSOLUTE_VOLUME = 1 << 13;

    /** Unique identifier of the volume group. */
    int groupId;
    /** Index in UI applied. */
    int volumeIndex;
    /** Muted attribute, orthogonal to volume index. */
    boolean muted;
    /**
     * Bitmask indicating a suggested UI behavior or characterising the volume event.
     * The bit masks are defined in the constants prefixed by VOLUME_FLAG_*.
     */
    int flags;
}
