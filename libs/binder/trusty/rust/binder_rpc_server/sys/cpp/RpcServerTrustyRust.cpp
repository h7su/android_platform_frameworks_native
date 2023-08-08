/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <android/binder_libbinder.h>
#include <binder/RpcServerTrusty.h>
#include <binder/RpcServerTrustyRust.h>
#include <binder/RpcSession.h>
#include <binder/RpcTransportTipcTrusty.h>

using android::RpcServer;
using android::RpcServerTrusty;
using android::RpcSession;
using android::RpcTransportCtxFactoryTipcTrusty;
using android::sp;
using android::wp;

struct RpcServerTrustyRust {
    sp<RpcServer> mRpcServer;

    RpcServerTrustyRust() = delete;
    RpcServerTrustyRust(sp<RpcServer> rpcServer) : mRpcServer(std::move(rpcServer)) {}
};

RpcServerTrustyRust* RpcServerTrustyRust_new(AIBinder* service) {
    auto rpcTransportCtxFactory = RpcTransportCtxFactoryTipcTrusty::make();
    if (rpcTransportCtxFactory == nullptr) {
        return nullptr;
    }

    auto ctx = rpcTransportCtxFactory->newServerCtx();
    if (ctx == nullptr) {
        return nullptr;
    }

    auto rpcServer = RpcServerTrusty::makeRpcServer(std::move(ctx));
    if (rpcServer == nullptr) {
        return nullptr;
    }
    rpcServer->setRootObject(AIBinder_toPlatformBinder(service));

    return new (std::nothrow) RpcServerTrustyRust(std::move(rpcServer));
}

RpcServerTrustyRust* RpcServerTrustyRust_newPerSession(AIBinder* (*cb)(const void*, size_t, void*),
                                                       void* arg) {
    auto rpcTransportCtxFactory = RpcTransportCtxFactoryTipcTrusty::make();
    if (rpcTransportCtxFactory == nullptr) {
        return nullptr;
    }

    auto ctx = rpcTransportCtxFactory->newServerCtx();
    if (ctx == nullptr) {
        return nullptr;
    }

    auto rpcServer = RpcServerTrusty::makeRpcServer(std::move(ctx));
    if (rpcServer == nullptr) {
        return nullptr;
    }

    rpcServer->setPerSessionRootObject(
            [cb, arg](wp<RpcSession> session, const void* addrPtr, size_t len) {
                auto* aib = (*cb)(addrPtr, len, arg);
                auto b = AIBinder_toPlatformBinder(aib);

                // We have a new sp<IBinder> backed by the same binder, so we can
                // finally release the AIBinder* from the callback
                AIBinder_decStrong(aib);

                return b;
            });

    return new (std::nothrow) RpcServerTrustyRust(std::move(rpcServer));
}

void RpcServerTrustyRust_delete(RpcServerTrustyRust* rstr) {
    delete rstr;
}

int RpcServerTrustyRust_handleConnect(RpcServerTrustyRust* rstr, handle_t chan, const uuid* peer,
                                      void** ctx_p) {
    return RpcServerTrusty::handleConnectInternal(rstr->mRpcServer.get(), chan, peer, ctx_p);
}

int RpcServerTrustyRust_handleMessage(void* ctx) {
    return RpcServerTrusty::handleMessageInternal(ctx);
}

void RpcServerTrustyRust_handleDisconnect(void* ctx) {
    RpcServerTrusty::handleDisconnectInternal(ctx);
}

void RpcServerTrustyRust_handleChannelCleanup(void* ctx) {
    RpcServerTrusty::handleChannelCleanup(ctx);
}
