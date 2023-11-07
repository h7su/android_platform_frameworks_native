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

#ifdef __ANDROID__
#include <android-base/unique_fd.h>
#else
// clang-format off

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

// DO NOT INCLUDE OTHER LIBBASE HEADERS HERE!
// This file gets used in libbinder, and libbinder is used everywhere.
// Including other headers from libbase frequently results in inclusion of
// android-base/macros.h, which causes macro collisions.

#if defined(__BIONIC__)
#include <android/fdsan.h>
#endif
#if !defined(_WIN32) && !defined(__TRUSTY__)
#include <sys/socket.h>
#endif

namespace android::binder {

// Container for a file descriptor that automatically closes the descriptor as
// it goes out of scope.
//
//      unique_fd ufd(open("/some/path", "r"));
//      if (ufd.get() == -1) return error;
//
//      // Do something useful, possibly including 'return'.
//
//      return 0; // Descriptor is closed for you.
//
// See also the Pipe()/Socketpair()/Fdopen()/Fdopendir() functions in this file
// that provide interoperability with the libc functions with the same (but
// lowercase) names.
//
// unique_fd is also known as ScopedFd/ScopedFD/scoped_fd; mentioned here to help
// you find this class if you're searching for one of those names.
//
// unique_fd itself is a specialization of unique_fd_impl with a default closer.
template <typename Closer>
class unique_fd_impl final {
 public:
  unique_fd_impl() {}

  explicit unique_fd_impl(int fd) { reset(fd); }
  ~unique_fd_impl() { reset(); }

  unique_fd_impl(const unique_fd_impl&) = delete;
  void operator=(const unique_fd_impl&) = delete;
  unique_fd_impl(unique_fd_impl&& other) noexcept { reset(other.release()); }
  unique_fd_impl& operator=(unique_fd_impl&& s) noexcept {
    int fd = s.fd_;
    s.fd_ = -1;
    reset(fd);
    return *this;
  }

  [[clang::reinitializes]] void reset(int new_value = -1) {
    int previous_errno = errno;

    if (fd_ != -1) {
    //TODO
    //LOG_ALWAYS_FATAL_IF(android_fdsan_close_with_tag,
    //    "Android fdsan is not supposed to be enabled outside Android");
      ::close(fd_);
    }

    fd_ = new_value;
    errno = previous_errno;
  }

  int get() const { return fd_; }

  // TODO: convert all uses to ok()
  bool operator<(int rhs) const { return get() < rhs; }
  bool operator!=(int rhs) const { return get() != rhs; }
  bool operator==(int rhs) const { return get() == rhs; }

  // Catch bogus error checks (i.e.: "!fd" instead of "fd != -1").
  bool operator!() const = delete;

  bool ok() const { return get() >= 0; }

  int release() __attribute__((warn_unused_result)) {
    int ret = fd_;
    fd_ = -1;
    return ret;
  }

 private:
  int fd_ = -1;
};

using unique_fd = unique_fd_impl<void>;

#if !defined(_WIN32) && !defined(__TRUSTY__)

// Inline functions, so that they can be used header-only.

#if 1
// See pipe(2).
// This helper hides the details of converting to unique_fd, and also hides the
// fact that macOS doesn't support O_CLOEXEC or O_NONBLOCK directly.
template <typename Closer>
inline bool Pipe(unique_fd_impl<Closer>* read, unique_fd_impl<Closer>* write,
                 int flags = O_CLOEXEC) {
  int pipefd[2];

#if defined(__linux__)
  if (pipe2(pipefd, flags) != 0) {
    return false;
  }
#else  // defined(__APPLE__)
  if (flags & ~(O_CLOEXEC | O_NONBLOCK)) {
    return false;
  }
  if (pipe(pipefd) != 0) {
    return false;
  }

  if (flags & O_CLOEXEC) {
    if (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) != 0) {
      close(pipefd[0]);
      close(pipefd[1]);
      return false;
    }
  }
  if (flags & O_NONBLOCK) {
    if (fcntl(pipefd[0], F_SETFL, O_NONBLOCK) != 0 || fcntl(pipefd[1], F_SETFL, O_NONBLOCK) != 0) {
      close(pipefd[0]);
      close(pipefd[1]);
      return false;
    }
  }
#endif

  read->reset(pipefd[0]);
  write->reset(pipefd[1]);
  return true;
}
#endif

#if 1
// See socketpair(2).
// This helper hides the details of converting to unique_fd.
template <typename Closer>
inline bool Socketpair(int domain, int type, int protocol, unique_fd_impl<Closer>* left,
                       unique_fd_impl<Closer>* right) {
  int sockfd[2];
  if (socketpair(domain, type, protocol, sockfd) != 0) {
    return false;
  }
  left->reset(sockfd[0]);
  right->reset(sockfd[1]);
  return true;
}

// See socketpair(2).
// This helper hides the details of converting to unique_fd.
template <typename Closer>
inline bool Socketpair(int type, unique_fd_impl<Closer>* left, unique_fd_impl<Closer>* right) {
  return Socketpair(AF_UNIX, type, 0, left, right);
}
#endif

#endif  // !defined(_WIN32) && !defined(__TRUSTY__)

// A wrapper type that can be implicitly constructed from either int or
// unique_fd. This supports cases where you don't actually own the file
// descriptor, and can't take ownership, but are temporarily acting as if
// you're the owner.
//
// One example would be a function that needs to also allow
// STDERR_FILENO, not just a newly-opened fd. Another example would be JNI code
// that's using a file descriptor that's actually owned by a
// ParcelFileDescriptor or whatever on the Java side, but where the JNI code
// would like to enforce this weaker sense of "temporary ownership".
//
// If you think of unique_fd as being like std::string in that represents
// ownership, borrowed_fd is like std::string_view (and int is like const
// char*).
struct borrowed_fd {
  /* implicit */ borrowed_fd(int fd) : fd_(fd) {}  // NOLINT
  template <typename T>
  /* implicit */ borrowed_fd(const unique_fd_impl<T>& ufd) : fd_(ufd.get()) {}  // NOLINT

  int get() const { return fd_; }

  bool operator>=(int rhs) const { return get() >= rhs; }
  bool operator<(int rhs) const { return get() < rhs; }
  bool operator==(int rhs) const { return get() == rhs; }
  bool operator!=(int rhs) const { return get() != rhs; }

 private:
  int fd_ = -1;
};
}  // namespace android::binder

namespace android::base {
  using android::binder::unique_fd;
  using android::binder::borrowed_fd;

#if !defined(_WIN32) && !defined(__TRUSTY__)
  using ::android::binder::Pipe;
  using android::binder::Socketpair;
#endif
}

// clang-format on
#endif // __ANDROID__
