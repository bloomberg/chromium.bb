// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_CRYPTO_QUIC_HKDF_H_
#define QUICHE_QUIC_CORE_CRYPTO_QUIC_HKDF_H_

#include <vector>

#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_string_piece.h"

namespace quic {

// QuicHKDF implements the key derivation function specified in RFC 5869
// (using SHA-256) and outputs key material, as needed by QUIC.
// See https://tools.ietf.org/html/rfc5869 for details.
class QUIC_EXPORT QuicHKDF {
 public:
  // |secret|: the input shared secret (or, from RFC 5869, the IKM).
  // |salt|: an (optional) public salt / non-secret random value. While
  // optional, callers are strongly recommended to provide a salt. There is no
  // added security value in making this larger than the SHA-256 block size of
  // 64 bytes.
  // |info|: an (optional) label to distinguish different uses of HKDF. It is
  // optional context and application specific information (can be a zero-length
  // string).
  // |key_bytes_to_generate|: the number of bytes of key material to generate
  // for both client and server.
  // |iv_bytes_to_generate|: the number of bytes of IV to generate for both
  // client and server.
  // |subkey_secret_bytes_to_generate|: the number of bytes of subkey secret to
  // generate, shared between client and server.
  QuicHKDF(QuicStringPiece secret,
           QuicStringPiece salt,
           QuicStringPiece info,
           size_t key_bytes_to_generate,
           size_t iv_bytes_to_generate,
           size_t subkey_secret_bytes_to_generate);

  // An alternative constructor that allows the client and server key/IV
  // lengths to be different.
  QuicHKDF(QuicStringPiece secret,
           QuicStringPiece salt,
           QuicStringPiece info,
           size_t client_key_bytes_to_generate,
           size_t server_key_bytes_to_generate,
           size_t client_iv_bytes_to_generate,
           size_t server_iv_bytes_to_generate,
           size_t subkey_secret_bytes_to_generate);

  ~QuicHKDF();

  QuicStringPiece client_write_key() const { return client_write_key_; }
  QuicStringPiece client_write_iv() const { return client_write_iv_; }
  QuicStringPiece server_write_key() const { return server_write_key_; }
  QuicStringPiece server_write_iv() const { return server_write_iv_; }
  QuicStringPiece subkey_secret() const { return subkey_secret_; }
  QuicStringPiece client_hp_key() const { return client_hp_key_; }
  QuicStringPiece server_hp_key() const { return server_hp_key_; }

 private:
  std::vector<uint8_t> output_;

  QuicStringPiece client_write_key_;
  QuicStringPiece server_write_key_;
  QuicStringPiece client_write_iv_;
  QuicStringPiece server_write_iv_;
  QuicStringPiece subkey_secret_;
  QuicStringPiece client_hp_key_;
  QuicStringPiece server_hp_key_;
};

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_CRYPTO_QUIC_HKDF_H_
