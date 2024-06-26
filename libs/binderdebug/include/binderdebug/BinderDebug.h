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

#include <utils/Errors.h>

#include <map>
#include <vector>

namespace android {

struct BinderPidInfo {
    std::map<uint64_t, std::vector<pid_t>> refPids; // cookie -> processes which hold binder
    uint32_t threadUsage;                           // number of threads in use
    uint32_t threadCount;                           // number of threads total
};

enum class BinderDebugContext {
    BINDER,
    HWBINDER,
    VNDBINDER,
};

/**
 * pid is the pid of the service
 */
status_t getBinderPidInfo(BinderDebugContext context, pid_t pid, BinderPidInfo* pidInfo);
/**
 * pid is typically the pid of this process that is making the query
 */
status_t getBinderClientPids(BinderDebugContext context, pid_t pid, pid_t servicePid,
                             int32_t handle, std::vector<pid_t>* pids);

/**
 * Get the transactions for a given process from /dev/binderfs/binder_logs/transactions
 * Return: OK if the file was found and the pid was found in the file.
 *         -errno if there was an issue opening the file
 *         NAME_NOT_FOUND if the pid wasn't found in the file
 */
status_t getBinderTransactions(pid_t pid, std::string& transactionOutput);

} // namespace  android
