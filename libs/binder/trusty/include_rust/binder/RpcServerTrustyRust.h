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
#pragma once

#include <lib/tipc/tipc_srv.h>
#include <lk/compiler.h>

__BEGIN_CDECLS

struct AIBinder;
struct RpcServerTrustyRust;

struct RpcServerTrustyRust* RpcServerTrustyRust_new(struct AIBinder*);
struct RpcServerTrustyRust* RpcServerTrustyRust_newPerSession(struct AIBinder* (*)(const void*,
                                                                                   size_t, void*),
                                                              void*);
void RpcServerTrustyRust_delete(struct RpcServerTrustyRust*);
int RpcServerTrustyRust_handleConnect(struct RpcServerTrustyRust*, handle_t, const struct uuid*,
                                      void**);
int RpcServerTrustyRust_handleMessage(void*);
void RpcServerTrustyRust_handleDisconnect(void*);
void RpcServerTrustyRust_handleChannelCleanup(void*);

__END_CDECLS
