#!/bin/sh

set -eu

TEMPDIR=`mktemp -d`
OUTDIR="$TEMPDIR/linux-binder"

cd "$ANDROID_BUILD_TOP/frameworks/native/libs/binder"
git clean -fdx
mkdir -p "$OUTDIR/frameworks/native/libs/binder"
cp -r . "$OUTDIR/frameworks/native/libs/binder"

mkdir -p "$OUTDIR/frameworks/native/libs/binder/linux/aidl/gen"
linux/aidl/copy-aidl-gen.sh "$OUTDIR/frameworks/native/libs/binder/linux/aidl/gen"

cd "$ANDROID_BUILD_TOP/frameworks/base/native/android"
git clean -fdx
mkdir -p "$OUTDIR/frameworks/base/native/android"
cp -r . "$OUTDIR/frameworks/base/native/android"

cd "$ANDROID_BUILD_TOP/frameworks/native/include/android"
git clean -fdx
mkdir -p "$OUTDIR/frameworks/native/include/android"
cp -r . "$OUTDIR/frameworks/native/include/android"

cd "$ANDROID_BUILD_TOP/external/boringssl"
git clean -fdx
mkdir -p "$OUTDIR/external/boringssl"
cp -r . "$OUTDIR/external/boringssl"

cd "$ANDROID_BUILD_TOP/external/fmtlib"
git clean -fdx
mkdir -p "$OUTDIR/external/fmtlib"
cp -r . "$OUTDIR/external/fmtlib"

cd "$ANDROID_BUILD_TOP/external/googletest"
git clean -fdx
mkdir -p "$OUTDIR/external/googletest"
cp -r . "$OUTDIR/external/googletest"

cd "$ANDROID_BUILD_TOP/system/libbase"
git clean -fdx
mkdir -p "$OUTDIR/system/libbase"
cp -r . "$OUTDIR/system/libbase"

cd "$ANDROID_BUILD_TOP/system/core/libcutils"
git clean -fdx
mkdir -p "$OUTDIR/system/core/libcutils"
cp -r . "$OUTDIR/system/core/libcutils"

cd "$ANDROID_BUILD_TOP/system/core/libutils"
git clean -fdx
mkdir -p "$OUTDIR/system/core/libutils"
cp -r . "$OUTDIR/system/core/libutils"

cd "$ANDROID_BUILD_TOP/system/tools/aidl"
git clean -fdx
mkdir -p "$OUTDIR/system/tools/aidl"
cp -r . "$OUTDIR/system/tools/aidl"

cd $TEMPDIR
tar -cvf "$ANDROID_BUILD_TOP/frameworks/native/libs/binder/linux/linux-binder.tar.gz" linux-binder >/dev/null
cd $ANDROID_BUILD_TOP
rm -rf $TEMPDIR
