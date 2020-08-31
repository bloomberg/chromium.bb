// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_RECEIVER_CHANNEL_TESTING_DEVICE_AUTH_TEST_HELPERS_H_
#define CAST_RECEIVER_CHANNEL_TESTING_DEVICE_AUTH_TEST_HELPERS_H_

#include <openssl/x509.h>

#include <vector>

#include "absl/strings/string_view.h"
#include "cast/common/certificate/testing/test_helpers.h"
#include "cast/receiver/channel/device_auth_namespace_handler.h"

namespace openscreen {
namespace cast {

class StaticCredentialsProvider final
    : public DeviceAuthNamespaceHandler::CredentialsProvider {
 public:
  StaticCredentialsProvider() = default;
  ~StaticCredentialsProvider() = default;

  absl::Span<const uint8_t> GetCurrentTlsCertAsDer() override {
    return absl::Span<uint8_t>(tls_cert_der);
  }
  const DeviceCredentials& GetCurrentDeviceCredentials() override {
    return device_creds;
  }

  DeviceCredentials device_creds;
  std::vector<uint8_t> tls_cert_der;
};

void InitStaticCredentialsFromFiles(StaticCredentialsProvider* creds,
                                    bssl::UniquePtr<X509>* parsed_cert,
                                    TrustStore* fake_trust_store,
                                    absl::string_view privkey_filename,
                                    absl::string_view chain_filename,
                                    absl::string_view tls_filename);

}  // namespace cast
}  // namespace openscreen

#endif  // CAST_RECEIVER_CHANNEL_TESTING_DEVICE_AUTH_TEST_HELPERS_H_
