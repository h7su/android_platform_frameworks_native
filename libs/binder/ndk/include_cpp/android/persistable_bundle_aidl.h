/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <android/binder_parcel.h>
#include <android/persistable_bundle.h>
#include <sys/cdefs.h>

#include <set>
#include <sstream>

namespace aidl::android::os {

/**
 * Wrapper class that enables interop with AIDL NDK generation
 * Takes ownership of the APersistableBundle* given to it in reset() and will automatically
 * destroy it in the destructor, similar to a smart pointer container
 */
class PersistableBundle {
   public:
    PersistableBundle() noexcept {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_new != nullptr) {
                mPBundle = APersistableBundle_new();
            }
        }
    }
    // takes ownership of the APersistableBundle*
    PersistableBundle(APersistableBundle* _Nonnull bundle) noexcept : mPBundle(bundle) {}
    // takes ownership of the APersistableBundle* PersistableBundle(PersistableBundle&& other)
    // noexcept : mPBundle(other.release()) {} duplicates, does not take ownership of the
    // APersistableBundle*
    PersistableBundle(const PersistableBundle& other) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_dup != nullptr) {
                mPBundle = APersistableBundle_dup(other.mPBundle);
            }
        }
    }
    // duplicates, does not take ownership of the APersistableBundle*
    PersistableBundle& operator=(const PersistableBundle& other) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_dup != nullptr) {
                mPBundle = APersistableBundle_dup(other.mPBundle);
            }
        }
        return *this;
    }

    ~PersistableBundle() { reset(); }

    binder_status_t readFromParcel(const AParcel* _Nonnull parcel) {
        reset();
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_readFromParcel != nullptr) {
                return APersistableBundle_readFromParcel(parcel, &mPBundle);
            }
        }
        return STATUS_FAILED_TRANSACTION;
    }

    binder_status_t writeToParcel(AParcel* _Nonnull parcel) const {
        if (!mPBundle) {
            return STATUS_BAD_VALUE;
        }
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_writeToParcel != nullptr) {
                return APersistableBundle_writeToParcel(mPBundle, parcel);
            }
        }
        return STATUS_FAILED_TRANSACTION;
    }

    /**
     * Destroys any currently owned APersistableBundle* and takes ownership of the given
     * APersistableBundle*
     *
     * @param pBundle The APersistableBundle to take ownership of
     */
    void reset(APersistableBundle* _Nullable pBundle = nullptr) noexcept {
        if (mPBundle) {
            if (__builtin_available(android __ANDROID_API_V__, *)) {
                if (APersistableBundle_delete != nullptr) {
                    APersistableBundle_delete(mPBundle);
                }
            }
            mPBundle = nullptr;
        }
        mPBundle = pBundle;
    }

    /**
     * Check the actual contents of the bundle for equality. This is typically
     * what should be used to check for equality.
     */
    bool deepEquals(const PersistableBundle& rhs) const {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_isEqual != nullptr) {
                return APersistableBundle_isEqual(get(), rhs.get());
            }
        }
        return false;
    }

    /**
     * NOTE: This does NOT check the contents of the PersistableBundle. This is
     * implemented for ordering. Use deepEquals() to check for equality between
     * two different PersistableBundle objects.
     */
    inline bool operator==(const PersistableBundle& rhs) const { return get() == rhs.get(); }
    inline bool operator!=(const PersistableBundle& rhs) const { return get() != rhs.get(); }

    inline bool operator<(const PersistableBundle& rhs) const { return get() < rhs.get(); }
    inline bool operator>(const PersistableBundle& rhs) const { return get() > rhs.get(); }
    inline bool operator>=(const PersistableBundle& rhs) const { return !(*this < rhs); }
    inline bool operator<=(const PersistableBundle& rhs) const { return !(*this > rhs); }

    PersistableBundle& operator=(PersistableBundle&& other) noexcept {
        reset(other.release());
        return *this;
    }

    /**
     * Stops managing any contained APersistableBundle*, returning it to the caller. Ownership
     * is released.
     * @return APersistableBundle* or null if this was empty
     */
    [[nodiscard]] APersistableBundle* _Nullable release() noexcept {
        APersistableBundle* _Nullable ret = mPBundle;
        mPBundle = nullptr;
        return ret;
    }

    inline std::string toString() const {
        if (!mPBundle) {
            return "<PersistableBundle: null>";
        } else if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_size != nullptr) {
                std::ostringstream os;
                os << "<PersistableBundle: ";
                os << "size: " << std::to_string(APersistableBundle_size(mPBundle));
                os << " >";
                return os.str();
            }
        }
        return "<PersistableBundle (unknown)>";
    }

    int32_t size() const {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_size != nullptr) {
                return APersistableBundle_size(mPBundle);
            }
        }
        return 0;
    }

    int32_t erase(const std::string& key) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_erase != nullptr) {
                return APersistableBundle_erase(mPBundle, key.c_str());
            }
        }
        return 0;
    }

    void putBoolean(const std::string& key, bool val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putBoolean != nullptr) {
                APersistableBundle_putBoolean(mPBundle, key.c_str(), val);
            }
        }
    }

    void putInt(const std::string& key, int32_t val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putInt != nullptr) {
                APersistableBundle_putInt(mPBundle, key.c_str(), val);
            }
        }
    }

    void putLong(const std::string& key, int64_t val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putLong != nullptr) {
                APersistableBundle_putLong(mPBundle, key.c_str(), val);
            }
        }
    }

    void putDouble(const std::string& key, double val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putDouble != nullptr) {
                APersistableBundle_putDouble(mPBundle, key.c_str(), val);
            }
        }
    }

    void putString(const std::string& key, const std::string& val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putString != nullptr) {
                APersistableBundle_putString(mPBundle, key.c_str(), val.c_str());
            }
        }
    }

    void putBooleanVector(const std::string& key, const std::vector<bool>& vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putBooleanVector != nullptr) {
                // std::vector<bool> has no ::data().
                int32_t num = vec.size();
                if (num > 0) {
                    bool* newVec = (bool*)malloc(num * sizeof(bool));
                    if (newVec) {
                        for (int32_t i = 0; i < num; i++) {
                            newVec[i] = vec[i];
                        }
                        APersistableBundle_putBooleanVector(mPBundle, key.c_str(), newVec, num);
                        free(newVec);
                    }
                }
            }
        }
    }

    void putIntVector(const std::string& key, const std::vector<int32_t>& vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putIntVector != nullptr) {
                int32_t num = vec.size();
                if (num > 0) {
                    APersistableBundle_putIntVector(mPBundle, key.c_str(), vec.data(), num);
                }
            }
        }
    }
    void putLongVector(const std::string& key, const std::vector<int64_t>& vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putLongVector != nullptr) {
                int32_t num = vec.size();
                if (num > 0) {
                    APersistableBundle_putLongVector(mPBundle, key.c_str(), vec.data(), num);
                }
            }
        }
    }
    void putDoubleVector(const std::string& key, const std::vector<double>& vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putDoubleVector != nullptr) {
                int32_t num = vec.size();
                if (num > 0) {
                    APersistableBundle_putDoubleVector(mPBundle, key.c_str(), vec.data(), num);
                }
            }
        }
    }
    void putStringVector(const std::string& key, const std::vector<std::string>& vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putStringVector != nullptr) {
                int32_t num = vec.size();
                if (num > 0) {
                    char** inVec = (char**)malloc(num * sizeof(char*));
                    if (inVec) {
                        for (int32_t i = 0; i < num; i++) {
                            inVec[i] = strdup(vec[i].c_str());
                        }
                        APersistableBundle_putStringVector(mPBundle, key.c_str(), inVec, num);
                        free(inVec);
                    }
                }
            }
        }
    }
    void putPersistableBundle(const std::string& key, const PersistableBundle& pBundle) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_putPersistableBundle != nullptr) {
                APersistableBundle_putPersistableBundle(mPBundle, key.c_str(), pBundle.mPBundle);
            }
        }
    }

    bool getBoolean(const std::string& key, bool* _Nonnull val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getBoolean != nullptr) {
                return APersistableBundle_getBoolean(mPBundle, key.c_str(), val);
            }
        }
        return false;
    }

    bool getInt(const std::string& key, int32_t* _Nonnull val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getInt != nullptr) {
                return APersistableBundle_getInt(mPBundle, key.c_str(), val);
            }
        }
        return false;
    }

    bool getLong(const std::string& key, int64_t* _Nonnull val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getLong != nullptr) {
                return APersistableBundle_getLong(mPBundle, key.c_str(), val);
            }
        }
        return false;
    }

    bool getDouble(const std::string& key, double* _Nonnull val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getDouble != nullptr) {
                return APersistableBundle_getDouble(mPBundle, key.c_str(), val);
            }
        }
        return false;
    }

    static char* _Nullable stringAllocator(int32_t bufferSizeBytes, void* _Nullable) {
        return (char*)malloc(bufferSizeBytes);
    }

    bool getString(const std::string& key, std::string* _Nonnull val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getString != nullptr) {
                char* outString = nullptr;
                bool ret = APersistableBundle_getString(mPBundle, key.c_str(), &outString,
                                                        &stringAllocator, nullptr);
                if (ret && outString) {
                    *val = std::string(outString);
                }
                return ret;
            }
        }
        return false;
    }

    template <typename T>
    bool getVecInternal(int32_t (*_Nonnull getVec)(const APersistableBundle* _Nonnull,
                                                   const char* _Nonnull, T* _Nullable, int32_t),
                        const APersistableBundle* _Nonnull pBundle, const char* _Nonnull key,
                        std::vector<T>* _Nonnull vec) {
        int32_t bytes = 0;
        // call first with nullptr to get required size in bytes
        bytes = getVec(pBundle, key, nullptr, 0);
        if (bytes > 0) {
            T* newVec = (T*)malloc(bytes);
            if (newVec) {
                bytes = getVec(pBundle, key, newVec, bytes);
                int32_t elements = bytes / sizeof(T);
                vec->clear();
                for (int32_t i = 0; i < elements; i++) {
                    vec->push_back(newVec[i]);
                }
                free(newVec);
                return true;
            }
        }
        return false;
    }

    bool getBooleanVector(const std::string& key, std::vector<bool>* _Nonnull vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getBooleanVector != nullptr) {
                return getVecInternal<bool>(&APersistableBundle_getBooleanVector, mPBundle,
                                            key.c_str(), vec);
            }
        }
        return false;
    }
    bool getIntVector(const std::string& key, std::vector<int32_t>* _Nonnull vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getIntVector != nullptr) {
                return getVecInternal<int32_t>(&APersistableBundle_getIntVector, mPBundle,
                                               key.c_str(), vec);
            }
        }
        return false;
    }
    bool getLongVector(const std::string& key, std::vector<int64_t>* _Nonnull vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getLongVector != nullptr) {
                return getVecInternal<int64_t>(&APersistableBundle_getLongVector, mPBundle,
                                               key.c_str(), vec);
            }
        }
        return false;
    }
    bool getDoubleVector(const std::string& key, std::vector<double>* _Nonnull vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getDoubleVector != nullptr) {
                return getVecInternal<double>(&APersistableBundle_getDoubleVector, mPBundle,
                                              key.c_str(), vec);
            }
        }
        return false;
    }

    // Takes ownership of and frees the char** and its elements.
    // Creates a new set or vector based on the array of char*.
    template <typename T>
    T moveStringsInternal(char* _Nullable* _Nonnull strings, int32_t bufferSizeBytes) {
        if (strings && bufferSizeBytes > 0) {
            int32_t num = bufferSizeBytes / sizeof(char*);
            T ret;
            for (int32_t i = 0; i < num; i++) {
                ret.insert(ret.end(), std::string(strings[i]));
                free(strings[i]);
            }
            free(strings);
            return ret;
        }
        return T();
    }

    bool getStringVector(const std::string& key, std::vector<std::string>* _Nonnull vec) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getStringVector != nullptr) {
                int32_t bytes = APersistableBundle_getStringVector(mPBundle, key.c_str(), nullptr,
                                                                   0, &stringAllocator, nullptr);
                if (bytes > 0) {
                    char** strings = (char**)malloc(bytes);
                    if (strings) {
                        bytes = APersistableBundle_getStringVector(
                                mPBundle, key.c_str(), strings, bytes, &stringAllocator, nullptr);
                        *vec = moveStringsInternal<std::vector<std::string>>(strings, bytes);
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool getPersistableBundle(const std::string& key, PersistableBundle* _Nonnull val) {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getPersistableBundle != nullptr) {
                APersistableBundle* bundle = nullptr;
                bool ret = APersistableBundle_getPersistableBundle(mPBundle, key.c_str(), &bundle);
                if (ret) {
                    *val = PersistableBundle(bundle);
                }
                return ret;
            }
        }
        return false;
    }

    std::set<std::string> getKeys(
            int32_t (*_Nonnull getTypedKeys)(const APersistableBundle* _Nonnull pBundle,
                                             char* _Nullable* _Nullable outKeys,
                                             int32_t bufferSizeBytes,
                                             APersistableBundle_stringAllocator stringAllocator,
                                             void* _Nullable),
            const APersistableBundle* _Nonnull pBundle) {
        // call first with nullptr to get required size in bytes
        int32_t bytes = getTypedKeys(pBundle, nullptr, 0, &stringAllocator, nullptr);
        if (bytes > 0) {
            char** keys = (char**)malloc(bytes);
            if (keys) {
                bytes = getTypedKeys(pBundle, keys, bytes, &stringAllocator, nullptr);
                return moveStringsInternal<std::set<std::string>>(keys, bytes);
            }
        }
        return {};
    }

    std::set<std::string> getBooleanKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getBooleanKeys != nullptr) {
                return getKeys(&APersistableBundle_getBooleanKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getIntKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getIntKeys != nullptr) {
                return getKeys(&APersistableBundle_getIntKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getLongKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getLongKeys != nullptr) {
                return getKeys(&APersistableBundle_getLongKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getDoubleKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getDoubleKeys != nullptr) {
                return getKeys(&APersistableBundle_getDoubleKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getStringKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getStringKeys != nullptr) {
                return getKeys(&APersistableBundle_getStringKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getBooleanVectorKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getBooleanVectorKeys != nullptr) {
                return getKeys(&APersistableBundle_getBooleanVectorKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getIntVectorKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getIntVectorKeys != nullptr) {
                return getKeys(&APersistableBundle_getIntVectorKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getLongVectorKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getLongVectorKeys != nullptr) {
                return getKeys(&APersistableBundle_getLongVectorKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getDoubleVectorKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getDoubleVectorKeys != nullptr) {
                return getKeys(&APersistableBundle_getDoubleVectorKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getStringVectorKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getStringVectorKeys != nullptr) {
                return getKeys(&APersistableBundle_getStringVectorKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getPersistableBundleKeys() {
        if (__builtin_available(android __ANDROID_API_V__, *)) {
            if (APersistableBundle_getPersistableBundleKeys != nullptr) {
                return getKeys(&APersistableBundle_getPersistableBundleKeys, mPBundle);
            }
        }
        return {};
    }
    std::set<std::string> getMonKeys() {
        // :P
        return {"c(o,o)b", "c(o,o)b"};
    }

   private:
    inline APersistableBundle* _Nullable get() const { return mPBundle; }
    APersistableBundle* _Nullable mPBundle = nullptr;
};

}  // namespace aidl::android::os
