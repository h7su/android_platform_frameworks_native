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

#define LOG_TAG "RpcServer"

#include <inttypes.h>

#include <thread>
#include <vector>

#include <binder/Functional.h>
#include <binder/Parcel.h>
#include <binder/RpcServer.h>
#include <binder/RpcTransportRaw.h>
#include <log/log.h>

#include "BuildFlags.h"
#include "FdTrigger.h"
#include "OS.h"
#include "RpcState.h"
#include "RpcTransportUtils.h"
#include "RpcWireFormat.h"
#include "Utils.h"

#ifdef __TRUSTY__
#include "trusty/TrustyStatus.h"
#else
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "RpcSocketAddress.h"
#endif

namespace android {

constexpr size_t kSessionIdBytes = 32;

using namespace android::binder::impl;
using android::binder::borrowed_fd;
using android::binder::unique_fd;

RpcServer::RpcServer(std::unique_ptr<RpcTransportCtx> ctx) : mCtx(std::move(ctx)) {}
RpcServer::~RpcServer() {
    RpcMutexUniqueLock _l(mLock);
    LOG_ALWAYS_FATAL_IF(mShutdownTrigger != nullptr, "Must call shutdown() before destructor");
}

sp<RpcServer> RpcServer::make(std::unique_ptr<RpcTransportCtxFactory> rpcTransportCtxFactory) {
    // Default is without TLS.
    if (rpcTransportCtxFactory == nullptr)
        rpcTransportCtxFactory = binder::os::makeDefaultRpcTransportCtxFactory();
    auto ctx = rpcTransportCtxFactory->newServerCtx();
    if (ctx == nullptr) return nullptr;
    return sp<RpcServer>::make(std::move(ctx));
}

#ifndef __TRUSTY__
status_t RpcServer::setupUnixDomainSocketBootstrapServer(unique_fd bootstrapFd) {
    return setupExternalServer(std::move(bootstrapFd), &RpcServer::recvmsgSocketConnection);
}

status_t RpcServer::setupUnixDomainServer(const char* path) {
    return setupSocketServer(UnixSocketAddress(path));
}

status_t RpcServer::setupVsockServer(unsigned int bindCid, unsigned int port) {
    return setupSocketServer(VsockSocketAddress(bindCid, port));
}

status_t RpcServer::setupInetServer(const char* address, unsigned int port,
                                    unsigned int* assignedPort) {
    if (assignedPort != nullptr) *assignedPort = 0;
    auto aiStart = InetSocketAddress::getAddrInfo(address, port);
    if (aiStart == nullptr) return UNKNOWN_ERROR;
    for (auto ai = aiStart.get(); ai != nullptr; ai = ai->ai_next) {
        if (ai->ai_addr == nullptr) continue;
        InetSocketAddress socketAddress(ai->ai_addr, ai->ai_addrlen, address, port);
        if (status_t status = setupSocketServer(socketAddress); status != OK) {
            continue;
        }

        LOG_ALWAYS_FATAL_IF(socketAddress.addr()->sa_family != AF_INET, "expecting inet");
        sockaddr_in addr{};
        socklen_t len = sizeof(addr);
        if (0 != getsockname(mServer.fd.get(), reinterpret_cast<sockaddr*>(&addr), &len)) {
            int savedErrno = errno;
            ALOGE("Could not getsockname at %s: %s", socketAddress.toString().c_str(),
                  strerror(savedErrno));
            return -savedErrno;
        }
        LOG_ALWAYS_FATAL_IF(len != sizeof(addr), "Wrong socket type: len %zu vs len %zu",
                            static_cast<size_t>(len), sizeof(addr));
        unsigned int realPort = ntohs(addr.sin_port);
        LOG_ALWAYS_FATAL_IF(port != 0 && realPort != port,
                            "Requesting inet server on %s but it is set up on %u.",
                            socketAddress.toString().c_str(), realPort);

        if (assignedPort != nullptr) {
            *assignedPort = realPort;
        }

        return OK;
    }
    ALOGE("None of the socket address resolved for %s:%u can be set up as inet server.", address,
          port);
    return UNKNOWN_ERROR;
}
#endif // __TRUSTY__

#ifdef TRUSTY_USERSPACE
status_t RpcServer::setupTrustyServer(tipc_hset* handleSet, std::string&& portName,
                                      std::shared_ptr<const TrustyPortAcl>&& portAcl,
                                      size_t msgMaxSize) {
    LOG_ALWAYS_FATAL_IF(portName.empty(), "Tipc port name should not be empty");
    LOG_ALWAYS_FATAL_IF(portAcl.get() == nullptr, "Tipc port ACL should be valid");
    LOG_ALWAYS_FATAL_IF(!mTrustyPortName.empty(), "Trusty server already set up");

    RpcMutexLockGuard _l(mLock);
    mTrustyPortName = std::move(portName);
    mTrustyPortAcl = std::move(portAcl);

    mTipcPort.name = mTrustyPortName.c_str();
    mTipcPort.msg_max_size = msgMaxSize;
    mTipcPort.msg_queue_len = 2;
    mTipcPort.priv = this;

    // TODO(b/266741352): follow-up to prevent needing this in the future
    // Trusty needs to be set to the latest stable version that is in prebuilts there.
    LOG_ALWAYS_FATAL_IF(!setProtocolVersion(0));

    if (mTrustyPortAcl) {
        // Initialize the array of pointers to uuids.
        // The pointers in mUuidPtrs should stay valid across moves of
        // RpcServer (the addresses of a std::vector's elements
        // shouldn't change when the vector is moved), but would be invalidated
        // by a copy which is why we disable the copy constructor and assignment
        // operator for RpcServer.
        auto numUuids = mTrustyPortAcl->uuids.size();
        mTrustyUuidPtrs.resize(numUuids);
        for (size_t i = 0; i < numUuids; i++) {
            mTrustyUuidPtrs[i] = &mTrustyPortAcl->uuids[i];
        }

        // Copy the contents of portAcl into the tipc_port_acl structure that we
        // pass to tipc_add_service
        mTipcPortAcl.flags = mTrustyPortAcl->flags;
        mTipcPortAcl.uuid_num = numUuids;
        mTipcPortAcl.uuids = mTrustyUuidPtrs.data();
        mTipcPortAcl.extra_data = mTrustyPortAcl->extraData;

        mTipcPort.acl = &mTipcPortAcl;
    } else {
        mTipcPort.acl = nullptr;
    }

    int rc = tipc_add_service(handleSet, &mTipcPort, 1, 0, &kTipcOps);
    if (rc != NO_ERROR) {
        ALOGE("Failed to create RpcServer: can't add service: %d", rc);
        return statusFromTrusty(rc);
    }

    return OK;
}

int RpcServer::handleTipcConnect(const tipc_port* port, handle_t chan, const uuid* peer,
                                 void** ctx_p) {
    auto* server = reinterpret_cast<RpcServer*>(const_cast<void*>(port->priv));
    server->mShutdownTrigger = FdTrigger::make();
    server->mConnectingThreads[rpc_this_thread::get_id()] = RpcMaybeThread();

    int rc = NO_ERROR;
    auto joinFn = [&](sp<RpcSession>&& session, RpcSession::PreJoinSetupResult&& result) {
        if (result.status != OK) {
            rc = statusToTrusty(result.status);
            return;
        }

        /* Save the session and connection for the other callbacks */
        auto* channelContext = new (std::nothrow) TipcChannelContext;
        if (channelContext == nullptr) {
            rc = ERR_NO_MEMORY;
            return;
        }

        channelContext->session = std::move(session);
        channelContext->connection = std::move(result.connection);

        *ctx_p = channelContext;
    };

    // We need to duplicate the channel handle here because the tipc library
    // owns the original handle and closes is automatically on channel cleanup.
    // We use dup() because Trusty does not have fcntl().
    // NOLINTNEXTLINE(android-cloexec-dup)
    handle_t chanDup = dup(chan);
    if (chanDup < 0) {
        return chanDup;
    }
    unique_fd clientFd(chanDup);
    android::RpcTransportFd transportFd(std::move(clientFd));

    std::array<uint8_t, RpcServer::kRpcAddressSize> addr;
    constexpr size_t addrLen = sizeof(*peer);
    memcpy(addr.data(), peer, addrLen);

    if (server->mConnectionFilter != nullptr && !server->mConnectionFilter(addr.data(), addrLen)) {
        ALOGE("Dropped client connection fd %d", chan);
        return ERR_ACCESS_DENIED;
    }

    RpcServer::establishConnection(sp<RpcServer>::fromExisting(server), std::move(transportFd),
                                   addr, addrLen, joinFn);

    return rc;
}

int RpcServer::handleTipcMessage(const tipc_port* /*port*/, handle_t /*chan*/, void* ctx) {
    auto* channelContext = reinterpret_cast<TipcChannelContext*>(ctx);
    LOG_ALWAYS_FATAL_IF(channelContext == nullptr,
                        "bad state: message received on uninitialized channel");

    auto& session = channelContext->session;
    auto& connection = channelContext->connection;
    status_t status =
            session->state()->drainCommands(connection, session, RpcState::CommandType::ANY);
    if (status != OK) {
        LOG_RPC_DETAIL("Binder connection thread closing w/ status %s",
                       statusToString(status).c_str());
    }

    return NO_ERROR;
}

void RpcServer::handleTipcDisconnect(const tipc_port* /*port*/, handle_t /*chan*/, void* ctx) {
    auto* channelContext = reinterpret_cast<TipcChannelContext*>(ctx);
    if (channelContext == nullptr) {
        // Connections marked "incoming" (outgoing from the server's side)
        // do not have a valid channel context because joinFn does not get
        // called for them. We ignore them here.
        return;
    }

    auto& session = channelContext->session;
    (void)session->shutdownAndWait(false);
}

void RpcServer::handleTipcChannelCleanup(void* ctx) {
    auto* channelContext = reinterpret_cast<TipcChannelContext*>(ctx);
    if (channelContext == nullptr) {
        return;
    }

    auto& session = channelContext->session;
    auto& connection = channelContext->connection;
    LOG_ALWAYS_FATAL_IF(!session->removeIncomingConnection(connection),
                        "bad state: connection object guaranteed to be in list");

    delete channelContext;
}
#endif // TRUSTY_USERSPACE

void RpcServer::setMaxThreads(size_t threads) {
    LOG_ALWAYS_FATAL_IF(threads <= 0, "RpcServer is useless without threads");
    LOG_ALWAYS_FATAL_IF(mJoinThreadRunning, "Cannot set max threads while running");
    mMaxThreads = threads;
}

size_t RpcServer::getMaxThreads() {
    return mMaxThreads;
}

bool RpcServer::setProtocolVersion(uint32_t version) {
    if (!RpcState::validateProtocolVersion(version)) {
        return false;
    }

    mProtocolVersion = version;
    return true;
}

void RpcServer::setSupportedFileDescriptorTransportModes(
        const std::vector<RpcSession::FileDescriptorTransportMode>& modes) {
    mSupportedFileDescriptorTransportModes.reset();
    for (RpcSession::FileDescriptorTransportMode mode : modes) {
        mSupportedFileDescriptorTransportModes.set(static_cast<size_t>(mode));
    }
}

void RpcServer::setRootObject(const sp<IBinder>& binder) {
    RpcMutexLockGuard _l(mLock);
    mRootObjectFactory = nullptr;
    mRootObjectWeak = mRootObject = binder;
}

void RpcServer::setRootObjectWeak(const wp<IBinder>& binder) {
    RpcMutexLockGuard _l(mLock);
    mRootObject.clear();
    mRootObjectFactory = nullptr;
    mRootObjectWeak = binder;
}
void RpcServer::setPerSessionRootObject(
        std::function<sp<IBinder>(wp<RpcSession> session, const void*, size_t)>&& makeObject) {
    RpcMutexLockGuard _l(mLock);
    mRootObject.clear();
    mRootObjectWeak.clear();
    mRootObjectFactory = std::move(makeObject);
}

void RpcServer::setConnectionFilter(std::function<bool(const void*, size_t)>&& filter) {
    RpcMutexLockGuard _l(mLock);
    LOG_ALWAYS_FATAL_IF(mShutdownTrigger != nullptr, "Already joined");
    mConnectionFilter = std::move(filter);
}

void RpcServer::setServerSocketModifier(std::function<void(borrowed_fd)>&& modifier) {
    RpcMutexLockGuard _l(mLock);
    LOG_ALWAYS_FATAL_IF(mServer.fd.ok(), "Already started");
    mServerSocketModifier = std::move(modifier);
}

sp<IBinder> RpcServer::getRootObject() {
    RpcMutexLockGuard _l(mLock);
    bool hasWeak = mRootObjectWeak.unsafe_get();
    sp<IBinder> ret = mRootObjectWeak.promote();
    ALOGW_IF(hasWeak && ret == nullptr, "RpcServer root object is freed, returning nullptr");
    return ret;
}

std::vector<uint8_t> RpcServer::getCertificate(RpcCertificateFormat format) {
    RpcMutexLockGuard _l(mLock);
    return mCtx->getCertificate(format);
}

#ifndef __TRUSTY__
static void joinRpcServer(sp<RpcServer>&& thiz) {
    thiz->join();
}

void RpcServer::start() {
    RpcMutexLockGuard _l(mLock);
    LOG_ALWAYS_FATAL_IF(mJoinThread.get(), "Already started!");
    mJoinThread =
            std::make_unique<RpcMaybeThread>(&joinRpcServer, sp<RpcServer>::fromExisting(this));
    rpcJoinIfSingleThreaded(*mJoinThread);
}

status_t RpcServer::acceptSocketConnection(const RpcServer& server, RpcTransportFd* out) {
    RpcTransportFd clientSocket(unique_fd(TEMP_FAILURE_RETRY(
            accept4(server.mServer.fd.get(), nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK))));
    if (!clientSocket.fd.ok()) {
        int savedErrno = errno;
        ALOGE("Could not accept4 socket: %s", strerror(savedErrno));
        return -savedErrno;
    }

    *out = std::move(clientSocket);
    return OK;
}

status_t RpcServer::recvmsgSocketConnection(const RpcServer& server, RpcTransportFd* out) {
    int zero = 0;
    iovec iov{&zero, sizeof(zero)};
    std::vector<std::variant<unique_fd, borrowed_fd>> fds;

    ssize_t num_bytes = binder::os::receiveMessageFromSocket(server.mServer, &iov, 1, &fds);
    if (num_bytes < 0) {
        int savedErrno = errno;
        ALOGE("Failed recvmsg: %s", strerror(savedErrno));
        return -savedErrno;
    }
    if (num_bytes == 0) {
        return DEAD_OBJECT;
    }
    if (fds.size() != 1) {
        ALOGE("Expected exactly one fd from recvmsg, got %zu", fds.size());
        return -EINVAL;
    }

    unique_fd fd(std::move(std::get<unique_fd>(fds.back())));
    if (status_t res = binder::os::setNonBlocking(fd); res != OK) return res;

    *out = RpcTransportFd(std::move(fd));
    return OK;
}

void RpcServer::join() {

    {
        RpcMutexLockGuard _l(mLock);
        LOG_ALWAYS_FATAL_IF(!mServer.fd.ok(), "RpcServer must be setup to join.");
        LOG_ALWAYS_FATAL_IF(mAcceptFn == nullptr, "RpcServer must have an accept() function");
        LOG_ALWAYS_FATAL_IF(mShutdownTrigger != nullptr, "Already joined");
        mJoinThreadRunning = true;
        mShutdownTrigger = FdTrigger::make();
        LOG_ALWAYS_FATAL_IF(mShutdownTrigger == nullptr, "Cannot create join signaler");
    }

    status_t status;
    while ((status = mShutdownTrigger->triggerablePoll(mServer, POLLIN)) == OK) {
        std::array<uint8_t, kRpcAddressSize> addr;
        static_assert(addr.size() >= sizeof(sockaddr_storage), "kRpcAddressSize is too small");
        socklen_t addrLen = addr.size();

        RpcTransportFd clientSocket;
        if ((status = mAcceptFn(*this, &clientSocket)) != OK) {
            if (status == DEAD_OBJECT)
                break;
            else
                continue;
        }

        LOG_RPC_DETAIL("accept on fd %d yields fd %d", mServer.fd.get(), clientSocket.fd.get());

        if (getpeername(clientSocket.fd.get(), reinterpret_cast<sockaddr*>(addr.data()),
                        &addrLen)) {
            ALOGE("Could not getpeername socket: %s", strerror(errno));
            continue;
        }

        if (mConnectionFilter != nullptr && !mConnectionFilter(addr.data(), addrLen)) {
            ALOGE("Dropped client connection fd %d", clientSocket.fd.get());
            continue;
        }

        {
            RpcMutexLockGuard _l(mLock);
            RpcMaybeThread thread =
                    RpcMaybeThread(&RpcServer::establishConnection,
                                   sp<RpcServer>::fromExisting(this), std::move(clientSocket), addr,
                                   addrLen, RpcSession::join);

            auto& threadRef = mConnectingThreads[thread.get_id()];
            threadRef = std::move(thread);
            rpcJoinIfSingleThreaded(threadRef);
        }
    }
    LOG_RPC_DETAIL("RpcServer::join exiting with %s", statusToString(status).c_str());

    if constexpr (kEnableRpcThreads) {
        RpcMutexLockGuard _l(mLock);
        mJoinThreadRunning = false;
    } else {
        // Multi-threaded builds clear this in shutdown(), but we need it valid
        // so the loop above exits cleanly
        mShutdownTrigger = nullptr;
    }
    mShutdownCv.notify_all();
}

bool RpcServer::shutdown() {
    RpcMutexUniqueLock _l(mLock);
    if (mShutdownTrigger == nullptr) {
        LOG_RPC_DETAIL("Cannot shutdown. No shutdown trigger installed (already shutdown, or not "
                       "joined yet?)");
        return false;
    }

    mShutdownTrigger->trigger();

    for (auto& [id, session] : mSessions) {
        (void)id;
        // server lock is a more general lock
        RpcMutexLockGuard _lSession(session->mMutex);
        session->mShutdownTrigger->trigger();
    }

    if constexpr (!kEnableRpcThreads) {
        // In single-threaded mode we're done here, everything else that
        // needs to happen should be at the end of RpcServer::join()
        return true;
    }

    while (mJoinThreadRunning || !mConnectingThreads.empty() || !mSessions.empty()) {
        if (std::cv_status::timeout == mShutdownCv.wait_for(_l, std::chrono::seconds(1))) {
            ALOGE("Waiting for RpcServer to shut down (1s w/o progress). Join thread running: %d, "
                  "Connecting threads: "
                  "%zu, Sessions: %zu. Is your server deadlocked?",
                  mJoinThreadRunning, mConnectingThreads.size(), mSessions.size());
        }
    }

    // At this point, we know join() is about to exit, but the thread that calls
    // join() may not have exited yet.
    // If RpcServer owns the join thread (aka start() is called), make sure the thread exits;
    // otherwise ~thread() may call std::terminate(), which may crash the process.
    // If RpcServer does not own the join thread (aka join() is called directly),
    // then the owner of RpcServer is responsible for cleaning up that thread.
    if (mJoinThread.get()) {
        mJoinThread->join();
        mJoinThread.reset();
    }

    mServer = RpcTransportFd();

    LOG_RPC_DETAIL("Finished waiting on shutdown.");

    mShutdownTrigger = nullptr;
    return true;
}
#endif // __TRUSTY__

std::vector<sp<RpcSession>> RpcServer::listSessions() {
    RpcMutexLockGuard _l(mLock);
    std::vector<sp<RpcSession>> sessions;
    for (auto& [id, session] : mSessions) {
        (void)id;
        sessions.push_back(session);
    }
    return sessions;
}

size_t RpcServer::numUninitializedSessions() {
    RpcMutexLockGuard _l(mLock);
    return mConnectingThreads.size();
}

void RpcServer::establishConnection(
        sp<RpcServer>&& server, RpcTransportFd clientFd, std::array<uint8_t, kRpcAddressSize> addr,
        size_t addrLen,
        std::function<void(sp<RpcSession>&&, RpcSession::PreJoinSetupResult&&)>&& joinFn) {
    // mShutdownTrigger can only be cleared once connection threads have joined.
    // It must be set before this thread is started
    LOG_ALWAYS_FATAL_IF(server->mShutdownTrigger == nullptr);
    LOG_ALWAYS_FATAL_IF(server->mCtx == nullptr);

    status_t status = OK;

    int clientFdForLog = clientFd.fd.get();
    auto client = server->mCtx->newTransport(std::move(clientFd), server->mShutdownTrigger.get());
    if (client == nullptr) {
        ALOGE("Dropping accept4()-ed socket because sslAccept fails");
        status = DEAD_OBJECT;
        // still need to cleanup before we can return
    } else {
        LOG_RPC_DETAIL("Created RpcTransport %p for client fd %d", client.get(), clientFdForLog);
    }

    RpcConnectionHeader header;
    if (status == OK) {
        iovec iov{&header, sizeof(header)};
        status = client->interruptableReadFully(server->mShutdownTrigger.get(), &iov, 1,
                                                std::nullopt, /*ancillaryFds=*/nullptr);
        if (status != OK) {
            ALOGE("Failed to read ID for client connecting to RPC server: %s",
                  statusToString(status).c_str());
            // still need to cleanup before we can return
        }
    }

    std::vector<uint8_t> sessionId;
    if (status == OK) {
        if (header.sessionIdSize > 0) {
            if (header.sessionIdSize == kSessionIdBytes) {
                sessionId.resize(header.sessionIdSize);
                iovec iov{sessionId.data(), sessionId.size()};
                status = client->interruptableReadFully(server->mShutdownTrigger.get(), &iov, 1,
                                                        std::nullopt, /*ancillaryFds=*/nullptr);
                if (status != OK) {
                    ALOGE("Failed to read session ID for client connecting to RPC server: %s",
                          statusToString(status).c_str());
                    // still need to cleanup before we can return
                }
            } else {
                ALOGE("Malformed session ID. Expecting session ID of size %zu but got %" PRIu16,
                      kSessionIdBytes, header.sessionIdSize);
                status = BAD_VALUE;
            }
        }
    }

    bool incoming = false;
    uint32_t protocolVersion = 0;
    bool requestingNewSession = false;

    if (status == OK) {
        incoming = header.options & RPC_CONNECTION_OPTION_INCOMING;
        protocolVersion = std::min(header.version,
                                   server->mProtocolVersion.value_or(RPC_WIRE_PROTOCOL_VERSION));
        requestingNewSession = sessionId.empty();

        if (requestingNewSession) {
            RpcNewSessionResponse response{
                    .version = protocolVersion,
            };

            iovec iov{&response, sizeof(response)};
            status = client->interruptableWriteFully(server->mShutdownTrigger.get(), &iov, 1,
                                                     std::nullopt, nullptr);
            if (status != OK) {
                ALOGE("Failed to send new session response: %s", statusToString(status).c_str());
                // still need to cleanup before we can return
            }
        }
    }

    RpcMaybeThread thisThread;
    sp<RpcSession> session;
    {
        RpcMutexUniqueLock _l(server->mLock);

        auto threadId = server->mConnectingThreads.find(rpc_this_thread::get_id());
        LOG_ALWAYS_FATAL_IF(threadId == server->mConnectingThreads.end(),
                            "Must establish connection on owned thread");
        thisThread = std::move(threadId->second);
        auto detachGuardLambda = [&]() {
            thisThread.detach();
            _l.unlock();
            server->mShutdownCv.notify_all();
        };
        auto detachGuard = make_scope_guard(std::ref(detachGuardLambda));
        server->mConnectingThreads.erase(threadId);

        if (status != OK || server->mShutdownTrigger->isTriggered()) {
            return;
        }

        if (requestingNewSession) {
            if (incoming) {
                ALOGE("Cannot create a new session with an incoming connection, would leak");
                return;
            }

            // Uniquely identify session at the application layer. Even if a
            // client/server use the same certificates, if they create multiple
            // sessions, we still want to distinguish between them.
            sessionId.resize(kSessionIdBytes);
            size_t tries = 0;
            do {
                // don't block if there is some entropy issue
                if (tries++ > 5) {
                    ALOGE("Cannot find new address: %s",
                          HexString(sessionId.data(), sessionId.size()).c_str());
                    return;
                }

                auto status = binder::os::getRandomBytes(sessionId.data(), sessionId.size());
                if (status != OK) {
                    ALOGE("Failed to read random session ID: %s", strerror(-status));
                    return;
                }
            } while (server->mSessions.end() != server->mSessions.find(sessionId));

            session = sp<RpcSession>::make(nullptr);
            session->setMaxIncomingThreads(server->mMaxThreads);
            if (!session->setProtocolVersion(protocolVersion)) return;

            if (header.fileDescriptorTransportMode <
                        server->mSupportedFileDescriptorTransportModes.size() &&
                server->mSupportedFileDescriptorTransportModes.test(
                        header.fileDescriptorTransportMode)) {
                session->setFileDescriptorTransportMode(
                        static_cast<RpcSession::FileDescriptorTransportMode>(
                                header.fileDescriptorTransportMode));
            } else {
                ALOGE("Rejecting connection: FileDescriptorTransportMode is not supported: %hhu",
                      header.fileDescriptorTransportMode);
                return;
            }

            // if null, falls back to server root
            sp<IBinder> sessionSpecificRoot;
            if (server->mRootObjectFactory != nullptr) {
                sessionSpecificRoot =
                        server->mRootObjectFactory(wp<RpcSession>(session), addr.data(), addrLen);
                if (sessionSpecificRoot == nullptr) {
                    ALOGE("Warning: server returned null from root object factory");
                }
            }

            if (!session->setForServer(server,
                                       sp<RpcServer::EventListener>::fromExisting(
                                               static_cast<RpcServer::EventListener*>(
                                                       server.get())),
                                       sessionId, sessionSpecificRoot)) {
                ALOGE("Failed to attach server to session");
                return;
            }

            server->mSessions[sessionId] = session;
        } else {
            auto it = server->mSessions.find(sessionId);
            if (it == server->mSessions.end()) {
                ALOGE("Cannot add thread, no record of session with ID %s",
                      HexString(sessionId.data(), sessionId.size()).c_str());
                return;
            }
            session = it->second;
        }

        if (incoming) {
            LOG_ALWAYS_FATAL_IF(OK != session->addOutgoingConnection(std::move(client), true),
                                "server state must already be initialized");
            return;
        }

        detachGuard.release();
        session->preJoinThreadOwnership(std::move(thisThread));
    }

    auto setupResult = session->preJoinSetup(std::move(client));

    // avoid strong cycle
    server = nullptr;

    joinFn(std::move(session), std::move(setupResult));
}

#ifndef __TRUSTY__
status_t RpcServer::setupSocketServer(const RpcSocketAddress& addr) {
    LOG_RPC_DETAIL("Setting up socket server %s", addr.toString().c_str());
    LOG_ALWAYS_FATAL_IF(hasServer(), "Each RpcServer can only have one server.");

    unique_fd socket_fd(TEMP_FAILURE_RETRY(
            socket(addr.addr()->sa_family, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0)));
    if (!socket_fd.ok()) {
        int savedErrno = errno;
        ALOGE("Could not create socket at %s: %s", addr.toString().c_str(), strerror(savedErrno));
        return -savedErrno;
    }

    if (addr.addr()->sa_family == AF_INET || addr.addr()->sa_family == AF_INET6) {
        int noDelay = 1;
        int result =
                setsockopt(socket_fd.get(), IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay));
        if (result < 0) {
            int savedErrno = errno;
            ALOGE("Could not set TCP_NODELAY on  %s", strerror(savedErrno));
            return -savedErrno;
        }
    }

    {
        RpcMutexLockGuard _l(mLock);
        if (mServerSocketModifier != nullptr) {
            mServerSocketModifier(socket_fd);
        }
    }

    if (0 != TEMP_FAILURE_RETRY(bind(socket_fd.get(), addr.addr(), addr.addrSize()))) {
        int savedErrno = errno;
        ALOGE("Could not bind socket at %s: %s", addr.toString().c_str(), strerror(savedErrno));
        return -savedErrno;
    }

    return setupRawSocketServer(std::move(socket_fd));
}

status_t RpcServer::setupRawSocketServer(unique_fd socket_fd) {
    LOG_ALWAYS_FATAL_IF(!socket_fd.ok(), "Socket must be setup to listen.");

    // Right now, we create all threads at once, making accept4 slow. To avoid hanging the client,
    // the backlog is increased to a large number.
    // TODO(b/189955605): Once we create threads dynamically & lazily, the backlog can be reduced
    //  to 1.
    if (0 != TEMP_FAILURE_RETRY(listen(socket_fd.get(), 50 /*backlog*/))) {
        int savedErrno = errno;
        ALOGE("Could not listen initialized Unix socket: %s", strerror(savedErrno));
        return -savedErrno;
    }
    if (status_t status = setupExternalServer(std::move(socket_fd)); status != OK) {
        ALOGE("Another thread has set up server while calling setupSocketServer. Race?");
        return status;
    }
    return OK;
}
#endif // __TRUSTY__

void RpcServer::onSessionAllIncomingThreadsEnded(const sp<RpcSession>& session) {
    const std::vector<uint8_t>& id = session->mId;
    LOG_ALWAYS_FATAL_IF(id.empty(), "Server sessions must be initialized with ID");
    LOG_RPC_DETAIL("Dropping session with address %s", HexString(id.data(), id.size()).c_str());

    RpcMutexLockGuard _l(mLock);
    auto it = mSessions.find(id);
    LOG_ALWAYS_FATAL_IF(it == mSessions.end(), "Bad state, unknown session id %s",
                        HexString(id.data(), id.size()).c_str());
    LOG_ALWAYS_FATAL_IF(it->second != session, "Bad state, session has id mismatch %s",
                        HexString(id.data(), id.size()).c_str());
    (void)mSessions.erase(it);
}

void RpcServer::onSessionIncomingThreadEnded() {
    mShutdownCv.notify_all();
}

bool RpcServer::hasServer() {
    RpcMutexLockGuard _l(mLock);
    return mServer.fd.ok();
}

unique_fd RpcServer::releaseServer() {
    RpcMutexLockGuard _l(mLock);
    return std::move(mServer.fd);
}

status_t RpcServer::setupExternalServer(
        unique_fd serverFd, std::function<status_t(const RpcServer&, RpcTransportFd*)>&& acceptFn) {
    RpcMutexLockGuard _l(mLock);
    if (mServer.fd.ok()) {
        ALOGE("Each RpcServer can only have one server.");
        return INVALID_OPERATION;
    }
    mServer = std::move(serverFd);
    mAcceptFn = std::move(acceptFn);
    return OK;
}

status_t RpcServer::setupExternalServer(unique_fd serverFd) {
    return setupExternalServer(std::move(serverFd), &RpcServer::acceptSocketConnection);
}

bool RpcServer::hasActiveRequests() {
    RpcMutexLockGuard _l(mLock);
    for (const auto& [_, session] : mSessions) {
        if (session->hasActiveRequests()) {
            return true;
        }
    }
    return !mServer.isInPollingState();
}

} // namespace android
