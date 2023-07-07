#!/bin/bash

# On Android, use the bindgen from the host output directory
# (where this script is also located)
BINDGEN="`dirname $0`/bindgen"

LIBBINDER_NDK_BINDGEN_ARGS=(
    # Unfortunately the only way to specify the rust_non_exhaustive enum
    # style for a type is to make it the default
    --default-enum-style="rust_non_exhaustive"
    # and then specify constified enums for the enums we don't want
    # rustified
    --constified-enum="android::c_interface::consts::.*"

    --allowlist-type="android::c_interface::.*"
    --allowlist-type="AStatus"
    --allowlist-type="AIBinder_Class"
    --allowlist-type="AIBinder"
    --allowlist-type="AIBinder_Weak"
    --allowlist-type="AIBinder_DeathRecipient"
    --allowlist-type="AParcel"
    --allowlist-type="binder_status_t"
    --allowlist-function=".*"
  )

exec $BINDGEN ${LIBBINDER_NDK_BINDGEN_ARGS[@]} "$@"
