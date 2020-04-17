/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstdint>
#include <memory>

#include <utils/Errors.h>
#include <android-base/unique_fd.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/Status.h>

// ---------------------------------------------------------------------------
// Implemented in Rust

extern "C" {

// An opaque pointer to any Rust struct. This is initialized when creating a
// BinderNative wrapper and passed back to the TransactCallback for onTransact.
struct RustObject;

}

// ---------------------------------------------------------------------------
// Implemented in C++

namespace android {

namespace c_interface {

class RustBBinder;

sp<IBinder>* Sp_CloneIBinder(const sp<IBinder> *sp);
void Sp_DropIBinder(sp<IBinder> *sp);
sp<IServiceManager>* Sp_CloneIServiceManager(const sp<IServiceManager> *sp);
void Sp_DropIServiceManager(sp<IServiceManager> *sp);
void Sp_DropRustBBinder(sp<RustBBinder> *sp);
sp<IInterface>* Sp_CloneIInterface(const sp<IInterface> *sp);
void Sp_DropIInterface(sp<IInterface> *sp);

IBinder* Sp_getIBinder(sp<IBinder> *sp);
IServiceManager* Sp_getIServiceManager(sp<IServiceManager> *sp);
IInterface* Sp_getIInterface(sp<IInterface> *sp);

typedef status_t(TransactCallback)(RustObject* object, uint32_t code, const Parcel* data,
                                   Parcel* reply, uint32_t flags);
typedef void(DestructCallback)(RustObject* object);

sp<RustBBinder>* NewRustBBinder(RustObject* object, const String16* descriptor,
                                  TransactCallback* transactCallback,
                                  DestructCallback destructCallback);
status_t RustBBinder_writeToParcel(const sp<RustBBinder>* binder, Parcel* parcel);
void RustBBinder_setExtension(sp<RustBBinder>* binder, const sp<IBinder>* ext);
status_t RustBBinder_getExtension(sp<RustBBinder>* binder, sp<IBinder>** out);
sp<IBinder>* RustBBinder_castToIBinder(sp<RustBBinder>* binder);

sp<IServiceManager>* DefaultServiceManager();

// ProcessState methods
void StartThreadPool();
void FlushCommands();

status_t IBinder_transact(IBinder* binder, uint32_t code, const Parcel* data,
                          Parcel* reply, uint32_t flags);
sp<IInterface>* IBinder_queryLocalInterface(IBinder* binder, const String16* descriptor);
const String16* IBinder_getInterfaceDescriptor(IBinder* binder);
status_t IBinder_pingBinder(IBinder* binder);
status_t IBinder_getExtension(IBinder* binder, sp<IBinder>** out);

Parcel* NewParcel();
status_t Parcel_readStrongBinder(const Parcel* parcel, sp<IBinder>** binder);
status_t Parcel_readString8(const Parcel* parcel, String8** string);
status_t Parcel_readString16(const Parcel* parcel, String16** string);
status_t Parcel_readBlob(const Parcel* parcel, size_t len, Parcel::ReadableBlob** blob);
status_t Parcel_writeBlob(Parcel* parcel, size_t len, bool mutableCopy,
                          Parcel::WritableBlob** blob);
const void* Parcel_ReadableBlob_data(const Parcel::ReadableBlob* blob);
void* Parcel_WritableBlob_data(Parcel::WritableBlob* blob);
size_t Parcel_ReadableBlob_size(const Parcel::ReadableBlob* blob);
size_t Parcel_WritableBlob_size(const Parcel::WritableBlob* blob);
void Parcel_ReadableBlob_clear(Parcel::ReadableBlob* blob);
void Parcel_WritableBlob_clear(Parcel::WritableBlob* blob);
void Parcel_ReadableBlob_release(Parcel::ReadableBlob* blob);
void Parcel_WritableBlob_release(Parcel::WritableBlob* blob);
void Parcel_ReadableBlob_Destructor(Parcel::ReadableBlob* blob);
void Parcel_WritableBlob_Destructor(Parcel::WritableBlob* blob);

const String16* IServiceManager_getInterfaceDescriptor(const IServiceManager* self);
sp<IBinder>* IServiceManager_getService(const IServiceManager* self, const String16* name);

String8* NewString8();
String8* NewString8FromUtf16(const char16_t* data, size_t len);
String8* NewString8FromUtf8(const char* data, size_t len);
const char* String8_data(const String8* S);
void String8_Destroy(String8* S);

String16* NewString16();
String16* NewString16FromUtf16(const char16_t* data, size_t len);
String16* NewString16FromUtf8(const char* data, size_t len);
const char16_t* String16_data(const String16* S);
void String16_Destroy(String16* S);

base::unique_fd* NewUniqueFd();
void UniqueFd_reset(base::unique_fd* self, int newValue);
void UniqueFd_destructor(base::unique_fd* self);

} // namespace c_interface

} // namespace android
