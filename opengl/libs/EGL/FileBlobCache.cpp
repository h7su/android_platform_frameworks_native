/*
 ** Copyright 2017, The Android Open Source Project
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

#include "FileBlobCache.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <log/log.h>

// Cache file header
static const char* cacheFileMagic = "EGL$";
static const size_t cacheFileHeaderSize = 8;

namespace android {

static uint32_t crc32c(const uint8_t* buf, size_t len) {
    const uint32_t polyBits = 0x82F63B78;
    uint32_t r = 0;
    for (size_t i = 0; i < len; i++) {
        r ^= buf[i];
        for (int j = 0; j < 8; j++) {
            if (r & 1) {
                r = (r >> 1) ^ polyBits;
            } else {
                r >>= 1;
            }
        }
    }
    return r;
}

FileBlobCache::FileBlobCache(size_t maxKeySize, size_t maxValueSize, size_t maxTotalSize,
        const std::string& filename)
        : BlobCache(maxKeySize, maxValueSize, maxTotalSize)
        , mFilename(filename) {
    if (mFilename.length() > 0) {
        size_t headerSize = cacheFileHeaderSize;

        int fd = open(mFilename.c_str(), O_RDONLY, 0);
        if (fd == -1) {
            if (errno != ENOENT) {
                ALOGE("error opening cache file %s: %s (%d)", mFilename.c_str(),
                        strerror(errno), errno);
            }
            return;
        }

        struct stat statBuf;
        if (fstat(fd, &statBuf) == -1) {
            ALOGE("error stat'ing cache file: %s (%d)", strerror(errno), errno);
            close(fd);
            return;
        }

        // Sanity check the size before trying to mmap it.
        size_t fileSize = statBuf.st_size;
        if (fileSize > mMaxTotalSize * 2) {
            ALOGE("cache file is too large: %#" PRIx64,
                  static_cast<off64_t>(statBuf.st_size));
            close(fd);
            return;
        }

        uint8_t* buf = reinterpret_cast<uint8_t*>(mmap(nullptr, fileSize,
                PROT_READ, MAP_PRIVATE, fd, 0));
        if (buf == MAP_FAILED) {
            ALOGE("error mmaping cache file: %s (%d)", strerror(errno),
                    errno);
            close(fd);
            return;
        }

        // Check the file magic and CRC
        size_t cacheSize = fileSize - headerSize;
        if (memcmp(buf, cacheFileMagic, 4) != 0) {
            ALOGE("cache file has bad mojo");
            close(fd);
            return;
        }
        uint32_t* crc = reinterpret_cast<uint32_t*>(buf + 4);
        if (crc32c(buf + headerSize, cacheSize) != *crc) {
            ALOGE("cache file failed CRC check");
            close(fd);
            return;
        }

        int err = unflatten(buf + headerSize, cacheSize);
        if (err < 0) {
            ALOGE("error reading cache contents: %s (%d)", strerror(-err),
                    -err);
            munmap(buf, fileSize);
            close(fd);
            return;
        }

        munmap(buf, fileSize);
        close(fd);
    }
}

void FileBlobCache::writeToFile() {
    if (mFilename.length() > 0) {
        size_t cacheSize = getFlattenedSize();
        size_t headerSize = cacheFileHeaderSize;
        const char* fname = mFilename.c_str();

        // Try to create the file with no permissions so we can write it
        // without anyone trying to read it.
        int fd = open(fname, O_CREAT | O_EXCL | O_RDWR, 0);
        if (fd == -1) {
            if (errno == EEXIST) {
                // The file exists, delete it and try again.
                if (unlink(fname) == -1) {
                    // No point in retrying if the unlink failed.
                    ALOGE("error unlinking cache file %s: %s (%d)", fname,
                            strerror(errno), errno);
                    return;
                }
                // Retry now that we've unlinked the file.
                fd = open(fname, O_CREAT | O_EXCL | O_RDWR, 0);
            }
            if (fd == -1) {
                ALOGE("error creating cache file %s: %s (%d)", fname,
                        strerror(errno), errno);
                return;
            }
        }

        size_t fileSize = headerSize + cacheSize;

        uint8_t* buf = new uint8_t [fileSize];
        if (!buf) {
            ALOGE("error allocating buffer for cache contents: %s (%d)",
                    strerror(errno), errno);
            close(fd);
            unlink(fname);
            return;
        }

        int err = flatten(buf + headerSize, cacheSize);
        if (err < 0) {
            ALOGE("error writing cache contents: %s (%d)", strerror(-err),
                    -err);
            delete [] buf;
            close(fd);
            unlink(fname);
            return;
        }

        // Write the file magic and CRC
        memcpy(buf, cacheFileMagic, 4);
        uint32_t* crc = reinterpret_cast<uint32_t*>(buf + 4);
        *crc = crc32c(buf + headerSize, cacheSize);

        if (write(fd, buf, fileSize) == -1) {
            ALOGE("error writing cache file: %s (%d)", strerror(errno),
                    errno);
            delete [] buf;
            close(fd);
            unlink(fname);
            return;
        }

        delete [] buf;
        fchmod(fd, S_IRUSR);
        close(fd);
    }
}

}
