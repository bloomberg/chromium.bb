// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_CRYPTO_CERTIFICATE_UTILS_H_
#define UTIL_CRYPTO_CERTIFICATE_UTILS_H_

#include <openssl/x509.h>
#include <stdint.h>

#include <chrono>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "util/crypto/rsa_private_key.h"

namespace openscreen {

// Creates a new self-signed X509 certificate having the given |name| and
// |duration| until expiration, and based on the given |key_pair|.
// |time_since_unix_epoch| is the current time.
ErrorOr<bssl::UniquePtr<X509>> CreateCertificate(
    absl::string_view name,
    std::chrono::seconds duration,
    const EVP_PKEY& key_pair,
    std::chrono::seconds time_since_unix_epoch = GetWallTimeSinceUnixEpoch());

// Exports the given X509 certificate as its DER-encoded binary form.
ErrorOr<std::vector<uint8_t>> ExportCertificate(const X509& certificate);

// Parses a DER-encoded X509 certificate from its binary form.
ErrorOr<bssl::UniquePtr<X509>> ImportCertificate(const uint8_t* der_x509_cert,
                                                 int der_x509_cert_length);

std::string GetSpkiTlv(X509* cert);

ErrorOr<uint64_t> ParseDerUint64(ASN1_INTEGER* asn1int);

}  // namespace openscreen

#endif  // UTIL_CRYPTO_CERTIFICATE_UTILS_H_
