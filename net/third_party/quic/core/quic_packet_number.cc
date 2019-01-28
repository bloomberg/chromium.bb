// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_packet_number.h"

namespace quic {
namespace {
const uint64_t kUninitializedPacketNumber = 0;
}  // namespace

QuicPacketNumber::QuicPacketNumber()
    : packet_number_(kUninitializedPacketNumber) {}

QuicPacketNumber::QuicPacketNumber(uint64_t packet_number)
    : packet_number_(packet_number) {
  DCHECK_NE(kUninitializedPacketNumber, packet_number)
      << "Use default constructor for uninitialized packet number";
}

void QuicPacketNumber::Clear() {
  packet_number_ = kUninitializedPacketNumber;
}

uint64_t QuicPacketNumber::Hash() const {
  DCHECK(IsInitialized());
  return packet_number_;
}

uint64_t QuicPacketNumber::ToUint64() const {
  DCHECK(IsInitialized());
  return packet_number_;
}

bool QuicPacketNumber::IsInitialized() const {
  return packet_number_ != kUninitializedPacketNumber;
}

QuicPacketNumber& QuicPacketNumber::operator++() {
  DCHECK(IsInitialized() && ToUint64() < std::numeric_limits<uint64_t>::max());
  packet_number_++;
  return *this;
}

QuicPacketNumber QuicPacketNumber::operator++(int) {
  DCHECK(IsInitialized() && ToUint64() < std::numeric_limits<uint64_t>::max());
  QuicPacketNumber previous(*this);
  packet_number_++;
  return previous;
}

QuicPacketNumber& QuicPacketNumber::operator--() {
  DCHECK(IsInitialized() && ToUint64() > 1);
  packet_number_--;
  return *this;
}

QuicPacketNumber QuicPacketNumber::operator--(int) {
  DCHECK(IsInitialized() && ToUint64() > 1);
  QuicPacketNumber previous(*this);
  packet_number_--;
  return previous;
}

QuicPacketNumber& QuicPacketNumber::operator+=(uint64_t delta) {
  DCHECK(IsInitialized() &&
         std::numeric_limits<uint64_t>::max() - ToUint64() >= delta);
  packet_number_ += delta;
  return *this;
}

QuicPacketNumber& QuicPacketNumber::operator-=(uint64_t delta) {
  DCHECK(IsInitialized() && ToUint64() > delta);
  packet_number_ -= delta;
  return *this;
}

std::ostream& operator<<(std::ostream& os, const QuicPacketNumber& p) {
  if (p.IsInitialized()) {
    os << p.packet_number_;
  } else {
    os << "uninitialized";
  }
  return os;
}

}  // namespace quic
