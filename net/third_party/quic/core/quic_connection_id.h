// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_CONNECTION_ID_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_CONNECTION_ID_H_

#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_uint128.h"

namespace quic {

typedef uint8_t QuicConnectionIdLength;

typedef uint64_t QuicConnectionId;

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

static_assert(sizeof(QuicConnectionId) == sizeof(uint64_t), "size changed");

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_CONNECTION_ID_H_
