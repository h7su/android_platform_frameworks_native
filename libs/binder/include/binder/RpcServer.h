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

#include <android-base/unique_fd.h>
#include <binder/IBinder.h>
#include <binder/RpcConnection.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>

namespace android {

class Parcel;
struct RpcWireAddress;

// FIXME: actually support this setup, instead of just one.
// FIXME: document security model
// This represents a server of a pool of connections
class RpcServer : public RefBase {
public:
    // FIXME: what's the best way to expose these APIs? One per-type?
    // FIXME: request port numbers from context manager
    // sp<RpcServer> makeVsockServer(unsigned int port);
    static sp<RpcServer> makeUnixServer(const char* path);

    // FIXME: add way to limit who can retrieve this
    void setRootObject(const sp<IBinder>& binder);

    // FIXME: add methods (or setup constructors) to modify thread counts
    // status_t setMaxThreads(size_t count);
    // ??? addThread();

    // FIXME: rename and document contract, since it can return
    // FIXME: actually, completely remove in threadpool case?
    void join();

protected:
    ~RpcServer();
private:
    RpcServer();

    // FIXME: support a list of connctions
    base::unique_fd mFd;

    std::vector<sp<IBinder>> mServed;  // for lifetime
};
} // namespace android
