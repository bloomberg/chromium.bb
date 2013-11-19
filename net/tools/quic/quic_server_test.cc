// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_server.h"

#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_utils.h"
#include "net/tools/quic/test_tools/mock_quic_dispatcher.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace net {
namespace tools {
namespace test {

namespace {

class QuicServerDispatchPacketTest : public ::testing::Test {
 public:
  QuicServerDispatchPacketTest()
      : crypto_config_("blah", QuicRandom::GetInstance()),
        dispatcher_(config_, crypto_config_, 1234, &eps_) {}


  void MaybeDispatchPacket(const QuicEncryptedPacket& packet) {
    IPEndPoint client_addr, server_addr;
    QuicServer::MaybeDispatchPacket(&dispatcher_, packet,
                                    client_addr, server_addr);
  }

 protected:
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  EpollServer eps_;
  MockQuicDispatcher dispatcher_;
};

TEST_F(QuicServerDispatchPacketTest, DoNotDispatchPacketWithoutGUID) {
  // Packet too short to be considered valid.
  unsigned char invalid_packet[] = { 0x00 };
  QuicEncryptedPacket encrypted_invalid_packet(
      QuicUtils::AsChars(invalid_packet), arraysize(invalid_packet), false);

  // We expect the invalid packet to be dropped, and ProcessPacket should never
  // be called.
  EXPECT_CALL(dispatcher_, ProcessPacket(_, _, _, _, _)).Times(0);
  MaybeDispatchPacket(encrypted_invalid_packet);
}

TEST_F(QuicServerDispatchPacketTest, DispatchValidPacket) {
  unsigned char valid_packet[] = {
    // public flags (8 byte guid)
    0x3C,
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // private flags
    0x00 };
  QuicEncryptedPacket encrypted_valid_packet(QuicUtils::AsChars(valid_packet),
                                             arraysize(valid_packet), false);

  EXPECT_CALL(dispatcher_, ProcessPacket(_, _, _, _, _)).Times(1);
  MaybeDispatchPacket(encrypted_valid_packet);
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
