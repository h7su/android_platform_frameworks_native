# Copyright (C) 2022 The Android Open Source Project
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
LIBBINDER_TESTS_DIR := frameworks/native/libs/binder/tests

MODULE := $(LOCAL_DIR)

MANIFEST := $(LOCAL_DIR)/manifest.json

MODULE_SRCS += \
	$(LOCAL_DIR)/binderRpcTest.cpp \
	$(LIBBINDER_TESTS_DIR)/binderRpcBasicTests.cpp \
	$(LIBBINDER_TESTS_DIR)/binderRpcTestCommon.cpp \

# For binderRpcTestFixture.h
MODULE_INCLUDES += $(LIBBINDER_TESTS_DIR)

MODULE_LIBRARY_DEPS += \
	$(LOCAL_DIR)/aidl \
	trusty/user/base/lib/googletest \
	trusty/user/base/lib/libstdc++-trusty \
	frameworks/native/libs/binder/trusty \
	frameworks/native/libs/binder/trusty/ndk \

include make/trusted_app.mk
