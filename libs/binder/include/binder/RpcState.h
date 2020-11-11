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
#include <binder/Parcel.h>
#include <binder/RpcAddress.h>
#include <binder/RpcConnection.h>

#include <map>

namespace android {

struct RpcCommand;

// FIXME: package private?
// FIXME: docs, justify global
// FIXME: make a lot of these functions private
class RpcState {
public:
    // FIXME: combine with process state or build similarly?
    static RpcState& self();

    // FIXME: do we need separate connection/fd args - need tests with
    // multi-threading/multiple connections to see
    status_t transact(const sp<RpcConnection>& connection,
                      const base::unique_fd& fd,
                      const RpcAddress* address,
                      uint32_t code,
                      const Parcel& data,
                      Parcel* reply,
                      uint32_t flags);
    status_t waitForReply(const sp<RpcConnection>& connection,
                          const base::unique_fd& fd,
                          Parcel* reply);
    status_t getAndExecuteCommand(const base::unique_fd& fd);

    // FIXME: document address lifetime, or switch address to be
    // owned by the binder
    const RpcAddress* attachBinder(const sp<IBinder>& binder);
    // FIXME: rename s/get/create/
    sp<IBinder> getOrLookupProxy(const sp<RpcConnection>& connection, RpcAddress&& address);

private:
    // FIXME: needing this function is a product of how we read packets
    status_t processServerCommand(const base::unique_fd& fd,
                                  const RpcCommand& command);

    // binders known by both sides of a connection
    // FIXME: synchronization
    // FIXME: avoid sp
    // FIXME: do not make copies of RpcAddress, since they will be large
    //     maybe it's best to keep here, so that sizeof(BpBinder) isn't too
    //     big
    // FIXME: unordered hash
    std::map<RpcAddress, sp<IBinder>> mBinders;
};

} // namespace android
