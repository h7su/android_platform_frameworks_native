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

#include <condition_variable>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <iostream>
#include <optional>

#include <hidl/Status.h>

namespace android {
namespace lshal {

static constexpr std::chrono::milliseconds IPC_CALL_WAIT{500};

class BackgroundTaskState {
public:
    explicit BackgroundTaskState(std::function<void(void)> &&func)
            : mFunc(std::move(func)) {}
    void waitStateTaken() {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondVar.wait(lock, [this](){ return this->mStateTaken; });
    }
    void notifyStateTaken() {
        std::unique_lock<std::mutex> lock(mMutex);
        mStateTaken = true;
        lock.unlock();
        mCondVar.notify_all();
    }
    void callFunc() {
        mFunc();
    }
    template<class C, class D>
    bool waitFinished(std::chrono::time_point<C, D> end) {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondVar.wait_until(lock, end, [this](){ return this->mFinished; });
        return mFinished;
    }
    void notifyFinished() {
        std::unique_lock<std::mutex> lock(mMutex);
        mFinished = true;
        lock.unlock();
        mCondVar.notify_all();
    }
private:
    std::mutex mMutex;
    std::condition_variable mCondVar;
    bool mStateTaken = false;
    bool mFinished = false;
    std::function<void(void)> mFunc;
};

void *callAndNotify(void *data) {

    // Take control of the shared state
    std::shared_ptr<BackgroundTaskState> state = *static_cast<std::shared_ptr<BackgroundTaskState>*>(data);

    // Notify main thread that the background thread has taken a strong reference to
    // the state object.
    state->notifyStateTaken();

    // call the slow function.
    state->callFunc();

    // Notify the main thread that the slow function has finished.
    state->notifyFinished();

    return nullptr;
}

template<class R, class P>
bool timeout(std::chrono::duration<R, P> delay, std::function<void(void)> &&func) {
    auto now = std::chrono::system_clock::now();
    auto state = std::make_shared<BackgroundTaskState>(std::move(func));
    pthread_t thread;
    if (pthread_create(&thread, nullptr, callAndNotify, &state)) {
        std::cerr << "FATAL: could not create background thread." << std::endl;
        return false;
    }
    // Wait until the background thread is started. This ensured that
    // the background thread does not access the stack of timeout() to get `state`
    // after timeout() has returned.
    state->waitStateTaken();

    // Wait for the background thread to execute the slow function, optionally abort.
    bool success = state->waitFinished(now + delay);

    if (!success) {
        pthread_kill(thread, SIGINT);
    }
    pthread_join(thread, nullptr);
    return success;
}

template<class R, class P, class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(std::chrono::duration<R, P> wait, const sp<I> &interfaceObject, Function &&func,
           Args &&... args) {
    using ::android::hardware::Status;
    typename std::result_of<Function(I *, Args...)>::type ret{Status::ok()};
    auto boundFunc = std::bind(std::forward<Function>(func),
            interfaceObject.get(), std::forward<Args>(args)...);
    bool success = timeout(wait, [&ret, &boundFunc] {
        ret = std::move(boundFunc());
    });
    if (!success) {
        return Status::fromStatusT(TIMED_OUT);
    }
    return ret;
}

template<class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(const sp<I> &interfaceObject, Function &&func, Args &&... args) {
    return timeoutIPC(IPC_CALL_WAIT, interfaceObject, func, args...);
}


}  // namespace lshal
}  // namespace android
