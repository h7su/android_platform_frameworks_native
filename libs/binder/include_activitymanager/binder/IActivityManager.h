/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef __ANDROID_VNDK__

#include <android/app/IProcessObserver.h>
#include <binder/IInterface.h>
#include <binder/IUidObserver.h>

namespace android {

// ------------------------------------------------------------------------------------

class IActivityManager : public IInterface
{
public:
    DECLARE_META_INTERFACE(ActivityManager)
    class RunningAppProcessInfo;

    virtual int openContentUri(const String16& stringUri) = 0;
    virtual status_t registerUidObserver(const sp<IUidObserver>& observer,
                                     const int32_t event,
                                     const int32_t cutpoint,
                                     const String16& callingPackage) = 0;
    virtual status_t registerUidObserverForUids(const sp<IUidObserver>& observer,
                                                const int32_t event, const int32_t cutpoint,
                                                const String16& callingPackage,
                                                const int32_t uids[], size_t nUids,
                                                /*out*/ sp<IBinder>& observerToken) = 0;
    virtual status_t unregisterUidObserver(const sp<IUidObserver>& observer) = 0;
    virtual status_t addUidToObserver(const sp<IBinder>& observerToken,
                                      const String16& callingPackage, int32_t uid) = 0;
    virtual status_t removeUidFromObserver(const sp<IBinder>& observerToken,
                                           const String16& callingPackage, int32_t uid) = 0;
    virtual bool isUidActive(const uid_t uid, const String16& callingPackage) = 0;
    virtual int32_t getUidProcessState(const uid_t uid, const String16& callingPackage) = 0;
    virtual status_t checkPermission(const String16& permission,
                                    const pid_t pid,
                                    const uid_t uid,
                                    int32_t* outResult) = 0;
    virtual status_t logFgsApiBegin(int32_t apiType, int32_t appUid, int32_t appPid) = 0;
    virtual status_t logFgsApiEnd(int32_t apiType, int32_t appUid, int32_t appPid) = 0;
    virtual status_t logFgsApiStateChanged(int32_t apiType, int32_t state, int32_t appUid,
                                           int32_t appPid) = 0;
    virtual status_t registerProcessObserver(const sp<app::IProcessObserver>& observer) = 0;
    virtual status_t unregisterProcessObserver(const sp<app::IProcessObserver>& observer) = 0;
    virtual status_t getRunningAppProcesses(::std::vector<RunningAppProcessInfo>* output) = 0;

    enum {
        OPEN_CONTENT_URI_TRANSACTION = IBinder::FIRST_CALL_TRANSACTION,
        REGISTER_UID_OBSERVER_TRANSACTION,
        UNREGISTER_UID_OBSERVER_TRANSACTION,
        REGISTER_UID_OBSERVER_FOR_UIDS_TRANSACTION,
        ADD_UID_TO_OBSERVER_TRANSACTION,
        REMOVE_UID_FROM_OBSERVER_TRANSACTION,
        IS_UID_ACTIVE_TRANSACTION,
        GET_UID_PROCESS_STATE_TRANSACTION,
        CHECK_PERMISSION_TRANSACTION,
        LOG_FGS_API_BEGIN_TRANSACTION,
        LOG_FGS_API_END_TRANSACTION,
        LOG_FGS_API_STATE_CHANGED_TRANSACTION,
        REGISTER_PROCESS_OBSERVER,
        UNREGISTER_PROCESS_OBSERVER,
        GET_RUNNING_APP_PROCESSES,
    };

    class RunningAppProcessInfo : public ::android::Parcelable {
    public:
        // The name of the process that this object is associated with
        ::std::string process_name;

        // The pid of this process; 0 if none
        int pid;

        // The user id of this process.
        int uid;

        // All packages that have been loaded into the process.
        std::vector<::std::string> pkg_list;

        // Additional packages loaded into the process as dependency.
        std::vector<::std::string> pkg_deps;

        // Flags of information.  May be any of <FLAG_CANT_SAVE_STATE>.
        int flags;

        // Last memory trim level reported to the process: corresponds to
        // the values supplied to android.content.ComponentCallbacks2#onTrimMemory(int)
        // ComponentCallbacks2.onTrimMemory(int).
        int last_trim_level;

        // The relative importance level that the system places on this process.
        // These constants are numbered so that "more important" values are
        // always smaller than "less important" values.
        int importance;

        // An additional ordering within a particular {@link #importance}
        // category, providing finer-grained information about the relative
        // utility of processes within a category.  This number means nothing
        // except that a smaller values are more recently used (and thus
        // more important).  Currently an LRU value is only maintained for
        // the {@link #IMPORTANCE_CACHED} category, though others may
        // be maintained in the future.
        int lru;

        // The reason for {@link #importance}, if any.
        int importance_reason_code;

        // For the specified values of {@link #importanceReasonCode}, this
        // is the process ID of the other process that is a client of this
        // process.  This will be 0 if no other process is using this one.
        int importance_reason_pid;

        // For the specified values of {@link #importanceReasonCode}, this
        // is the name of the component that is being used in this process.
        ::std::string important_reason_component_package;
        ::std::string important_reason_component_class;

        // When {@link #importanceReasonPid} is non-0, this is the importance
        // of the other pid.
        int importance_reason_importance;

        // Current process state, as per PROCESS_STATE_* constants.
        int process_state;

        // Whether the app is focused in multi-window environment.
        bool is_focused;

        // Copy of {@link com.android.server.am.ProcessRecord#lastActivityTime} of the process.
        int64_t last_activity_time;

        ::android::status_t readFromParcel(const ::android::Parcel* _aidl_parcel) final;

        ::android::status_t writeToParcel(::android::Parcel*) const final;
    };
};

// ------------------------------------------------------------------------------------

} // namespace android

#else // __ANDROID_VNDK__
#error "This header is not visible to vendors"
#endif // __ANDROID_VNDK__
