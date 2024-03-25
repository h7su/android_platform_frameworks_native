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

// Wraps the transport layer of RPC. Implementation uses Trusty IPC.

#pragma once

#include <memory>

#include <binder/RpcTransport.h>

namespace android {

#define MAX_SHMS 16

// RpcTransportCtxFactory for writing Trusty IPC clients in Android.
class RpcTransportCtxFactoryTipcAndroid : public RpcTransportCtxFactory {
public:
    static std::unique_ptr<RpcTransportCtxFactory> make();

    std::unique_ptr<RpcTransportCtx> newServerCtx() const override;
    std::unique_ptr<RpcTransportCtx> newClientCtx() const override;
    const char* toCString() const override;

private:
    RpcTransportCtxFactoryTipcAndroid() = default;
};

} // namespace android
