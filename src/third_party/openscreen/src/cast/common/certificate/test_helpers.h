// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_CERTIFICATE_TEST_HELPERS_H_
#define CAST_COMMON_CERTIFICATE_TEST_HELPERS_H_

#include <string>
#include <vector>

#include "cast/common/certificate/cast_cert_validator_internal.h"
#include "cast/common/certificate/types.h"

namespace cast {
namespace certificate {
namespace testing {

std::string ReadEntireFileToString(const std::string& filename);
std::vector<std::string> ReadCertificatesFromPemFile(
    const std::string& filename);

class SignatureTestData {
 public:
  SignatureTestData();
  ~SignatureTestData();

  ConstDataSpan message;
  ConstDataSpan sha1;
  ConstDataSpan sha256;
};

SignatureTestData ReadSignatureTestData(const std::string& filename);

std::unique_ptr<TrustStore> CreateTrustStoreFromPemFile(
    const std::string& filename);

}  // namespace testing
}  // namespace certificate
}  // namespace cast

#endif  // CAST_COMMON_CERTIFICATE_TEST_HELPERS_H_
