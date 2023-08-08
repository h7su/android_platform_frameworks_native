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

use binder::{
    unstable_api::{AIBinder, AsNative},
    SpIBinder,
};
use libc::size_t;
use std::ffi::c_void;
use std::ptr;
use tipc::{ConnectResult, Handle, MessageResult, PortCfg, TipcError, UnbufferedService, Uuid};

pub trait PerSessionCallback: FnMut(Uuid) -> Option<SpIBinder> + 'static {}
impl<T> PerSessionCallback for T where T: FnMut(Uuid) -> Option<SpIBinder> + 'static {}

pub struct RpcServer {
    inner: *mut binder_rpc_unstable_bindgen::ARpcServerTrusty,
    #[allow(dead_code)] // Never read from Rust directly
    per_session_cb: Option<Box<Box<dyn PerSessionCallback>>>,
}

/// SAFETY: The opaque handle points to a heap allocation
/// that should be process-wide and not tied to the current thread.
unsafe impl Send for RpcServer {}
/// SAFETY: The underlying C++ RpcServer class is thread-safe.
unsafe impl Sync for RpcServer {}

impl Drop for RpcServer {
    fn drop(&mut self) {
        // SAFETY: `ARpcServerTrusty_delete` is the correct destructor to call
        // on pointers returned by `ARpcServerTrusty_new`.
        unsafe {
            binder_rpc_unstable_bindgen::ARpcServerTrusty_delete(self.inner);
        }
    }
}

impl RpcServer {
    /// Allocates a new RpcServer object.
    pub fn new(mut service: SpIBinder) -> RpcServer {
        let service = service.as_native_mut();

        // SAFETY: Takes ownership of the returned handle, which has correct refcount.
        let inner = unsafe { binder_rpc_unstable_bindgen::ARpcServerTrusty_new(service.cast()) };

        RpcServer { inner, per_session_cb: None }
    }

    /// Allocates a new per-session RpcServer object.
    ///
    /// Per-session objects take a closure that gets called once
    /// for every new connection. The closure gets the UUID of
    /// the peer and can accept or reject that connection.
    pub fn new_per_session(f: impl PerSessionCallback) -> RpcServer {
        // We need to use a Box<Box<dyn PerSessionCallback>> for a couple of reasons:
        // * impl PerSessionCallback can only be stored long-term in a Box<dyn PerSessionCallback>
        // * The C callback needs a stable pointer to that inner Box (i.e. its address cannot
        //   change). The easiest way to accomplish that is to wrap it in an outer Box. Stable
        //   address means that Box<T> can move around, but the T always stays at the same address.
        let mut boxed_cb: Box<Box<dyn PerSessionCallback>> = Box::new(Box::new(f));
        let boxed_cb_ptr: *mut Box<dyn PerSessionCallback> = boxed_cb.as_mut();

        // SAFETY: Takes ownership of the returned handle, which has correct refcount.
        let inner = unsafe {
            binder_rpc_unstable_bindgen::ARpcServerTrusty_newPerSession(
                Some(per_session_cb_wrapper),
                boxed_cb_ptr.cast(),
            )
        };

        RpcServer { inner, per_session_cb: Some(boxed_cb) }
    }
}

extern "C" fn per_session_cb_wrapper(
    uuid_ptr: *const c_void,
    len: size_t,
    arg: *mut c_void,
) -> *mut AIBinder {
    let cb_ptr: *mut Box<dyn PerSessionCallback> = arg.cast();
    // SAFETY: This callback should only get called while the RpcServer is alive.
    let cb = unsafe { &mut *cb_ptr };

    if len != std::mem::size_of::<Uuid>() {
        return ptr::null_mut();
    }

    // SAFETY: On the previous lines we check that we got exactly the right amount of bytes.
    let uuid = unsafe {
        let mut uuid = std::mem::MaybeUninit::<Uuid>::uninit();
        uuid.as_mut_ptr().copy_from(uuid_ptr.cast(), 1);
        uuid.assume_init()
    };

    match cb(uuid) {
        Some(mut b) => {
            // Prevent AIBinder_decStrong from being called before AIBinder_toPlatformBinder.
            // The per-session callback in C++ is supposed to call AIBinder_decStrong on the
            // pointer we return here.
            let aib = b.as_native_mut().cast();
            std::mem::forget(b);
            aib
        }
        None => ptr::null_mut(),
    }
}

pub struct RpcServerConnection {
    ctx: *mut c_void,
}

impl Drop for RpcServerConnection {
    fn drop(&mut self) {
        // We do not need to close handle_fd since we do not own it.
        unsafe {
            binder_rpc_unstable_bindgen::ARpcServerTrusty_handleChannelCleanup(self.ctx);
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
        let mut conn = RpcServerConnection { ctx: std::ptr::null_mut() };
        let rc = unsafe {
            binder_rpc_unstable_bindgen::ARpcServerTrusty_handleConnect(
                self.inner,
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

    fn on_message(
        &self,
        conn: &Self::Connection,
        _handle: &Handle,
        buffer: &mut [u8],
    ) -> tipc::Result<MessageResult> {
        assert!(buffer.is_empty());
        let rc = unsafe { binder_rpc_unstable_bindgen::ARpcServerTrusty_handleMessage(conn.ctx) };
        if rc < 0 {
            Err(TipcError::from_uapi(rc.into()))
        } else {
            Ok(MessageResult::MaintainConnection)
        }
    }

    fn on_disconnect(&self, conn: &Self::Connection) {
        unsafe { binder_rpc_unstable_bindgen::ARpcServerTrusty_handleDisconnect(conn.ctx) };
    }
}
