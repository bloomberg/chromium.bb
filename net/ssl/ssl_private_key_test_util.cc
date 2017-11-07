// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_private_key_test_util.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "crypto/openssl_util.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

using net::test::IsOk;

namespace net {

namespace {

bool VerifyWithOpenSSL(uint16_t algorithm,
                       const base::StringPiece& digest,
                       EVP_PKEY* key,
                       const base::StringPiece& signature) {
  bssl::UniquePtr<EVP_PKEY_CTX> ctx(EVP_PKEY_CTX_new(key, nullptr));
  if (!ctx || !EVP_PKEY_verify_init(ctx.get()) ||
      !EVP_PKEY_CTX_set_signature_md(
          ctx.get(), SSL_get_signature_algorithm_digest(algorithm)) ||
      !EVP_PKEY_verify(
          ctx.get(), reinterpret_cast<const uint8_t*>(signature.data()),
          signature.size(), reinterpret_cast<const uint8_t*>(digest.data()),
          digest.size())) {
    return false;
  }

  return true;
}

void OnSignComplete(base::RunLoop* loop,
                    Error* out_error,
                    std::string* out_signature,
                    Error error,
                    const std::vector<uint8_t>& signature) {
  *out_error = error;
  out_signature->assign(signature.begin(), signature.end());
  loop->Quit();
}

Error DoKeySigningWithWrapper(SSLPrivateKey* key,
                              uint16_t algorithm,
                              const base::StringPiece& digest,
                              std::string* result) {
  Error error;
  base::RunLoop loop;
  key->SignDigest(
      algorithm, digest,
      base::Bind(OnSignComplete, base::Unretained(&loop),
                 base::Unretained(&error), base::Unretained(result)));
  loop.Run();
  return error;
}

}  // namespace

void TestSSLPrivateKeyMatches(SSLPrivateKey* key, const std::string& pkcs8) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // Create the equivalent OpenSSL key.
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(pkcs8.data()), pkcs8.size());
  bssl::UniquePtr<EVP_PKEY> openssl_key(EVP_parse_private_key(&cbs));
  ASSERT_TRUE(openssl_key);
  EXPECT_EQ(0u, CBS_len(&cbs));

  // Test all supported algorithms.
  std::vector<uint16_t> preferences = key->GetAlgorithmPreferences();

  // To support TLS 1.1 and earlier, RSA keys must implicitly support MD5-SHA1,
  // despite not being advertised.
  preferences.push_back(SSL_SIGN_RSA_PKCS1_MD5_SHA1);

  for (uint16_t algorithm : preferences) {
    SCOPED_TRACE(
        SSL_get_signature_algorithm_name(algorithm, 0 /* exclude curve */));
    // BoringSSL will skip signatures algorithms that don't match the key type.
    if (EVP_PKEY_id(openssl_key.get()) !=
        SSL_get_signature_algorithm_key_type(algorithm)) {
      continue;
    }

    const EVP_MD* md = SSL_get_signature_algorithm_digest(algorithm);
    ASSERT_TRUE(md);
    std::string digest(EVP_MD_size(md), 'a');

    // Test the key generates valid signatures.
    std::string signature;
    Error error = DoKeySigningWithWrapper(key, algorithm, digest, &signature);
    EXPECT_THAT(error, IsOk());
    EXPECT_TRUE(
        VerifyWithOpenSSL(algorithm, digest, openssl_key.get(), signature));
  }
}

}  // namespace net
