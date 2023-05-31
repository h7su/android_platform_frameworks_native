/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include "OS.h"

#if defined(__APPLE__)
#error "This module is not intended for MacOS"
#endif
#if defined(_WIN32)
#error "This module is not intended for Windows"
#endif

namespace android::binder::os {

uint64_t GetThreadId() {
    return gettid();
}

bool report_sysprop_change() {
    return false;
}

} // namespace android::binder::os
