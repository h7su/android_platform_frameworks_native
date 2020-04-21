#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals, unused)]

use crate::proxy::Interface;
use crate::sys::libbinder_bindings::*;
use crate::utils::{AsNative, Sp, Str16, String16};

use std::mem::MaybeUninit;
use std::os::raw::c_int;
use std::ptr;

/// Service manager for C++ services.
///
/// This interface is only for legacy ABI compatibility. An AIDL-based
/// implementation is available in interfaces::IServiceManager.
wrap_sp! {
    pub struct IServiceManager(Sp<android_IServiceManager>) {
        getter: android_c_interface_Sp_getIServiceManager,
        destructor: android_c_interface_Sp_DropIServiceManager,
        clone: android_c_interface_Sp_CloneIServiceManager,
    }
}

#[repr(i32)]
pub enum DumpFlags {
    // Must match values in IServiceManager.aidl
    /// Allows services to dump sections according to priorities.
    PriorityCritical = android_IServiceManager_DUMP_FLAG_PRIORITY_CRITICAL,
    PriorityHigh = android_IServiceManager_DUMP_FLAG_PRIORITY_HIGH,
    PriorityNormal = android_IServiceManager_DUMP_FLAG_PRIORITY_NORMAL,
    /// Services are by default registered with a Default dump priority. Default
    /// priority has the same priority as Normal priority but the services are
    /// not called with dump priority arguments.
    PriorityDefault = android_IServiceManager_DUMP_FLAG_PRIORITY_DEFAULT,
    PriorityAll = android_IServiceManager_DUMP_FLAG_PRIORITY_ALL,
    Proto = android_IServiceManager_DUMP_FLAG_PROTO,
}

impl IServiceManager {
    // Must match values in IServiceManager.aidl
    /// Allows services to dump sections according to priorities.
    pub const DUMP_FLAG_PRIORITY_CRITICAL: c_int =
        android_IServiceManager_DUMP_FLAG_PRIORITY_CRITICAL;
    pub const DUMP_FLAG_PRIORITY_HIGH: c_int = android_IServiceManager_DUMP_FLAG_PRIORITY_HIGH;
    pub const DUMP_FLAG_PRIORITY_NORMAL: c_int = android_IServiceManager_DUMP_FLAG_PRIORITY_NORMAL;
    /// Services are by default registered with a DEFAULT dump priority. DEFAULT
    /// priority has the same priority as NORMAL priority but the services are
    /// not called with dump priority arguments.
    pub const DUMP_FLAG_PRIORITY_DEFAULT: c_int =
        android_IServiceManager_DUMP_FLAG_PRIORITY_DEFAULT;
    pub const DUMP_FLAG_PRIORITY_ALL: c_int = android_IServiceManager_DUMP_FLAG_PRIORITY_ALL;
    pub const DUMP_FLAG_PROTO: c_int = android_IServiceManager_DUMP_FLAG_PROTO;

    /// Return list of all existing services.
    // pub unsafe fn listServices(&self, dumpsysFlags: c_int) -> Vec<String16> {
    //     IServiceManager_listServices(self.0)
    // }

    // // for ABI compatibility
    pub unsafe fn getInterfaceDescriptor(&self) -> &Str16 {
        Str16::from_ptr(android_c_interface_IServiceManager_getInterfaceDescriptor(
            self.0.as_native(),
        ))
    }

    /// Retrieve an existing service, blocking for a few seconds if it doesn't
    /// yet exist.
    pub unsafe fn getService(&self, name: &String16) -> Option<Interface> {
        Interface::from_raw(android_c_interface_IServiceManager_getService(
            self.0.as_native(),
            name.as_native(),
        ))
    }

    // /// Retrieve an existing service, non-blocking.
    // pub unsafe fn checkService(&self, name: &String16) -> Sp<IBinder> {
    //     let mut sm = Sp::null();
    //     (*self.vtable).checkService.unwrap()(&mut sm, self, name);
    //     sm
    // }

    // /// Register a service.
    // pub unsafe fn addService(
    //     &self,
    //     name: &String16,
    //     service: *const Sp<IBinder>,
    //     allowIsolated: bool,
    //     dumpsysFlags: i32,
    // ) -> android_status_t {
    //     (*self.vtable).addService.unwrap()(self, name, service, allowIsolated, dumpsysFlags)
    // }

    // /// Efficiently wait for a service.
    // ///
    // /// Returns nullptr only for permission problem or fatal error.
    // pub unsafe fn waitForService(&self, name: &String16) -> Sp<IBinder> {
    //     let mut sm = Sp::null();
    //     (*self.vtable).waitForService.unwrap()(&mut sm, self, name);
    //     sm
    // }

    // /// Check if a service is declared (e.g. VINTF manifest).
    // ///
    // /// If this returns true, waitForService should always be able to return the
    // /// service.
    // pub unsafe fn isDeclared(&self, name: &String16) -> bool {
    //     (*self.vtable).isDeclared.unwrap()(self, name)
    // }
}

pub unsafe fn defaultServiceManager() -> Option<IServiceManager> {
    IServiceManager::from_raw(android_c_interface_DefaultServiceManager())
}
