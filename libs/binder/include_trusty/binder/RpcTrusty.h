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

#include <binder/IBinder.h>

namespace android {

sp<IBinder> RpcTrustyConnect(const char* device, const char* port);

/**
 * if numIncomingThreads > 0; 'shutdown' on this session must also be called.
 * Otherwise, a threadpool will leak.
 */
sp<RpcSession> RpcTrustyConnectWithCallbackSession(const char* device, const char* port,
                                                   size_t numIncomingThreads);

} // namespace android
