#!/bin/sh

set -eu

TEMPDIR=`mktemp -d`
OUTDIR="$TEMPDIR/linux-binder"

cd "$ANDROID_BUILD_TOP/frameworks/native/libs/binder"
git clean -fdx
mkdir -p "$OUTDIR/frameworks/native/libs/binder"
cp -r . "$OUTDIR/frameworks/native/libs/binder"

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

cd "$ANDROID_BUILD_TOP/system/core/libutils/binder"
git clean -fdx
mkdir -p "$OUTDIR/system/core/libutils/binder"
cp -r . "$OUTDIR/system/core/libutils/binder"

mkdir -p "$OUTDIR/prebuilts/host/linux-x86/bin"
mkdir -p "$OUTDIR/prebuilts/host/linux-x86/lib64"
cp "$ANDROID_HOST_OUT/bin/aidl" "$OUTDIR/prebuilts/host/linux-x86/bin"
cp "$ANDROID_HOST_OUT/lib64/libc++.so" "$OUTDIR/prebuilts/host/linux-x86/lib64"

cd $TEMPDIR
tar -cvf "$ANDROID_BUILD_TOP/frameworks/native/libs/binder/linux/linux-binder.tar.gz" linux-binder >/dev/null
cd $ANDROID_BUILD_TOP
rm -rf $TEMPDIR
