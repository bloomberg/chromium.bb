// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/test_tools/quic_test_utils.h"

#include "base/sha1.h"
#include "net/quic/quic_connection.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_epoll_connection_helper.h"

using base::StringPiece;
using net::test::MockHelper;

namespace net {
namespace tools {
namespace test {

MockConnection::MockConnection(bool is_server)
    : QuicConnection(kTestConnectionId,
                     IPEndPoint(net::test::Loopback4(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     new testing::NiceMock<MockPacketWriter>(),
                     is_server, QuicSupportedVersions()),
      writer_(net::test::QuicConnectionPeer::GetWriter(this)),
      helper_(helper()) {
}

MockConnection::MockConnection(IPEndPoint address,
                               bool is_server)
    : QuicConnection(kTestConnectionId, address,
                     new testing::NiceMock<MockHelper>(),
                     new testing::NiceMock<MockPacketWriter>(),
                     is_server, QuicSupportedVersions()),
      writer_(net::test::QuicConnectionPeer::GetWriter(this)),
      helper_(helper()) {
}

MockConnection::MockConnection(QuicConnectionId connection_id,
                               bool is_server)
    : QuicConnection(connection_id,
                     IPEndPoint(net::test::Loopback4(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     new testing::NiceMock<MockPacketWriter>(),
                     is_server, QuicSupportedVersions()),
      writer_(net::test::QuicConnectionPeer::GetWriter(this)),
      helper_(helper()) {
}

MockConnection::MockConnection(bool is_server,
                               const QuicVersionVector& supported_versions)
    : QuicConnection(kTestConnectionId,
                     IPEndPoint(net::test::Loopback4(), kTestPort),
                     new testing::NiceMock<MockHelper>(),
                     new testing::NiceMock<MockPacketWriter>(),
                     is_server, supported_versions),
      writer_(net::test::QuicConnectionPeer::GetWriter(this)),
      helper_(helper()) {
}

MockConnection::~MockConnection() {
}

void MockConnection::AdvanceTime(QuicTime::Delta delta) {
  static_cast<MockHelper*>(helper())->AdvanceTime(delta);
}

uint64 SimpleRandom::RandUint64() {
  unsigned char hash[base::kSHA1Length];
  base::SHA1HashBytes(reinterpret_cast<unsigned char*>(&seed_), sizeof(seed_),
                      hash);
  memcpy(&seed_, hash, sizeof(seed_));
  return seed_;
}

TestSession::TestSession(QuicConnection* connection,
                         const QuicConfig& config)
    : QuicSession(connection, config),
      crypto_stream_(NULL) {
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

bool TestDecompressorVisitor::OnDecompressedData(StringPiece data) {
  data.AppendToString(&data_);
  return true;
}

void TestDecompressorVisitor::OnDecompressionError() {
  error_ = true;
}

MockAckNotifierDelegate::MockAckNotifierDelegate() {
}

MockAckNotifierDelegate::~MockAckNotifierDelegate() {
}

}  // namespace test
}  // namespace tools
}  // namespace net
