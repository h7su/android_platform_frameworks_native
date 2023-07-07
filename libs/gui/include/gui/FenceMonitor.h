/*
 * Copyright 2023 The Android Open Source Project
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

#include <cstdint>
#include <deque>
#include <mutex>

#include <ui/Fence.h>

namespace android::gui {

class FenceMonitor {
public:
    explicit FenceMonitor(const char* name);
    void queueFence(const sp<Fence>& fence);

private:
    void loop();
    void threadLoop();

    const char* mName;
    uint32_t mFencesQueued;
    uint32_t mFencesSignaled;
    std::deque<sp<Fence>> mQueue;
    std::condition_variable mCondition;
    std::mutex mMutex;
};

} // namespace android::gui