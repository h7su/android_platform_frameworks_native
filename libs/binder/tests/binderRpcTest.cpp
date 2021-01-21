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

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <android-base/logging.h>
#include <BnBinderRpcSession.h>
#include <BnBinderRpcTest.h>
#include <android-base/logging.h>
#include <binder/Binder.h>
#include <binder/BpBinder.h>
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/RpcConnection.h>
#include <binder/RpcServer.h>
#include <gtest/gtest.h>

#include "../RpcState.h" // for debugging

namespace android {

using android::binder::Status;

#define EXPECT_OK(status) \
  do { \
    Status stat = (status); \
    EXPECT_TRUE(stat.isOk()) << stat; \
  } while(false)

class MyBinderRpcSession : public BnBinderRpcSession {
public:
    static std::atomic<int32_t> gNum;

    MyBinderRpcSession(const std::string& name) : mName(name) {
        gNum++;
    }
    Status getName(std::string* name) override {
        *name = mName;
        return Status::ok();
    }
    ~MyBinderRpcSession() {
        gNum--;
    }
private:
    std::string mName;
};
std::atomic<int32_t> MyBinderRpcSession::gNum;

class MyBinderRpcTest : public BnBinderRpcTest {
public:
    sp<RpcConnection> connection;

    Status sendString(const std::string& str) override {
        std::cout << "Child received string: " << str << std::endl;
        return Status::ok();
    }
    Status doubleString(const std::string& str, std::string* strstr) override {
        std::cout << "Child received string: " << str << std::endl;
        *strstr = str + str;
        return Status::ok();
    }
    Status countBinders(int32_t* out) override {
        if (connection == nullptr) {
            return Status::fromExceptionCode(Status::EX_NULL_POINTER);
        }
        *out = connection->state()->countBinders();
        if (*out != 1) {
            connection->state()->dump();
        }
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
    Status alwaysGiveMeTheSameBinder(sp<IBinder>* out) override {
        static sp<IBinder> binder = new BBinder;
        *out = binder;
        return Status::ok();
    }
    Status openSession(const std::string& name, sp<IBinderRpcSession>* out) override {
        *out = new MyBinderRpcSession(name);
        return Status::ok();
    }
    Status getNumOpenSessions(int32_t* out) override {
        *out = MyBinderRpcSession::gNum;
        return Status::ok();
    }

    std::mutex blockMutex;
    Status lock() override {
        blockMutex.lock();
        return Status::ok();
    }
    Status unlockInMs(int32_t ms) override {
        usleep(ms * 1000);
        blockMutex.unlock();
        return Status::ok();
    }
    Status lockUnlock() override {
        std::lock_guard<std::mutex> _l(blockMutex);
        return Status::ok();
    }

    Status die() override {
        exit(1);
    }
};

class Process {
public:
    Process(const std::function<void()>& f) {
        if (0 == (mPid = fork())) {
            // racey: assume parent doesn't crash before this is set
            prctl(PR_SET_PDEATHSIG, SIGHUP);

            f();
        }
    }
    ~Process() {
        if (mPid != 0) {
            kill(mPid, SIGKILL);
        }
    }
private:
    pid_t mPid = 0;
};

static std::string allocateSocketAddress() {
    CHECK_EQ(gettid(), getpid()) << "Parent process must decide socket ownership.";

    static size_t id = 0;

    return "/dev/binderRpcTest_" + std::to_string(id++);
};

struct ProcessConnection {
    // reference to process hosting a socket server
    Process host;

    // client connection object associated with other process
    sp<RpcConnection> connection;

    // pre-fetched root object
    sp<IBinder> rootBinder;

    ~ProcessConnection() {
        rootBinder = nullptr;
        EXPECT_NE(nullptr, connection);
        EXPECT_NE(nullptr, connection->state());
        EXPECT_EQ(0, connection->state()->countBinders()) << (connection->state()->dump(), "dump:");

        wp<RpcConnection> weakConnection = connection;
        connection = nullptr;
        EXPECT_EQ(nullptr, weakConnection.promote()) << "Leaked connection";
    }
};

// This creates a new process serving an interface on a certain number of
// threads.
ProcessConnection createRpcTestSocketServerProcess(
    size_t numThreads, const std::function<void(const sp<RpcServer>&,
                                                const sp<RpcConnection>&)>& configure) {

    std::vector<std::string> socketAddresses;
    for (size_t i = 0; i < numThreads; i++) {
        std::string addr = allocateSocketAddress();
        unlink(addr.c_str());
        socketAddresses.push_back(addr);
    }

    auto ret = ProcessConnection {
        .host = Process([&] {
            sp<RpcServer> server = RpcServer::make();
            sp<RpcConnection> connection = server->addClientConnection();
            for (const auto& addr : socketAddresses) {
                CHECK(connection->addUnixDomainServer(addr.c_str())) << addr;
            }
            configure(server, connection);

            std::vector<std::thread> pool;
            for (size_t i = 0; i + 1 < socketAddresses.size(); i++) {
                pool.push_back(std::thread([=]{ connection->join(); }));
            }
            connection->join();
            for (auto& t : pool) t.join();
        }),
        .connection = RpcConnection::make(),
    };

    // wait up to 1s for sockets to be created
    constexpr useconds_t kMaxWaitUs = 1000000;
    constexpr useconds_t kWaitDivision = 100;
    for (size_t i = 0; i < kWaitDivision; i++) {
        usleep(kMaxWaitUs / kWaitDivision);
        bool waitMore = false;
        for (const auto& addr : socketAddresses) {
            if (0 != access(addr.c_str(), F_OK)) {
                waitMore = true;
                break;
            }
        }
        if (!waitMore) break;
    }

    for (const auto& addr : socketAddresses) {
       CHECK(ret.connection->addUnixDomainClient(addr.c_str()));
    }

    ret.rootBinder = ret.connection->getRootObject();
    return ret;
}

// Process connection where the process hosts IBinderRpcTest, the server used
// for most testing here
struct BinderRpcTestProcessConnection {
    ProcessConnection processConnection;

    // pre-fetched root object
    sp<IBinder> rootBinder;

    // pre-casted root object
    sp<IBinderRpcTest> rootIface;

    ~BinderRpcTestProcessConnection() {
        int32_t remoteBinders;
        EXPECT_OK(rootIface->countBinders(&remoteBinders));
        // should only be the root binder object, iface
        EXPECT_EQ(remoteBinders, 1);

        rootIface = nullptr;
        rootBinder = nullptr;
    }
};

BinderRpcTestProcessConnection createRpcTestSocketServerProcess(size_t numThreads) {
    BinderRpcTestProcessConnection ret {
        .processConnection = createRpcTestSocketServerProcess(
            numThreads,
            [&](const sp<RpcServer>& server, const sp<RpcConnection>& connection) {
                sp<MyBinderRpcTest> service = new MyBinderRpcTest;
                server->setRootObject(service);
                service->connection = connection;  // for testing only
            }),
    };

    ret.rootBinder = ret.processConnection.rootBinder;
    ret.rootIface = interface_cast<IBinderRpcTest>(ret.rootBinder);

    return ret;
}

TEST(BinderRpc, RootObjectIsNull) {
    auto proc = createRpcTestSocketServerProcess(1,
         [] (const sp<RpcServer>& server, const sp<RpcConnection>&) {
             // this is the default, but to be explicit
             server->setRootObject(nullptr);
         });

    // retrieved by getRootObject when process is created above
    EXPECT_EQ(nullptr, proc.rootBinder);

    // make sure we can retrieve it again (process doesn't crash)
    EXPECT_EQ(nullptr, proc.connection->getRootObject());
}

TEST(BinderRpc, Ping) {
    auto proc = createRpcTestSocketServerProcess(1);
    ASSERT_NE(proc.rootBinder, nullptr);
    EXPECT_EQ(OK, proc.rootBinder->pingBinder());
}

TEST(BinderRpc, TransactionsMustBeMarkedRpc) {
    auto proc = createRpcTestSocketServerProcess(1);
    Parcel data;
    Parcel reply;
    EXPECT_EQ(BAD_TYPE, proc.rootBinder->transact(IBinder::PING_TRANSACTION, data, &reply, 0));
}

TEST(BinderRpc, UnknownTransaction) {
    auto proc = createRpcTestSocketServerProcess(1);
    Parcel data;
    data.setAttachedBinder(proc.rootBinder);
    Parcel reply;
    EXPECT_EQ(UNKNOWN_TRANSACTION, proc.rootBinder->transact(1337, data, &reply, 0));
}

TEST(BinderRpc, SendSomethingOneway) {
    auto proc = createRpcTestSocketServerProcess(1);
    EXPECT_OK(proc.rootIface->sendString("asdf"));
}

TEST(BinderRpc, SendAndGetResultBack) {
    auto proc = createRpcTestSocketServerProcess(1);
    std::string doubled;
    EXPECT_OK(proc.rootIface->doubleString("cool ", &doubled));
    EXPECT_EQ("cool cool ", doubled);
}

TEST(BinderRpc, SendAndGetResultBackBig) {
    auto proc = createRpcTestSocketServerProcess(1);
    std::string single = std::string(1024, 'a');
    std::string doubled;
    EXPECT_OK(proc.rootIface->doubleString(single, &doubled));
    EXPECT_EQ(single + single, doubled);
}

// FIXME: test that oneway ordering guarantees are preserved on mutli-threaded
// connections
// FIXME: test for FD leaks

TEST(BinderRpc, CallMeBack) {
    auto proc = createRpcTestSocketServerProcess(1);

    int32_t pingResult;
    EXPECT_OK(proc.rootIface->pingMe(new MyBinderRpcSession("foo"), &pingResult));
    EXPECT_EQ(OK, pingResult);

    EXPECT_EQ(0, MyBinderRpcSession::gNum);
}

TEST(BinderRpc, RepeatBinder) {
    auto proc = createRpcTestSocketServerProcess(1);

    sp<IBinder> inBinder = new MyBinderRpcSession("foo");
    sp<IBinder> outBinder;
    EXPECT_OK(proc.rootIface->repeatBinder(inBinder, &outBinder));
    EXPECT_EQ(inBinder, outBinder);

    wp<IBinder> weak = inBinder;
    inBinder = nullptr;
    outBinder = nullptr;

    // FIXME: this is to force reading a reply, which as a side effect
    // will read all the pending dec refs from the other process
    EXPECT_EQ(OK, proc.rootBinder->pingBinder());

    EXPECT_EQ(nullptr, weak.promote());

    EXPECT_EQ(0, MyBinderRpcSession::gNum);
}

TEST(BinderRpc, RepeatBinderNull) {
    auto proc = createRpcTestSocketServerProcess(1);

    sp<IBinder> outBinder;
    EXPECT_OK(proc.rootIface->repeatBinder(nullptr, &outBinder));
    EXPECT_EQ(nullptr, outBinder);
}

// START TESTS FOR LIMITATIONS OF SOCKET BINDER
// These are behavioral differences form regular binder, where certain usecases
// aren't supported.

TEST(BinderRpc, CannotMixBindersBetweenUnrelatedSocketConnections) {
    auto proc1 = createRpcTestSocketServerProcess(1);
    auto proc2 = createRpcTestSocketServerProcess(1);

    sp<IBinder> outBinder;
    EXPECT_EQ(INVALID_OPERATION,
              proc1.rootIface->repeatBinder(proc2.rootBinder, &outBinder).transactionError());
}

TEST(BinderRpc, CannotSendRegularBinderOverSocketBinder) {
    auto proc = createRpcTestSocketServerProcess(1);

    sp<IBinder> someRealBinder = IInterface::asBinder(defaultServiceManager());
    sp<IBinder> outBinder;
    EXPECT_EQ(INVALID_OPERATION,
              proc.rootIface->repeatBinder(someRealBinder, &outBinder).transactionError());
}

TEST(BinderRpc, CannotSendSocketBinderOverRegularBinder) {
    auto proc = createRpcTestSocketServerProcess(1);

    // for historical reasons, IServiceManager interface only returns the
    // exception code
    EXPECT_EQ(binder::Status::EX_TRANSACTION_FAILED,
              defaultServiceManager()->addService(String16("not_suspicious"), proc.rootBinder));
}

// END TESTS FOR LIMITATIONS OF SOCKET BINDER

TEST(BinderRpc, RepeatRootObject) {
    auto proc = createRpcTestSocketServerProcess(1);

    sp<IBinder> outBinder;
    EXPECT_OK(proc.rootIface->repeatBinder(proc.rootBinder, &outBinder));
    EXPECT_EQ(proc.rootBinder, outBinder);
}

TEST(BinderRpc, NestedTransactions) {
    auto proc = createRpcTestSocketServerProcess(1);

    auto nastyNester = sp<MyBinderRpcTest>::make();
    EXPECT_OK(proc.rootIface->nestMe(nastyNester, 10));

    wp<IBinder> weak = nastyNester;
    nastyNester = nullptr;
    EXPECT_EQ(nullptr, weak.promote());
}

TEST(BinderRpc, SameBinderEquality) {
    auto proc = createRpcTestSocketServerProcess(1);

    sp<IBinder> a;
    EXPECT_OK(proc.rootIface->alwaysGiveMeTheSameBinder(&a));

    sp<IBinder> b;
    EXPECT_OK(proc.rootIface->alwaysGiveMeTheSameBinder(&b));

    EXPECT_EQ(a, b);
}

TEST(BinderRpc, SameBinderEqualityWeak) {
    auto proc = createRpcTestSocketServerProcess(1);

    sp<IBinder> a;
    EXPECT_OK(proc.rootIface->alwaysGiveMeTheSameBinder(&a));
    wp<IBinder> weak = a;
    a = nullptr;

    sp<IBinder> b;
    EXPECT_OK(proc.rootIface->alwaysGiveMeTheSameBinder(&b));

    GTEST_SKIP() << "Weak binders don't work properly";
    // FIXME: crashes b/c onIncStrongAttempted unconditionally returns true, and
    // beyond that, because RpcConnection loses track of a on last strong ref
    EXPECT_EQ(b, weak.promote());
}

#define expectSessions(expected, iface) do { \
        int session; \
        EXPECT_OK((iface)->getNumOpenSessions(&session)); \
        EXPECT_EQ(expected, session); \
    } while(false)

TEST(BinderRpc, SingleSession) {
    auto proc = createRpcTestSocketServerProcess(1);

    sp<IBinderRpcSession> session;
    EXPECT_OK(proc.rootIface->openSession("aoeu", &session));
    std::string out;
    EXPECT_OK(session->getName(&out));
    EXPECT_EQ("aoeu", out);

    expectSessions(1, proc.rootIface);
    session = nullptr;
    expectSessions(0, proc.rootIface);
}

TEST(BinderRpc, ManySessions) {
    auto proc = createRpcTestSocketServerProcess(1);

    std::vector<sp<IBinderRpcSession>> sessions;

    for (size_t i = 0; i < 15; i++) {
        expectSessions(i, proc.rootIface);
        sp<IBinderRpcSession> session;
        EXPECT_OK(proc.rootIface->openSession(std::to_string(i), &session));
        sessions.push_back(session);
    }
    expectSessions(sessions.size(), proc.rootIface);
    for (size_t i = 0; i < sessions.size(); i++) {
        std::string out;
        EXPECT_OK(sessions.at(i)->getName(&out));
        EXPECT_EQ(std::to_string(i), out);
    }
    expectSessions(sessions.size(), proc.rootIface);

    while (!sessions.empty()) {
        sessions.pop_back();
        expectSessions(sessions.size(), proc.rootIface);
    }
    expectSessions(0, proc.rootIface);
}

size_t epochMillis() {
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;
    using std::chrono::seconds;
    using std::chrono::system_clock;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

TEST(BinderRpc, SaturateThreadPool) {
    constexpr size_t kNumThreads = 3;

    auto proc = createRpcTestSocketServerProcess(kNumThreads);

    EXPECT_OK(proc.rootIface->lock());

    // block all but one thread taking locks
    std::vector<std::thread> ts;
    for (size_t i = 0; i < kNumThreads - 1; i++) {
        ts.push_back(std::thread([&]{proc.rootIface->lockUnlock();}));
    }

    usleep(100000); // give chance for calls on other threads

    // other calls still work
    EXPECT_EQ(OK, proc.rootBinder->pingBinder());

    constexpr size_t blockTimeMs = 500;
    size_t epochMsBefore = epochMillis();
    // after this, we should never see a response within this time
    EXPECT_OK(proc.rootIface->unlockInMs(blockTimeMs));

    // this call should be blocked for blockTimeMs
    EXPECT_EQ(OK, proc.rootBinder->pingBinder());

    size_t epochMsAfter = epochMillis();
    EXPECT_GE(epochMsAfter, epochMsBefore + blockTimeMs) << epochMsBefore;

    for (auto& t : ts) t.join();
}

TEST(BinderRpc, ThreadingStressTest) {
    constexpr size_t kNumClientThreads = 10;
    constexpr size_t kNumServerThreads = 10;
    constexpr size_t kNumCalls = 100;

    auto proc = createRpcTestSocketServerProcess(kNumServerThreads);

    std::vector<std::thread> threads;
    for (size_t i = 0; i < kNumClientThreads; i++) {
        threads.push_back(std::thread([&] {
            for (size_t j = 0; j < kNumCalls; j++) {
                sp<IBinder> out;
                proc.rootIface->repeatBinder(proc.rootBinder, &out);
                EXPECT_EQ(proc.rootBinder, out);
            }
        }));
    }

    for (auto& t : threads) t.join();
}

TEST(BinderRpc, Die) {
    // FIXME: can we handle this generally from the library, or do we need to do
    // it as part of setup, for instance?
    signal(SIGPIPE, SIG_IGN);

    auto proc = createRpcTestSocketServerProcess(1);
    EXPECT_OK(proc.rootIface->die());
}

extern "C" int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    android::base::InitLogging(argv, android::base::StderrLogger, android::base::DefaultAborter);
    return RUN_ALL_TESTS();
}

}  // namespace android
