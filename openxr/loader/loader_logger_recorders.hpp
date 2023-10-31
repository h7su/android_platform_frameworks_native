// Copyright (c) 2017-2023, The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Initial Author: Ryan Pavlik <ryan.pavlik@collabora.com>
//

#pragma once

#include "loader_logger.hpp"

#include <openxr/openxr.h>

#include <memory>

//! Standard Error logger, on by default. Disabled with environment variable XR_LOADER_DEBUG = "none".
std::unique_ptr<LoaderLogRecorder> MakeStdErrLoaderLogRecorder(void* user_data);

//! Standard Output logger used with XR_LOADER_DEBUG environment variable.
std::unique_ptr<LoaderLogRecorder> MakeStdOutLoaderLogRecorder(void* user_data, XrLoaderLogMessageSeverityFlags flags);

//! Android liblog ("logcat") logger
std::unique_ptr<LoaderLogRecorder> MakeLogcatLoaderLogRecorder();

// Debug Utils logger used with XR_EXT_debug_utils
std::unique_ptr<LoaderLogRecorder> MakeDebugUtilsLoaderLogRecorder(const XrDebugUtilsMessengerCreateInfoEXT* create_info,
                                                                   XrDebugUtilsMessengerEXT debug_messenger);

// TODO: Add other Derived classes:
//  - FileLoaderLogRecorder     - During/after xrCreateInstance
//  - PipeLoaderLogRecorder?    - During/after xrCreateInstance
