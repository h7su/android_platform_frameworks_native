use binder::client::{BinderContainer, BinderInterface};
use binder::interfaces::{BpServiceManager, IServiceManager};
use binder::service::{Binder, BinderNative};
use binder::{declare_binder_interface, start_thread_pool};
use binder::{BinderResult, Parcel, TransactionCode, TransactionFlags};

struct TestService;

impl Binder for TestService {
    const INTERFACE_DESCRIPTOR: &'static str = "android.os.ITest";

    fn on_transact(
        &mut self,
        _code: TransactionCode,
        _data: &Parcel,
        reply: &mut Parcel,
        _flags: TransactionFlags,
    ) -> BinderResult<()> {
        reply.write_utf8_as_utf16("testing service")?;
        Ok(())
    }
}

pub trait ITest: BinderContainer {
    fn test(&mut self) -> BinderResult<String>;
}

declare_binder_interface!(BpTest, ITest, "android.os.ITest");

impl ITest for BpTest {
    fn test(&mut self) -> BinderResult<String> {
        let mut reply = Parcel::new();
        self.0.transact(
            BinderInterface::FIRST_CALL_TRANSACTION,
            &Parcel::new(),
            &mut reply,
            0,
        )?;
        Ok(reply.read_string16().to_string())
    }
}

#[test]
fn run_server() {
    start_thread_pool();
    let mut sm: BpServiceManager =
        binder::get_service("manager").expect("Did not get manager binder service");
    let binder_native = BinderNative::new(Box::new(TestService));
    let res = sm.add_service("testing", &binder_native, false, 0);
    assert!(res.is_ok());

    let mut test_client: BpTest =
        binder::get_service("testing").expect("Did not get manager binder service");
    assert_eq!(test_client.test(), Ok("testing service".to_string()));
}
