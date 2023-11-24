//! pls compile

use ::IBinderFoo::aidl::IBinderFoo::{BnBinderFoo, IBinderFoo};

/// Test struct
pub struct BinderFoo;

impl binder::Interface for BinderFoo {}

impl IBinderFoo for BinderFoo {
    fn foo(&self) -> binder::Result<()> {
        Ok(())
    }
}

fn main() {
    let service = BinderFoo;
    let service_binder = BnBinderFoo::new_binder(service, binder::BinderFeatures::default());
    binder::ProcessState::start_thread_pool();
    binder::add_service("foo-service", service_binder.as_binder())
        .expect("Failed to register service");
    binder::ProcessState::join_thread_pool();
}
