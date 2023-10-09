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

#if defined(TRUSTY_USERSPACE)
#include <openssl/rand.h>
#include <trusty_ipc.h>
#else
#include <lib/rand/rand.h>
#endif

#include <binder/RpcTransportTipcTrusty.h>
#include <log/log.h>
#include <trusty_log.h>

#include "../OS.h"
#include "TrustyStatus.h"

#include <cstdarg>

using android::binder::borrowed_fd;
using android::binder::unique_fd;

namespace android::binder::os {

void trace_begin(uint64_t, const char*) {}

void trace_end(uint64_t) {}

uint64_t GetThreadId() {
    return 0;
}

bool report_sysprop_change() {
    return false;
}

status_t setNonBlocking(borrowed_fd /*fd*/) {
    // Trusty IPC syscalls are all non-blocking by default.
    return OK;
}

status_t getRandomBytes(uint8_t* data, size_t size) {
#if defined(TRUSTY_USERSPACE)
    int res = RAND_bytes(data, size);
    return res == 1 ? OK : UNKNOWN_ERROR;
#else
    int res = rand_get_bytes(data, size);
    return res == 0 ? OK : UNKNOWN_ERROR;
#endif // TRUSTY_USERSPACE
}

status_t dupFileDescriptor(int oldFd, int* newFd) {
    int res = dup(oldFd);
    if (res < 0) {
        return statusFromTrusty(res);
    }

    *newFd = res;
    return OK;
}

std::unique_ptr<RpcTransportCtxFactory> makeDefaultRpcTransportCtxFactory() {
    return RpcTransportCtxFactoryTipcTrusty::make();
}

ssize_t sendMessageOnSocket(
        const RpcTransportFd& /* socket */, iovec* /* iovs */, int /* niovs */,
        const std::vector<std::variant<unique_fd, borrowed_fd>>* /* ancillaryFds */) {
    errno = ENOTSUP;
    return -1;
}

ssize_t receiveMessageFromSocket(
        const RpcTransportFd& /* socket */, iovec* /* iovs */, int /* niovs */,
        std::vector<std::variant<unique_fd, borrowed_fd>>* /* ancillaryFds */) {
    errno = ENOTSUP;
    return -1;
}

} // namespace android::binder::os

void android_log_stub_print(int priority [[maybe_unused]], const char* tag, const char* fmt, ...) {
    const auto preamble = (tag == nullptr || tag[0] == '\0') ? "libbinder" : "libbinder-";
#ifdef TRUSTY_USERSPACE
    // since there is no _tlog variant that takes va_list, we have to print message to buffer first
    thread_local char thread_buffer[256];
    char* buffer = thread_buffer;
    ssize_t buffer_size = sizeof(buffer);

    auto pos = snprintf(buffer, buffer_size, "%s%s", preamble, tag);
    if (pos < 0 || pos >= buffer_size) {
        _tlog("%s%s: can't generate log message prefix", preamble, tag);
        return;
    }
    buffer += pos;
    buffer_size -= pos;

    va_list args;
    va_start(args, fmt);
    pos = vsnprintf(buffer, buffer_size, fmt, args);
    va_end(args);
    if (pos < 0 || pos >= buffer_size) {
        _tlog("%s%s: can't generate log message", preamble, tag);
        return;
    }

    buffer[pos] = '\0';
    _tlog("%s", buffer);
#else
    // mapping taken from kernel trusty_log.h (TLOGx)
    int kernelLogLevel;
    if (priority <= ANDROID_LOG_DEBUG) {
        kernelLogLevel = LK_DEBUGLEVEL_ALWAYS;
    } else if (priority == ANDROID_LOG_INFO) {
        kernelLogLevel = LK_DEBUGLEVEL_SPEW;
    } else if (priority == ANDROID_LOG_WARN) {
        kernelLogLevel = LK_DEBUGLEVEL_INFO;
    } else if (priority == ANDROID_LOG_ERROR) {
        kernelLogLevel = LK_DEBUGLEVEL_CRITICAL;
    } else { /* priority >= ANDROID_LOG_FATAL */
        kernelLogLevel = LK_DEBUGLEVEL_CRITICAL;
    }

    // from _dprintf_internal
    if (kernelLogLevel <= LK_LOGLEVEL) {
        va_list args;
        va_start(args, fmt);
        // Kernel logs to stdout, while host utils to stderr. Let's stick with the latter.
        fprintf(stderr, "%s%s", preamble, tag);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
#endif
}
