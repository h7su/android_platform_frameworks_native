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

#ifndef FUZZ_TESTINPUTLISTENER_H
#define FUZZ_TESTINPUTLISTENER_H

#include "InputListener.h"

namespace android {

// --- TestInputListener ---

class TestInputListener : public InputListenerInterface {
private:
    std::vector<NotifyConfigurationChangedArgs> mNotifyConfigurationChangedArgsQueue;
    std::vector<NotifyDeviceResetArgs> mNotifyDeviceResetArgsQueue;
    std::vector<NotifyKeyArgs> mNotifyKeyArgsQueue;
    std::vector<NotifyMotionArgs> mNotifyMotionArgsQueue;
    std::vector<NotifySwitchArgs> mNotifySwitchArgsQueue;

public:
    TestInputListener();
    virtual ~TestInputListener();

    void assertNotifyConfigurationChangedWasCalled(
            NotifyConfigurationChangedArgs* outEventArgs = nullptr);

    void assertNotifyConfigurationChangedWasNotCalled();

    void assertNotifyDeviceResetWasCalled(NotifyDeviceResetArgs* outEventArgs = nullptr);

    void assertNotifyDeviceResetWasNotCalled();

    void assertNotifyKeyWasCalled(NotifyKeyArgs* outEventArgs = nullptr);

    void assertNotifyKeyWasNotCalled();

    void assertNotifyMotionWasCalled(NotifyMotionArgs* outEventArgs = nullptr);

    void assertNotifyMotionWasNotCalled();

    void assertNotifySwitchWasCalled(NotifySwitchArgs* outEventArgs = nullptr);

private:
    virtual void notifyConfigurationChanged(const NotifyConfigurationChangedArgs* args);

    virtual void notifyDeviceReset(const NotifyDeviceResetArgs* args);

    virtual void notifyKey(const NotifyKeyArgs* args);

    virtual void notifyMotion(const NotifyMotionArgs* args);

    virtual void notifySwitch(const NotifySwitchArgs* args);
};

} // namespace android

#endif // FUZZ_TESTINPUTLISTENER_H
