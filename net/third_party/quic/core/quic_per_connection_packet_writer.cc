// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_per_connection_packet_writer.h"

namespace quic {

QuicPerConnectionPacketWriter::QuicPerConnectionPacketWriter(
    QuicPacketWriter* shared_writer)
    : shared_writer_(shared_writer) {}

QuicPerConnectionPacketWriter::~QuicPerConnectionPacketWriter() = default;

WriteResult QuicPerConnectionPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const QuicIpAddress& self_address,
    const QuicSocketAddress& peer_address,
    PerPacketOptions* options) {
  return shared_writer_->WritePacket(buffer, buf_len, self_address,
                                     peer_address, options);
}

bool QuicPerConnectionPacketWriter::IsWriteBlockedDataBuffered() const {
  return shared_writer_->IsWriteBlockedDataBuffered();
}

bool QuicPerConnectionPacketWriter::IsWriteBlocked() const {
  return shared_writer_->IsWriteBlocked();
}

void QuicPerConnectionPacketWriter::SetWritable() {
  shared_writer_->SetWritable();
}

QuicByteCount QuicPerConnectionPacketWriter::GetMaxPacketSize(
    const QuicSocketAddress& peer_address) const {
  return shared_writer_->GetMaxPacketSize(peer_address);
}

bool QuicPerConnectionPacketWriter::SupportsReleaseTime() const {
  return shared_writer_->SupportsReleaseTime();
}

bool QuicPerConnectionPacketWriter::IsBatchMode() const {
  return shared_writer_->IsBatchMode();
}

char* QuicPerConnectionPacketWriter::GetNextWriteLocation() const {
  return shared_writer_->GetNextWriteLocation();
}

WriteResult QuicPerConnectionPacketWriter::Flush() {
  return shared_writer_->Flush();
}

}  // namespace quic
