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

QuicVersion QuicVersionMax() { return QuicSupportedVersions().front(); }

QuicVersion QuicVersionMin() { return QuicSupportedVersions().back(); }

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               int fd,
                               EpollServer* eps,
                               bool is_server)
    : QuicConnection(guid, address,
                     new QuicEpollConnectionHelper(eps),
                     new QuicDefaultPacketWriter(fd), is_server,
                     QuicSupportedVersions()),
      has_mock_helper_(false),
      writer_(net::test::QuicConnectionPeer::GetWriter(this)),
      helper_(helper()) {
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               bool is_server)
    : QuicConnection(guid, address, new testing::NiceMock<MockHelper>(),
                     new testing::NiceMock<MockPacketWriter>(),
                     is_server, QuicSupportedVersions()),
      has_mock_helper_(true),
      writer_(net::test::QuicConnectionPeer::GetWriter(this)),
      helper_(helper()) {
}

MockConnection::MockConnection(QuicGuid guid,
                               IPEndPoint address,
                               QuicConnectionHelperInterface* helper,
                               QuicPacketWriter* writer,
                               bool is_server)
    : QuicConnection(guid, address, helper, writer, is_server,
                     QuicSupportedVersions()),
      has_mock_helper_(false) {
}

MockConnection::~MockConnection() {
}

void MockConnection::AdvanceTime(QuicTime::Delta delta) {
  CHECK(has_mock_helper_) << "Cannot advance time unless a MockClock is being"
                             " used";
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
                         const QuicConfig& config,
                         bool is_server)
    : QuicSession(connection, config, is_server),
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

MockQuicSessionOwner::MockQuicSessionOwner() {
}

MockQuicSessionOwner::~MockQuicSessionOwner() {
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
