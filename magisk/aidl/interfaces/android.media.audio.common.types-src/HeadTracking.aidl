/*
 * Copyright 2023 The Android Open Source Project
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
 * HeadTracking settings which can be used for Spatialization, including information of the user's
 * head position and orientation.
 *
 * This information can be used to update the view or content based on the user's head movement.
 *
 * {@hide}
 */
@JavaDerive(equals=true, toString=true)
@VintfStability
parcelable HeadTracking {
    /**
     * The head tracking mode supported by the spatializer effect implementation.
     * Used by methods of the ISpatializer interface.
     * {@hide}
     */
    @Backing(type="byte")
    enum Mode {
        /**
         * Head tracking is active in a mode not listed below (forward compatibility).
         */
        OTHER = 0,

        /**
         * Head tracking is disabled
         */
        DISABLED = 1,

        /**
         * Head tracking is performed relative to the real work environment.
         */
        RELATIVE_WORLD = 2,

        /**
         * Head tracking is performed relative to the device's screen.
         */
        RELATIVE_SCREEN = 3,
    }

    /**
     * The head tracking sensor connection modes.
     * {@hide}
     */
    @Backing(type="byte")
    enum ConnectionMode {
        /**
         * The audio framework provides pre-processed IMU (Inertial Measurement Unit) data in
         * "head-to-stage" vector format to HAL.
         */
        FRAMEWORK_PROCESSED = 0,

        /**
         * The Spatializer effect directly connects to the sensor via the sensor software stack.
         * The audio framework just controls the enabled state of the sensor.
         *
         * Can be used by software implementations which do not want to benefit from the
         * AOSP libheadtracking IMU data preprocessing.
         * Can also be used by DSP offloaded Spatializer implementations.
         */
        DIRECT_TO_SENSOR_SW = 1,

        /**
         * The Spatializer effect directly connects to the sensor via hardware tunneling.
         * The audio framework just controls the enabled state of the sensor.
         *
         * Can be used by DSP offloaded Spatializer implementations.
         */
        DIRECT_TO_SENSOR_TUNNEL = 2,
    }

    /**
     * The Headtracking sensor data.
     * In the current version, the only sensor data format framework send to HAL Spatializer is
     * "head-to-stage". Other formats can be added in future versions, hence a union is used.
     * {@hide}
     */
    union SensorData {
        /**
         * Vector representing of the "head-to-stage" pose with six floats: first three are a
         * translation vector, and the last three are a rotation vector.
         */
        float[6] headToStage = {0f, 0f, 0f, 0f, 0f, 0f};
    }
}
