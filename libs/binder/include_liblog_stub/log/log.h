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

#include <android/log.h>

// TODO: rename level to prio/priority
#ifndef ANDROID_LOG_STUB_LEVEL
#define ANDROID_LOG_STUB_LEVEL ANDROID_LOG_INFO
#endif

#ifndef LOG_TAG
#define LOG_TAG nullptr
#endif

constexpr bool __android_log_stub_is_loggable(android_LogPriority priority) {
    return ANDROID_LOG_STUB_LEVEL <= priority;
}

extern "C" void android_log_stub_print(int level, const char* tag, const char* fmt, ...);

#define ALOG(priority, tag, fmt, ...)                                                        \
    do {                                                                                     \
        if (false)[[/*VERY*/ unlikely]] { /* ignore unused __VA_ARGS__ */                    \
            std::fprintf(stderr, fmt __VA_OPT__(, ) __VA_ARGS__);                            \
        }                                                                                    \
        if constexpr (__android_log_stub_is_loggable(ANDROID_##priority)) {                  \
            android_log_stub_print(ANDROID_##priority, tag, fmt __VA_OPT__(, ) __VA_ARGS__); \
        }                                                                                    \
        if constexpr (ANDROID_##priority == ANDROID_LOG_FATAL) std::abort();                 \
    } while (false)

#define IF_ALOGV() if constexpr (__android_log_stub_is_loggable(ANDROID_LOG_VERBOSE))
#define IF_ALOGD() if constexpr (__android_log_stub_is_loggable(ANDROID_LOG_DEBUG))
#define IF_ALOGI() if constexpr (__android_log_stub_is_loggable(ANDROID_LOG_INFO))
#define IF_ALOGW() if constexpr (__android_log_stub_is_loggable(ANDROID_LOG_WARN))
#define IF_ALOGE() if constexpr (__android_log_stub_is_loggable(ANDROID_LOG_ERROR))

#define ALOGV(...) ALOG(LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) ALOG(LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) ALOG(LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOG_FATAL(...) ALOG(LOG_FATAL, LOG_TAG, __VA_ARGS__)
#define LOG_ALWAYS_FATAL LOG_FATAL

#define ALOG_IF(cond, level, tag, ...) \
    if (cond) [[unlikely]]             \
    ALOG(level, tag, #cond ":" __VA_ARGS__)

#define ALOGV_IF(cond, ...) ALOG_IF(cond, LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGD_IF(cond, ...) ALOG_IF(cond, LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGI_IF(cond, ...) ALOG_IF(cond, LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW_IF(cond, ...) ALOG_IF(cond, LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE_IF(cond, ...) ALOG_IF(cond, LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOG_FATAL_IF(cond, ...) ALOG_IF(cond, LOG_FATAL, LOG_TAG, __VA_ARGS__)
#define LOG_ALWAYS_FATAL_IF LOG_FATAL_IF

#define ALOG_ASSERT(cond, ...) LOG_FATAL_IF(!(cond), ##__VA_ARGS__)

inline int android_errorWriteLog(int tag, const char* subTag) {
    ALOGE("android_errorWriteLog(%x, %s)", tag, subTag);
    return 0;
}
