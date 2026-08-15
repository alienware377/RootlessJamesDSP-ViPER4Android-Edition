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
#include <cutils/native_handle.h>
#include <fmq/MessageQueueBase.h>
#include <utils/Log.h>
#include <type_traits>

using android::hardware::MQFlavor;

typedef uint64_t RingBufferPosition;

namespace android {

template <typename T>
struct FlavorTypeToValue;

/*
 * AIDL parcelables will have the typedef fixed_size. It is std::true_type when the
 * parcelable is annotated with @FixedSize, and std::false_type when not. Other types
 * should not have the fixed_size typedef, so they will always resolve to std::false_type.
 */
template <typename T, typename = void>
struct has_typedef_fixed_size : std::false_type {};

template <typename T>
struct has_typedef_fixed_size<T, std::void_t<typename T::fixed_size>> : T::fixed_size {};

#define STATIC_AIDL_TYPE_CHECK(T)                                                                  \
    static_assert(has_typedef_fixed_size<T>::value == true || std::is_fundamental<T>::value ||     \
                          std::is_enum<T>::value,                                                  \
                  "Only fundamental types, enums, and AIDL parcelables annotated with @FixedSize " \
                  "and built for the NDK backend are supported as payload types(T).");

template <template <typename> class C1>
struct Base {};

template <typename T, typename BaseTypes, typename U>
struct Queue : Base<BaseTypes::template B> {};

template <typename T, typename U, typename BackendTypes>
struct AidlMessageQueueBase
    : public MessageQueueBase<BackendTypes::template AidlMQDescriptorShimType, T,
                              FlavorTypeToValue<U>::value> {
    STATIC_AIDL_TYPE_CHECK(T);
    typedef typename BackendTypes::FileDescriptorType FileDescriptorType;
    typedef typename BackendTypes::GrantorDescriptorType GrantorDescriptorType;
    typedef typename BackendTypes::template AidlMQDescriptorShimType<T, FlavorTypeToValue<U>::value>
            Descriptor;
    /**
     * This constructor uses the external descriptor used with AIDL interfaces.
     * It will create an FMQ based on the descriptor that was obtained from
     * another FMQ instance for communication.
     *
     * @param desc Descriptor from another FMQ that contains all of the
     * information required to create a new instance of that queue.
     * @param resetPointers Boolean indicating whether the read/write pointers
     * should be reset or not.
     */
    AidlMessageQueueBase(const typename BackendTypes::template MQDescriptorType<T, U>& desc,
                         bool resetPointers = true);
    ~AidlMessageQueueBase() = default;

    /**
     * This constructor uses Ashmem shared memory to create an FMQ
     * that can contain a maximum of 'numElementsInQueue' elements of type T.
     *
     * @param numElementsInQueue Capacity of the AidlMessageQueueBase in terms of T.
     * @param configureEventFlagWord Boolean that specifies if memory should
     * also be allocated and mapped for an EventFlag word.
     * @param bufferFd User-supplied file descriptor to map the memory for the ringbuffer
     * By default, bufferFd=-1 means library will allocate ashmem region for ringbuffer.
     * MessageQueue takes ownership of the file descriptor.
     * @param bufferSize size of buffer in bytes that bufferFd represents. This
     * size must be larger than or equal to (numElementsInQueue * sizeof(T)).
     * Otherwise, operations will cause out-of-bounds memory access.
     */
    AidlMessageQueueBase(size_t numElementsInQueue, bool configureEventFlagWord,
                         android::base::unique_fd bufferFd, size_t bufferSize);

    AidlMessageQueueBase(size_t numElementsInQueue, bool configureEventFlagWord = false)
        : AidlMessageQueueBase(numElementsInQueue, configureEventFlagWord,
                               android::base::unique_fd(), 0) {}

    template <typename V = T>
    AidlMessageQueueBase(size_t numElementsInQueue, bool configureEventFlagWord = false,
                         std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum = sizeof(T))
        : AidlMessageQueueBase(numElementsInQueue, configureEventFlagWord,
                               android::base::unique_fd(), 0, quantum) {}

    template <typename V = T>
    AidlMessageQueueBase(size_t numElementsInQueue, bool configureEventFlagWord,
                         android::base::unique_fd bufferFd, size_t bufferSize,
                         std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum);
    typename BackendTypes::template MQDescriptorType<T, U> dupeDesc() const;

  private:
    AidlMessageQueueBase(const AidlMessageQueueBase& other) = delete;
    AidlMessageQueueBase& operator=(const AidlMessageQueueBase& other) = delete;
    AidlMessageQueueBase() = delete;
};

template <typename T, typename U, typename BackendTypes>
AidlMessageQueueBase<T, U, BackendTypes>::AidlMessageQueueBase(
        const typename BackendTypes::template MQDescriptorType<T, U>& desc, bool resetPointers)
    : MessageQueueBase<BackendTypes::template AidlMQDescriptorShimType, T,
                       FlavorTypeToValue<U>::value>(Descriptor(desc), resetPointers) {}

template <typename T, typename U, typename BackendTypes>
AidlMessageQueueBase<T, U, BackendTypes>::AidlMessageQueueBase(size_t numElementsInQueue,
                                                               bool configureEventFlagWord,
                                                               android::base::unique_fd bufferFd,
                                                               size_t bufferSize)
    : MessageQueueBase<BackendTypes::template AidlMQDescriptorShimType, T,
                       FlavorTypeToValue<U>::value>(numElementsInQueue, configureEventFlagWord,
                                                    std::move(bufferFd), bufferSize) {}

template <typename T, typename U, typename BackendTypes>
template <typename V>
AidlMessageQueueBase<T, U, BackendTypes>::AidlMessageQueueBase(
        size_t numElementsInQueue, bool configureEventFlagWord, android::base::unique_fd bufferFd,
        size_t bufferSize, std::enable_if_t<std::is_same_v<V, MQErased>, size_t> quantum)
    : MessageQueueBase<BackendTypes::template AidlMQDescriptorShimType, T,
                       FlavorTypeToValue<U>::value>(numElementsInQueue, configureEventFlagWord,
                                                    std::move(bufferFd), bufferSize, quantum) {}

template <typename T, typename U, typename BackendTypes>
typename BackendTypes::template MQDescriptorType<T, U>
AidlMessageQueueBase<T, U, BackendTypes>::dupeDesc() const {
    auto* shim = MessageQueueBase<BackendTypes::template AidlMQDescriptorShimType, T,
                                  FlavorTypeToValue<U>::value>::getDesc();
    if (shim) {
        std::vector<GrantorDescriptorType> grantors;
        for (const auto& grantor : shim->grantors()) {
            GrantorDescriptorType gd;
            gd.fdIndex = static_cast<int32_t>(grantor.fdIndex);
            gd.offset = static_cast<int32_t>(grantor.offset);
            gd.extent = static_cast<int64_t>(grantor.extent);
            grantors.push_back(gd);
        }
        std::vector<FileDescriptorType> fds;
        std::vector<int> ints;
        int data_index = 0;
        for (; data_index < shim->handle()->numFds; data_index++) {
            fds.push_back(BackendTypes::createFromInt(dup(shim->handle()->data[data_index])));
        }
        for (; data_index < shim->handle()->numFds + shim->handle()->numInts; data_index++) {
            ints.push_back(shim->handle()->data[data_index]);
        }
        typename BackendTypes::template MQDescriptorType<T, U> desc;

        desc.grantors = grantors;
        desc.handle.fds = std::move(fds);
        desc.handle.ints = ints;
        desc.quantum = static_cast<int32_t>(shim->getQuantum());
        desc.flags = static_cast<int32_t>(shim->getFlags());
        return desc;
    } else {
        return typename BackendTypes::template MQDescriptorType<T, U>();
    }
}

}  // namespace android
