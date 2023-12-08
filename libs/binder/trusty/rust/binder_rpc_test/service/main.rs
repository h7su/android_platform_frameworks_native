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
#![allow(unused)]
use binder::{
    BinderFeatures, IBinder, Interface, ParcelFileDescriptor, SpIBinder, Status, StatusCode, Strong,
};
use binder_rpc_test_aidl::aidl::IBinderRpcCallback::IBinderRpcCallback;
use binder_rpc_test_aidl::aidl::IBinderRpcSession::{BnBinderRpcSession, IBinderRpcSession};
use binder_rpc_test_aidl::aidl::IBinderRpcTest::{BnBinderRpcTest, IBinderRpcTest};
use binder_rpc_test_session::MyBinderRpcSession;
use rpcbinder::RpcServer;
use std::rc::Rc;
use std::sync::Mutex;
use tipc::{service_dispatcher, wrap_service, Manager, PortCfg};

const RUST_SERVICE_PORT: &str = "com.android.trusty.rust.binderRpcTestService.V1";

// -----------------------------------------------------------------------------

static SESSION_COUNT: Mutex<i32> = Mutex::new(0);
static HOLD_BINDER: Mutex<Option<SpIBinder>> = Mutex::new(None);
static SAME_BINDER: Mutex<Option<SpIBinder>> = Mutex::new(None);

#[derive(Debug, Default)]
struct TestService {
    port: i32,
    m_name: String,
}

impl TestService {
    fn new(name: &str) -> Self {
        *SESSION_COUNT.lock().unwrap() += 1;
        Self { m_name: name.to_string(), ..Default::default() }
    }

    fn get_instance_count() -> i32 {
        *SESSION_COUNT.lock().unwrap()
    }
}

impl Drop for TestService {
    fn drop(&mut self) {
        *SESSION_COUNT.lock().unwrap() -= 1;
    }
}

impl Interface for TestService {}

impl IBinderRpcSession for TestService {
    fn getName(&self) -> Result<String, Status> {
        Ok(self.m_name.clone())
    }
}

impl IBinderRpcTest for TestService {
    fn sendString(&self, _: &str) -> Result<(), Status> {
        // This is a oneway function, so caller returned immediately and gives back an Ok(()) regardless of what this returns
        Ok(())
    }
    fn doubleString(&self, s: &str) -> Result<String, Status> {
        let ss = [s, s].concat();
        Ok(ss)
    }
    fn getClientPort(&self) -> Result<i32, Status> {
        Ok(self.port)
    }
    fn countBinders(&self) -> Result<Vec<i32>, Status> {
        // TODO #### Where do I get server reference to look at sessions?  Is that the same as the service (self)? ###
        todo!()
    }
    fn getNullBinder(&self) -> Result<SpIBinder, Status> {
        Err(Status::from(StatusCode::UNKNOWN_TRANSACTION))
    }
    fn pingMe(&self, binder: &SpIBinder) -> Result<i32, Status> {
        match binder.clone().ping_binder() {
            Ok(n) => Ok(StatusCode::OK as i32),
            Err(e) => Err(Status::from(e)),
        }
    }
    fn repeatBinder(&self, binder: Option<&SpIBinder>) -> Result<Option<SpIBinder>, Status> {
        match binder {
            Some(x) => Ok(Some(x.clone())),
            None => Err(Status::from(StatusCode::BAD_VALUE)),
        }
    }
    fn holdBinder(&self, binder: Option<&SpIBinder>) -> Result<(), Status> {
        *HOLD_BINDER.lock().unwrap() = binder.cloned();
        Ok(())
    }
    fn getHeldBinder(&self) -> Result<Option<SpIBinder>, Status> {
        Ok((*HOLD_BINDER.lock().unwrap()).clone())
    }
    fn nestMe(
        &self,
        binder: &Strong<(dyn IBinderRpcTest + 'static)>,
        count: i32,
    ) -> Result<(), Status> {
        if count < 0 {
            Ok(())
        } else {
            binder.nestMe(binder, count - 1)
        }
    }
    fn alwaysGiveMeTheSameBinder(&self) -> Result<SpIBinder, Status> {
        let mut locked = SAME_BINDER.lock().unwrap();
        if *locked == None {
            *locked = Some(
                BnBinderRpcTest::new_binder(TestService::default(), BinderFeatures::default())
                    .as_binder(),
            )
        }
        Ok((*locked).clone().unwrap())
    }
    fn openSession(&self, name: &str) -> Result<Strong<(dyn IBinderRpcSession + 'static)>, Status> {
        let s = BnBinderRpcSession::new_binder(
            MyBinderRpcSession::new(name),
            BinderFeatures::default(),
        );
        Ok(s)
    }
    fn getNumOpenSessions(&self) -> Result<i32, Status> {
        let count = MyBinderRpcSession::get_instance_count();
        Ok(count)
    }
    fn lock(&self) -> Result<(), Status> {
        todo!()
    }
    fn unlockInMsAsync(&self, _: i32) -> Result<(), Status> {
        todo!()
    }
    fn lockUnlock(&self) -> Result<(), Status> {
        todo!()
    }
    fn sleepMs(&self, _: i32) -> Result<(), Status> {
        todo!()
    }
    fn sleepMsAsync(&self, _: i32) -> Result<(), Status> {
        todo!()
    }
    fn doCallback(
        &self,
        _: &Strong<(dyn IBinderRpcCallback + 'static)>,
        _: bool,
        _: bool,
        _: &str,
    ) -> Result<(), Status> {
        todo!()
    }
    fn doCallbackAsync(
        &self,
        _: &Strong<(dyn IBinderRpcCallback + 'static)>,
        _: bool,
        _: bool,
        _: &str,
    ) -> Result<(), Status> {
        todo!()
    }
    fn die(&self, _: bool) -> Result<(), Status> {
        Err(Status::from(StatusCode::UNKNOWN_TRANSACTION))
    }
    fn scheduleShutdown(&self) -> Result<(), Status> {
        todo!()
    }
    fn useKernelBinderCallingId(&self) -> Result<(), Status> {
        todo!()
    }
    fn echoAsFile(&self, _: &str) -> Result<ParcelFileDescriptor, Status> {
        todo!()
    }
    fn concatFiles(&self, _: &[ParcelFileDescriptor]) -> Result<ParcelFileDescriptor, Status> {
        todo!()
    }
    fn blockingSendFdOneway(&self, _: &ParcelFileDescriptor) -> Result<(), Status> {
        todo!()
    }
    fn blockingRecvFd(&self) -> Result<ParcelFileDescriptor, Status> {
        todo!()
    }
    fn blockingSendIntOneway(&self, _: i32) -> Result<(), Status> {
        todo!()
    }
    fn blockingRecvInt(&self) -> Result<i32, Status> {
        todo!()
    }
}

wrap_service!(TestRpcServer(RpcServer: UnbufferedService));

service_dispatcher! {
    enum TestDispatcher {
        TestRpcServer,
    }
}

fn main() {
    let mut dispatcher = TestDispatcher::<1>::new().expect("Could not create test dispatcher");

    let service = BnBinderRpcTest::new_binder(TestService::default(), BinderFeatures::default());
    let rpc_server = TestRpcServer::new(
        RpcServer::new_trusty(service.as_binder()).expect("Could noot create RpcServer"),
    );
    rpc_server.0.set_per_session_root_object(move |_session, _uuid| Some(service.as_binder()));

    let cfg = PortCfg::new(RUST_SERVICE_PORT)
        .expect("Could not create port config")
        .allow_ta_connect()
        .allow_ns_connect();

    dispatcher.add_service(Rc::new(rpc_server), cfg).expect("Could not add service to dispatcher");

    Manager::<_, _, 1, 4>::new_with_dispatcher(dispatcher, [])
        .expect("Could not create service manager")
        .run_event_loop()
        .expect("Test event loop failed");
}
