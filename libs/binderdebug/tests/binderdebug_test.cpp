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

#include <binder/Binder.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binderdebug/BinderDebug.h>
#include <gtest/gtest.h>

namespace android {

TEST_F(BinderDebugTests, BinderPid) {
    BinderPidInfo pidInfo;
    android::ProcessState::self()->setThreadPoolMaxThreadCount(1);
    android::defaultServiceManager()->addService(String16("binderdebug"), new android::BBinder);
    ProcessState::self()->startThreadPool();
    const auto& status = getBinderPidInfo("binder", getpid(), &pidInfo);
    ASSERT_EQ(status, OK);
    // There should be one referenced PID for servicemanager
    EXPECT_TRUE(!pidInfo.refPids.empty());
}

TEST_F(BinderDebugTests, BinderThreads) {
    BinderPidInfo pidInfo;
    android::ProcessState::self()->setThreadPoolMaxThreadCount(1);
    android::defaultServiceManager()->addService(String16("binderdebug"), new android::BBinder);
    ProcessState::self()->startThreadPool();
    const auto& status = getBinderPidInfo("binder", getpid(), &pidInfo);
    ASSERT_EQ(status, OK);
    EXPECT_EQ(pidInfo.threadUsage, 0);
    EXPECT_EQ(pidInfo.threadCount, 2);
}

// FIXME why is main not being compiled/called?
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    android::ProcessState::self()->setThreadPoolMaxThreadCount(1);
    android::defaultServiceManager()->addService(String16("binderdebug"), new android::BBinder);
    ProcessState::self()->startThreadPool();

    return RUN_ALL_TESTS();
}

} // namespace  android
