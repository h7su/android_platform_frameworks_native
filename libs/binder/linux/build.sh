#!/bin/sh

# Install dependencies:
# sudo apt install cmake ninja-build libgtest-dev

# Build prerequisites:
# m aidl

PREBUILT_HOST_OUT=`pwd`/../../../../../prebuilts/host/linux-x86
if [ -f "$PREBUILT_HOST_OUT/bin/aidl" ]; then
    export PATH="$PREBUILT_HOST_OUT/bin:$PATH"
    if [ -z $ANDROID_BUILD_TOP ]; then
        ANDROID_BUILD_TOP=`pwd`/../../../../..
    fi
    if [ -z $ANDROID_HOST_OUT ]; then
        ANDROID_HOST_OUT=$PREBUILT_HOST_OUT
    fi
fi
if [ -z $CC ]; then
    echo "CC not set, setting default"
    export CC=gcc
fi
if [ -z $CXX ]; then
    echo "CXX not set, setting default"
    export CXX=g++
fi

set -eu

if [ ! -f $ANDROID_HOST_OUT/bin/aidl ]; then
    echo "Please run:"
    echo "m -j aidl"
    exit
fi

cd "$ANDROID_BUILD_TOP/frameworks/native/libs/binder/linux"

# Release build doesn't pass tests on RPi due to some race condition, let's stick to Debug for now
cmake \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DANDROID_BUILD_TOP=$ANDROID_BUILD_TOP \
    -DAIDL_BIN=$ANDROID_HOST_OUT/bin/aidl \
    -B build

cmake --build build
