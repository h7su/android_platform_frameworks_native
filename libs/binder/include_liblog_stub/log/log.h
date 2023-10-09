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

#ifndef ANDROID_LOG_STUB_LEVEL
#define ANDROID_LOG_STUB_LEVEL ANDROID_LOG_INFO
#endif

#ifndef LOG_TAG
#define LOG_TAG ""
#endif

constexpr bool __android_log_stub_is_loggable(int prio) {
    if (prio < ANDROID_LOG_VERBOSE) [[unlikely]] {
        prio = ANDROID_LOG_VERBOSE;
    }
    if (prio > ANDROID_LOG_FATAL) [[unlikely]] {
        prio = ANDROID_LOG_FATAL;
    }
    return ANDROID_LOG_STUB_LEVEL <= prio;
}

#define ALOG(level, tag, fmt, ...)                                        \
    do {                                                                  \
        if (false) { /* make some compilers happy */                      \
            std::fprintf(stderr, fmt, ##__VA_ARGS__);                     \
        }                                                                 \
        if constexpr (__android_log_stub_is_loggable(ANDROID_##level)) {  \
            std::fprintf(stderr, tag ": " fmt "\n", ##__VA_ARGS__);       \
        }                                                                 \
        if constexpr (ANDROID_##level == ANDROID_LOG_FATAL) std::abort(); \
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

#define android_errorWriteLog(tag, subTag) \
    std::fprintf(stderr, "android_errorWriteLog: tag:%x subTag:%s\n", tag, subTag)
