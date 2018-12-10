// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_connection_id.h"

#include <iomanip>

#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quic/platform/api/quic_logging.h"

namespace quic {

QuicConnectionId EmptyQuicConnectionId() {
  return 0;
}

QuicConnectionId QuicConnectionIdFromUInt64(uint64_t connection_id64) {
  return connection_id64;
}

uint64_t QuicConnectionIdToUInt64(QuicConnectionId connection_id) {
  return connection_id;
}

bool QuicConnectionIdIsEmpty(QuicConnectionId connection_id) {
  return connection_id == 0;
}

}  // namespace quic
