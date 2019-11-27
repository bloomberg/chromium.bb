// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/certificate_utils.h"

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>

#include <chrono>

#include "gtest/gtest.h"
#include "platform/api/time.h"
#include "platform/base/error.h"
#include "util/std_util.h"

namespace openscreen {
namespace {

constexpr char kName[] = "test.com";
constexpr auto kDuration = std::chrono::seconds(31556952);

bssl::UniquePtr<EVP_PKEY> GenerateRsaKeypair() {
  bssl::UniquePtr<BIGNUM> prime(BN_new());
  EXPECT_NE(0, BN_set_word(prime.get(), RSA_F4));

  bssl::UniquePtr<RSA> rsa(RSA_new());
  EXPECT_NE(0, RSA_generate_key_ex(rsa.get(), 2048, prime.get(), nullptr));

  bssl::UniquePtr<EVP_PKEY> pkey(EVP_PKEY_new());
  EXPECT_NE(0, EVP_PKEY_set1_RSA(pkey.get(), rsa.get()));

  return pkey;
}

TEST(CertificateUtilTest, CreatesValidCertificate) {
  bssl::UniquePtr<EVP_PKEY> pkey = GenerateRsaKeypair();

  ErrorOr<bssl::UniquePtr<X509>> certificate =
      CreateCertificate(kName, kDuration, *pkey);
  ASSERT_TRUE(certificate.is_value());

  // Validate the generated certificate.
  EXPECT_NE(0, X509_verify(certificate.value().get(), pkey.get()));
}

TEST(CertificateUtilTest, ExportsAndImportsCertificate) {
  bssl::UniquePtr<EVP_PKEY> pkey = GenerateRsaKeypair();
  ErrorOr<bssl::UniquePtr<X509>> certificate =
      CreateCertificate(kName, kDuration, *pkey);
  ASSERT_TRUE(certificate.is_value());

  ErrorOr<std::vector<uint8_t>> exported =
      ExportCertificate(*certificate.value());
  ASSERT_TRUE(exported.is_value()) << exported.error();
  EXPECT_FALSE(exported.value().empty());

  ErrorOr<bssl::UniquePtr<X509>> imported =
      ImportCertificate(exported.value().data(), exported.value().size());
  ASSERT_TRUE(imported.is_value()) << imported.error();
  ASSERT_TRUE(imported.value().get());

  // Validate the imported certificate.
  EXPECT_NE(0, X509_verify(imported.value().get(), pkey.get()));
}

}  // namespace
}  // namespace openscreen
