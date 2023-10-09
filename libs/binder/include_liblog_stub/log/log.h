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

#include <cstdio>
#include <cstdlib>

extern "C" {

// TODO: use original log levels?
#define BINDER_LOG_LEVEL_NONE 0
#define BINDER_LOG_LEVEL_NORMAL 1
#define BINDER_LOG_LEVEL_VERBOSE 2

// TODO: set through Android.bp?
#ifndef BINDER_LOG_LEVEL
#define BINDER_LOG_LEVEL BINDER_LOG_LEVEL_NORMAL
#endif // BINDER_LOG_LEVEL

inline void __android_log_stub_ignore(...) {}
#define __ANDROID_LOG_STUB_IGNORE(...)          \
    while (0) {                                 \
        __android_log_stub_ignore(__VA_ARGS__); \
    }

// TODO: this never logs
#define ALOG(level, ...) __ANDROID_LOG_STUB_IGNORE(__VA_ARGS__)

#if BINDER_LOG_LEVEL >= BINDER_LOG_LEVEL_NORMAL
#define ALOGD(fmt, ...) std::fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define ALOGI(fmt, ...) std::fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define ALOGW(fmt, ...) std::fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define ALOGE(fmt, ...) std::fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else // BINDER_LOG_LEVEL >= BINDER_LOG_LEVEL_NORMAL
#define ALOGD(fmt, ...) __ANDROID_LOG_STUB_IGNORE(__VA_ARGS__)
#define ALOGI(fmt, ...) __ANDROID_LOG_STUB_IGNORE(__VA_ARGS__)
#define ALOGW(fmt, ...) __ANDROID_LOG_STUB_IGNORE(__VA_ARGS__)
#define ALOGE(fmt, ...) __ANDROID_LOG_STUB_IGNORE(__VA_ARGS__)
#endif // BINDER_LOG_LEVEL >= BINDER_LOG_LEVEL_NORMAL

#if BINDER_LOG_LEVEL >= BINDER_LOG_LEVEL_VERBOSE
#define IF_ALOGV() if (true)
#define ALOGV(fmt, ...) std::fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#else // BINDER_LOG_LEVEL >= BINDER_LOG_LEVEL_VERBOSE
#define IF_ALOGV() if (false)
#define ALOGV(fmt, ...) __ANDROID_LOG_STUB_IGNORE(__VA_ARGS__)
#endif // BINDER_LOG_LEVEL >= BINDER_LOG_LEVEL_VERBOSE

#define ALOGI_IF(cond, ...)                \
    do {                                   \
        if (cond) {                        \
            ALOGI(#cond ": " __VA_ARGS__); \
        }                                  \
    } while (0)
#define ALOGE_IF(cond, ...)                \
    do {                                   \
        if (cond) {                        \
            ALOGE(#cond ": " __VA_ARGS__); \
        }                                  \
    } while (0)
#define ALOGW_IF(cond, ...)                \
    do {                                   \
        if (cond) {                        \
            ALOGW(#cond ": " __VA_ARGS__); \
        }                                  \
    } while (0)

#define LOG_FATAL(fmt, ...)                                            \
    do {                                                               \
        std::fprintf(stderr, "fatal error: " fmt "\n", ##__VA_ARGS__); \
        std::abort();                                                  \
    } while (0)
#define LOG_FATAL_IF(cond, ...)                \
    do {                                       \
        if (cond) {                            \
            LOG_FATAL(#cond ": " __VA_ARGS__); \
        }                                      \
    } while (0)

#define LOG_ALWAYS_FATAL LOG_FATAL
#define LOG_ALWAYS_FATAL_IF LOG_FATAL_IF

#define ALOG_ASSERT(cond, ...) LOG_FATAL_IF(!(cond), ##__VA_ARGS__)

#define android_errorWriteLog(tag, subTag)                                              \
    do {                                                                                \
        std::fprintf(stderr, "android_errorWriteLog: tag:%x subTag:%s\n", tag, subTag); \
    } while (0)
}
