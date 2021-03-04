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

//! Safe Rust interface to Android `libbinder`.
//!
//! This crate is primarily designed as an target for a Rust AIDL compiler
//! backend, and should generally not be used directly by users. It is built on
//! top of the binder NDK library to be usable by APEX modules, and therefore
//! only exposes functionality available in the NDK interface.
//!
//! # Example
//!
//! The following example illustrates how the AIDL backend will use this crate.
//!
//! ```
//! use binder::{
//!     declare_binder_interface, Binder, IBinder, Interface, Remotable, Parcel, SpIBinder,
//!     StatusCode, TransactionCode,
//! };
//!
//! // Generated by AIDL compiler
//! pub trait ITest: Interface {
//!     fn test(&self) -> binder::Result<String>;
//! }
//!
//! // Creates a new local (native) service object, BnTest, and a remote proxy
//! // object, BpTest, that are the typed interfaces for their respective ends
//! // of the binder transaction. Generated by AIDL compiler.
//! declare_binder_interface! {
//!     ITest["android.os.ITest"] {
//!         native: BnTest(on_transact),
//!         proxy: BpTest,
//!     }
//! }
//!
//! // Generated by AIDL compiler
//! fn on_transact(
//!     service: &dyn ITest,
//!     code: TransactionCode,
//!     _data: &Parcel,
//!     reply: &mut Parcel,
//! ) -> binder::Result<()> {
//!     match code {
//!         SpIBinder::FIRST_CALL_TRANSACTION => {
//!             reply.write(&service.test()?)?;
//!             Ok(())
//!         }
//!         _ => Err(StatusCode::UNKNOWN_TRANSACTION),
//!     }
//! }
//!
//! // Generated by AIDL compiler
//! impl ITest for Binder<BnTest> {
//!     fn test(&self) -> binder::Result<String> {
//!         self.0.test()
//!     }
//! }
//!
//! // Generated by AIDL compiler
//! impl ITest for BpTest {
//!     fn test(&self) -> binder::Result<String> {
//!        let reply = self
//!            .as_binder()
//!            .transact(SpIBinder::FIRST_CALL_TRANSACTION, 0, |_| Ok(()))?;
//!        reply.read()
//!     }
//! }
//!
//! // User implemented:
//!
//! // Local implementation of the ITest remotable interface.
//! struct TestService;
//!
//! impl Interface for TestService {}
//!
//! impl ITest for TestService {
//!     fn test(&self) -> binder::Result<String> {
//!        Ok("testing service".to_string())
//!     }
//! }
//! ```

#[macro_use]
mod proxy;

#[macro_use]
mod binder;
mod error;
mod native;
mod state;

use binder_ndk_sys as sys;

pub mod parcel;

pub use crate::binder::{
    FromIBinder, IBinder, Interface, InterfaceClass, Remotable, Strong, TransactionCode,
    TransactionFlags, Weak,
};
pub use error::{status_t, ExceptionCode, Result, Status, StatusCode};
pub use native::add_service;
pub use native::Binder;
pub use parcel::Parcel;
pub use proxy::{get_interface, get_service};
pub use proxy::{AssociateClass, DeathRecipient, Proxy, SpIBinder, WpIBinder};
pub use state::{ProcessState, ThreadState};

/// The public API usable outside AIDL-generated interface crates.
pub mod public_api {
    pub use super::parcel::ParcelFileDescriptor;
    pub use super::{add_service, get_interface};
    pub use super::{
        DeathRecipient, ExceptionCode, IBinder, Interface, ProcessState, SpIBinder, Status,
        StatusCode, Strong, ThreadState, Weak, WpIBinder,
    };

    /// Binder result containing a [`Status`] on error.
    pub type Result<T> = std::result::Result<T, Status>;
}
