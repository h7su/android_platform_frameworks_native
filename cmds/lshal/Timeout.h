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
#include <condition_variable>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>

#include <hidl/Status.h>

namespace android {
namespace lshal {

enum class State : int32_t { INITIALIZED, STARTED, FINISHED, RETRIEVED };

// Check that state is not RETRIEVED.
void checkNotRetrieved(State state);

namespace {

constexpr std::chrono::milliseconds IPC_CALL_WAIT{500};

// A background task that wraps a function. The function takes no arguments
// and return some value. For functions with arguments, use std::bind.
template <class Function>
class BackgroundTask {
    using ReturnType = typename std::result_of<Function()>::type;

public:
    // public so it can be accessed by std::make_shared
    explicit BackgroundTask(Function&& func) : mFunc(std::move(func)) {}

    // Runs the given function in a background thread with the given timeout. If
    // deadline has reached before the function returns, std::nullopt is
    // returned.
    template <class R, class P>
    static std::optional<ReturnType> runWithTimeout(std::chrono::duration<R, P> delay,
                                                    Function&& func) {
        auto now = std::chrono::system_clock::now();
        auto task = std::make_shared<BackgroundTask>(std::move(func));
        pthread_t thread;
        if (pthread_create(&thread, nullptr, BackgroundTask::callAndNotify, &task)) {
            std::cerr << "FATAL: could not create background thread." << std::endl;
            return std::nullopt;
        }
        // Wait until the background thread is started. This ensured that
        // the background thread does not access the stack of timeout() to get `task`
        // after timeout() has returned.
        task->waitStarted();

        // Wait for the background thread to execute the slow function, optionally abort.
        auto ret = task->waitFinishedAndRetrieve(now + delay);

        if (!ret.has_value()) {
            pthread_kill(thread, SIGINT);
        }
        pthread_join(thread, nullptr);
        return ret;
    }

private:
    // Entry point of the background thread. Interpret data as a shared_ptr<BackgroundTask>*
    // and execute the slow function stored in it.
    static void* callAndNotify(void* data) {
        // Background thread holds a reference of the shared task object
        std::shared_ptr<BackgroundTask> task = *static_cast<std::shared_ptr<BackgroundTask>*>(data);

        // Notify main thread that the background thread has taken a strong reference to
        // the task object. The main thread may check time after this point.
        {
            std::lock_guard<std::mutex> lock(task->mMutex);
            task->mState = State::STARTED;
        }
        task->mCondVar.notify_all();

        // call the slow function.
        auto ret = task->mFunc();

        // Notify the main thread that the slow function has finished.
        {
            std::unique_lock<std::mutex> lock(task->mMutex);
            task->mState = State::FINISHED;
            task->mRet = std::move(ret);
        }
        task->mCondVar.notify_all();

        return nullptr;
    }

    // Called by the main thread. Wait for the background thread to start.
    void waitStarted() {
        std::unique_lock<std::mutex> lock(mMutex);
        mCondVar.wait(lock, [this]() { return this->mState >= State::STARTED; });
    }

    // Called by the main thread. Wait for the background function to finish or
    // the given time has reached. Then return the function value.
    template <class C, class D>
    std::optional<ReturnType> waitFinishedAndRetrieve(std::chrono::time_point<C, D> end) {
        std::unique_lock<std::mutex> lock(mMutex);
        checkNotRetrieved(mState);
        mCondVar.wait_until(lock, end, [this]() { return this->mState == State::FINISHED; });
        mState = State::RETRIEVED;
        return std::move(mRet);
    }

    std::mutex mMutex;
    std::condition_variable mCondVar;
    State mState = State::INITIALIZED;
    std::optional<ReturnType> mRet;
    Function mFunc;
};

} // namespace

// Call function on interfaceObject and wait for result until the given timeout has reached.
template<class R, class P, class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(std::chrono::duration<R, P> wait, const sp<I> &interfaceObject, Function &&func,
           Args &&... args) {
    using ::android::hardware::Status;

    auto boundFunc = std::bind(std::forward<Function>(func),
            interfaceObject.get(), std::forward<Args>(args)...);

    auto ret = BackgroundTask<decltype(boundFunc)>::runWithTimeout(wait, std::move(boundFunc));
    if (!ret.has_value()) {
        return Status::fromStatusT(TIMED_OUT);
    }
    return std::move(*ret);
}

// Call function on interfaceObject and wait for result until the default timeout has reached.
template<class Function, class I, class... Args>
typename std::result_of<Function(I *, Args...)>::type
timeoutIPC(const sp<I> &interfaceObject, Function &&func, Args &&... args) {
    return timeoutIPC(IPC_CALL_WAIT, interfaceObject, func, args...);
}

} // namespace lshal
} // namespace android
