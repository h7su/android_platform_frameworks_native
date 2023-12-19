/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <gtest/gtest.h>

#include "NameUtil.h"

namespace android {

TEST(ServiceManager, NativeName) {
    NativeName nname;
    EXPECT_TRUE(NativeName::fill("mapper@5.0::IMapper/default", &nname));
    EXPECT_EQ("mapper", nname.package);
    EXPECT_EQ(vintf::Version(5, 0), nname.version);
    EXPECT_EQ("IMapper", nname.iface);
    EXPECT_EQ("default", nname.instance);
}

TEST(ServiceManager, NativeName_WithoutIface) {
    NativeName nname;
    EXPECT_TRUE(NativeName::fill("mapper@5.0/default", &nname));
    EXPECT_EQ("mapper", nname.package);
    EXPECT_EQ(vintf::Version(5, 0), nname.version);
    EXPECT_EQ("", nname.iface);
    EXPECT_EQ("default", nname.instance);
}

TEST(ServiceManager, NativeName_WithEmptyIface) {
    NativeName nname;
    EXPECT_FALSE(NativeName::fill("mapper@5.0::/default", &nname));
}

TEST(ServiceManager, NativeName_WithoutInstance) {
    NativeName nname;
    EXPECT_FALSE(NativeName::fill("mapper@5.0::IMapper", &nname));
}

TEST(ServiceManager, NativeName_WithoutIfaceAndInstance) {
    NativeName nname;
    EXPECT_FALSE(NativeName::fill("mapper@5.0", &nname));
}

TEST(ServiceManager, NativeName_WithoutVersion) {
    NativeName nname;
    EXPECT_FALSE(NativeName::fill("mapper::IMapper/default", &nname));
}

TEST(ServiceManager, NativeName_WithoutVersionAndIface) {
    NativeName nname;
    EXPECT_FALSE(NativeName::fill("mapper/default", &nname));
}

TEST(ServiceManager, NativeName_WithSingleDigitVersion) {
    NativeName nname;
    EXPECT_FALSE(NativeName::fill("mapper@5/default", &nname));
    EXPECT_FALSE(NativeName::fill("mapper@v/default", &nname));
    EXPECT_FALSE(NativeName::fill("mapper@1.2.3/default", &nname));
}

} // namespace android
