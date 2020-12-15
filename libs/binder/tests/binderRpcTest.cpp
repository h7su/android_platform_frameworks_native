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

#include <unistd.h>
#include <sys/prctl.h>

#include <cstdlib>
#include <iostream>

#include <android-base/logging.h>
#include <BnBinderRpcSession.h>
#include <BnBinderRpcTest.h>
#include <android-base/logging.h>
#include <binder/Binder.h>
#include <binder/BpBinder.h>
#include <binder/IServiceManager.h>
#include <binder/RpcConnection.h>
#include <binder/RpcServer.h>
#include <gtest/gtest.h>

#include "../RpcState.h" // for debugging

namespace android {

using android::binder::Status;

const char* kSock = "/dev/some_socket";

#define EXPECT_OK(status) \
  do { \
    Status stat = (status); \
    EXPECT_TRUE(stat.isOk()) << stat; \
  } while(false)

// FIXME: atomic for multithreading
static int gNumSessions = 0;

class MyBinderRpcSession : public BnBinderRpcSession {
public:
    MyBinderRpcSession(const std::string& name) : mName(name) {
        gNumSessions++;
    }
    Status getName(std::string* name) override {
        *name = mName;
        return Status::ok();
    }
    ~MyBinderRpcSession() {
        CHECK(gNumSessions > 0) << gNumSessions;
        gNumSessions--;
    }
private:
    std::string mName;
};

class MyBinderRpcTest : public BnBinderRpcTest {
public:
    Status sendString(const std::string& str) override {
        std::cout << "Child received string: " << str << std::endl;
        return Status::ok();
    }
    Status doubleString(const std::string& str, std::string* strstr) override {
        std::cout << "Child received string: " << str << std::endl;
        *strstr = str + str;
        return Status::ok();
    }
    Status pingMe(const sp<IBinder>& binder, int32_t* out) override {
        if (binder == nullptr) {
            std::cout << "Received null binder!" << std::endl;
            return Status::fromExceptionCode(Status::EX_NULL_POINTER);
        }
        *out = binder->pingBinder();
        return Status::ok();
    }
    Status repeatBinder(const sp<IBinder>& binder, sp<IBinder>* out) override {
        *out = binder;
        return Status::ok();
    }
    Status nestMe(const sp<IBinderRpcTest>& binder, int count) override {
        if (count <= 0) return Status::ok();
        return binder->nestMe(this, count - 1);
    }
    Status openSession(const std::string& name, sp<IBinderRpcSession>* out) override {
        *out = new MyBinderRpcSession(name);
        return Status::ok();
    }
    Status getNumOpenSessions(int32_t* out) override {
        *out = gNumSessions;
        return Status::ok();
    }
};

static sp<RpcConnection> gConnection;

static sp<IBinder> getServerBinder() {
    if (gConnection == nullptr) {
        std::cout << "NULL CONNECTION!!!" << std::endl;
        abort();
    }

    return gConnection->getRootObject();
}
static sp<IBinderRpcTest> getServerInterface() {
    return interface_cast<IBinderRpcTest>(getServerBinder());
}

TEST(BinderRpc, DidntBreakRegularBinder) {
    EXPECT_EQ(OK, IInterface::asBinder(defaultServiceManager())->pingBinder());
}

TEST(BinderRpc, Ping) {
    sp<IBinder> binder = getServerBinder();
    EXPECT_EQ(OK, binder->pingBinder());
}

TEST(BinderRpc, TransactionsMustBeMarked) {
    sp<IBinder> binder = getServerBinder();

    Parcel data;
    Parcel reply;
    EXPECT_EQ(BAD_TYPE, binder->transact(IBinder::PING_TRANSACTION, data, &reply, 0));
}

TEST(BinderRpc, SendSomethingOneway) {
    EXPECT_OK(getServerInterface()->sendString("asdf"));
}

TEST(BinderRpc, SendAndGetResultBack) {
    std::string doubled;
    EXPECT_OK(getServerInterface()->doubleString("cool ", &doubled));
    EXPECT_EQ("cool cool ", doubled);
}

// FIXME: add test force write objects to parcel, and make sure it is rejected
// FIXME: add test can't write file descriptor
// FIXME: test multiple servers/connections to the same process
// FIXME: test chained/layer connections (passing binder through procs
// a->b->c->a
// FIXME: test that oneway ordering guarantees are preserved on mutli-threaded
// connections
// FIXME: add test for server processing unknown command
// FIXME: test for BpBinder getting destroyed
// FIXME: test for # of addresses to BpBinder stored in the process
// FIXME: test for FD leaks

TEST(BinderRpc, CallMeBack) {
    sp<IBinder> binder = new BBinder;
    int32_t pingResult;
    EXPECT_OK(getServerInterface()->pingMe(binder, &pingResult));
    EXPECT_EQ(OK, pingResult);
}

TEST(BinderRpc, RepeatBinder) {
    sp<IBinder> binder = new BBinder;
    sp<IBinder> outBinder;
    EXPECT_OK(getServerInterface()->repeatBinder(binder, &outBinder));
    EXPECT_EQ(binder, outBinder);

    wp<IBinder> weak = binder;
    binder = nullptr;
    outBinder = nullptr;

    // FIXME: this is to force reading a reply, which as a side effect
    // will read all the pending dec refs from the other process
    EXPECT_EQ(OK, getServerBinder()->pingBinder());

    EXPECT_EQ(nullptr, weak.promote());
}

TEST(BinderRpc, NestedTransactions) {
    auto nastyNester = sp<MyBinderRpcTest>::make();
    EXPECT_OK(getServerInterface()->nestMe(nastyNester, 10));

    // FIXME: this is to force reading a reply, which as a side effect
    // will read all the pending dec refs from the other process
    EXPECT_EQ(OK, getServerBinder()->pingBinder());

    wp<IBinder> weak = nastyNester;
    nastyNester = nullptr;
    EXPECT_EQ(nullptr, weak.promote());
}

#define expectSessions(expected) do { \
        int session; \
        EXPECT_OK(getServerInterface()->getNumOpenSessions(&session)); \
        EXPECT_EQ(expected, session); \
    } while(false)

TEST(BinderRpc, SingleSession) {
    sp<IBinderRpcSession> session;
    EXPECT_OK(getServerInterface()->openSession("aoeu", &session));
    std::string out;
    EXPECT_OK(session->getName(&out));
    EXPECT_EQ("aoeu", out);

    expectSessions(1);
    session = nullptr;  // FIXME: should need flush commands???
    expectSessions(0);
}

// FIXME: many flush commands should be needed
TEST(BinderRpc, ManySessions) {
    std::vector<sp<IBinderRpcSession>> sessions;

    for (size_t i = 0; i < 15; i++) {
        expectSessions(i);
        sp<IBinderRpcSession> session;
        EXPECT_OK(getServerInterface()->openSession(std::to_string(i), &session));
        sessions.push_back(session);
    }
    expectSessions(sessions.size());
    for (size_t i = 0; i < sessions.size(); i++) {
        std::string out;
        EXPECT_OK(sessions.at(i)->getName(&out));
        EXPECT_EQ(std::to_string(i), out);
    }
    expectSessions(sessions.size());

    while (!sessions.empty()) {
        sessions.pop_back();
        expectSessions(sessions.size());
    }
    expectSessions(0);
}

extern "C" int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    android::base::InitLogging(argv, android::base::StderrLogger, android::base::DefaultAborter);

    // - FIXME - stop using unix domain sockets, or implement some other
    // mechanism to make sure it's not in use
    unlink(kSock);

    pid_t childPid = fork();
    if (childPid == 0) {
        prctl(PR_SET_PDEATHSIG, SIGHUP);  // technically racey
        sp<RpcServer> server = RpcServer::makeUnixServer(kSock);
        server->setRootObject(new MyBinderRpcTest());
        server->join();
        return EXIT_FAILURE;
    }

    // FIXME
    usleep(10000); // time for connection to be created (might be refused) ?

    // FIXME: this connection should be automatically created in the background
    // by whatever method we get ahold of a server. Currently, we just magically
    // know the address (and the top-level binder is always registered with the
    // first identifier, 0).
    gConnection = RpcConnection::connect(kSock);

    std::cout << "This pid: " << getpid() << std::endl;
    std::cout << "Child pid: " << childPid << std::endl;

    int ret = RUN_ALL_TESTS();
    RpcState::self().dump();
    gConnection = nullptr;
    usleep(100000); // time for logs
    return ret;
}

}  // namespace android
