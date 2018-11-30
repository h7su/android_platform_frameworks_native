/**
 * Copyright (c) 2016, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "dumpstate"

#include "DumpstateService.h"

#include <android-base/stringprintf.h>

#include "android/os/BnDumpstate.h"

#include "DumpstateInternal.h"

using android::base::StringPrintf;

namespace android {
namespace os {

namespace {

static binder::Status exception(uint32_t code, const std::string& msg) {
    MYLOGE("%s (%d) ", msg.c_str(), code);
    return binder::Status::fromExceptionCode(code, String8(msg.c_str()));
}

static binder::Status error(uint32_t code, const std::string& msg) {
    MYLOGE("%s (%d) ", msg.c_str(), code);
    return binder::Status::fromServiceSpecificError(code, String8(msg.c_str()));
}

static void* callAndNotify(void* data) {
    Dumpstate& ds = *static_cast<Dumpstate*>(data);
    // TODO(111441001): Return status on listener.
    ds.Run();
    MYLOGE("Finished Run()\n");
    return nullptr;
}

class DumpstateToken : public BnDumpstateToken {};
}

DumpstateService::DumpstateService() : ds_(Dumpstate::GetInstance()) {
}

char const* DumpstateService::getServiceName() {
    return "dumpstate";
}

status_t DumpstateService::Start() {
    IPCThreadState::self()->disableBackgroundScheduling(true);
    status_t ret = BinderService<DumpstateService>::publish();
    if (ret != android::OK) {
        return ret;
    }
    sp<ProcessState> ps(ProcessState::self());
    ps->startThreadPool();
    ps->giveThreadPoolName();
    return android::OK;
}

binder::Status DumpstateService::setListener(const std::string& name,
                                             const sp<IDumpstateListener>& listener,
                                             bool getSectionDetails,
                                             sp<IDumpstateToken>* returned_token) {
    *returned_token = nullptr;
    if (name.empty()) {
        MYLOGE("setListener(): name not set\n");
        return binder::Status::ok();
    }
    if (listener == nullptr) {
        MYLOGE("setListener(): listener not set\n");
        return binder::Status::ok();
    }
    std::lock_guard<std::mutex> lock(lock_);
    if (ds_.listener_ != nullptr) {
        MYLOGE("setListener(%s): already set (%s)\n", name.c_str(), ds_.listener_name_.c_str());
        return binder::Status::ok();
    }

    ds_.listener_name_ = name;
    ds_.listener_ = listener;
    ds_.report_section_ = getSectionDetails;
    *returned_token = new DumpstateToken();

    return binder::Status::ok();
}

binder::Status DumpstateService::startBugreport(int, int bugreport_mode, int32_t* returned_id) {
    // TODO(111441001): return a request id here.
    *returned_id = -1;
    MYLOGI("startBugreport() with mode: %d\n", bugreport_mode);

    if (bugreport_mode != Dumpstate::BugreportMode::BUGREPORT_FULL &&
        bugreport_mode != Dumpstate::BugreportMode::BUGREPORT_INTERACTIVE &&
        bugreport_mode != Dumpstate::BugreportMode::BUGREPORT_REMOTE &&
        bugreport_mode != Dumpstate::BugreportMode::BUGREPORT_WEAR &&
        bugreport_mode != Dumpstate::BugreportMode::BUGREPORT_TELEPHONY &&
        bugreport_mode != Dumpstate::BugreportMode::BUGREPORT_WIFI) {
        return exception(binder::Status::EX_ILLEGAL_ARGUMENT,
                         StringPrintf("Invalid bugreport mode: %d", bugreport_mode));
    }

    std::unique_ptr<Dumpstate::DumpOptions> options = std::make_unique<Dumpstate::DumpOptions>();
    options->Initialize(static_cast<Dumpstate::BugreportMode>(bugreport_mode));
    ds_.SetOptions(std::move(options));

    pthread_t thread;
    status_t err = pthread_create(&thread, nullptr, callAndNotify, &ds_);
    if (err != 0) {
        return error(err, "Could not create a background thread.");
    }
    return binder::Status::ok();
}

status_t DumpstateService::dump(int fd, const Vector<String16>&) {
    dprintf(fd, "id: %d\n", ds_.id_);
    dprintf(fd, "pid: %d\n", ds_.pid_);
    dprintf(fd, "update_progress: %s\n", ds_.options_->do_progress_updates ? "true" : "false");
    dprintf(fd, "update_progress_threshold: %d\n", ds_.update_progress_threshold_);
    dprintf(fd, "last_updated_progress: %d\n", ds_.last_updated_progress_);
    dprintf(fd, "progress:\n");
    ds_.progress_->Dump(fd, "  ");
    dprintf(fd, "args: %s\n", ds_.options_->args.c_str());
    dprintf(fd, "extra_options: %s\n", ds_.options_->extra_options.c_str());
    dprintf(fd, "version: %s\n", ds_.version_.c_str());
    dprintf(fd, "bugreport_dir: %s\n", ds_.bugreport_dir_.c_str());
    dprintf(fd, "bugreport_internal_dir_: %s\n", ds_.bugreport_internal_dir_.c_str());
    dprintf(fd, "screenshot_path: %s\n", ds_.screenshot_path_.c_str());
    dprintf(fd, "log_path: %s\n", ds_.log_path_.c_str());
    dprintf(fd, "tmp_path: %s\n", ds_.tmp_path_.c_str());
    dprintf(fd, "path: %s\n", ds_.path_.c_str());
    dprintf(fd, "extra_options: %s\n", ds_.options_->extra_options.c_str());
    dprintf(fd, "base_name: %s\n", ds_.base_name_.c_str());
    dprintf(fd, "name: %s\n", ds_.name_.c_str());
    dprintf(fd, "now: %ld\n", ds_.now_);
    dprintf(fd, "is_zipping: %s\n", ds_.IsZipping() ? "true" : "false");
    dprintf(fd, "listener: %s\n", ds_.listener_name_.c_str());
    dprintf(fd, "notification title: %s\n", ds_.options_->notification_title.c_str());
    dprintf(fd, "notification description: %s\n", ds_.options_->notification_description.c_str());

    return NO_ERROR;
}
}  // namespace os
}  // namespace android
