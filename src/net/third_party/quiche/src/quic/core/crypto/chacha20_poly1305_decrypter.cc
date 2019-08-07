// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/crypto/chacha20_poly1305_decrypter.h"

#include "third_party/boringssl/src/include/openssl/aead.h"
#include "third_party/boringssl/src/include/openssl/tls1.h"

namespace quic {

namespace {

const size_t kKeySize = 32;
const size_t kNonceSize = 12;

}  // namespace

ChaCha20Poly1305Decrypter::ChaCha20Poly1305Decrypter()
    : ChaChaBaseDecrypter(EVP_aead_chacha20_poly1305,
                          kKeySize,
                          kAuthTagSize,
                          kNonceSize,
                          /* use_ietf_nonce_construction */ false) {
  static_assert(kKeySize <= kMaxKeySize, "key size too big");
  static_assert(kNonceSize <= kMaxNonceSize, "nonce size too big");
}

ChaCha20Poly1305Decrypter::~ChaCha20Poly1305Decrypter() {}

uint32_t ChaCha20Poly1305Decrypter::cipher_id() const {
  return TLS1_CK_CHACHA20_POLY1305_SHA256;
}

}  // namespace quic
