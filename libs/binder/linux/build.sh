#!/bin/bash

${ANDROID_BUILD_TOP}/build/soong/soong_ui.bash --make-mode binder_cmake_tstlib
cp ${ANDROID_BUILD_TOP}/out/soong/.intermediates/frameworks/native/libs/binder/binder_cmake_tstlib/*/*/CMakeLists.txt ./CMakeLists.txt
cmake \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DANDROID_BUILD_TOP=$ANDROID_BUILD_TOP \
    -B build
cmake --build build
