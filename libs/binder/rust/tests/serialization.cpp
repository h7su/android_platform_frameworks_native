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

#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/Status.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <optional>

using namespace std;
using namespace android;

#define ASSERT(cond)                                                                              \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            cerr << __func__ << ":" << __LINE__ << " condition:" << #cond << " failed\n" << endl; \
            return false;                                                                         \
        }                                                                                         \
    } while (0);
#define ASSERT_STATUS(status) ASSERT(status == OK)

vector<int8_t> i8s = {-128, 0, 117, 127};
vector<uint8_t> u8s = {0, 42, 117, 255};
vector<char16_t> chars = {0, 42, 117, numeric_limits<char16_t>::max()};
vector<int32_t> i32s = {numeric_limits<int32_t>::min(), 0, 117, numeric_limits<int32_t>::max()};
vector<int64_t> i64s = {numeric_limits<int64_t>::min(), 0, 117, numeric_limits<int64_t>::max()};
vector<uint64_t> u64s = {0, 42, 117, numeric_limits<uint64_t>::max()};
vector<float> floats = {
        numeric_limits<float>::quiet_NaN(),
        -numeric_limits<float>::infinity(),
        117.0,
        numeric_limits<float>::infinity(),
};
vector<double> doubles = {
        numeric_limits<double>::quiet_NaN(),
        -numeric_limits<double>::infinity(),
        117.0,
        numeric_limits<double>::infinity(),
};
vector<bool> bools = {true, false, false, true};
vector<optional<String16>> strings = {String16(""), nullopt, String16("test"), String16("😁")};

extern "C" {
bool send_transaction();
}

bool send_transaction() {
    android::Parcel parcel;

    parcel.writeInterfaceToken(String16("read_parcel_test"));

    parcel.writeBool(true);
    parcel.writeBool(false);
    parcel.writeBoolVector(bools);
    parcel.writeBoolVector(nullopt);

    parcel.writeByte(0);
    parcel.writeByte(1);
    parcel.writeByte(numeric_limits<int8_t>::max());
    parcel.writeByteVector(i8s);
    parcel.writeByteVector(u8s);
    parcel.writeByteVector(optional<vector<int8_t>>({}));

    parcel.writeChar(0);
    parcel.writeChar(1);
    parcel.writeChar(numeric_limits<char16_t>::max());
    parcel.writeCharVector(chars);
    parcel.writeCharVector(nullopt);

    parcel.writeInt32(0);
    parcel.writeInt32(1);
    parcel.writeInt32(numeric_limits<int32_t>::max());
    parcel.writeInt32Vector(i32s);
    parcel.writeInt32Vector(nullopt);

    parcel.writeInt64(0);
    parcel.writeInt64(1);
    parcel.writeInt64(numeric_limits<int64_t>::max());
    parcel.writeInt64Vector(i64s);
    parcel.writeInt64Vector(nullopt);

    parcel.writeUint64(0);
    parcel.writeUint64(1);
    parcel.writeUint64(numeric_limits<uint64_t>::max());
    parcel.writeUint64Vector(u64s);
    parcel.writeUint64Vector(nullopt);

    parcel.writeFloat(0);
    parcel.writeFloatVector(floats);
    parcel.writeFloatVector(nullopt);

    parcel.writeDouble(0);
    parcel.writeDoubleVector(doubles);
    parcel.writeDoubleVector(nullopt);

    parcel.writeUtf8AsUtf16(string("testing"));
    parcel.writeString16(nullopt);
    parcel.writeString16Vector(strings);
    parcel.writeString16Vector(nullopt);

    binder::Status::ok().writeToParcel(&parcel);
    binder::Status::fromExceptionCode(binder::Status::EX_NULL_POINTER, "a status message")
            .writeToParcel(&parcel);
    binder::Status::fromServiceSpecificError(42, "a service-specific error").writeToParcel(&parcel);

    auto sm = defaultServiceManager();
    auto service = sm->getService(String16("read_parcel_test"));

    parcel.writeStrongBinder(service);
    parcel.writeStrongBinder(nullptr);
    parcel.writeStrongBinderVector({service, nullptr});
    parcel.writeStrongBinderVector(nullopt);

    android::Parcel reply;
    ASSERT_STATUS(service->transact(IBinder::FIRST_CALL_TRANSACTION, parcel, &reply, 0));

    vector<bool> read_bools;
    optional<vector<bool>> maybe_bools;
    ASSERT(reply.readBool() == true);
    ASSERT(reply.readBool() == false);
    ASSERT_STATUS(reply.readBoolVector(&read_bools));
    ASSERT(read_bools == bools);
    ASSERT_STATUS(reply.readBoolVector(&maybe_bools));
    ASSERT(maybe_bools == nullopt);

    vector<int8_t> read_i8s;
    vector<uint8_t> read_u8s;
    optional<vector<int8_t>> maybe_i8s;
    ASSERT(reply.readByte() == 0);
    ASSERT(reply.readByte() == 1);
    ASSERT(reply.readByte() == numeric_limits<int8_t>::max());
    ASSERT_STATUS(reply.readByteVector(&read_i8s));
    ASSERT(read_i8s == i8s);
    ASSERT_STATUS(reply.readByteVector(&read_u8s));
    ASSERT(read_u8s == u8s);
    ASSERT_STATUS(reply.readByteVector(&maybe_i8s));
    ASSERT(maybe_i8s == nullopt);

    vector<char16_t> read_chars;
    optional<vector<char16_t>> maybe_chars;
    ASSERT(reply.readChar() == 0);
    ASSERT(reply.readChar() == 1);
    ASSERT(reply.readChar() == numeric_limits<char16_t>::max());
    ASSERT_STATUS(reply.readCharVector(&read_chars));
    ASSERT(read_chars == chars);
    ASSERT_STATUS(reply.readCharVector(&maybe_chars));
    ASSERT(maybe_chars == nullopt);

    vector<int32_t> read_i32s;
    optional<vector<int32_t>> maybe_i32s;
    ASSERT(reply.readInt32() == 0);
    ASSERT(reply.readInt32() == 1);
    ASSERT(reply.readInt32() == numeric_limits<int32_t>::max());
    ASSERT_STATUS(reply.readInt32Vector(&read_i32s));
    ASSERT(read_i32s == i32s);
    ASSERT_STATUS(reply.readInt32Vector(&maybe_i32s));
    ASSERT(maybe_i32s == nullopt);

    vector<int64_t> read_i64s;
    optional<vector<int64_t>> maybe_i64s;
    ASSERT(reply.readInt64() == 0);
    ASSERT(reply.readInt64() == 1);
    ASSERT(reply.readInt64() == numeric_limits<int64_t>::max());
    ASSERT_STATUS(reply.readInt64Vector(&read_i64s));
    ASSERT(read_i64s == i64s);
    ASSERT_STATUS(reply.readInt64Vector(&maybe_i64s));
    ASSERT(maybe_i64s == nullopt);

    vector<uint64_t> read_u64s;
    optional<vector<uint64_t>> maybe_u64s;
    ASSERT(reply.readUint64() == 0);
    ASSERT(reply.readUint64() == 1);
    ASSERT(reply.readUint64() == numeric_limits<uint64_t>::max());
    ASSERT_STATUS(reply.readUint64Vector(&read_u64s));
    ASSERT(read_u64s == u64s);
    ASSERT_STATUS(reply.readUint64Vector(&maybe_u64s));
    ASSERT(maybe_u64s == nullopt);

    vector<float> read_floats;
    optional<vector<float>> maybe_floats;
    ASSERT(reply.readFloat() == 0);
    ASSERT_STATUS(reply.readFloatVector(&read_floats));
    ASSERT(isnan(read_floats[0]));
    ASSERT(read_floats[1] == floats[1]);
    ASSERT(read_floats[2] == floats[2]);
    ASSERT(read_floats[3] == floats[3]);
    ASSERT_STATUS(reply.readFloatVector(&maybe_floats));
    ASSERT(maybe_floats == nullopt);

    vector<double> read_doubles;
    optional<vector<double>> maybe_doubles;
    ASSERT(reply.readDouble() == 0);
    ASSERT_STATUS(reply.readDoubleVector(&read_doubles));
    ASSERT(isnan(read_doubles[0]));
    ASSERT(read_doubles[1] == doubles[1]);
    ASSERT(read_doubles[2] == doubles[2]);
    ASSERT(read_doubles[3] == doubles[3]);
    ASSERT_STATUS(reply.readDoubleVector(&maybe_doubles));
    ASSERT(maybe_doubles == nullopt);

    optional<String16> maybe_string;
    optional<vector<optional<String16>>> read_strings;
    ASSERT(reply.readString16() == String16("testing"));
    ASSERT_STATUS(reply.readString16(&maybe_string));
    ASSERT(maybe_string == nullopt);
    ASSERT_STATUS(reply.readString16Vector(&read_strings));
    ASSERT(read_strings == strings);
    ASSERT_STATUS(reply.readString16Vector(&read_strings));
    ASSERT(read_strings == nullopt);

    binder::Status status;

    ASSERT_STATUS(status.readFromParcel(reply));
    ASSERT(status.isOk());

    ASSERT_STATUS(status.readFromParcel(reply));
    ASSERT(status.exceptionCode() == binder::Status::EX_NULL_POINTER);
    ASSERT(status.exceptionMessage() == "a status message");

    ASSERT_STATUS(status.readFromParcel(reply));
    ASSERT(status.serviceSpecificErrorCode() == 42);
    ASSERT(status.exceptionMessage() == "a service-specific error");

    optional<vector<sp<IBinder>>> binders;
    ASSERT(reply.readStrongBinder());
    ASSERT(!reply.readStrongBinder());
    ASSERT_STATUS(reply.readStrongBinderVector(&binders));
    ASSERT(binders->size() == 2);
    ASSERT((*binders)[0]);
    ASSERT(!(*binders)[1]);
    ASSERT_STATUS(reply.readStrongBinderVector(&binders));
    ASSERT(!binders);

    int32_t end;
    ASSERT(reply.readInt32(&end) == NOT_ENOUGH_DATA);

    return true;
}
