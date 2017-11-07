// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_private_key.h"

#include "base/logging.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace net {

std::vector<uint16_t> SSLPrivateKey::DefaultAlgorithmPreferences(int type) {
  switch (type) {
    case EVP_PKEY_RSA:
      return {
          SSL_SIGN_RSA_PKCS1_SHA512, SSL_SIGN_RSA_PKCS1_SHA384,
          SSL_SIGN_RSA_PKCS1_SHA256, SSL_SIGN_RSA_PKCS1_SHA1,
      };
    case EVP_PKEY_EC:
      return {
          SSL_SIGN_ECDSA_SECP521R1_SHA512, SSL_SIGN_ECDSA_SECP384R1_SHA384,
          SSL_SIGN_ECDSA_SECP256R1_SHA256, SSL_SIGN_ECDSA_SHA1,
      };
    default:
      NOTIMPLEMENTED();
      return {};
  };
}

}  // namespace net
