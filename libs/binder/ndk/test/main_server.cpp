/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <binder/ABinderProcess.h>
#include <iface/iface.h>

class MyFoo : public IFoo {
    void doubleNumber(int32_t in, int32_t* out) override { *out = 2 * in; }
};

int main() {
    ABinderProcess* process = ABinderProcess_threadpool(1);

    // FIXME: refcounting
    MyFoo* foo = new MyFoo;
    AIBinder* binder = IFoo::newLocalBinder(foo);

    // FIXME: should have to pass in ABinderProcess?
    AIBinder_register(binder, IFoo::kSomeInstanceName);
    ABinderProcess_join(process);

    return 1;
}