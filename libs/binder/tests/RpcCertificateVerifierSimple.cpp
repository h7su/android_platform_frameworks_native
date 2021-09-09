/*
 * Copyright (C) 2021 The Android Open Source Project
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
#define LOG_TAG "RpcCertificateVerifierSimple"
#include <log/log.h>

#include "RpcCertificateVerifierSimple.h"

namespace android {

status_t RpcCertificateVerifierSimple::verify(const X509*, uint8_t*) {
    // TODO(b/195166979): implement this
    return OK;
}

status_t RpcCertificateVerifierSimple::addTrustedPeerCertificate(CertificateFormat,
                                                                 std::string_view) {
    // TODO(b/195166979): implement this
    return OK;
}

} // namespace android
