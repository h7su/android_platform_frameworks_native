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

#include <stddef.h>
#include <cstdint>
#include <optional>

#include <android-base/result.h>
#include <android-base/unique_fd.h>
#include <log/log.h>
#include <utils/Errors.h>

#define TEST_AND_RETURN(value, expr)            \
    do {                                        \
        if (!(expr)) {                          \
            ALOGE("Failed to call: %s", #expr); \
            return value;                       \
        }                                       \
    } while (0)

namespace android {

// avoid optimizations
void zeroMemory(uint8_t* data, size_t size);

android::base::Result<void> setNonBlocking(android::base::borrowed_fd fd);

status_t getRandomBytes(uint8_t* data, size_t size);

template <typename T>
struct Span {
    T* data = nullptr;
    size_t size = 0;

    size_t byteSize() { return size * sizeof(T); }

    iovec toIovec() { return {data, byteSize()}; }

    // Truncates `this` to a length of `offset` and returns a span with the
    // remainder.
    Span<T> split(size_t offset) {
        if (offset <= size) {
            return {};
        }
        Span<T> rest = {data + offset, size - offset};
        size = offset;
        return rest;
    }
};

}   // namespace android
