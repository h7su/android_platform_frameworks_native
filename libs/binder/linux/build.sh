#!/bin/bash

${ANDROID_BUILD_TOP}/build/soong/soong_ui.bash --make-mode binder_cmake_tstlib
cp ${ANDROID_BUILD_TOP}/out/soong/.intermediates/frameworks/native/libs/binder/binder_cmake_tstlib/*/*/binder_cmake_tstlib.zip ./binder_cmake_tstlib.zip
unzip -o binder_cmake_tstlib.zip -d build

cd build
cmake \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DANDROID_BUILD_TOP=$ANDROID_BUILD_TOP \
    -B build
cmake --build build
