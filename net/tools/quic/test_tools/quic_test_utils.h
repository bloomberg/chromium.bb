// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_connection.h"
#include "testing/gmock/include/gmock/gmock.h"

#ifndef NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

namespace net {

class EpollServer;
class IPEndPoint;

namespace tools {
namespace test {

class MockConnection : public QuicConnection {
 public:
  // Uses a QuicConnectionHelper created with fd and eps.
  MockConnection(QuicGuid guid,
                 IPEndPoint address,
                 int fd,
                 EpollServer* eps,
                 bool is_server);
  // Uses a MockHelper.
  MockConnection(QuicGuid guid, IPEndPoint address, bool is_server);
  MockConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicConnectionHelperInterface* helper, bool is_server);
  virtual ~MockConnection();

  // If the constructor that uses a MockHelper has been used then this method
  // will advance the time of the MockClock.
  void AdvanceTime(QuicTime::Delta delta);

  MOCK_METHOD3(ProcessUdpPacket, void(const IPEndPoint& self_address,
                                      const IPEndPoint& peer_address,
                                      const QuicEncryptedPacket& packet));
  MOCK_METHOD1(SendConnectionClose, void(QuicErrorCode error));
  MOCK_METHOD2(SendConnectionCloseWithDetails, void(
      QuicErrorCode error,
      const std::string& details));
  MOCK_METHOD2(SendRstStream, void(QuicStreamId id,
                                   QuicRstStreamErrorCode error));
  MOCK_METHOD3(SendGoAway, void(QuicErrorCode error,
                                QuicStreamId last_good_stream_id,
                                const std::string& reason));
  MOCK_METHOD0(OnCanWrite, bool());

  void ReallyProcessUdpPacket(const IPEndPoint& self_address,
                              const IPEndPoint& peer_address,
                              const QuicEncryptedPacket& packet) {
    return QuicConnection::ProcessUdpPacket(self_address, peer_address, packet);
  }

  virtual bool OnProtocolVersionMismatch(QuicVersionTag version) {
    return false;
  }

 private:
  const bool has_mock_helper_;

  DISALLOW_COPY_AND_ASSIGN(MockConnection);
};

}  // namespace test
}  // namespace tools
}  // namespace net

#endif  // NET_TOOLS_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
