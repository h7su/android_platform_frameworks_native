/*
 * Copyright (C) 2022 The Android Open Source Project
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

use crate::session::FileDescriptorTransportMode;
use binder::{unstable_api::AsNative, SpIBinder};
use binder_rpc_unstable_bindgen::ARpcServer;
use foreign_types::{foreign_type, ForeignType, ForeignTypeRef};
use std::io::{Error, ErrorKind};

#[cfg(not(target_os = "trusty"))]
use std::ffi::CString;
#[cfg(not(target_os = "trusty"))]
use std::os::unix::io::{IntoRawFd, OwnedFd};

foreign_type! {
    type CType = binder_rpc_unstable_bindgen::ARpcServer;
    fn drop = binder_rpc_unstable_bindgen::ARpcServer_free;

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

#[cfg(not(target_os = "trusty"))]
impl RpcServer {
    /// Creates a binder RPC server, serving the supplied binder service implementation on the given
    /// vsock port. Only connections from the given CID are accepted.
    ///
    // Set `cid` to libc::VMADDR_CID_ANY to accept connections from any client.
    // Set `cid` to libc::VMADDR_CID_LOCAL to only bind to the local vsock interface.
    pub fn new_vsock(mut service: SpIBinder, cid: u32, port: u32) -> Result<RpcServer, Error> {
        let service = service.as_native_mut();

        // SAFETY: Service ownership is transferring to the server and won't be valid afterward.
        // Plus the binder objects are threadsafe.
        unsafe {
            Self::checked_from_ptr(binder_rpc_unstable_bindgen::ARpcServer_newVsock(
                service, cid, port,
            ))
        }
    }

    /// Creates a binder RPC server, serving the supplied binder service implementation on the given
    /// socket file descriptor. The socket should be bound to an address before calling this
    /// function.
    pub fn new_bound_socket(
        mut service: SpIBinder,
        socket_fd: OwnedFd,
    ) -> Result<RpcServer, Error> {
        let service = service.as_native_mut();

        // SAFETY: Service ownership is transferring to the server and won't be valid afterward.
        // Plus the binder objects are threadsafe.
        // The server takes ownership of the socket FD.
        unsafe {
            Self::checked_from_ptr(binder_rpc_unstable_bindgen::ARpcServer_newBoundSocket(
                service,
                socket_fd.into_raw_fd(),
            ))
        }
    }

    /// Creates a binder RPC server that bootstraps sessions using an existing Unix domain socket
    /// pair, with a given root IBinder object. Callers should create a pair of SOCK_STREAM Unix
    /// domain sockets, pass one to the server and the other to the client. Multiple client session
    /// can be created from the client end of the pair.
    pub fn new_unix_domain_bootstrap(
        mut service: SpIBinder,
        bootstrap_fd: OwnedFd,
    ) -> Result<RpcServer, Error> {
        let service = service.as_native_mut();

        // SAFETY: Service ownership is transferring to the server and won't be valid afterward.
        // Plus the binder objects are threadsafe.
        // The server takes ownership of the bootstrap FD.
        unsafe {
            Self::checked_from_ptr(binder_rpc_unstable_bindgen::ARpcServer_newUnixDomainBootstrap(
                service,
                bootstrap_fd.into_raw_fd(),
            ))
        }
    }

    /// Creates a binder RPC server, serving the supplied binder service implementation on the given
    /// IP address and port.
    pub fn new_inet(mut service: SpIBinder, address: &str, port: u32) -> Result<RpcServer, Error> {
        let address = match CString::new(address) {
            Ok(s) => s,
            Err(e) => {
                log::error!("Cannot convert {} to CString. Error: {:?}", address, e);
                return Err(Error::from(ErrorKind::InvalidInput));
            }
        };
        let service = service.as_native_mut();

        // SAFETY: Service ownership is transferring to the server and won't be valid afterward.
        // Plus the binder objects are threadsafe.
        unsafe {
            Self::checked_from_ptr(binder_rpc_unstable_bindgen::ARpcServer_newInet(
                service,
                address.as_ptr(),
                port,
            ))
        }
    }
}

#[cfg(target_os = "trusty")]
impl RpcServer {
    /// Creates a binder RPC server that can be added to a tipc Dispatcher.
    pub fn new_trusty(mut service: SpIBinder) -> Result<RpcServer, Error> {
        let service = service.as_native_mut();

        // SAFETY: Takes ownership of the returned handle, which has correct refcount.
        unsafe {
            Self::checked_from_ptr(binder_rpc_unstable_bindgen::ARpcServer_newTrusty(service))
        }
    }
}

impl RpcServer {
    unsafe fn checked_from_ptr(ptr: *mut ARpcServer) -> Result<RpcServer, Error> {
        if ptr.is_null() {
            return Err(Error::new(ErrorKind::Other, "Failed to start server"));
        }
        // SAFETY: Our caller must pass us a valid or null pointer, and we've checked that it's not
        // null.
        Ok(unsafe { RpcServer::from_ptr(ptr) })
    }
}

impl RpcServerRef {
    /// Sets the list of file descriptor transport modes supported by this server.
    pub fn set_supported_file_descriptor_transport_modes(
        &self,
        modes: &[FileDescriptorTransportMode],
    ) {
        // SAFETY: Does not keep the pointer after returning does, nor does it
        // read past its boundary. Only passes the 'self' pointer as an opaque handle.
        unsafe {
            binder_rpc_unstable_bindgen::ARpcServer_setSupportedFileDescriptorTransportModes(
                self.as_ptr(),
                modes.as_ptr(),
                modes.len(),
            )
        }
    }
}

#[cfg(not(target_os = "trusty"))]
impl RpcServerRef {
    /// Starts a new background thread and calls join(). Returns immediately.
    pub fn start(&self) {
        // SAFETY: RpcServerRef wraps a valid pointer to an ARpcServer.
        unsafe { binder_rpc_unstable_bindgen::ARpcServer_start(self.as_ptr()) };
    }

    /// Joins the RpcServer thread. The call blocks until the server terminates.
    /// This must be called from exactly one thread.
    pub fn join(&self) {
        // SAFETY: RpcServerRef wraps a valid pointer to an ARpcServer.
        unsafe { binder_rpc_unstable_bindgen::ARpcServer_join(self.as_ptr()) };
    }

    /// Shuts down the running RpcServer. Can be called multiple times and from
    /// multiple threads. Called automatically during drop().
    pub fn shutdown(&self) -> Result<(), Error> {
        // SAFETY: RpcServerRef wraps a valid pointer to an ARpcServer.
        if unsafe { binder_rpc_unstable_bindgen::ARpcServer_shutdown(self.as_ptr()) } {
            Ok(())
        } else {
            Err(Error::from(ErrorKind::UnexpectedEof))
        }
    }
}

#[cfg(target_os = "trusty")]
pub struct RpcServerConnection {
    ctx: *mut std::ffi::c_void,
}

#[cfg(target_os = "trusty")]
impl Drop for RpcServerConnection {
    fn drop(&mut self) {
        // SAFETY: handleTipcChannelCleanup is the destructor for TipcChannelContext.
        // ARpcServer_handleTipcConnect stores a pointer to a newly allocated
        // TipcChannelContext on success in ctx.
        // We do not need to close handle_fd since we do not own it.
        unsafe {
            binder_rpc_unstable_bindgen::ARpcServer_handleTipcChannelCleanup(self.ctx);
        }
    }
}

#[cfg(target_os = "trusty")]
impl tipc::UnbufferedService for RpcServer {
    type Connection = RpcServerConnection;

    fn on_connect(
        &self,
        _port: &tipc::PortCfg,
        handle: &tipc::Handle,
        peer: &tipc::Uuid,
    ) -> tipc::Result<tipc::ConnectResult<Self::Connection>> {
        let mut conn = RpcServerConnection { ctx: std::ptr::null_mut() };
        // SAFETY: self.as_ptr() should be a pointer to a valid ARpcServer.
        // conn.ctx is a raw pointer that the callee will store the valid address
        // of a newly allocated TipcChannelContext upon success. It is our
        // responsibility to call ARpcServer_handleTipcChannelCleanup later
        // to clean it up.
        let rc = unsafe {
            binder_rpc_unstable_bindgen::ARpcServer_handleTipcConnect(
                self.as_ptr(),
                handle.as_raw_fd(),
                peer.as_ptr().cast(),
                &mut conn.ctx,
            )
        };
        if rc < 0 {
            Err(tipc::TipcError::from_uapi(rc.into()))
        } else {
            Ok(tipc::ConnectResult::Accept(conn))
        }
    }

    fn on_message(
        &self,
        conn: &Self::Connection,
        _handle: &tipc::Handle,
        buffer: &mut [u8],
    ) -> tipc::Result<tipc::MessageResult> {
        assert!(buffer.is_empty());
        // SAFETY: We should only be calling this with a valid ctx
        // returned by ARpcServer_handleTipcConnect.
        let rc = unsafe { binder_rpc_unstable_bindgen::ARpcServer_handleTipcMessage(conn.ctx) };
        if rc < 0 {
            Err(tipc::TipcError::from_uapi(rc.into()))
        } else {
            Ok(tipc::MessageResult::MaintainConnection)
        }
    }

    fn on_disconnect(&self, conn: &Self::Connection) {
        // SAFETY: We should only be calling this with a valid ctx
        // returned by ARpcServer_handleTipcConnect.
        unsafe { binder_rpc_unstable_bindgen::ARpcServer_handleTipcDisconnect(conn.ctx) };
    }
}
