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
#![cfg(test)]
#![allow(unused)]

use binder::{BinderFeatures, IBinder, Interface, Status, StatusCode, Strong};
use binder_rpc_test_aidl::aidl::IBinderRpcSession::{BnBinderRpcSession, IBinderRpcSession};
use binder_rpc_test_aidl::aidl::IBinderRpcTest::{BnBinderRpcTest, IBinderRpcTest};
use binder_rpc_test_session::MyBinderRpcSession;
use log::{info, warn};
use rpcbinder::RpcSession;
use trusty_std::ffi::{CString, FallibleCString};

test::init!();

const SERVICE_PORT: &str = "com.android.trusty.binderRpcTestService.V1";
const RUST_SERVICE_PORT: &str = "com.android.trusty.rust.binderRpcTestService.V1";

fn get_service(port: &str) -> Strong<dyn IBinderRpcTest> {
    let port = CString::try_new(port).expect("Failed to allocate port name");
    RpcSession::new().setup_trusty_client(port.as_c_str()).expect("Failed to create session")
}

// ----------

#[test]
fn ping() {
    let srv = get_service(SERVICE_PORT);
    assert_eq!(srv.as_binder().ping_binder(), Ok(()));
}

#[test]
fn ping_rust() {
    let srv = get_service(RUST_SERVICE_PORT);
    assert_eq!(srv.as_binder().ping_binder(), Ok(()));
}

// ----------

#[test]
fn send_something_oneway() {
    let srv = get_service(SERVICE_PORT);
    assert_eq!(srv.sendString("Foo"), Ok(()));
}

#[test]
fn send_something_oneway_rust() {
    let srv = get_service(RUST_SERVICE_PORT);
    assert_eq!(srv.sendString("Foo"), Ok(()));
}

// ----------

#[test]
fn send_and_get_result_back() {
    let srv = get_service(SERVICE_PORT);
    assert_eq!(srv.doubleString("Foo"), Ok(String::from("FooFoo")));
}

#[test]
fn send_and_get_result_back_rust() {
    let srv = get_service(RUST_SERVICE_PORT);
    assert_eq!(srv.doubleString("Foo"), Ok(String::from("FooFoo")));
}

// ----------

#[test]
fn send_and_get_result_back_big() {
    let srv = get_service(SERVICE_PORT);
    let single_len = 512;
    let single = "a".repeat(single_len);
    assert_eq!(srv.doubleString(&single), Ok(String::from(single.clone() + &single)));
}

#[test]
fn send_and_get_result_back_big_rust() {
    let srv = get_service(RUST_SERVICE_PORT);
    let single_len = 512;
    let single = "a".repeat(single_len);
    assert_eq!(srv.doubleString(&single), Ok(String::from(single.clone() + &single)));
}

// ----------

#[test]
fn invalid_null_binder_return() {
    let srv = get_service(SERVICE_PORT);
    assert_eq!(srv.getNullBinder(), Err(Status::from(StatusCode::UNEXPECTED_NULL)));
}

#[test]
fn invalid_null_binder_return_rust() {
    let srv = get_service(RUST_SERVICE_PORT);
    assert_eq!(srv.getNullBinder(), Err(Status::from(StatusCode::UNKNOWN_TRANSACTION)));
}

// ----------

#[test]
fn call_me_back() {
    let srv = get_service(SERVICE_PORT);

    let binder =
        BnBinderRpcSession::new_binder(MyBinderRpcSession::new("Foo"), BinderFeatures::default())
            .as_binder();
    let result = srv.pingMe(&binder);
    assert_eq!(result, Ok(0));
}

#[test]
fn call_me_back_rust() {
    let srv = get_service(RUST_SERVICE_PORT);

    let binder =
        BnBinderRpcSession::new_binder(MyBinderRpcSession::new("Foo"), BinderFeatures::default())
            .as_binder();
    let result = srv.pingMe(&binder);
    assert_eq!(result, Ok(0));
}

// ----------

#[test]
fn repeat_binder() {
    let srv = get_service(SERVICE_PORT);

    let in_binder =
        BnBinderRpcSession::new_binder(MyBinderRpcSession::new("Foo"), BinderFeatures::default())
            .as_binder();
    let result = srv.repeatBinder(Some(&in_binder));
    assert_eq!(result.unwrap().unwrap(), in_binder);
}

#[test]
fn repeat_binder_rust() {
    let srv = get_service(RUST_SERVICE_PORT);

    let in_binder =
        BnBinderRpcSession::new_binder(MyBinderRpcSession::new("Foo"), BinderFeatures::default())
            .as_binder();
    let result = srv.repeatBinder(Some(&in_binder));
    assert_eq!(result.unwrap().unwrap(), in_binder);
}

// ----------

#[test]
fn repeat_their_binder() {
    let srv = get_service(SERVICE_PORT);

    let session = srv.openSession("Test");
    assert!(session.is_ok());

    let in_binder = session.unwrap().as_binder();
    let out_binder = srv.repeatBinder(Some(&in_binder));
    assert_eq!(out_binder.unwrap().unwrap(), in_binder);
}

#[test]
fn repeat_their_binder_rust() {
    let srv = get_service(RUST_SERVICE_PORT);

    let session = srv.openSession("Test");
    assert!(session.is_ok());

    let in_binder = session.unwrap().as_binder();
    let out_binder = srv.repeatBinder(Some(&in_binder));
    assert_eq!(out_binder.unwrap().unwrap(), in_binder);
}

// ----------

#[test]
fn hold_binder() {
    let srv = get_service(SERVICE_PORT);
    let name = "Foo";

    let binder =
        BnBinderRpcSession::new_binder(MyBinderRpcSession::new(name), BinderFeatures::default())
            .as_binder();
    assert!(srv.holdBinder(Some(&binder)).is_ok());

    let held = srv.getHeldBinder();
    assert!(held.is_ok());
    let held = held.unwrap();
    assert!(held.is_some());
    let held = held.unwrap();
    assert_eq!(binder, held);

    let session = held.into_interface::<dyn IBinderRpcSession>();
    assert!(session.is_ok());

    let session_name = session.unwrap().getName();
    assert!(session_name.is_ok());
    let session_name = session_name.unwrap();
    assert_eq!(session_name, name);

    assert!(srv.holdBinder(None).is_ok());
}

#[test]
fn hold_binder_rust() {
    let srv = get_service(RUST_SERVICE_PORT);
    let name = "Foo";

    let binder =
        BnBinderRpcSession::new_binder(MyBinderRpcSession::new(name), BinderFeatures::default())
            .as_binder();
    assert!(srv.holdBinder(Some(&binder)).is_ok());

    let held = srv.getHeldBinder();
    assert!(held.is_ok());
    let held = held.unwrap();
    assert!(held.is_some());
    let held = held.unwrap();
    assert_eq!(binder, held);

    let session = held.into_interface::<dyn IBinderRpcSession>();
    assert!(session.is_ok());

    let session_name = session.unwrap().getName();
    assert!(session_name.is_ok());
    let session_name = session_name.unwrap();
    assert_eq!(session_name, name);

    assert!(srv.holdBinder(None).is_ok());
}

// ----------

#[test]
fn nested_transactions() {
    let srv = get_service(SERVICE_PORT);
    let binder =
        BnBinderRpcTest::new_binder(MyBinderRpcSession::new("Nest"), BinderFeatures::default());
    assert!(srv.nestMe(&binder, 10).is_ok());
}

#[test]
fn nested_transactions_rust() {
    let srv = get_service(RUST_SERVICE_PORT);
    let binder =
        BnBinderRpcTest::new_binder(MyBinderRpcSession::new("Nest"), BinderFeatures::default());
    assert!(srv.nestMe(&binder, 10).is_ok());
}

// ----------

#[test]
fn same_binder_equality() {
    let srv = get_service(SERVICE_PORT);

    let a = srv.alwaysGiveMeTheSameBinder();
    assert!(a.is_ok());

    let b = srv.alwaysGiveMeTheSameBinder();
    assert!(b.is_ok());

    assert_eq!(a.unwrap(), b.unwrap());
}

#[test]
fn same_binder_equality_rust() {
    let srv = get_service(RUST_SERVICE_PORT);

    let a = srv.alwaysGiveMeTheSameBinder();
    assert!(a.is_ok());

    let b = srv.alwaysGiveMeTheSameBinder();
    assert!(b.is_ok());

    assert_eq!(a.unwrap(), b.unwrap());
}

// ----------

#[test]
fn single_session() {
    let srv = get_service(SERVICE_PORT);

    let session = srv.openSession("aoeu");
    assert!(session.is_ok());
    let session = session.unwrap();
    let name = session.getName();
    assert!(name.is_ok());
    assert_eq!(name.unwrap(), "aoeu");

    let count = srv.getNumOpenSessions();
    assert!(count.is_ok());
    assert_eq!(count.unwrap(), 1);

    drop(session);
    let count = srv.getNumOpenSessions();
    assert!(count.is_ok());
    assert_eq!(count.unwrap(), 0);
}

#[test]
fn single_session_rust() {
    let srv = get_service(RUST_SERVICE_PORT);

    let session = srv.openSession("aoeu");
    assert!(session.is_ok());
    let session = session.unwrap();
    let name = session.getName();
    assert!(name.is_ok());
    assert_eq!(name.unwrap(), "aoeu");

    let count = srv.getNumOpenSessions();
    assert!(count.is_ok());
    assert_eq!(count.unwrap(), 1);

    drop(session);
    let count = srv.getNumOpenSessions();
    assert!(count.is_ok());
    assert_eq!(count.unwrap(), 0);
}

// ----------

fn expect_sessions(expected: i32, srv: &Strong<dyn IBinderRpcTest>) {
    let count = srv.getNumOpenSessions();
    assert!(count.is_ok());
    assert_eq!(expected, count.unwrap());
}

#[test]
fn many_session() {
    let srv = get_service(SERVICE_PORT);

    let mut sessions = Vec::new();

    for i in 0..15 {
        expect_sessions(i, &srv);

        let session = srv.openSession(&(i.to_string()));
        assert!(session.is_ok());
        sessions.push(session.unwrap());
    }

    expect_sessions(sessions.len() as i32, &srv);

    for i in 0..sessions.len() {
        let name = sessions[i].getName();
        assert!(name.is_ok());
        assert_eq!(name.unwrap(), i.to_string());
    }

    expect_sessions(sessions.len() as i32, &srv);

    while !sessions.is_empty() {
        sessions.pop();

        expect_sessions(sessions.len() as i32, &srv);
    }

    expect_sessions(0, &srv);
}

#[test]
fn many_session_rust() {
    let srv = get_service(RUST_SERVICE_PORT);

    let mut sessions = Vec::new();

    for i in 0..15 {
        expect_sessions(i, &srv);

        let session = srv.openSession(&(i.to_string()));
        assert!(session.is_ok());
        sessions.push(session.unwrap());
    }

    expect_sessions(sessions.len() as i32, &srv);

    for i in 0..sessions.len() {
        let name = sessions[i].getName();
        assert!(name.is_ok());
        assert_eq!(name.unwrap(), i.to_string());
    }

    expect_sessions(sessions.len() as i32, &srv);

    while !sessions.is_empty() {
        sessions.pop();

        expect_sessions(sessions.len() as i32, &srv);
    }

    expect_sessions(0, &srv);
}

// ===========================================================

// #[test]
// fn get_client_port_test() {
//     let srv = get_service(SERVICE_PORT);
//     // TODO: result from getClientPort dependent on prior tests
//     // assert_eq!(srv.getClientPort(), Ok(2));

//     // TODO: Every new server increases that server's port number
//     // TODO: Similar behavoir for rust

//     // let srv2 = get_service(SERVICE_PORT);
//     // info!("#### srv2: {:?}", srv2.getClientPort());
//     // info!("### srv: {:?}", srv.getClientPort());
// }

// #[test]
// fn get_client_port_test_rust() {
//     let srv = get_service(RUST_SERVICE_PORT);
//     assert_eq!(srv.getClientPort(), Ok(0));
// }

// ----------

// #[test]
// fn count_binders_test() {
//     let srv = get_service(RUST_SERVICE_PORT);
//     let v = srv.countBinders();
//     println!("{:?}", v);
// }
