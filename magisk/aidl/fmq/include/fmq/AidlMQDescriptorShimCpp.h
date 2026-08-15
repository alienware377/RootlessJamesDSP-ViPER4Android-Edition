/*
 * Copyright (C) 2020 The Android Open Source Project
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
#pragma once
#include <cutils/native_handle.h>
#include <limits>
#include <type_traits>

#include <android/hardware/common/fmq/MQDescriptor.h>
#include <fmq/MQDescriptorBase.h>

#include "AidlMQDescriptorShimBase.h"

namespace android {
namespace details {

using ::android::hardware::MQFlavor;

struct BackendTypesStoreCpp {
    template <typename T, typename flavor>
    using MQDescriptorType = android::hardware::common::fmq::MQDescriptor<T, flavor>;
    using SynchronizedReadWriteType = android::hardware::common::fmq::SynchronizedReadWrite;
    using UnsynchronizedWriteType = android::hardware::common::fmq::UnsynchronizedWrite;
};

template <typename T, MQFlavor flavor>
struct AidlMQDescriptorShimCpp : public AidlMQDescriptorShimBase<T, flavor, BackendTypesStoreCpp> {
    // Takes ownership of handle
    AidlMQDescriptorShimCpp(const std::vector<android::hardware::GrantorDescriptor>& grantors,
                            native_handle_t* nHandle, size_t size);

    // Takes ownership of handle
    AidlMQDescriptorShimCpp(
            const android::hardware::common::fmq::MQDescriptor<
                    T, typename std::conditional<
                               flavor == hardware::kSynchronizedReadWrite,
                               android::hardware::common::fmq::SynchronizedReadWrite,
                               android::hardware::common::fmq::UnsynchronizedWrite>::type>& desc);

    // Takes ownership of handle
    AidlMQDescriptorShimCpp(size_t bufferSize, native_handle_t* nHandle, size_t messageSize,
                            bool configureEventFlag = false);

    explicit AidlMQDescriptorShimCpp(const AidlMQDescriptorShimCpp& other)
        : AidlMQDescriptorShimCpp(0, nullptr, 0) {
        *this = other;
    }
};

template <typename T, MQFlavor flavor>
AidlMQDescriptorShimCpp<T, flavor>::AidlMQDescriptorShimCpp(
        const android::hardware::common::fmq::MQDescriptor<
                T, typename std::conditional<
                           flavor == hardware::kSynchronizedReadWrite,
                           android::hardware::common::fmq::SynchronizedReadWrite,
                           android::hardware::common::fmq::UnsynchronizedWrite>::type>& desc)
    : AidlMQDescriptorShimBase<T, flavor, BackendTypesStoreCpp>(desc) {}

template <typename T, MQFlavor flavor>
AidlMQDescriptorShimCpp<T, flavor>::AidlMQDescriptorShimCpp(
        const std::vector<android::hardware::GrantorDescriptor>& grantors, native_handle_t* nhandle,
        size_t size)
    : AidlMQDescriptorShimBase<T, flavor, BackendTypesStoreCpp>(grantors, nhandle, size) {}

template <typename T, MQFlavor flavor>
AidlMQDescriptorShimCpp<T, flavor>::AidlMQDescriptorShimCpp(size_t bufferSize,
                                                            native_handle_t* nHandle,
                                                            size_t messageSize,
                                                            bool configureEventFlag)
    : AidlMQDescriptorShimBase<T, flavor, BackendTypesStoreCpp>(bufferSize, nHandle, messageSize,
                                                                configureEventFlag) {}

}  // namespace details
}  // namespace android
