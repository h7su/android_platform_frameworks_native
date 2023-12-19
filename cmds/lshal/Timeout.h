/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <chrono>
#include <future>

#include <hidl/Status.h>
#include <utils/Errors.h>

namespace android {
namespace lshal {

constexpr std::chrono::milliseconds IPC_CALL_WAIT{500};

// Call function on interfaceObject and wait for result until the given timeout has reached.
// Callback functions pass to timeoutIPC() may executed after the this function
// has returned, especially if deadline has been reached. Hence, care must be taken when passing
// data between the background thread and the main thread. See b/311143089.
template<class R, class P, class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(std::chrono::duration<R, P> wait, const sp<I> &interfaceObject, Function &&func,
           Args &&... args) {
    using ::android::hardware::Status;

    // Execute on a background thread but do not defer execution.
    auto future =
            std::async(std::launch::async, func, interfaceObject, std::forward<Args>(args)...);
    auto status = future.wait_for(wait);
    if (status == std::future_status::timeout) {
        return Status::fromStatusT(TIMED_OUT);
    }
    if (status != std::future_status::ready) {
        return Status::fromExceptionCode(Status::Exception::EX_ILLEGAL_STATE,
                                         "Illegal future_status");
    }
    return future.get();
}

// Call function on interfaceObject and wait for result until the default timeout has reached.
// Callback functions pass to timeoutIPC() may executed after the this function
// has returned, especially if deadline has been reached. Hence, care must be taken when passing
// data between the background thread and the main thread. See b/311143089.
template<class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(const sp<I> &interfaceObject, Function &&func, Args &&... args) {
    return timeoutIPC(IPC_CALL_WAIT, interfaceObject, func, args...);
}

} // namespace lshal
} // namespace android
