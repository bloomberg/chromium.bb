// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_TLS_CREDENTIALS_H_
#define PLATFORM_BASE_TLS_CREDENTIALS_H_

#include <openssl/crypto.h>
#include <openssl/ssl.h>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "platform/base/macros.h"

namespace openscreen {
namespace platform {

class TlsCredentials {
 public:
  // TlsCredentials generates a self signed certificate given (1) the name
  // to use for the certificate, (2) the length of time the certificate will
  // be valid, and (3) a private key used to generate the certificate.
  TlsCredentials(absl::string_view name,
                 std::chrono::seconds certificate_duration,
                 bssl::UniquePtr<EVP_PKEY> private_public_keypair);

  // The raw, generated self signed certficate.
  const std::vector<uint8_t>& raw_der_certificate() {
    return raw_der_certificate_;
  }

  // The OpenSSL encoded self signed certificate.
  const CRYPTO_BUFFER& certificate() const { return *certificate_; }

  // The original private key provided on construction.
  const EVP_PKEY& private_key() const { return *private_key_; }

  // A base64 encoded version of the private/public keypair's private key
  // provided on construction.
  const std::vector<uint8_t>& private_key_base64() const {
    return private_key_base64_;
  }

  // A base64 encoded version of the private/public keypair's associated public
  // key.
  const std::vector<uint8_t>& public_key_base64() const {
    return public_key_base64_;
  }

  // A SHA-256 digest of the private/public keypair's associated public key.
  const std::vector<uint8_t>& public_key_hash() const {
    return public_key_hash_;
  }

 private:
  bssl::UniquePtr<CRYPTO_BUFFER> certificate_;

  bssl::UniquePtr<EVP_PKEY> private_key_;
  std::vector<uint8_t> private_key_base64_;
  std::vector<uint8_t> public_key_base64_;
  std::vector<uint8_t> public_key_hash_;

  std::vector<uint8_t> raw_der_certificate_;
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_BASE_TLS_CREDENTIALS_H_
