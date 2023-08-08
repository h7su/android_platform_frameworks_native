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

use binder::{unstable_api::AsNative, SpIBinder};
use binder_rpc_server_sys::*;
use foreign_types::{foreign_type, ForeignType};
use tipc::{ConnectResult, Handle, MessageResult, PortCfg, TipcError, UnbufferedService, Uuid};
use trusty_sys::c_void;

foreign_type! {
    type CType = RpcServerTrustyRust;
    fn drop = RpcServerTrustyRust_delete;

    /// A type that represents a foreign instance of RpcServer.
    #[derive(Debug)]
    pub struct RpcServer;
    /// A borrowed RpcServer.
    pub struct RpcServerRef;
}

/// SAFETY: The opaque handle can be cloned freely.
unsafe impl Send for RpcServer {}
/// SAFETY: The underlying C++ RpcServer class is thread-safe.
unsafe impl Sync for RpcServer {}

impl RpcServer {
    /// Allocates a new RpcServer object.
    pub fn new(mut service: SpIBinder) -> RpcServer {
        let service = service.as_native_mut();

        // SAFETY: Takes ownership of the returned handle, which has correct refcount.
        unsafe { RpcServer::from_ptr(RpcServerTrustyRust_new(service.cast())) }
    }
}

pub struct RpcServerConnection {
    handle_fd: i32,
    ctx: *mut c_void,
}

impl Drop for RpcServerConnection {
    fn drop(&mut self) {
        // We do not need to close handle_fd since we do not own it.
        unsafe {
            RpcServerTrustyRust_handleChannelCleanup(self.ctx);
        }
    }
}

impl UnbufferedService for RpcServer {
    type Connection = RpcServerConnection;

    fn on_connect(
        &self,
        _port: &PortCfg,
        handle: &Handle,
        peer: &Uuid,
    ) -> tipc::Result<ConnectResult<Self::Connection>> {
        let mut conn =
            RpcServerConnection { handle_fd: handle.as_raw_fd(), ctx: std::ptr::null_mut() };
        let rc = unsafe {
            RpcServerTrustyRust_handleConnect(
                self.as_ptr(),
                handle.as_raw_fd(),
                peer.as_ptr().cast(),
                &mut conn.ctx,
            )
        };
        if rc < 0 {
            Err(TipcError::from_uapi(rc.into()))
        } else {
            Ok(ConnectResult::Accept(conn))
        }
    }

    fn on_message(&self, conn: &Self::Connection, handle: &Handle) -> tipc::Result<MessageResult> {
        assert!(conn.handle_fd == handle.as_raw_fd());
        let rc = unsafe { RpcServerTrustyRust_handleMessage(handle.as_raw_fd(), conn.ctx) };
        if rc < 0 {
            Err(TipcError::from_uapi(rc.into()))
        } else {
            Ok(MessageResult::MaintainConnection)
        }
    }

    fn on_disconnect(&self, conn: &Self::Connection) {
        unsafe { RpcServerTrustyRust_handleDisconnect(conn.handle_fd, conn.ctx) };
    }
}
