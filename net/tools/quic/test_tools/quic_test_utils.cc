// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_test_utils.h"

#include "net/quic/quic_connection.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"

using base::StringPiece;
using net::test::MakeAckFrame;
using net::test::MockHelper;
using net::test::QuicConnectionPeer;

namespace net {
namespace tools {
namespace test {

QuicAckFrame MakeAckFrameWithNackRanges(
    size_t num_nack_ranges, QuicPacketSequenceNumber least_unacked) {
  QuicAckFrame ack = MakeAckFrame(2 * num_nack_ranges + least_unacked);
  // Add enough missing packets to get num_nack_ranges nack ranges.
  for (QuicPacketSequenceNumber i = 1; i < 2 * num_nack_ranges; i += 2) {
    ack.missing_packets.insert(least_unacked + i);
  }
  return ack;
}

TestSession::TestSession(QuicConnection* connection, const QuicConfig& config)
    : QuicSession(connection, config),
      crypto_stream_(nullptr) {
  InitializeSession();
}

TestSession::~TestSession() {}

void TestSession::SetCryptoStream(QuicCryptoStream* stream) {
  crypto_stream_ = stream;
}

QuicCryptoStream* TestSession::GetCryptoStream() {
  return crypto_stream_;
}

MockPacketWriter::MockPacketWriter() {
}

MockPacketWriter::~MockPacketWriter() {
}

MockQuicServerSessionVisitor::MockQuicServerSessionVisitor() {
}

MockQuicServerSessionVisitor::~MockQuicServerSessionVisitor() {
}

MockAckNotifierDelegate::MockAckNotifierDelegate() {
}

MockAckNotifierDelegate::~MockAckNotifierDelegate() {
}

TestWriterFactory::TestWriterFactory() : current_writer_(nullptr) {}
TestWriterFactory::~TestWriterFactory() {}

QuicPacketWriter* TestWriterFactory::Create(QuicPacketWriter* writer,
                                            QuicConnection* connection) {
  return new PerConnectionPacketWriter(this, writer, connection);
}

void TestWriterFactory::OnPacketSent(WriteResult result) {
  if (current_writer_ != nullptr && result.status == WRITE_STATUS_ERROR) {
    current_writer_->connection()->OnWriteError(result.error_code);
    current_writer_ = nullptr;
  }
}

void TestWriterFactory::Unregister(PerConnectionPacketWriter* writer) {
  if (current_writer_ == writer) {
    current_writer_ = nullptr;
  }
}

TestWriterFactory::PerConnectionPacketWriter::PerConnectionPacketWriter(
    TestWriterFactory* factory,
    QuicPacketWriter* writer,
    QuicConnection* connection)
    : QuicPerConnectionPacketWriter(writer, connection),
      factory_(factory) {
}

TestWriterFactory::PerConnectionPacketWriter::~PerConnectionPacketWriter() {
  factory_->Unregister(this);
}

WriteResult TestWriterFactory::PerConnectionPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const IPAddressNumber& self_address,
    const IPEndPoint& peer_address) {
  // A DCHECK(factory_current_writer_ == nullptr) would be wrong here -- this
  // class may be used in a setting where connection()->OnPacketSent() is called
  // in a different way, so TestWriterFactory::OnPacketSent might never be
  // called.
  factory_->current_writer_ = this;
  return QuicPerConnectionPacketWriter::WritePacket(buffer,
                                                    buf_len,
                                                    self_address,
                                                    peer_address);
}

MockTimeWaitListManager::MockTimeWaitListManager(
    QuicPacketWriter* writer,
    QuicServerSessionVisitor* visitor,
    EpollServer* eps)
    : QuicTimeWaitListManager(writer, visitor, eps, QuicSupportedVersions()) {
}

MockTimeWaitListManager::~MockTimeWaitListManager() {
}

}  // namespace test
}  // namespace tools
}  // namespace net
