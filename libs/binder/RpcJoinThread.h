/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include <mutex>
#include <thread>

#include <android-base/unique_fd.h>
#include <binder/RpcServer.h>

namespace android {

// Wrapper of a thread that solely calls RpcServer::join().
struct RpcJoinThread {
    // Terminates join().
    ~RpcJoinThread();

    // Configure RpcServer with |rootObject|, |maxRpcThreads| threads and |socketFd|. Starts the
    // join thread.
    status_t initialize(const sp<IBinder>& rootObject, size_t maxRpcThreads,
                        android::base::unique_fd socketFd);

    // Configure RpcServer with |maxRpcThreads| threads.
    void setMaxThreads(size_t maxRpcThreads);

private:
    void run();

    std::unique_ptr<RpcServer::Pipe> mPipe;
    sp<RpcServer> mRpcServer;
    std::unique_ptr<std::thread> mThread;
};
} // namespace android
