// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "util/crypto/rsa_private_key.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "openssl/bn.h"
#include "openssl/bytestring.h"
#include "openssl/evp.h"
#include "openssl/mem.h"
#include "openssl/rsa.h"
#include "platform/api/logging.h"
#include "util/crypto/openssl_util.h"

namespace openscreen {
RSAPrivateKey::~RSAPrivateKey() = default;

// static
std::unique_ptr<RSAPrivateKey> RSAPrivateKey::Create(uint16_t num_bits) {
  OpenSSLErrStackTracer err_tracer(CURRENT_LOCATION);

  bssl::UniquePtr<RSA> rsa_key(RSA_new());
  bssl::UniquePtr<BIGNUM> exponent(BN_new());
  if (!rsa_key.get() || !exponent.get() || !BN_set_word(exponent.get(), 65537L))
    return nullptr;

  if (!RSA_generate_key_ex(rsa_key.get(), num_bits, exponent.get(), nullptr))
    return nullptr;

  std::unique_ptr<RSAPrivateKey> result(new RSAPrivateKey);
  result->key_.reset(EVP_PKEY_new());
  if (!result->key_ || !EVP_PKEY_set1_RSA(result->key_.get(), rsa_key.get()))
    return nullptr;

  return result;
}

// static
std::unique_ptr<RSAPrivateKey> RSAPrivateKey::CreateFromPrivateKeyInfo(
    const std::vector<uint8_t>& input) {
  OpenSSLErrStackTracer err_tracer(CURRENT_LOCATION);

  CBS private_key_cbs;
  CBS_init(&private_key_cbs, input.data(), input.size());
  bssl::UniquePtr<EVP_PKEY> private_key(
      EVP_parse_private_key(&private_key_cbs));
  if (!private_key || CBS_len(&private_key_cbs) != 0 ||
      EVP_PKEY_id(private_key.get()) != EVP_PKEY_RSA)
    return nullptr;

  std::unique_ptr<RSAPrivateKey> result(new RSAPrivateKey);
  result->key_ = std::move(private_key);
  return result;
}

// static
std::unique_ptr<RSAPrivateKey> RSAPrivateKey::CreateFromKey(EVP_PKEY* key) {
  OSP_DCHECK(key);
  if (EVP_PKEY_type(key->type) != EVP_PKEY_RSA)
    return nullptr;
  std::unique_ptr<RSAPrivateKey> result(new RSAPrivateKey);
  result->key_ = bssl::UpRef(key);
  return result;
}

std::unique_ptr<RSAPrivateKey> RSAPrivateKey::Copy() const {
  std::unique_ptr<RSAPrivateKey> result(new RSAPrivateKey);
  bssl::UniquePtr<RSA> rsa(EVP_PKEY_get1_RSA(key_.get()));
  if (!rsa)
    return nullptr;
  result->key_.reset(EVP_PKEY_new());
  if (!EVP_PKEY_set1_RSA(result->key_.get(), rsa.get()))
    return nullptr;
  return result;
}

bool RSAPrivateKey::ExportPrivateKey(std::vector<uint8_t>* output) const {
  OpenSSLErrStackTracer err_tracer(CURRENT_LOCATION);
  uint8_t* der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_private_key(cbb.get(), key_.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  output->assign(der, der + der_len);
  OPENSSL_free(der);
  return true;
}

bool RSAPrivateKey::ExportPublicKey(std::vector<uint8_t>* output) const {
  OpenSSLErrStackTracer err_tracer(CURRENT_LOCATION);
  uint8_t* der;
  size_t der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), key_.get()) ||
      !CBB_finish(cbb.get(), &der, &der_len)) {
    return false;
  }
  output->assign(der, der + der_len);
  OPENSSL_free(der);
  return true;
}

RSAPrivateKey::RSAPrivateKey() = default;
}  // namespace openscreen
