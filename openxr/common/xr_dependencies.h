// Copyright (c) 2018-2023, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// This file includes headers with types which openxr.h depends on in order
// to compile when platforms, graphics apis, and the like are enabled.

#pragma once

#include <android/native_window.h>
#include <android/window.h>
#include <android/native_window_jni.h>

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
#include <EGL/egl.h>
#endif  // XR_USE_GRAPHICS_API_OPENGL_ES

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#endif  // XR_USE_GRAPHICS_API_VULKAN
