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

#define LOG_TAG "FdTrigger"
#include <log/log.h>

#include <poll.h>

#include <android-base/macros.h>

#include "FdTrigger.h"
namespace android {

std::unique_ptr<FdTrigger> FdTrigger::make() {
    auto ret = std::make_unique<FdTrigger>();
    if (!android::base::Pipe(&ret->mRead, &ret->mWrite)) {
        ALOGE("Could not create pipe %s", strerror(errno));
        return nullptr;
    }
    return ret;
}

void FdTrigger::trigger() {
    mWrite.reset();
}

bool FdTrigger::isTriggered() {
    return mWrite == -1;
}

status_t FdTrigger::triggerablePoll(base::borrowed_fd fd, int16_t event) {
    LOG_ALWAYS_FATAL_IF(event == 0, "triggerablePoll %d with event 0 is not allowed", fd.get());
    pollfd pfd[]{{.fd = fd.get(), .events = static_cast<int16_t>(event), .revents = 0},
                 {.fd = mRead.get(), .events = 0, .revents = 0}};
    int ret = TEMP_FAILURE_RETRY(poll(pfd, arraysize(pfd), -1));
    if (ret < 0) {
        return -errno;
    }
    LOG_ALWAYS_FATAL_IF(ret == 0, "poll(%d) returns 0 with infinite timeout", fd.get());
    // At least one FD has events. Check them.
    // Detect explicit trigger(): DEAD_OBJECT
    if (pfd[1].revents & POLLHUP) {
        return DEAD_OBJECT;
    }
    // (pfd[0].revents & event) is the only success condition (note event != 0).
    // All other cases, including POLLERR / POLLNVAL in either FD, results in DEAD_OBJECT.
    return pfd[0].revents & event ? OK : DEAD_OBJECT;
}

} // namespace android
