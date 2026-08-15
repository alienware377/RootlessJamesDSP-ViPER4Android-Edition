/*
 * Copyright (C) 2023 The Android Open Source Project
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
union Range {
  android.hardware.audio.effect.Range.VendorExtensionRange[] vendorExtension = {};
  android.hardware.audio.effect.Range.AcousticEchoCancelerRange[] acousticEchoCanceler;
  android.hardware.audio.effect.Range.AutomaticGainControlV1Range[] automaticGainControlV1;
  android.hardware.audio.effect.Range.AutomaticGainControlV2Range[] automaticGainControlV2;
  android.hardware.audio.effect.Range.BassBoostRange[] bassBoost;
  android.hardware.audio.effect.Range.DownmixRange[] downmix;
  android.hardware.audio.effect.Range.DynamicsProcessingRange[] dynamicsProcessing;
  android.hardware.audio.effect.Range.EnvironmentalReverbRange[] environmentalReverb;
  android.hardware.audio.effect.Range.EqualizerRange[] equalizer;
  android.hardware.audio.effect.Range.HapticGeneratorRange[] hapticGenerator;
  android.hardware.audio.effect.Range.LoudnessEnhancerRange[] loudnessEnhancer;
  android.hardware.audio.effect.Range.NoiseSuppressionRange[] noiseSuppression;
  android.hardware.audio.effect.Range.PresetReverbRange[] presetReverb;
  android.hardware.audio.effect.Range.VirtualizerRange[] virtualizer;
  android.hardware.audio.effect.Range.VisualizerRange[] visualizer;
  android.hardware.audio.effect.Range.VolumeRange[] volume;
  android.hardware.audio.effect.Range.SpatializerRange[] spatializer;
  @VintfStability
  parcelable AcousticEchoCancelerRange {
    android.hardware.audio.effect.AcousticEchoCanceler min;
    android.hardware.audio.effect.AcousticEchoCanceler max;
  }
  @VintfStability
  parcelable AutomaticGainControlV1Range {
    android.hardware.audio.effect.AutomaticGainControlV1 min;
    android.hardware.audio.effect.AutomaticGainControlV1 max;
  }
  @VintfStability
  parcelable AutomaticGainControlV2Range {
    android.hardware.audio.effect.AutomaticGainControlV2 min;
    android.hardware.audio.effect.AutomaticGainControlV2 max;
  }
  @VintfStability
  parcelable BassBoostRange {
    android.hardware.audio.effect.BassBoost min;
    android.hardware.audio.effect.BassBoost max;
  }
  @VintfStability
  parcelable DownmixRange {
    android.hardware.audio.effect.Downmix min;
    android.hardware.audio.effect.Downmix max;
  }
  @VintfStability
  parcelable DynamicsProcessingRange {
    android.hardware.audio.effect.DynamicsProcessing min;
    android.hardware.audio.effect.DynamicsProcessing max;
  }
  @VintfStability
  parcelable EnvironmentalReverbRange {
    android.hardware.audio.effect.EnvironmentalReverb min;
    android.hardware.audio.effect.EnvironmentalReverb max;
  }
  @VintfStability
  parcelable EqualizerRange {
    android.hardware.audio.effect.Equalizer min;
    android.hardware.audio.effect.Equalizer max;
  }
  @VintfStability
  parcelable HapticGeneratorRange {
    android.hardware.audio.effect.HapticGenerator min;
    android.hardware.audio.effect.HapticGenerator max;
  }
  @VintfStability
  parcelable LoudnessEnhancerRange {
    android.hardware.audio.effect.LoudnessEnhancer min;
    android.hardware.audio.effect.LoudnessEnhancer max;
  }
  @VintfStability
  parcelable NoiseSuppressionRange {
    android.hardware.audio.effect.NoiseSuppression min;
    android.hardware.audio.effect.NoiseSuppression max;
  }
  @VintfStability
  parcelable PresetReverbRange {
    android.hardware.audio.effect.PresetReverb min;
    android.hardware.audio.effect.PresetReverb max;
  }
  @VintfStability
  parcelable SpatializerRange {
    android.hardware.audio.effect.Spatializer min;
    android.hardware.audio.effect.Spatializer max;
  }
  @VintfStability
  parcelable VendorExtensionRange {
    android.hardware.audio.effect.VendorExtension min;
    android.hardware.audio.effect.VendorExtension max;
  }
  @VintfStability
  parcelable VirtualizerRange {
    android.hardware.audio.effect.Virtualizer min;
    android.hardware.audio.effect.Virtualizer max;
  }
  @VintfStability
  parcelable VisualizerRange {
    android.hardware.audio.effect.Visualizer min;
    android.hardware.audio.effect.Visualizer max;
  }
  @VintfStability
  parcelable VolumeRange {
    android.hardware.audio.effect.Volume min;
    android.hardware.audio.effect.Volume max;
  }
}
