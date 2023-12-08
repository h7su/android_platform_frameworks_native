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

use binder::{BinderFeatures, IBinder, Interface, Status, StatusCode, Strong};
use binder_rpc_test_aidl::aidl::IBinderRpcSession::{BnBinderRpcSession, IBinderRpcSession};
use binder_rpc_test_aidl::aidl::IBinderRpcTest::IBinderRpcTest;
use log::{info, warn};
use rpcbinder::RpcSession;
use std::sync::Mutex;
use trusty_std::ffi::{CString, FallibleCString};

static G_NUM: Mutex<i32> = Mutex::new(0);

#[derive(Debug, Default)]
pub struct MyBinderRpcSession {
    m_name: String,
}

impl MyBinderRpcSession {
    pub fn new(name: &str) -> Self {
        Self::increment_instance_count();
        Self { m_name: name.to_string() }
    }

    pub fn get_instance_count() -> i32 {
        *G_NUM.lock().unwrap()
    }

    fn increment_instance_count() {
        *G_NUM.lock().unwrap() += 1;
    }

    fn decrement_instance_count() {
        *G_NUM.lock().unwrap() -= 1;
    }
}

// TODO: clone never seems to be called - WHY ?????
impl Clone for MyBinderRpcSession {
    fn clone(&self) -> MyBinderRpcSession {
        MyBinderRpcSession::increment_instance_count();
        Self { m_name: self.m_name.clone() }
    }
}

impl Drop for MyBinderRpcSession {
    fn drop(&mut self) {
        MyBinderRpcSession::decrement_instance_count();
    }
}

impl Interface for MyBinderRpcSession {}

impl IBinderRpcSession for MyBinderRpcSession {
    fn getName(&self) -> Result<String, Status> {
        Ok(self.m_name.clone())
    }
}
