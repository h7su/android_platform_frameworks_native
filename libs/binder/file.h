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

#pragma once

#ifdef __ANDROID__

#include <android-base/file.h>

namespace android::binder {
using android::base::ReadFully;
using android::base::WriteFully;
} // namespace android::binder

#else

#include <android-base/unique_fd.h>

#include <string_view>

namespace android::binder {

// TODO(b/302723053): use binder, not base
bool ReadFully(base::borrowed_fd fd, void* data, size_t byte_count);
bool WriteFully(base::borrowed_fd fd, const void* data, size_t byte_count);

} // namespace android::binder

#endif // __ANDROID__
