// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_CONNECTION_ID_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_CONNECTION_ID_H_

#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_uint128.h"

namespace quic {

enum QuicConnectionIdLength {
  PACKET_0BYTE_CONNECTION_ID = 0,
  PACKET_8BYTE_CONNECTION_ID = 8,
};

// Connection IDs can be 0-18 bytes per IETF specifications.
const uint8_t kQuicMaxConnectionIdLength = 18;

class QUIC_EXPORT_PRIVATE QuicConnectionId {
 public:
  QuicConnectionId();  // Creates an all-zero connection ID.

  // Creator from host byte order uint64_t.
  explicit QuicConnectionId(uint64_t connection_id64);

  ~QuicConnectionId();

  // Immutable pointer to the connection ID bytes.
  const char* data() const;

  // Mutable pointer to the connection ID bytes.
  char* mutable_data();

  // Always returns 8.
  QuicConnectionIdLength length() const;

  // Returns whether the connection ID is zero.
  bool IsEmpty() const;

  // Converts to host byte order uint64_t.
  uint64_t ToUInt64() const;

  // Hash() is required to use connection IDs as keys in hash tables.
  size_t Hash() const;

  // operator<< allows easily logging connection IDs.
  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicConnectionId& v);

  bool operator==(const QuicConnectionId& v) const;
  bool operator!=(const QuicConnectionId& v) const;
  // operator< is required to use connection IDs as keys in hash tables.
  bool operator<(const QuicConnectionId& v) const;

 private:
  // The connection ID is currently represented in host byte order in |id64_|.
  // In the future, it will be saved in the first |length_| bytes of |data_|.
  //
  // Fields currently commented out since they trigger -Wunused-private-field
  // in Chromium build:
  //   uint8_t data_[kQuicMaxConnectionIdLength];
  //   QuicConnectionIdLength length_;
  uint64_t id64_;  // host byte order
};

// Creates an all-zero connection ID.
QUIC_EXPORT_PRIVATE QuicConnectionId EmptyQuicConnectionId();

// Converts connection ID from host-byte-order uint64_t to QuicConnectionId.
// This is currently the identity function.
QUIC_EXPORT_PRIVATE QuicConnectionId
QuicConnectionIdFromUInt64(uint64_t connection_id64);

// Converts connection ID from QuicConnectionId to host-byte-order uint64_t.
// This is currently the identity function.
QUIC_EXPORT_PRIVATE uint64_t
QuicConnectionIdToUInt64(QuicConnectionId connection_id);

// Returns whether the connection ID is zero.
QUIC_EXPORT_PRIVATE bool QuicConnectionIdIsEmpty(
    QuicConnectionId connection_id);

// QuicConnectionIdHash can be passed as hash argument to hash tables.
class QuicConnectionIdHash {
 public:
  size_t operator()(QuicConnectionId const& connection_id) const noexcept {
    return connection_id.Hash();
  }
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_CONNECTION_ID_H_
