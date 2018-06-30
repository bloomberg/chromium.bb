// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_HKDF_H_
#define NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_HKDF_H_

#include "net/third_party/quic/platform/impl/quic_hkdf_impl.h"

#include "net/third_party/quic/platform/api/quic_string_piece.h"

namespace quic {

// QuicHKDF implements the key derivation function specified in RFC 5869
// (using SHA-256) and outputs key material, as needed by QUIC.
// See https://tools.ietf.org/html/rfc5869 for details.
class QuicHKDF {
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
           size_t subkey_secret_bytes_to_generate)
      : impl_(secret,
              salt,
              info,
              key_bytes_to_generate,
              iv_bytes_to_generate,
              subkey_secret_bytes_to_generate) {}

  // An alternative constructor that allows the client and server key/IV
  // lengths to be different.
  QuicHKDF(QuicStringPiece secret,
           QuicStringPiece salt,
           QuicStringPiece info,
           size_t client_key_bytes_to_generate,
           size_t server_key_bytes_to_generate,
           size_t client_iv_bytes_to_generate,
           size_t server_iv_bytes_to_generate,
           size_t subkey_secret_bytes_to_generate)
      : impl_(secret,
              salt,
              info,
              client_key_bytes_to_generate,
              server_key_bytes_to_generate,
              client_iv_bytes_to_generate,
              server_iv_bytes_to_generate,
              subkey_secret_bytes_to_generate) {}

  ~QuicHKDF() = default;

  QuicStringPiece client_write_key() const { return impl_.client_write_key(); }
  QuicStringPiece client_write_iv() const { return impl_.client_write_iv(); }
  QuicStringPiece server_write_key() const { return impl_.server_write_key(); }
  QuicStringPiece server_write_iv() const { return impl_.server_write_iv(); }
  QuicStringPiece subkey_secret() const { return impl_.subkey_secret(); }

 private:
  QuicHKDFImpl impl_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_PLATFORM_API_QUIC_HKDF_H_
