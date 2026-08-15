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
///////////////////////////////////////////////////////////////////////////////
// THIS FILE IS IMMUTABLE. DO NOT EDIT IN ANY CASE.                          //
///////////////////////////////////////////////////////////////////////////////

// This file is a snapshot of an AIDL file. Do not edit it manually. There are
// two cases:
// 1). this is a frozen version file - do not edit this in any case.
// 2). this is a 'current' file. If you make a backwards compatible change to
//     the interface (from the latest frozen version), the build system will
//     prompt you to update this file with `m <name>-update-api`.
//
// You must not make a backward incompatible change to any AIDL file built
// with the aidl_interface module type with versions property set. The module
// type is used to build AIDL files in a way that they can be used across
// independently updatable components of the system. If a device is shipped
// with such a backward incompatible change, it has a high risk of breaking
// later when a module using the interface is updated, e.g., Mainline modules.

package android.hardware.audio.effect;
@VintfStability
parcelable Descriptor {
  android.hardware.audio.effect.Descriptor.Common common;
  android.hardware.audio.effect.Capability capability;
  const String EFFECT_TYPE_UUID_AEC = "7b491460-8d4d-11e0-bd61-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_AGC1 = "0a8abfe0-654c-11e0-ba26-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_AGC2 = "ae3c653b-be18-4ab8-8938-418f0a7f06ac";
  const String EFFECT_TYPE_UUID_BASS_BOOST = "0634f220-ddd4-11db-a0fc-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_DOWNMIX = "381e49cc-a858-4aa2-87f6-e8388e7601b2";
  const String EFFECT_TYPE_UUID_DYNAMICS_PROCESSING = "7261676f-6d75-7369-6364-28e2fd3ac39e";
  const String EFFECT_TYPE_UUID_ENV_REVERB = "c2e5d5f0-94bd-4763-9cac-4e234d06839e";
  const String EFFECT_TYPE_UUID_EQUALIZER = "0bed4300-ddd6-11db-8f34-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_ERASER = "fa81a9ac-588b-11ed-9b6a-0242ac120002";
  const String EFFECT_TYPE_UUID_HAPTIC_GENERATOR = "1411e6d6-aecd-4021-a1cf-a6aceb0d71e5";
  const String EFFECT_TYPE_UUID_LOUDNESS_ENHANCER = "fe3199be-aed0-413f-87bb-11260eb63cf1";
  const String EFFECT_TYPE_UUID_NS = "58b4b260-8e06-11e0-aa8e-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_PRESET_REVERB = "47382d60-ddd8-11db-bf3a-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_SPATIALIZER = "ccd4cf09-a79d-46c2-9aae-06a1698d6c8f";
  const String EFFECT_TYPE_UUID_VIRTUALIZER = "37cc2c00-dddd-11db-8577-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_VISUALIZER = "e46b26a0-dddd-11db-8afd-0002a5d5c51b";
  const String EFFECT_TYPE_UUID_VOLUME = "09e8ede0-ddde-11db-b4f6-0002a5d5c51b";
  @VintfStability
  parcelable Identity {
    android.media.audio.common.AudioUuid type;
    android.media.audio.common.AudioUuid uuid;
    @nullable android.media.audio.common.AudioUuid proxy;
  }
  @VintfStability
  parcelable Common {
    android.hardware.audio.effect.Descriptor.Identity id;
    android.hardware.audio.effect.Flags flags;
    int cpuLoad;
    int memoryUsage;
    @utf8InCpp String name;
    @utf8InCpp String implementor;
  }
}
