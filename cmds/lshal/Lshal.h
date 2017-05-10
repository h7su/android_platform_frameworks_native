/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef FRAMEWORK_NATIVE_CMDS_LSHAL_LSHAL_H_
#define FRAMEWORK_NATIVE_CMDS_LSHAL_LSHAL_H_

#include <iostream>
#include <string>

#include <android-base/macros.h>
#include <android/hidl/manager/1.0/IServiceManager.h>
#include <utils/StrongPointer.h>

#include "NullableOStream.h"
#include "MockableServiceManager.h"
#include "utils.h"

namespace android {
namespace lshal {

class Lshal {
public:
    Lshal();
    // for testing purposes
    Lshal(std::ostream &out, std::ostream &err,
            sp<MockableServiceManager> serviceManager,
            sp<MockableServiceManager> passthroughManager);
    Status main(const Arg &arg);
    void usage(const std::string &command = "") const;
    NullableOStream<std::ostream> err() const;
    NullableOStream<std::ostream> out() const;
    const sp<MockableServiceManager> &serviceManager() const;
    const sp<MockableServiceManager> &passthroughManager() const;

    Status emitDebugInfo(
            const std::string &interfaceName,
            const std::string &instanceName,
            const std::vector<std::string> &options,
            std::ostream &out) const;
private:
    Status parseArgs(const Arg &arg);
    std::string mCommand;
    Arg mCmdArgs;
    NullableOStream<std::ostream> mErr;
    NullableOStream<std::ostream> mOut;
    sp<MockableServiceManager> mServiceManager;
    sp<MockableServiceManager> mPassthroughManager;

    DISALLOW_COPY_AND_ASSIGN(Lshal);
};

}  // namespace lshal
}  // namespace android

#endif  // FRAMEWORK_NATIVE_CMDS_LSHAL_LSHAL_H_
