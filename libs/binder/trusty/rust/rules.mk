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

LOCAL_DIR := $(GET_LOCAL_DIR)
BINDER_RUST_DIR := frameworks/native/libs/binder/rust

MODULE := $(LOCAL_DIR)

MODULE_SRCS := $(BINDER_RUST_DIR)/src/lib.rs

MODULE_CRATE_NAME := binder

MODULE_LIBRARY_DEPS += \
	frameworks/native/libs/binder/trusty \
	frameworks/native/libs/binder/trusty/ndk \
	frameworks/native/libs/binder/trusty/rust/binder_ndk_sys \
	trusty/user/base/lib/downcast-rust \
	trusty/user/base/lib/trusty-sys \

# Trusty does not have `ProcessState`, so there are a few
# doc links in `IBinder` that are still broken.
MODULE_RUSTFLAGS += \
	--allow rustdoc::broken-intra-doc-links \

include make/library.mk
