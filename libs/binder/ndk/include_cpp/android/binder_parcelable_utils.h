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

/**
 * @addtogroup NdkBinder
 * @{
 */

/**
 * @file binder_parcelable_utils.h
 * @brief Helper for parcelable.
 */

#pragma once
#include <android/binder_parcel_utils.h>
#include <optional>

namespace ndk {
// Also see Parcelable.h in libbinder.
typedef int32_t parcelable_stability_t;
enum {
    STABILITY_LOCAL,
    STABILITY_VINTF,  // corresponds to @VintfStability
};
#define RETURN_ON_FAILURE(expr)                   \
    do {                                          \
        binder_status_t _status = (expr);         \
        if (_status != STATUS_OK) return _status; \
    } while (false)

class AParcelableHolder {
   public:
    AParcelableHolder() = delete;
    explicit AParcelableHolder(parcelable_stability_t stability)
        : mParcel(AParcel_create()), mStability(stability) {}

    virtual ~AParcelableHolder() = default;

    binder_status_t writeToParcel(AParcel* parcel) const {
        std::lock_guard<std::mutex> l(mMutex);
        RETURN_ON_FAILURE(AParcel_writeInt32(parcel, static_cast<int32_t>(this->mStability)));
        RETURN_ON_FAILURE(AParcel_writeInt32(parcel, AParcel_getDataSize(this->mParcel.get())));
        RETURN_ON_FAILURE(AParcel_appendFrom(this->mParcel.get(), parcel, 0,
                                             AParcel_getDataSize(this->mParcel.get())));
        return STATUS_OK;
    }

    binder_status_t readFromParcel(const AParcel* parcel) {
        std::lock_guard<std::mutex> l(mMutex);

        AParcel_setDataSize(mParcel.get(), 0);
        AParcel_setDataPosition(mParcel.get(), 0);

        RETURN_ON_FAILURE(AParcel_readInt32(parcel, &this->mStability));
        int32_t rawDataSize;

        binder_status_t status = AParcel_readInt32(parcel, &rawDataSize);

        if (status != STATUS_OK || rawDataSize < 0) {
            return status != STATUS_OK ? status : STATUS_BAD_VALUE;
        }

        int32_t dataSize = rawDataSize;

        int32_t dataStartPos = AParcel_getDataPosition(parcel);

        if (dataStartPos > INT32_MAX - dataSize) {
            return STATUS_BAD_VALUE;
        }

        status = AParcel_appendFrom(parcel, mParcel.get(), dataStartPos, dataSize);
        if (status != STATUS_OK) {
            return status;
        }
        return AParcel_setDataPosition(parcel, dataStartPos + dataSize);
    }

    template <typename T>
    bool setParcelable(T* p) {
        std::lock_guard<std::mutex> l(mMutex);
        if (p && this->mStability > T::_aidl_stability) {
            return false;
        }
        AParcel_setDataSize(mParcel.get(), 0);
        AParcel_setDataPosition(mParcel.get(), 0);
        AParcel_writeString(mParcel.get(), T::descriptor, strlen(T::descriptor));
        p->writeToParcel(mParcel.get());
        return true;
    }

    template <typename T>
    std::unique_ptr<T> getParcelable() const {
        std::lock_guard<std::mutex> l(mMutex);
        const std::string parcelableDesc(T::descriptor);
        AParcel_setDataPosition(mParcel.get(), 0);
        if (AParcel_getDataSize(mParcel.get()) == 0) {
            return nullptr;
        }
        std::string parcelableDescInParcel;
        binder_status_t status = AParcel_readString(mParcel.get(), &parcelableDescInParcel);
        if (status != STATUS_OK || parcelableDesc != parcelableDescInParcel) {
            return nullptr;
        }
        std::unique_ptr<T> ret = std::make_unique<T>();
        status = ret->readFromParcel(this->mParcel.get());
        if (status != STATUS_OK) {
            return nullptr;
        }
        return std::move(ret);
    }

   private:
    mutable ndk::ScopedAParcel mParcel;
    mutable std::mutex mMutex;
    parcelable_stability_t mStability;
};
}  // namespace ndk

/** @} */
