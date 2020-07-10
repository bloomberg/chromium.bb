// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_INTERNAL_H_
#define CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_INTERNAL_H_

#include <openssl/x509.h>

#include <vector>

#include "platform/base/error.h"

namespace cast {
namespace certificate {

struct TrustStore {
  std::vector<bssl::UniquePtr<X509>> certs;
};

// Adds a trust anchor given a DER-encoded certificate from static
// storage.
template <size_t N>
bssl::UniquePtr<X509> MakeTrustAnchor(const uint8_t (&data)[N]) {
  const uint8_t* dptr = data;
  return bssl::UniquePtr<X509>{d2i_X509(nullptr, &dptr, N)};
}

struct ConstDataSpan;
struct DateTime;

bool VerifySignedData(const EVP_MD* digest,
                      EVP_PKEY* public_key,
                      const ConstDataSpan& data,
                      const ConstDataSpan& signature);

// Parses DateTime with additional restrictions laid out by RFC 5280
// 4.1.2.5.2.
bool ParseAsn1GeneralizedTime(ASN1_GENERALIZEDTIME* time, DateTime* out);
bool GetCertValidTimeRange(X509* cert,
                           DateTime* not_before,
                           DateTime* not_after);

struct CertificatePathResult {
  bssl::UniquePtr<X509> target_cert;
  std::vector<bssl::UniquePtr<X509>> intermediate_certs;
  std::vector<X509*> path;
};

openscreen::Error FindCertificatePath(const std::vector<std::string>& der_certs,
                                      const DateTime& time,
                                      CertificatePathResult* result_path,
                                      TrustStore* trust_store);

}  // namespace certificate
}  // namespace cast

#endif  // CAST_COMMON_CERTIFICATE_CAST_CERT_VALIDATOR_INTERNAL_H_
