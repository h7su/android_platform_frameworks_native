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

#pragma once

#include <stdint.h>
#include <sys/types.h>

#include <gui/DisplayCaptureArgs.h>
#include <gui/SpHash.h>
#include <unordered_set>

namespace android::gui {

struct LayerCaptureArgs : CaptureArgs {
    sp<IBinder> layerHandle;
    std::unordered_set<sp<IBinder>, SpHash<IBinder>> excludeHandles;
    bool childrenOnly{false};

    status_t writeToParcel(Parcel* output) const override;
    status_t readFromParcel(const Parcel* input) override;
};

}; // namespace android::gui
