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
union Parameter {
  android.hardware.audio.effect.Parameter.Common common;
  android.media.audio.common.AudioDeviceDescription[] deviceDescription;
  android.media.audio.common.AudioMode mode;
  android.media.audio.common.AudioSource source;
  boolean offload;
  android.hardware.audio.effect.Parameter.VolumeStereo volumeStereo;
  android.hardware.audio.effect.Parameter.Specific specific;
  android.hardware.audio.common.SinkMetadata sinkMetadata;
  android.hardware.audio.common.SourceMetadata sourceMetadata;
  @VintfStability
  union Id {
    android.hardware.audio.effect.VendorExtension vendorEffectTag;
    android.hardware.audio.effect.AcousticEchoCanceler.Id acousticEchoCancelerTag;
    android.hardware.audio.effect.AutomaticGainControlV1.Id automaticGainControlV1Tag;
    android.hardware.audio.effect.AutomaticGainControlV2.Id automaticGainControlV2Tag;
    android.hardware.audio.effect.BassBoost.Id bassBoostTag;
    android.hardware.audio.effect.Downmix.Id downmixTag;
    android.hardware.audio.effect.DynamicsProcessing.Id dynamicsProcessingTag;
    android.hardware.audio.effect.EnvironmentalReverb.Id environmentalReverbTag;
    android.hardware.audio.effect.Equalizer.Id equalizerTag;
    android.hardware.audio.effect.HapticGenerator.Id hapticGeneratorTag;
    android.hardware.audio.effect.LoudnessEnhancer.Id loudnessEnhancerTag;
    android.hardware.audio.effect.NoiseSuppression.Id noiseSuppressionTag;
    android.hardware.audio.effect.PresetReverb.Id presetReverbTag;
    android.hardware.audio.effect.Virtualizer.Id virtualizerTag;
    android.hardware.audio.effect.Visualizer.Id visualizerTag;
    android.hardware.audio.effect.Volume.Id volumeTag;
    android.hardware.audio.effect.Parameter.Tag commonTag;
    android.hardware.audio.effect.Spatializer.Id spatializerTag;
    android.hardware.audio.effect.Eraser.Id eraserTag;
  }
  @VintfStability
  parcelable Common {
    int session;
    int ioHandle;
    android.media.audio.common.AudioConfig input;
    android.media.audio.common.AudioConfig output;
  }
  @VintfStability
  parcelable VolumeStereo {
    float left;
    float right;
  }
  @VintfStability
  union Specific {
    android.hardware.audio.effect.VendorExtension vendorEffect;
    android.hardware.audio.effect.AcousticEchoCanceler acousticEchoCanceler;
    android.hardware.audio.effect.AutomaticGainControlV1 automaticGainControlV1;
    android.hardware.audio.effect.AutomaticGainControlV2 automaticGainControlV2;
    android.hardware.audio.effect.BassBoost bassBoost;
    android.hardware.audio.effect.Downmix downmix;
    android.hardware.audio.effect.DynamicsProcessing dynamicsProcessing;
    android.hardware.audio.effect.EnvironmentalReverb environmentalReverb;
    android.hardware.audio.effect.Equalizer equalizer;
    android.hardware.audio.effect.HapticGenerator hapticGenerator;
    android.hardware.audio.effect.LoudnessEnhancer loudnessEnhancer;
    android.hardware.audio.effect.NoiseSuppression noiseSuppression;
    android.hardware.audio.effect.PresetReverb presetReverb;
    android.hardware.audio.effect.Virtualizer virtualizer;
    android.hardware.audio.effect.Visualizer visualizer;
    android.hardware.audio.effect.Volume volume;
    android.hardware.audio.effect.Spatializer spatializer;
    android.hardware.audio.effect.Eraser eraser;
  }
}
