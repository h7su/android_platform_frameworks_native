extern crate bindgen;

use std::path::Path;

fn main() {
    println!("cargo:rustc-link-lib=binder");
    println!("cargo:rerun-if-changed=src/sys/BinderBindings.h");

    let bindings = bindgen::Builder::default()
        // These include paths will be provided by soong once it supports
        // bindgen.
        .clang_arg("-I../include")
        .clang_arg("-I../../../../../system/core/libutils/include")
        .clang_arg("-I../../../../../system/core/liblog/include")
        .clang_arg("-I../../../../../system/core/libsystem/include")
        .clang_arg("-I../../../../../system/core/base/include")
        .clang_arg("-I../../../../../system/core/libcutils/include")
        .clang_args(&["-x", "c++"])
        .clang_arg("-std=gnu++17")
        // Our interface shims
        .header("src/sys/BinderBindings.h")
        .whitelist_function("android::c_interface::.*")
        .opaque_type("android::c_interface::BinderNative")
        // Simple types we can export from C++. Make sure these types are ALL
        // POD.
        .whitelist_type("android::status_t")
        .whitelist_type("android::TransactionCode")
        .whitelist_type("android::TransactionFlags")
        .whitelist_type("android::binder::Status")
        .whitelist_type("binder_size_t")
        // Types used as opaque pointers from C++
        .opaque_type("android::BBinder")
        .opaque_type("android::BpBinder")
        .opaque_type("android::IBinder")
        .opaque_type("android::IBinder_DeathRecipient")
        .opaque_type("android::IPCThreadState")
        .opaque_type("android::IInterface")
        .opaque_type("android::IServiceManager")
        .opaque_type("android::Parcel")
        .opaque_type("android::Parcel_ReadableBlob")
        .opaque_type("android::Parcel_WritableBlob")
        .opaque_type("android::Parcelable")
        .opaque_type("android::ProcessState")
        .opaque_type("android::String8")
        .opaque_type("android::String16")
        .opaque_type("android::thread_id_t")
        .opaque_type("android::wp")
        .opaque_type("android::Vector")
        .opaque_type("std::.*")
        // We provide our own sp definition
        .blacklist_type("android::sp")
        // We don't want to ever see these types, as they should not be exposed
        .blacklist_type("android::Parcel_Blob")
        .blacklist_function("android::Parcel_Blob.*")
        .blacklist_type("android::Parcel_Flattenable.*")
        .blacklist_type("android::ProcessState_handle_entry")
        .blacklist_type("android::RefBase.*")
        .blacklist_function("android::RefBase.*")
        .blacklist_type("android::wp_weakref_type")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks))
        .derive_debug(false)
        .generate()
        .expect("Bindgen failed to generate bindings for libbinder");

    // write bindings
    let out_path = Path::new("src/sys/libbinder_bindings.rs");
    bindings.write_to_file(&out_path).unwrap_or_else(|_| {
        panic!(
            "Bindgen failed to writing bindings to {}",
            out_path.display()
        )
    });
}
