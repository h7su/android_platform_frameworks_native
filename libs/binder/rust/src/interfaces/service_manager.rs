//! Manually implemented IServiceManager AIDL interface.

use crate::binder::IBinder;
use crate::parcel::Parcel;
use crate::service_manager::DumpFlags;
use crate::sys::Status;
use crate::utils::String16;
use crate::{Binder, Interface, Result, Service};

declare_binder_interface!(
    BpServiceManager,
    IServiceManager,
    "android.os.IServiceManager"
);

/// Binder interface for finding and publishing system services.
pub trait IServiceManager {
    const INTERFACE_DESCRIPTOR: &'static str = "android.os.IServiceManager";

    /// Retrieve an existing service called `name` from the service manager.
    ///
    /// This is the same as checkService (returns immediately) but exists for
    /// legacy purposes.
    ///
    /// Returns null if the service does not exist.
    fn get_service(&mut self, name: &str) -> Result<Interface>;

    /// Retrieve an existing service called `name` from the service
    /// manager. Non-blocking. Returns null if the service does not exist.
    fn check_service(&mut self, name: &str) -> Result<Interface>;

    /// Place a new service called `name` into the service manager.
    fn add_service<T: Binder>(
        &mut self,
        name: &str,
        service: &Service<T>,
        allow_isolated: bool,
        dumpsys_flags: DumpFlags,
    ) -> Result<()>;

    /// Return a list of all currently running services.
    fn list_services(&mut self, dump_priority: DumpFlags) -> Result<Vec<String16>>;

    /// Returns whether a given interface is declared on the device, even if it
    /// is not started yet. For instance, this could be a service declared in
    /// the VINTF manifest.
    fn is_declared(&mut self, name: &str) -> Result<bool>;
}

impl IServiceManager for BpServiceManager {
    fn get_service(&mut self, name: &str) -> Result<Interface> {
        let mut data = Parcel::new();
        unsafe {
            data.write_interface_token(&Self::INTERFACE_DESCRIPTOR.into())?;
        }
        data.write_utf8_as_utf16(name)?;
        let mut reply = Parcel::new();
        self.0.transact(
            Interface::FIRST_CALL_TRANSACTION + 0, // getService
            &data,
            Some(&mut reply),
            0,
        )?;
        Status::from_parcel(&reply)?;
        reply.read::<Interface>()
    }

    fn check_service(&mut self, name: &str) -> Result<Interface> {
        let mut data = Parcel::new();
        unsafe {
            data.write_interface_token(&Self::INTERFACE_DESCRIPTOR.into())?;
        }
        data.write_utf8_as_utf16(name)?;
        let mut reply = Parcel::new();
        self.0.transact(
            Interface::FIRST_CALL_TRANSACTION + 1, // checkService
            &data,
            Some(&mut reply),
            0,
        )?;
        Status::from_parcel(&reply)?;
        reply.read::<Interface>()
    }

    fn add_service<T: Binder>(
        &mut self,
        name: &str,
        service: &Service<T>,
        allow_isolated: bool,
        dump_priority: DumpFlags,
    ) -> Result<()> {
        let mut data = Parcel::new();
        unsafe {
            data.write_interface_token(&Self::INTERFACE_DESCRIPTOR.into())?;
        }
        data.write_utf8_as_utf16(name)?;
        data.write_service(service)?;
        data.write_bool(allow_isolated)?;
        data.write_i32(dump_priority as i32)?;
        let mut reply = Parcel::new();
        self.0.transact(
            Interface::FIRST_CALL_TRANSACTION + 2, // addService
            &data,
            Some(&mut reply),
            0,
        )?;
        let status = Status::from_parcel(&reply)?;
        status.into()
    }

    fn list_services(&mut self, dump_priority: DumpFlags) -> Result<Vec<String16>> {
        let mut data = Parcel::new();
        unsafe {
            data.write_interface_token(&Self::INTERFACE_DESCRIPTOR.into())?;
        }
        data.write_i32(dump_priority as i32)?;
        let mut reply = Parcel::new();
        self.0.transact(
            Interface::FIRST_CALL_TRANSACTION + 3, // listServices
            &data,
            Some(&mut reply),
            0,
        )?;
        Status::from_parcel(&reply)?;

        unimplemented!("need to implement readUtf8VectorFromUtf16Vector");
        // reply.readUtf8VectorFromUtf16Vector()
    }

    fn is_declared(&mut self, name: &str) -> Result<bool> {
        let mut data = Parcel::new();
        unsafe {
            data.write_interface_token(&Self::INTERFACE_DESCRIPTOR.into())?;
        }
        data.write_utf8_as_utf16(name)?;
        let mut reply = Parcel::new();
        self.0.transact(
            Interface::FIRST_CALL_TRANSACTION + 6, // isDeclared
            &data,
            Some(&mut reply),
            0,
        )?;
        Status::from_parcel(&reply)?;
        reply.read_bool()
    }
}

#[test]
fn test_get_service() {
    let mut sm: BpServiceManager =
        crate::get_service("manager").expect("Did not get manager binder service");
    let sm = sm
        .get_service("manager")
        .expect("Did not get manager binder service via IServiceManager interface");
    assert_eq!(
        sm.get_interface_descriptor().to_string(),
        "android.os.IServiceManager"
    );
}

#[test]
fn test_check_service() {
    let mut sm: BpServiceManager =
        crate::get_service("manager").expect("Did not get manager binder service");
    let sm = sm
        .check_service("manager")
        .expect("Did not get manager binder service via IServiceManager interface");
    assert_eq!(
        sm.get_interface_descriptor().to_string(),
        "android.os.IServiceManager"
    );
}

#[test]
fn test_add_service() {
    use crate::{Binder, Service, TransactionCode, TransactionFlags};

    struct TestService;

    impl Binder for TestService {
        const INTERFACE_DESCRIPTOR: &'static str = "TestService";

        fn on_transact(
            &self,
            _code: TransactionCode,
            _data: &Parcel,
            reply: Option<&mut Parcel>,
            _flags: TransactionFlags,
        ) -> Result<()> {
            if let Some(reply) = reply {
                reply.write_utf8_as_utf16("testing service")?;
            }
            Ok(())
        }
    }

    let mut sm: BpServiceManager =
        crate::get_service("manager").expect("Did not get manager binder service");

    let binder_native = Service::new(TestService);
    assert!(sm
        .add_service("testing", &binder_native, false, DumpFlags::PriorityDefault)
        .is_ok());
}

#[test]
fn test_is_declared() {
    let mut sm: BpServiceManager =
        crate::get_service("manager").expect("Did not get manager binder service");
    // TODO: Figure out how to test a true result from is_declared. AFAICT this
    // requires a VINTF interface?
    assert_eq!(sm.is_declared("bogus_service"), Ok(false));
}
