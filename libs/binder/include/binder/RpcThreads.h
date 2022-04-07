/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef BINDER_RPC_NO_THREADS
#include <thread>
#endif

namespace android {

#ifdef BINDER_RPC_NO_THREADS
class RpcMutex {};

class RpcMutexUniqueLock {
public:
    RpcMutexUniqueLock(RpcMutex&) {}
    void unlock() {}
};

class RpcMutexLockGuard {
public:
    RpcMutexLockGuard(RpcMutex&) {}
};
#else
using RpcMutex = std::mutex;
using RpcMutexUniqueLock = std::unique_lock<std::mutex>;
using RpcMutexLockGuard = std::lock_guard<std::mutex>;
#endif

} // namespace android
