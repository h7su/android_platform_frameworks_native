# Copyright (C) 2023 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

cmake_minimum_required(VERSION 3.18)

include_guard(GLOBAL)
# TODO: it's libutils_binder now
project(libutils CXX)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")
include(AppendCxxFlagIfSupported)

set(CMAKE_CXX_STANDARD 20)

set(LIBUTILS_CXX_FLAGS
    "-DNDEBUG"
    "-Wall"
    "-DANDROID_UTILS_REF_BASE_DISABLE_IMPLICIT_CONSTRUCTION"
    "-DANDROID_UTILS_CALLSTACK_ENABLED=0"
    "-fPIC"
    "-Wno-attributes"
    "-Wno-invalid-offsetof"
    "-Wno-sign-compare"
)

append_cxx_flag_if_supported(-Wno-class-memaccess LIBUTILS_CXX_FLAGS)

if (NOT LIBUTILS_DIR AND ANDROID_BUILD_TOP)
    set(LIBUTILS_DIR "${ANDROID_BUILD_TOP}/system/core/libutils/binder")
endif()

set(LIBUTILS_SRCS
    "${LIBUTILS_DIR}/Errors.cpp"
    "${LIBUTILS_DIR}/RefBase.cpp"
    "${LIBUTILS_DIR}/SharedBuffer.cpp"
    "${LIBUTILS_DIR}/String8.cpp"
    "${LIBUTILS_DIR}/String16.cpp"
    "${LIBUTILS_DIR}/StrongPointer.cpp"
    "${LIBUTILS_DIR}/Unicode.cpp"
    "${LIBUTILS_DIR}/VectorImpl.cpp"
)

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../liblog" liblog EXCLUDE_FROM_ALL)

add_library(android_libutils ${LIBUTILS_SRCS})

target_include_directories(android_libutils
    PUBLIC
    "${LIBUTILS_DIR}/include"
)

target_link_libraries(android_libutils android_liblog)

target_compile_options(android_libutils PRIVATE ${LIBUTILS_CXX_FLAGS})
