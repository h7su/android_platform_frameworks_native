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
#define LOG_TAG "bugreportz"

#include "bugreportz.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android/os/BnDumpstate.h>
#include <android/os/BnDumpstateListener.h>
#include <android/os/IDumpstate.h>
#include <binder/IPCThreadState.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <cutils/android_filesystem_config.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <future>

using namespace android;
using android::base::StringPrintf;

static constexpr std::string_view kDUMPSTATE_DIRECTORY{"/bugreports"};
static constexpr std::string_view kBEGIN_PREFIX{"BEGIN:"};
static constexpr std::string_view kPROGRESS_PREFIX{"PROGRESS:"};

static void write_line(int fd, const std::string& line, bool show_progress) {
    if (line.empty()) return;

    // When not invoked with the -p option, it must skip BEGIN and PROGRESS lines otherwise it
    // will break adb (which is expecting either OK or FAIL).
    if (!show_progress && (android::base::StartsWith(line, kPROGRESS_PREFIX) ||
                           android::base::StartsWith(line, kBEGIN_PREFIX)))
        return;

    android::base::WriteStringToFd(line, fd);
}

/* Create parent directories for a given |path|. Keep in sync with `create_parent_dirs` in dumpstate. */
static void CreateParentDirs(const char* path) {
    char* chp = const_cast<char*>(path);

    /* skip initial slash */
    if (chp[0] == '/') chp++;

    /* create leading directories, if necessary */
    struct stat dir_stat;
    while (chp && chp[0]) {
        chp = strchr(chp, '/');
        if (chp) {
            *chp = 0;
            if (stat(path, &dir_stat) == -1 || !S_ISDIR(dir_stat.st_mode)) {
                ALOGI("Creating directory %s\n", path);
                if (mkdir(path, 0770)) { /* drwxrwx--- */
                    ALOGE("Unable to create directory %s: %s\n", path, strerror(errno));
                } else if (chown(path, AID_SHELL, AID_SHELL)) {
                    ALOGE("Unable to change ownership of dir %s: %s\n", path, strerror(errno));
                }
            }
            *chp++ = '/';
        }
    }
}

/*
 * Get the path of a file with the extension |suffix| based on the parent |directory|,
 * device name, build ID and current localtime. The returning result is similar to the
 * one in Dumpstate::GetPath except in some bugreport mode, telephony and wifi, it would
 * have additional suffix in the base name.
 */
static std::string GetPath(const std::string& directory, const std::string& suffix) {
    std::string build_id = android::base::GetProperty("ro.build.id", "UNKNOWN_BUILD");
    std::string device_name = android::base::GetProperty("ro.product.name", "UNKNOWN_DEVICE");
    std::string base_name = StringPrintf("bugreport-%s-%s", device_name.c_str(), build_id.c_str());
    char date[80];
    time_t t = time(nullptr);
    strftime(date, sizeof(date), "%Y-%m-%d-%H-%M-%S", localtime(&t));
    return StringPrintf("%s/%s-%s%s", directory.data(), base_name.c_str(), date, suffix.c_str());
}

class DumpstateListener : public android::os::BnDumpstateListener,
                          public android::IBinder::DeathRecipient {
  public:
    DumpstateListener(const std::string& path, bool show_progress, int fd, std::promise<void>&& p)
        : out_fd_(fd), show_progress_(show_progress), pr_(std::move(p)), bugreport_path_(path) {
    }

    binder::Status onProgress(int32_t progress) override {
        if (progress == 0) {
            begin_ = true;
            write(StringPrintf("%s:%s\n", kBEGIN_PREFIX.data(), bugreport_path_.c_str()));
        } else {
            write(StringPrintf("%s:%d/100\n", kPROGRESS_PREFIX.data(), progress));
        }
        return binder::Status::ok();
    }

    binder::Status onError(int32_t error_code) override {
        std::lock_guard<std::mutex> lock(lock_);
        // Shows "*" in log_path instead of real dumpstate's pid.
        std::string log_path{GetPath(kDUMPSTATE_DIRECTORY.data(), "-dumpstate_log-*.txt")};
        write(StringPrintf(
            "FAIL:Could not create zip file, check %s for more details. Error code %d\n",
            log_path.c_str(), error_code));
        pr_.set_value();
        return binder::Status::ok();
    }

    binder::Status onFinished() override {
        std::lock_guard<std::mutex> lock(lock_);
        write(StringPrintf("OK:%s\n", bugreport_path_.c_str()));
        pr_.set_value();
        return binder::Status::ok();
    }

    binder::Status onScreenshotTaken(bool success) override {
        std::lock_guard<std::mutex> lock(lock_);
        write(StringPrintf("%s:Result of taking screenshot: %s\n", kPROGRESS_PREFIX.data(),
                           success ? "success" : "failure"));
        return binder::Status::ok();
    }

    binder::Status onUiIntensiveBugreportDumpsFinished(
        const android::String16& callingpackage) override {
        std::lock_guard<std::mutex> lock(lock_);
        write(StringPrintf("%s:Calling package of ui intensive bugreport dumps finished: %s\n",
                           kPROGRESS_PREFIX.data(), String8(callingpackage).c_str()));
        return binder::Status::ok();
    }

    void binderDied(const android::wp<::android::IBinder>& who) override {
        ALOGE("The dumpstate daemon has died [%p]", who.unsafe_get());
        std::cout << "FAIL:Binder died. Could not take the bugreport.\n";
        android::IPCThreadState::self()->stopProcess();
        exit(EXIT_FAILURE);
    }

  private:
    int out_fd_;
    bool show_progress_ = false;
    bool begin_ = false;
    std::mutex lock_;
    std::promise<void> pr_;
    std::string bugreport_path_;

    inline void write(const std::string& line) {
        ::write_line(out_fd_, line, show_progress_);
    }
};

inline int OpenForWrite(const std::string& filename) {
    return TEMP_FAILURE_RETRY(open(
        filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, S_IRUSR | S_IWUSR));
}

int bugreportz(bool show_progress) {
    sp<android::os::IDumpstate> ds;
    status_t st = getService(String16("dumpstate"), &ds);
    if (st != OK) {
        std::cout << "Fail:Unable to get service binder: 'dumpstate' status=" << st << std::endl;
        return EXIT_FAILURE;
    }

    // As a Binder server for incoming callbacks we have to initialize the pool.
    ProcessState::self()->startThreadPool();

    // Ensure DUMPSTATE_DIRECTORY exist and open zip for write
    std::string bugreport_path(GetPath(kDUMPSTATE_DIRECTORY.data(), ".zip"));
    android::base::unique_fd bugreport_fd(OpenForWrite(bugreport_path));
    // Calling API with default fd, -1, of screenshot file will fail.
    android::base::unique_fd screenshot_fd(OpenForWrite("/dev/null"));
    CreateParentDirs(bugreport_path.c_str());

    std::promise<void> p;
    std::future<void> future_done = p.get_future();
    sp<DumpstateListener> listener(
        new DumpstateListener(bugreport_path, show_progress, dup(STDOUT_FILENO), std::move(p)));
    if (android::IInterface::asBinder(ds.get())->linkToDeath(listener) != android::NO_ERROR) {
        std::cout << "FAIL:Unable to register DeathRecipient for IDumpstate\n";
        return EXIT_FAILURE;
    }
    binder::Status status = ds->startBugreport(
        /* callingUid= */ AID_SHELL, /* callingPackage= */ "", std::move(bugreport_fd),
        std::move(screenshot_fd), android::os::IDumpstate::BUGREPORT_MODE_FULL, listener,
        /* isScreenshotRequested= */ false);
    if (!status.isOk()) {
        std::cout << "FAIL:Could not take the bugreport.\n";
        return EXIT_FAILURE;
    }
    future_done.wait();
    return EXIT_SUCCESS;
}

int bugreportz_stream(int s) {
    while (1) {
        char buffer[65536];
        ssize_t bytes_read = TEMP_FAILURE_RETRY(read(s, buffer, sizeof(buffer)));
        if (bytes_read == 0) {
            break;
        } else if (bytes_read == -1) {
            // EAGAIN really means time out, so change the errno.
            if (errno == EAGAIN) {
                errno = ETIMEDOUT;
            }
            printf("FAIL:Bugreport read terminated abnormally (%s)\n", strerror(errno));
            return EXIT_FAILURE;
        }

        if (!android::base::WriteFully(android::base::borrowed_fd(STDOUT_FILENO), buffer,
                                       bytes_read)) {
            printf("Failed to write data to stdout: trying to send %zd bytes (%s)\n", bytes_read,
                   strerror(errno));
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}
