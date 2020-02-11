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

#include <android-base/macros.h>
#include <hwbinder/Parcel.h>

#include <string>

#include "Command.h"
#include "utils.h"

namespace android {
namespace lshal {

class Lshal;

class CallCommand : public Command {
public:
    explicit CallCommand(Lshal &lshal) : Command(lshal) {}
    ~CallCommand() = default;
    Status main(const Arg &arg) override;
    void usage() const override;
    std::string getSimpleDescription() const override;
    std::string getName() const override;

private:
    Status parseArgs(const Arg &arg);

    std::string mInterfaceDescriptor;
    std::string mInterfaceInstance;

    uint32_t mCode;
    hardware::Parcel mData;

    DISALLOW_COPY_AND_ASSIGN(CallCommand);
};

} // namespace lshal
} // namespace android
