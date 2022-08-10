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

#pragma once

#include <binder/IBinder.h>

namespace android {

/*
 * Used to manage AIDL's *Delegator types.
 * This is used to:
 * - create a new *Delegator object that delegates to the binder argument.
 * - or return an existing *Delegator object that already delegates to the
 * binder argument.
 * - or return the underlying delegate binder if the binder argument is a
 * *Delegator itself.
 *
 * @param binder - the binder to delegate to or unwrap
 * @param isDelegatorId - a unique value that is used to determine if this
 * binder is a Delegator
 * @param hasDelegatorId - a unique value that is used to determine if this
 * binder has a Delegator
 * @return pointer to the *Delegator object or the unwrapped binder object
 */
template <typename T>
sp<T> delegate(const sp<T>& binder) {
    // is binder itself a delegator?
    if (T::asBinder(binder)->findObject(T::DefaultDelegator::_aidl_is_delegator_id)) {
        if (T::asBinder(binder)->findObject(T::DefaultDelegator::_aidl_has_delegator_id)) {
            LOG_ALWAYS_FATAL("This binder has a delegator and is also delegator itself! This is "
                             "likely an unintended mixing of binders.");
            return nullptr;
        }
        // unwrap the delegator
        return sp<typename T::DefaultDelegator>::cast(binder)->getImpl();
    }

    // the binder is not a delegator, so construct one
    sp<IBinder> newDelegator = T::asBinder(binder)->lookupOrCreateWeak(
            T::DefaultDelegator::_aidl_has_delegator_id,
            [](const void* args) -> sp<IBinder> {
                auto delegator =
                        sp<typename T::DefaultDelegator>::make(*static_cast<const sp<T>*>(args));
                // make sure we know this binder is a delegator by attaching a unique ID
                (void)delegator->attachObject(T::DefaultDelegator::_aidl_is_delegator_id,
                                              reinterpret_cast<void*>(0x1), nullptr, nullptr);
                return delegator;
            },
            static_cast<const void*>(&binder));
    return sp<typename T::DefaultDelegator>::cast(newDelegator);
}

} // namespace android
