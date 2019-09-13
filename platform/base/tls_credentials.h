// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TLS_CREDENTIALS_H_
#define PLATFORM_BASE_TLS_CREDENTIALS_H_

#include <openssl/crypto.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/base/macros.h"
#include "util/crypto/rsa_private_key.h"

namespace openscreen {
namespace platform {

class TlsCredentials {
 public:
  // We are move only due to unique pointers.
  TlsCredentials(TlsCredentials&&) = default;
  TlsCredentials& operator=(TlsCredentials&&) = default;

  // TlsCredentials generates a self signed certificate given (1) the name
  // to use for the certificate, (2) the length of time the certificate will
  // be valid, and (3) a private/public key pair.
  //
  // NOTE: the ownership of EVP_PKEY is automatically managed by OpenSSL using
  // ref counting, even if you store it in a bssl::UniquePtr.
  static ErrorOr<TlsCredentials> Create(
      absl::string_view name,
      std::chrono::seconds certificate_duration,
      ClockNowFunctionPtr now_function,
      EVP_PKEY* key_pair);

  // The OpenSSL encoded self signed certificate.
  const X509& certificate() const { return *certificate_; }

  // The original key pair provided on construction.
  const RSAPrivateKey& key_pair() const { return key_pair_; }

  // A base64 encoded version of the private key provided on construction.
  const std::vector<uint8_t>& private_key_base64() const {
    return private_key_base64_;
  }

  // A base64 encoded version of the public key provided on construction.
  const std::vector<uint8_t>& public_key_base64() const {
    return public_key_base64_;
  }

  // A SHA-256 digest of the public key provided on construction.
  const std::vector<uint8_t>& public_key_hash() const {
    return public_key_hash_;
  }

  // The DER-encoded raw bytes of the generated self signed certficate.
  const std::vector<uint8_t>& raw_der_certificate() const {
    return raw_der_certificate_;
  }

 private:
  TlsCredentials(bssl::UniquePtr<X509> certificate,
                 RSAPrivateKey key_pair,
                 std::vector<uint8_t> private_key_base64,
                 std::vector<uint8_t> public_key_base64,
                 std::vector<uint8_t> public_key_hash,
                 std::vector<uint8_t> raw_der_certificate);

  bssl::UniquePtr<X509> certificate_;
  RSAPrivateKey key_pair_;
  std::vector<uint8_t> private_key_base64_;
  std::vector<uint8_t> public_key_base64_;
  std::vector<uint8_t> public_key_hash_;
  std::vector<uint8_t> raw_der_certificate_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_TLS_CREDENTIALS_H_
