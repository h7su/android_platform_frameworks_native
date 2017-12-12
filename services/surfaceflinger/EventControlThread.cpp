/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "EventControlThread.h"
#include "SurfaceFlinger.h"

namespace android {

EventControlThread::EventControlThread(const sp<SurfaceFlinger>& flinger):
        mFlinger(flinger),
        mVsyncEnabled(false) {
}

void EventControlThread::setVsyncEnabled(bool enabled) {
    Mutex::Autolock lock(mMutex);
    mVsyncEnabled = enabled;
    mCond.signal();
}

bool EventControlThread::threadLoop() {
    enum class VsyncState {Unset, On, Off};
    auto currentVsyncState = VsyncState::Unset;

    while (true) {
        auto requestedVsyncState = VsyncState::On;
        {
            Mutex::Autolock lock(mMutex);
            requestedVsyncState =
                    mVsyncEnabled ? VsyncState::On : VsyncState::Off;
            while (currentVsyncState == requestedVsyncState) {
                status_t err = mCond.wait(mMutex);
                if (err != NO_ERROR) {
                    ALOGE("error waiting for new events: %s (%d)",
                          strerror(-err), err);
                    return false;
                }
                requestedVsyncState =
                        mVsyncEnabled ? VsyncState::On : VsyncState::Off;
            }
        }

        bool enable = requestedVsyncState == VsyncState::On;
#ifdef USE_HWC2
        mFlinger->setVsyncEnabled(HWC_DISPLAY_PRIMARY, enable);
#else
        mFlinger->eventControl(HWC_DISPLAY_PRIMARY,
                SurfaceFlinger::EVENT_VSYNC, enable);
#endif
        currentVsyncState = requestedVsyncState;
    }

    return false;
}

} // namespace android
