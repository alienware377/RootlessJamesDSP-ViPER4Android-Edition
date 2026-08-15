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
#include <aidl/android/hardware/common/fmq/MQDescriptor.h>
#include <aidl/android/hardware/common/fmq/SynchronizedReadWrite.h>
#include <aidl/android/hardware/common/fmq/UnsynchronizedWrite.h>
#include "AidlMQDescriptorShim.h"
#include "AidlMessageQueueBase.h"

namespace android {
using aidl::android::hardware::common::fmq::MQDescriptor;
using aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using aidl::android::hardware::common::fmq::UnsynchronizedWrite;
using android::details::AidlMQDescriptorShim;
using android::hardware::MQFlavor;

template <>
struct FlavorTypeToValue<aidl::android::hardware::common::fmq::SynchronizedReadWrite> {
    static constexpr MQFlavor value = hardware::kSynchronizedReadWrite;
};

template <>
struct FlavorTypeToValue<aidl::android::hardware::common::fmq::UnsynchronizedWrite> {
    static constexpr MQFlavor value = hardware::kUnsynchronizedWrite;
};

struct BackendTypesStore {
    template <typename T, MQFlavor flavor>
    using AidlMQDescriptorShimType = android::details::AidlMQDescriptorShim<T, flavor>;
    using GrantorDescriptorType = aidl::android::hardware::common::fmq::GrantorDescriptor;
    template <typename T, typename flavor>
    using MQDescriptorType = aidl::android::hardware::common::fmq::MQDescriptor<T, flavor>;
    using FileDescriptorType = ndk::ScopedFileDescriptor;
    static FileDescriptorType createFromInt(int fd) { return FileDescriptorType(fd); }
};

template <typename T, typename U>
struct AidlMessageQueue final : public AidlMessageQueueBase<T, U, BackendTypesStore> {
    AidlMessageQueue(const MQDescriptor<T, U>& desc, bool resetPointers = true);
    ~AidlMessageQueue() = default;

    /**
     * This constructor uses Ashmem shared memory to create an FMQ
     * that can contain a maximum of 'numElementsInQueue' elements of type T.
     *
     * @param numElementsInQueue Capacity of the AidlMessageQueue in terms of T.
     * @param configureEventFlagWord Boolean that specifies if memory should
     * also be allocated and mapped for an EventFlag word.
     * @param bufferFd User-supplied file descriptor to map the memory for the ringbuffer
     * By default, bufferFd=-1 means library will allocate ashmem region for ringbuffer.
     * MessageQueue takes ownership of the file descriptor.
     * @param bufferSize size of buffer in bytes that bufferFd represents. This
     * size must be larger than or equal to (numElementsInQueue * sizeof(T)).
     * Otherwise, operations will cause out-of-bounds memory access.
     */
    AidlMessageQueue(size_t numElementsInQueue, bool configureEventFlagWord,
                     android::base::unique_fd bufferFd, size_t bufferSize);

    AidlMessageQueue(size_t numElementsInQueue, bool configureEventFlagWord = false)
        : AidlMessageQueue(numElementsInQueue, configureEventFlagWord, android::base::unique_fd(),
                           0) {}

    template <typename V = T>
    AidlMessageQueue(size_t numElementsInQueue, bool configureEventFlagWord = false,
                     std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum = sizeof(T))
        : AidlMessageQueue(numElementsInQueue, configureEventFlagWord, android::base::unique_fd(),
                           0, quantum) {}

    template <typename V = T>
    AidlMessageQueue(size_t numElementsInQueue, bool configureEventFlagWord,
                     android::base::unique_fd bufferFd, size_t bufferSize,
                     std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum);
};

template <typename T, typename U>
AidlMessageQueue<T, U>::AidlMessageQueue(const MQDescriptor<T, U>& desc, bool resetPointers)
    : AidlMessageQueueBase<T, U, BackendTypesStore>(desc, resetPointers) {}

template <typename T, typename U>
AidlMessageQueue<T, U>::AidlMessageQueue(size_t numElementsInQueue, bool configureEventFlagWord,
                                         android::base::unique_fd bufferFd, size_t bufferSize)
    : AidlMessageQueueBase<T, U, BackendTypesStore>(numElementsInQueue, configureEventFlagWord,
                                                    std::move(bufferFd), bufferSize) {}

template <typename T, typename U>
template <typename V>
AidlMessageQueue<T, U>::AidlMessageQueue(
        size_t numElementsInQueue, bool configureEventFlagWord, android::base::unique_fd bufferFd,
        size_t bufferSize, std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum)
    : AidlMessageQueueBase<T, U, BackendTypesStore>(numElementsInQueue, configureEventFlagWord,
                                                    std::move(bufferFd), bufferSize, quantum) {}

}  // namespace android
