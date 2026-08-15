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

#include <aidl/android/hardware/common/fmq/MQDescriptor.h>
#include <fmq/MQDescriptorBase.h>
#include "AidlMQDescriptorShimBase.h"

namespace android {
namespace details {
using aidl::android::hardware::common::fmq::MQDescriptor;
using aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using aidl::android::hardware::common::fmq::UnsynchronizedWrite;
using android::hardware::MQFlavor;

struct BackendTypesStore {
    template <typename T, typename flavor>
    using MQDescriptorType = aidl::android::hardware::common::fmq::MQDescriptor<T, flavor>;
    using SynchronizedReadWriteType = aidl::android::hardware::common::fmq::SynchronizedReadWrite;
    using UnsynchronizedWriteType = aidl::android::hardware::common::fmq::UnsynchronizedWrite;
};

template <typename T, MQFlavor flavor>
struct AidlMQDescriptorShim : public AidlMQDescriptorShimBase<T, flavor, BackendTypesStore> {
    // Takes ownership of handle
    AidlMQDescriptorShim(const std::vector<android::hardware::GrantorDescriptor>& grantors,
                         native_handle_t* nHandle, size_t size);

    // Takes ownership of handle
    AidlMQDescriptorShim(
            const MQDescriptor<
                    T, typename std::conditional<flavor == hardware::kSynchronizedReadWrite,
                                                 SynchronizedReadWrite, UnsynchronizedWrite>::type>&
                    desc);

    // Takes ownership of handle
    AidlMQDescriptorShim(size_t bufferSize, native_handle_t* nHandle, size_t messageSize,
                         bool configureEventFlag = false);

    explicit AidlMQDescriptorShim(const AidlMQDescriptorShim& other)
        : AidlMQDescriptorShim(0, nullptr, 0) {
        *this = other;
    }
};

template <typename T, MQFlavor flavor>
AidlMQDescriptorShim<T, flavor>::AidlMQDescriptorShim(
        const MQDescriptor<T, typename std::conditional<flavor == hardware::kSynchronizedReadWrite,
                                                        SynchronizedReadWrite,
                                                        UnsynchronizedWrite>::type>& desc)
    : AidlMQDescriptorShimBase<T, flavor, BackendTypesStore>(desc) {}

template <typename T, MQFlavor flavor>
AidlMQDescriptorShim<T, flavor>::AidlMQDescriptorShim(
        const std::vector<android::hardware::GrantorDescriptor>& grantors, native_handle_t* nhandle,
        size_t size)
    : AidlMQDescriptorShimBase<T, flavor, BackendTypesStore>(grantors, nhandle, size) {}

template <typename T, MQFlavor flavor>
AidlMQDescriptorShim<T, flavor>::AidlMQDescriptorShim(size_t bufferSize, native_handle_t* nHandle,
                                                      size_t messageSize, bool configureEventFlag)
    : AidlMQDescriptorShimBase<T, flavor, BackendTypesStore>(bufferSize, nHandle, messageSize,
                                                             configureEventFlag) {}
}  // namespace details
}  // namespace android
