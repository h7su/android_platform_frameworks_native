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

#ifndef ANDROID_INSTALLD_UNIQUE_FILE_H
#define ANDROID_INSTALLD_UNIQUE_FILE_H

#include <functional>
#include <string>

namespace android {
namespace installd {

// A file management helper that serves two purposes:
//
// 1. Closes the file description on destruction, similar unique_fd.
// 2. Runs a cleanup function on after close, if not cancelled.
//
// The class does not assume the relationship between the given fd and file path.
//
// Example:
//
//   UniqueFile file(open(...),
//                           filepath,
//                           [](const std::string& path) {
//                               unlink(path.c_str());
//                           });
//   if (file.fd() == -1) {
//       // Error opening...
//   }
//
//   ...
//   if (error) {
//       // At this point, when the UniqueFile is destructed, the cleanup function will run
//       // (e.g. to delete the file) after the fd is closed.
//       return -1;
//   }
//
//   (Success case)
//   file.DisableCleanup();
//   // At this point, when the UniqueFile is destructed, the cleanup function will not run
//   // (e.g. leaving the file around) after the fd is closed.
//
class UniqueFile {
 private:
    using CleanUpFunction = std::function<void (const std::string&)>;

 public:
    UniqueFile();
    UniqueFile(int value, std::string path);
    UniqueFile(int value, std::string path, CleanUpFunction cleanup);
    UniqueFile(UniqueFile&& other);
    ~UniqueFile();

    UniqueFile& operator=(UniqueFile&& other);

    int fd() const {
        return value_;
    }

    const std::string& path() const {
      return path_;
    }

    void DisableAutoClose() {
        auto_close_ = false;
    }

    void DisableCleanup() {
        do_cleanup_ = false;
    }

    void reset();
    void reset(int new_value, const std::string& path, CleanUpFunction new_cleanup = nullptr);

    // This creates UniqueFile with temporary work file.
    // There can be existing file in the path and regardless of existing file, a temporary work
    // file is created and fd() returns the temporary work file.
    // When the UniqueFile is destructed, it will either rename temporary file to the given
    // path name (=when DisableCleanup() is called) or keep the original file and delete the
    // temporary file (=when DisableCleanup() is not called). In both cases, after destruction,
    // the temporary file no longer exists.
    // If temporary file already exists when this is called, existing temporary file will be deleted
    // as it can be a left-over from abnormal situation like system crash.
    static UniqueFile CreateWritableFileWithTmpWorkFile(const std::string& path, int permissions);

    // Remove the specified file together with tmp file generated for the path from
    // CreateWritableFileWithTmpWorkFile call.
    static void RemoveFileAndTmpFile(const std::string& path);

 private:
    void release();

    int value_;
    std::string path_;
    CleanUpFunction cleanup_;
    bool do_cleanup_;
    bool auto_close_;
    bool has_tmp_file_ = false;
};

}  // namespace installd
}  // namespace android

#endif  // ANDROID_INSTALLD_UNIQUE_FILE_H
