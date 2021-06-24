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

#define LOG_TAG "RpcRawTransport"
#include <log/log.h>

#include <binder/RpcTransportRaw.h>

#include "RpcState.h"

namespace android {

namespace {

// RpcTransport with TLS disabled.
class RpcTransportRaw : public RpcTransport {
public:
    explicit RpcTransportRaw(android::base::unique_fd socket) : mSocket(std::move(socket)) {}
    int send(const void *buf, int size) override {
        int ret = TEMP_FAILURE_RETRY(::send(mSocket.get(), buf, size, MSG_NOSIGNAL));
        if (ret < 0) {
            ALOGE("%s: send(): %s", __PRETTY_FUNCTION__, strerror(errno));
        }
        return ret;
    }
    int recv(void *buf, int size) override {
        int ret = TEMP_FAILURE_RETRY(::recv(mSocket.get(), buf, size, MSG_NOSIGNAL));
        if (ret < 0) {
            ALOGE("%s: recv(): %s", __PRETTY_FUNCTION__, strerror(errno));
        }
        return ret;
    }
    int peek(void *buf, int size) override {
        int ret = TEMP_FAILURE_RETRY(::recv(mSocket.get(), buf, size, MSG_PEEK | MSG_DONTWAIT));
        if (ret < 0) {
            LOG_RPC_DETAIL("%s: recv(): %s", __PRETTY_FUNCTION__, strerror(errno));
        }
        return ret;
    }
    bool pending() override { return false; }
    android::base::borrowed_fd pollSocket() const override { return mSocket; }

private:
    android::base::unique_fd mSocket;
};

// RpcTransportCtx with TLS disabled.
class RpcTransportCtxRaw : public RpcTransportCtx {
public:
    std::unique_ptr<RpcTransport> newTransport(android::base::unique_fd fd) const {
        return std::make_unique<RpcTransportRaw>(std::move(fd));
    }
};
} // namespace

std::unique_ptr<RpcTransportCtx> RpcTransportCtxFactoryRaw::newServerCtx() const {
    return std::make_unique<RpcTransportCtxRaw>();
}

std::unique_ptr<RpcTransportCtx> RpcTransportCtxFactoryRaw::newClientCtx() const {
    return std::make_unique<RpcTransportCtxRaw>();
}

bool RpcTransportCtxFactoryRaw::tlsEnabled() const {
    return false;
}

} // namespace android
