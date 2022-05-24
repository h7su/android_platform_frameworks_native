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

#define LOG_TAG "RpcServerTrusty"

#include <binder/Parcel.h>
#include <binder/RpcServer.h>
#include <binder/RpcServerTrusty.h>
#include <binder/RpcThreads.h>
#include <binder/RpcTransportTrusty.h>
#include <log/log.h>

#include "../FdTrigger.h"
#include "../RpcState.h"
#include "Utils.h"

namespace android {

sp<RpcServerTrusty> RpcServerTrusty::make(
        std::unique_ptr<RpcTransportCtxFactory> rpcTransportCtxFactory) {
    // Default is without TLS.
    if (rpcTransportCtxFactory == nullptr)
        rpcTransportCtxFactory = RpcTransportCtxFactoryTrusty::make();
    auto ctx = rpcTransportCtxFactory->newServerCtx();
    if (ctx == nullptr) return nullptr;
    return sp<RpcServerTrusty>::make(std::move(ctx));
}

status_t RpcServerTrusty::setupTrustyServer(tipc_hset* handleSet, std::string portName,
                                            std::shared_ptr<const PortAcl> portAcl,
                                            size_t msgMaxSize) {
    mPortName = std::move(portName);
    mTipcPort.name = mPortName.c_str();
    mTipcPort.msg_max_size = msgMaxSize;
    mTipcPort.msg_queue_len = 6; // Three each way
    mTipcPort.priv = this;

    if (portAcl) {
        mPortAcl = std::move(portAcl);

        // Initialize the array of pointers to uuids.
        // The pointers in mUuidPtrs should stay valid across moves of
        // RpcServerTrusty (the addresses of a std::vector's elements
        // shouldn't change when the vector is moved), but would be invalidated
        // by a copy which is why we disable the copy constructor and assignment
        // operator for RpcServerTrusty.
        auto numUuids = mPortAcl->uuids.size();
        mUuidPtrs.resize(numUuids);
        for (size_t i = 0; i < numUuids; i++) {
            mUuidPtrs[i] = &mPortAcl->uuids[i];
        }

        // Copy the contents of portAcl into the tipc_port_acl structure that we
        // pass to tipc_add_service
        mTipcPortAcl.flags = mPortAcl->flags;
        mTipcPortAcl.uuid_num = numUuids;
        mTipcPortAcl.uuids = mUuidPtrs.data();
        mTipcPortAcl.extra_data = mPortAcl->extraData;

        mTipcPort.acl = &mTipcPortAcl;
    } else {
        mTipcPort.acl = nullptr;
    }

    int rc = tipc_add_service(handleSet, &mTipcPort, 1, 1, &kTipcOps);
    return statusFromTrusty(rc);
}

int RpcServerTrusty::handleConnect(const tipc_port* port, handle_t chan, const uuid* peer,
                                   void** ctx_p) {
    auto* server = reinterpret_cast<RpcServerTrusty*>(const_cast<void*>(port->priv));
    server->mShutdownTrigger = FdTrigger::make();
    server->mConnectingThreads[rpc_this_thread::get_id()] = RpcMaybeThread();

    int rc = NO_ERROR;
    auto joinFn = [&](sp<RpcSession>&& session, RpcSession::PreJoinSetupResult&& result) {
        if (result.status != OK) {
            rc = statusToTrusty(result.status);
            return;
        }

        /* Save the session for easy access */
        *ctx_p = session.get();
    };

    base::unique_fd clientFd(chan);
    std::array<uint8_t, kRpcAddressSize> addr;
    constexpr size_t addrLen = sizeof(*peer);
    memcpy(addr.data(), peer, addrLen);
    establishConnection(sp<RpcServer>::fromExisting(server), std::move(clientFd), addr, addrLen,
                        joinFn);

    return rc;
}

int RpcServerTrusty::handleMessage(const tipc_port* port, handle_t chan, void* ctx) {
    auto* session = reinterpret_cast<RpcSession*>(ctx);
    status_t status = session->state()->drainCommands(session->mConnections.mIncoming[0], session,
                                                      RpcState::CommandType::ANY);
    if (status != OK) {
        LOG_RPC_DETAIL("Binder connection thread closing w/ status %s",
                       statusToString(status).c_str());
    }

    return NO_ERROR;
}

void RpcServerTrusty::handleDisconnect(const tipc_port* port, handle_t chan, void* ctx) {}

void RpcServerTrusty::handleChannelCleanup(void* ctx) {
    auto* session = reinterpret_cast<RpcSession*>(ctx);
    auto& connection = session->mConnections.mIncoming[0];
    LOG_ALWAYS_FATAL_IF(!session->removeIncomingConnection(connection),
                        "bad state: connection object guaranteed to be in list");
}

} // namespace android
