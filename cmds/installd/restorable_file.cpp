/*
 * Copyright (C) 2021 The Android Open Source Project
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

#include "restorable_file.h"

#include <string>

#include <fcntl.h>
#include <unistd.h>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

namespace {

constexpr char kTmpFileSuffix[] = ".tmp";

std::string GetTmpFilePath(const std::string& path) {
    return android::base::StringPrintf("%s%s", path.c_str(), kTmpFileSuffix);
}

void UnlinkPossiblyNonExistingFile(const std::string& path) {
    if (unlink(path.c_str()) < 0) {
        if (errno != ENOENT && errno != EROFS) { // EROFS reported even if it does not exist.
            PLOG(ERROR) << "Cannot unlink: " << path;
        }
    }
}

} // namespace

namespace android {
namespace installd {

RestorableFile::RestorableFile() : RestorableFile(-1, "") {}

RestorableFile::RestorableFile(int value, const std::string& path) : unique_file_(value, path) {
    // As cleanup is null, this does not make much difference but use unique_file_ only for closing
    // tmp file.
    unique_file_.DisableCleanup();
}

RestorableFile::~RestorableFile() {
    reset();
}

void RestorableFile::reset() {
    // need to copy before reset clears it.
    std::string path(unique_file_.path());
    unique_file_.reset();
    if (!path.empty()) {
        UnlinkPossiblyNonExistingFile(GetTmpFilePath(path));
    }
}

void RestorableFile::CommitWorkFile() {
    std::string path(unique_file_.path());
    unique_file_.reset();
    if (!path.empty()) {
        if (rename(GetTmpFilePath(path).c_str(), path.c_str()) < 0) {
            PLOG(ERROR) << "Cannot rename " << GetTmpFilePath(path) << " to " << path;
        }
    }
}

const UniqueFile& RestorableFile::GetUniqueFile() {
    return unique_file_;
}

RestorableFile RestorableFile::CreateWritableFile(const std::string& path, int permissions) {
    std::string tmp_file_path = GetTmpFilePath(path);
    // If old tmp file exists, delete it.
    UnlinkPossiblyNonExistingFile(tmp_file_path);
    int fd = -1;
    if (!path.empty()) {
        fd = open(tmp_file_path.c_str(), O_RDWR | O_CREAT, permissions);
        if (fd < 0) {
            PLOG(ERROR) << "Cannot create file: " << tmp_file_path;
        }
    }
    RestorableFile rf(fd, path);
    return rf;
}

void RestorableFile::RemoveAllFiles(const std::string& path) {
    UnlinkPossiblyNonExistingFile(GetTmpFilePath(path));
    UnlinkPossiblyNonExistingFile(path);
}

} // namespace installd
} // namespace android
