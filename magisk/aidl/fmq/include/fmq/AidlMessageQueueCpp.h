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

#pragma once
#include <android/hardware/common/fmq/MQDescriptor.h>
#include <android/hardware/common/fmq/SynchronizedReadWrite.h>
#include <android/hardware/common/fmq/UnsynchronizedWrite.h>
#include <fmq/MQDescriptorBase.h>
#include "AidlMQDescriptorShimCpp.h"
#include "AidlMessageQueueBase.h"
namespace android {

template <>
struct FlavorTypeToValue<android::hardware::common::fmq::SynchronizedReadWrite> {
    static constexpr MQFlavor value = hardware::kSynchronizedReadWrite;
};

template <>
struct FlavorTypeToValue<android::hardware::common::fmq::UnsynchronizedWrite> {
    static constexpr MQFlavor value = hardware::kUnsynchronizedWrite;
};

struct BackendTypesStoreCpp {
    template <typename T, MQFlavor flavor>
    using AidlMQDescriptorShimType = android::details::AidlMQDescriptorShimCpp<T, flavor>;
    using GrantorDescriptorType = android::hardware::common::fmq::GrantorDescriptor;
    template <typename T, typename flavor>
    using MQDescriptorType = android::hardware::common::fmq::MQDescriptor<T, flavor>;
    using FileDescriptorType = os::ParcelFileDescriptor;
    static FileDescriptorType createFromInt(int fd) {
        return FileDescriptorType(binder::unique_fd(fd));
    }
};

template <typename T, typename U>
struct AidlMessageQueueCpp final : public AidlMessageQueueBase<T, U, BackendTypesStoreCpp> {
    AidlMessageQueueCpp(const android::hardware::common::fmq::MQDescriptor<T, U>& desc,
                        bool resetPointers = true);
    ~AidlMessageQueueCpp() = default;

    /**
     * This constructor uses Ashmem shared memory to create an FMQ
     * that can contain a maximum of 'numElementsInQueue' elements of type T.
     *
     * @param numElementsInQueue Capacity of the AidlMessageQueueCpp in terms of T.
     * @param configureEventFlagWord Boolean that specifies if memory should
     * also be allocated and mapped for an EventFlag word.
     * @param bufferFd User-supplied file descriptor to map the memory for the ringbuffer
     * By default, bufferFd=-1 means library will allocate ashmem region for ringbuffer.
     * MessageQueue takes ownership of the file descriptor.
     * @param bufferSize size of buffer in bytes that bufferFd represents. This
     * size must be larger than or equal to (numElementsInQueue * sizeof(T)).
     * Otherwise, operations will cause out-of-bounds memory access.
     */
    AidlMessageQueueCpp(size_t numElementsInQueue, bool configureEventFlagWord,
                        android::base::unique_fd bufferFd, size_t bufferSize);

    AidlMessageQueueCpp(size_t numElementsInQueue, bool configureEventFlagWord = false)
        : AidlMessageQueueCpp(numElementsInQueue, configureEventFlagWord,
                              android::base::unique_fd(), 0) {}

    template <typename V = T>
    AidlMessageQueueCpp(size_t numElementsInQueue, bool configureEventFlagWord = false,
                        std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum = sizeof(T))
        : AidlMessageQueueCpp(numElementsInQueue, configureEventFlagWord,
                              android::base::unique_fd(), 0, quantum) {}

    template <typename V = T>
    AidlMessageQueueCpp(size_t numElementsInQueue, bool configureEventFlagWord,
                        android::base::unique_fd bufferFd, size_t bufferSize,
                        std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum);
};

template <typename T, typename U>
AidlMessageQueueCpp<T, U>::AidlMessageQueueCpp(
        const android::hardware::common::fmq::MQDescriptor<T, U>& desc, bool resetPointers)
    : AidlMessageQueueBase<T, U, BackendTypesStoreCpp>(desc, resetPointers) {}

template <typename T, typename U>
AidlMessageQueueCpp<T, U>::AidlMessageQueueCpp(size_t numElementsInQueue,
                                               bool configureEventFlagWord,
                                               android::base::unique_fd bufferFd, size_t bufferSize)
    : AidlMessageQueueBase<T, U, BackendTypesStoreCpp>(numElementsInQueue, configureEventFlagWord,
                                                       std::move(bufferFd), bufferSize) {}

template <typename T, typename U>
template <typename V>
AidlMessageQueueCpp<T, U>::AidlMessageQueueCpp(
        size_t numElementsInQueue, bool configureEventFlagWord, android::base::unique_fd bufferFd,
        size_t bufferSize, std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum)
    : AidlMessageQueueBase<T, U, BackendTypesStoreCpp>(numElementsInQueue, configureEventFlagWord,
                                                       std::move(bufferFd), bufferSize, quantum) {}

}  // namespace android