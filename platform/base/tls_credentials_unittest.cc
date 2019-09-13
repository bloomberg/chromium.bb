// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/base/tls_credentials.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <algorithm>
#include <chrono>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "platform/test/fake_clock.h"
#include "util/std_util.h"

namespace openscreen {
namespace platform {

using std::chrono::seconds;
using testing::EndsWith;
using testing::StartsWith;

namespace {

bssl::UniquePtr<EVP_PKEY> GenerateRsaKeypair() {
  bssl::UniquePtr<BIGNUM> prime(BN_new());
  EXPECT_NE(0, BN_set_word(prime.get(), RSA_F4));

  bssl::UniquePtr<RSA> rsa(RSA_new());
  EXPECT_NE(0, RSA_generate_key_ex(rsa.get(), 2048, prime.get(), nullptr));

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  EXPECT_NE(0, EVP_PKEY_set1_RSA(pkey.get(), rsa.get()));

  return pkey;
}

}  // namespace

TEST(TlsCredentialsTest, CredentialsAreGeneratedAppropriately) {
  bssl::UniquePtr<EVP_PKEY> pkey = GenerateRsaKeypair();

  FakeClock clock(Clock::now());
  ErrorOr<TlsCredentials> creds_or_error = TlsCredentials::Create(
      "test.com", seconds(31556952), platform::FakeClock::now, pkey.get());
  EXPECT_TRUE(creds_or_error.is_value());
  TlsCredentials credentials = std::move(creds_or_error.value());

  // Validate the generated certificate. A const cast is necessary because
  // openssl is not const correct for this method.
  EXPECT_NE(0,
            X509_verify(const_cast<X509*>(&credentials.certificate()),
                        const_cast<EVP_PKEY*>(credentials.key_pair().key())));

  const auto raw_cert = credentials.raw_der_certificate();
  EXPECT_GT(raw_cert.size(), 0u);

  // Calling d2i_X509 does validation of the certificate, beyond the checking
  // done in the i2d_X509 method that creates the raw_der_certificate.
  const unsigned char* raw_cert_begin = raw_cert.data();
  const bssl::UniquePtr<X509> x509_certificate(
      d2i_X509(nullptr, &raw_cert_begin, raw_cert.size()));
  EXPECT_NE(nullptr, x509_certificate);

  // Validate the private key
  // NOTE: both the private and public keys are base64 encoded, so we can't
  // actually validate their contents properly.
  const auto private_key = credentials.private_key_base64();
  EXPECT_GT(private_key.size(), 0u);

  // Validate the public key
  const auto public_key = credentials.public_key_base64();
  EXPECT_GT(public_key.size(), 0u);

  // Validate the hash
  // A SHA-256 hash should always be 256 bits, or 32 bytes.
  const unsigned int kSha256HashSizeInBytes = 32;
  EXPECT_EQ(credentials.public_key_hash().size(), kSha256HashSizeInBytes);
}

}  // namespace platform
}  // namespace openscreen
