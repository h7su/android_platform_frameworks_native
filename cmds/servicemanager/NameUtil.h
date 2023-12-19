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

#pragma once

#include <string>

#ifndef VENDORSERVICEMANAGER
#include <vintf/VintfObject.h>
#include <vintf/parse_string.h>
#endif

namespace android {

#ifndef VENDORSERVICEMANAGER

struct NativeName {
    std::string package;
    vintf::Version version;
    std::string iface; // optional
    std::string instance;

    static bool fill(const std::string& name, NativeName* nname) {
        // package@version[::interface]/instance
        size_t at = name.find('@');
        size_t slash = name.rfind('/');
        if (at == std::string::npos || slash == std::string::npos || at > slash) {
            return false;
        }

        nname->package = name.substr(0, at);
        size_t colon = name.find("::", at + 1);
        if (colon != std::string::npos) {
            if (!parse(name.substr(at + 1, colon - at - 1), &nname->version)) return false;
            nname->iface = name.substr(colon + 2, slash - colon - 2);
            // "::" should be followed by non-empty interface
            if (nname->iface.empty()) {
                return false;
            }
        } else {
            if (!parse(name.substr(at + 1, slash - at - 1), &nname->version)) return false;
        }
        nname->instance = name.substr(slash + 1);
        return true;
    }
};

#endif

} // namespace android
