// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_connection_id.h"

#include <iomanip>

#include "net/third_party/quic/platform/api/quic_endian.h"
#include "net/third_party/quic/platform/api/quic_logging.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"

namespace quic {

QuicConnectionId::QuicConnectionId() : id64_(0) {}

QuicConnectionId::QuicConnectionId(uint64_t connection_id64)
    : id64_(connection_id64) {}

QuicConnectionId::~QuicConnectionId() {}

uint64_t QuicConnectionId::ToUInt64() const {
  return id64_;
}

const char* QuicConnectionId::data() const {
  return reinterpret_cast<const char*>(&id64_);
}

char* QuicConnectionId::mutable_data() {
  return reinterpret_cast<char*>(&id64_);
}

QuicConnectionIdLength QuicConnectionId::length() const {
  return PACKET_8BYTE_CONNECTION_ID;
}

bool QuicConnectionId::IsEmpty() const {
  return id64_ == 0;
}

size_t QuicConnectionId::Hash() const {
  return id64_;
}

std::ostream& operator<<(std::ostream& os, const QuicConnectionId& v) {
  os << v.id64_;
  return os;
}

bool QuicConnectionId::operator==(const QuicConnectionId& v) const {
  return id64_ == v.id64_;
}

bool QuicConnectionId::operator!=(const QuicConnectionId& v) const {
  return !(v == *this);
}

bool QuicConnectionId::operator<(const QuicConnectionId& v) const {
  return id64_ < v.id64_;
}

QuicConnectionId EmptyQuicConnectionId() {
  return QuicConnectionId();
}

QuicConnectionId QuicConnectionIdFromUInt64(uint64_t connection_id64) {
  return QuicConnectionId(connection_id64);
}

uint64_t QuicConnectionIdToUInt64(QuicConnectionId connection_id) {
  return connection_id.ToUInt64();
}

bool QuicConnectionIdIsEmpty(QuicConnectionId connection_id) {
  return connection_id.IsEmpty();
}

}  // namespace quic
