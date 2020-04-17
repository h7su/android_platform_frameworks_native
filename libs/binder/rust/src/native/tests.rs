use crate::String16;
use super::defaultServiceManager;
use super::{IBinder, Parcel};

#[test]
fn connect_to_servicemanager() {
    unsafe {
        let service_manager = defaultServiceManager();
        assert!(!service_manager.is_null());

        service_manager.getInterfaceDescriptor();
    }
}

#[test]
fn raw_transact_interface() {
    unsafe {
        let service_manager = defaultServiceManager();
        assert!(!service_manager.is_null());

        let mut sm = service_manager.getService(&String16::from("manager"));
        assert!(!sm.is_null());

        let input = Parcel::new();
        let mut output = Parcel::new();
        let status = sm.transact(IBinder::INTERFACE_TRANSACTION, &input, &mut output, 0);
        assert_eq!(status, 0);
        let interface = output.read_string16();
        assert_eq!(interface.to_string(), "android.os.IServiceManager");
        assert_eq!(interface, *(service_manager.getInterfaceDescriptor() as *const String16));
    }
}
