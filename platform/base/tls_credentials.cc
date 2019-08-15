// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/tls_credentials.h"

namespace openscreen {
namespace platform {

// TODO(jophba): write constructor and certificate generation code.
TlsCredentials::TlsCredentials(
    absl::string_view name,
    std::chrono::seconds certificate_duration,
    bssl::UniquePtr<EVP_PKEY> private_public_keypair) {}

}  // namespace platform
}  // namespace openscreen
