#include <android/binder_libbinder.h>
#include <android/persistable_bundle.h>
#include <binder/PersistableBundle.h>
#include <log/log.h>

__BEGIN_DECLS

struct APersistableBundle {
    APersistableBundle(const APersistableBundle& pBundle) : mPBundle(pBundle.mPBundle) {}
    APersistableBundle() = default;
    android::os::PersistableBundle mPBundle;
};

APersistableBundle* _Nullable APersistableBundle_new() {
    return new (std::nothrow) APersistableBundle();
}

APersistableBundle* _Nullable APersistableBundle_dup(const APersistableBundle* pBundle) {
    if (pBundle) {
        return new APersistableBundle(*pBundle);
    } else {
        return new APersistableBundle();
    }
}

void APersistableBundle_delete(APersistableBundle* pBundle) {
    if (pBundle) {
        free(pBundle);
    }
}

bool APersistableBundle_isEqual(const APersistableBundle* lhs, const APersistableBundle* rhs) {
    if (lhs && rhs) {
        return lhs->mPBundle == rhs->mPBundle;
    } else {
        return false;
    }
}

binder_status_t APersistableBundle_readFromParcel(const AParcel* parcel,
                                                  APersistableBundle* _Nullable* outPBundle) {
    if (!parcel || !outPBundle) return STATUS_BAD_VALUE;
    APersistableBundle* newPBundle = APersistableBundle_new();
    if (newPBundle == nullptr) return STATUS_NO_MEMORY;
    binder_status_t status =
            newPBundle->mPBundle.readFromParcel(AParcel_viewPlatformParcel(parcel));
    if (status == STATUS_OK) {
        *outPBundle = newPBundle;
    }
    return status;
}

binder_status_t APersistableBundle_writeToParcel(const APersistableBundle* pBundle,
                                                 AParcel* parcel) {
    if (!parcel || !pBundle) return STATUS_BAD_VALUE;
    return pBundle->mPBundle.writeToParcel(AParcel_viewPlatformParcel(parcel));
}

size_t APersistableBundle_size(APersistableBundle* pBundle) {
    return pBundle->mPBundle.size();
}
size_t APersistableBundle_erase(APersistableBundle* pBundle, const std::string& key) {
    return pBundle->mPBundle.erase(android::String16(key.c_str()));
}
void APersistableBundle_putBoolean(APersistableBundle* pBundle, const std::string& key, bool val) {
    return pBundle->mPBundle.putBoolean(android::String16(key.c_str()), val);
}
void APersistableBundle_putInt(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putLong(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putDouble(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putString(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putBooleanVector(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putIntVector(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putLongVector(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putDoubleVector(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putStringVector(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_putPersistableBundle(APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getBoolean(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getInt(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getLong(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getDouble(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getString(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getBooleanVector(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getIntVector(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getLongVector(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getDoubleVector(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getStringVector(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getPersistableBundle(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getBooleanKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getIntKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getLongKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getDoubleKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getStringKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getBooleanVectorKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getIntVectorKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getLongVectorKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getDoubleVectorKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getStringVectorKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}
void APersistableBundle_getPersistableBundleKeys(const APersistableBundle* pBundle) {
    (void)pBundle;
}

__END_DECLS
