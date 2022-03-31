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

#define LOG_TAG "RpcTrustyTransport"

#include <trusty_ipc.h>
#include <uapi/err.h>

#include <binder/RpcSession.h>
#include <binder/RpcTransportTrusty.h>
#include <log/log.h>

#include "../FdTrigger.h"
#include "../RpcState.h"

using android::base::Error;
using android::base::Result;

namespace android {

namespace {

// RpcTransport for Trusty.
class RpcTransportTrusty : public RpcTransport {
public:
    explicit RpcTransportTrusty(android::base::unique_fd socket) : mSocket(std::move(socket)) {}
    ~RpcTransportTrusty() {
        if (mHaveMessage) {
            put_msg(mSocket.get(), mMessageInfo.id);
        }
    }

    status_t peek(void* buf, size_t size, size_t* out_size) override {
        auto status = ensureMessage(false);
        if (status != OK) {
            return status;
        }

        if (!mHaveMessage) {
            return WOULD_BLOCK;
        }

        iovec iov{
                .iov_base = buf,
                .iov_len = std::min(size, mMessageInfo.len - mMessageOffset),
        };
        ipc_msg_t msg{
                .num_iov = 1,
                .iov = &iov,
                .num_handles = 0,
                .handles = nullptr,
        };
        int rc = read_msg(mSocket.get(), mMessageInfo.id, mMessageOffset, &msg);
        if (rc < 0) {
            return RpcSession::statusFromTrusty(rc);
        }

        *out_size = static_cast<size_t>(rc);
        return OK;
    }

    status_t interruptableWriteFully(FdTrigger* fdTrigger, iovec* iovs, int niovs,
                                     const std::function<status_t()>& altPoll) override {
        if (niovs < 0) {
            return BAD_VALUE;
        }

        size_t size = 0;
        for (int i = 0; i < niovs; i++) {
            size += iovs[i].iov_len;
        }

        ipc_msg_t msg{
                .num_iov = static_cast<uint32_t>(niovs),
                .iov = iovs,
                .num_handles = 0,
                .handles = nullptr,
        };
        int rc = send_msg(mSocket.get(), &msg);
        if (rc < 0) {
            return RpcSession::statusFromTrusty(rc);
        }
        /* TODO: on partial send, advance the iovecs and retry */
        LOG_ALWAYS_FATAL_IF(static_cast<size_t>(rc) != size,
                            "Sent the wrong number of bytes %d!=%zu", rc, size);
        return OK;
    }

    status_t interruptableReadFully(FdTrigger* fdTrigger, iovec* iovs, int niovs,
                                    const std::function<status_t()>& altPoll) override {
        if (niovs < 0) {
            return BAD_VALUE;
        }
        // If iovs has one or more empty vectors at the end and
        // we somehow advance past all the preceding vectors and
        // pass some or all of the empty ones to sendmsg/recvmsg,
        // the call will return processSize == 0. In that case
        // we should be returning OK but instead return DEAD_OBJECT.
        // To avoid this problem, we make sure here that the last
        // vector at iovs[niovs - 1] has a non-zero length.
        while (niovs > 0 && iovs[niovs - 1].iov_len == 0) {
            niovs--;
        }
        if (niovs == 0) {
            // The vectors are all empty, so we have nothing to read.
            return OK;
        }

        auto status = ensureMessage(true);
        if (status != OK) {
            return status;
        }

        while (true) {
            ipc_msg_t msg{
                    .num_iov = static_cast<uint32_t>(niovs),
                    .iov = iovs,
                    .num_handles = 0,
                    .handles = nullptr,
            };
            int rc = read_msg(mSocket.get(), mMessageInfo.id, mMessageOffset, &msg);
            if (rc < 0) {
                return RpcSession::statusFromTrusty(rc);
            }

            size_t processSize = static_cast<size_t>(rc);
            mMessageOffset += processSize;
            LOG_ALWAYS_FATAL_IF(mMessageOffset > mMessageInfo.len);

            while (processSize > 0 && niovs > 0) {
                auto& iov = iovs[0];
                if (processSize < iov.iov_len) {
                    // Advance the base of the current iovec
                    iov.iov_base = reinterpret_cast<char*>(iov.iov_base) + processSize;
                    iov.iov_len -= processSize;
                    break;
                }

                // The current iovec was fully written
                processSize -= iov.iov_len;
                iovs++;
                niovs--;
            }
            if (niovs == 0) {
                LOG_ALWAYS_FATAL_IF(processSize > 0,
                                    "Reached the end of iovecs "
                                    "with %zd bytes remaining",
                                    processSize);

                /* Release the message if we're done */
                if (mMessageOffset == mMessageInfo.len) {
                    put_msg(mSocket.get(), mMessageInfo.id);
                    mHaveMessage = false;
                }

                return OK;
            }
        }
    }

private:
    status_t ensureMessage(bool wait) {
        int rc;
        if (mHaveMessage) {
            if (mMessageOffset < mMessageInfo.len) {
                return OK;
            }

            rc = put_msg(mSocket.get(), mMessageInfo.id);
            if (rc < 0) {
                return RpcSession::statusFromTrusty(rc);
            }
            mHaveMessage = false;
        }

        /* TODO: interruptible wait, maybe with a timeout??? */
        uevent uevt;
        rc = ::wait(mSocket.get(), &uevt, wait ? INFINITE_TIME : 0);
        if (rc < 0) {
            if (rc == ERR_TIMED_OUT && !wait) {
                // If we timed out with wait==false, then there's no message
                return OK;
            }
            return RpcSession::statusFromTrusty(rc);
        }
        if (!(uevt.event & IPC_HANDLE_POLL_MSG)) {
            /* No message, terminate here and leave mHaveMessage false */
            return OK;
        }

        rc = get_msg(mSocket.get(), &mMessageInfo);
        if (rc < 0) {
            return RpcSession::statusFromTrusty(rc);
        }

        mHaveMessage = true;
        mMessageOffset = 0;
        return OK;
    }

    base::unique_fd mSocket;

    bool mHaveMessage = false;
    ipc_msg_info mMessageInfo;
    size_t mMessageOffset;
};

// RpcTransportCtx for Trusty.
class RpcTransportCtxTrusty : public RpcTransportCtx {
public:
    std::unique_ptr<RpcTransport> newTransport(android::base::unique_fd fd,
                                               FdTrigger*) const override {
        return std::make_unique<RpcTransportTrusty>(std::move(fd));
    }
    std::vector<uint8_t> getCertificate(RpcCertificateFormat) const override { return {}; }
};

} // namespace

std::unique_ptr<RpcTransportCtx> RpcTransportCtxFactoryTrusty::newServerCtx() const {
    return std::make_unique<RpcTransportCtxTrusty>();
}

std::unique_ptr<RpcTransportCtx> RpcTransportCtxFactoryTrusty::newClientCtx() const {
    return std::make_unique<RpcTransportCtxTrusty>();
}

const char* RpcTransportCtxFactoryTrusty::toCString() const {
    return "trusty";
}

std::unique_ptr<RpcTransportCtxFactory> RpcTransportCtxFactoryTrusty::make() {
    return std::unique_ptr<RpcTransportCtxFactoryTrusty>(new RpcTransportCtxFactoryTrusty());
}

} // namespace android
