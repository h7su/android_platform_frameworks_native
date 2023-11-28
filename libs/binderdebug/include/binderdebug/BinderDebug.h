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

/**
 * Filled by getBinderTransactions(pid_t pid, BinderTransactionInfo& trInfo), see function header
 * description.
 */
struct BinderTransactionInfo {
    std::vector<pid_t> scannedPids;   // The pids having their binder info files scanned.
    std::vector<std::string> trLines; // Lines with outgoing, incoming or pending binder
                                      // transaction from scanned binder info files.
};

enum class BinderDebugContext {
    BINDER,
    HWBINDER,
    VNDBINDER,
    ALLBINDERS, // All binder info files, regardless context, will be scanned.
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
 * On return, trInfo is filled with binder transaction information from Android binderfs filesystem.
 *
 * Starting with given pid, binder info files for all to-pids in outgoing transactions are
 * scanned recursively.
 * From the scanned binder info files, all lines with binder transactions is added to trInfo
 * All the pids that had their binder files scanned are added to trInfo
 * Returns status_t from <utils/Errors.h>, OK is 0.
 */
status_t getBinderTransactions(BinderDebugContext context, pid_t pid,
                               BinderTransactionInfo& trInfo);

} // namespace  android
