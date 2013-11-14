// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_connection_helper.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_framer.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/test_tools/mock_epoll_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::FramerVisitorCapturingFrames;
using net::test::MockSendAlgorithm;
using net::test::QuicConnectionPeer;
using net::test::MockConnectionVisitor;
using net::tools::test::MockEpollServer;
using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace net {
namespace tools {
namespace test {
namespace {

const char kData[] = "foo";
const bool kFromPeer = true;

class TestWriter : public QuicPacketWriter {
 public:
  // QuicPacketWriter
  virtual WriteResult WritePacket(
      const char* buffer, size_t buf_len,
      const IPAddressNumber& self_address,
      const IPEndPoint& peer_address,
      QuicBlockedWriterInterface* blocked_writer) OVERRIDE {
    QuicFramer framer(QuicSupportedVersions(), QuicTime::Zero(), true);
    FramerVisitorCapturingFrames visitor;
    framer.set_visitor(&visitor);
    QuicEncryptedPacket packet(buffer, buf_len);
    EXPECT_TRUE(framer.ProcessPacket(packet));
    header_ = *visitor.header();
    return WriteResult(WRITE_STATUS_OK, packet.length());
  }

  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE {
    return false;
  }

  // Returns the header from the last packet written.
  const QuicPacketHeader& header() { return header_; }

 private:
  QuicPacketHeader header_;
};

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicGuid guid,
                 IPEndPoint address,
                 QuicEpollConnectionHelper* helper,
                 TestWriter* writer)
      : QuicConnection(guid, address, helper, writer, false,
                       QuicSupportedVersions()) {
  }

  void SendAck() {
    QuicConnectionPeer::SendAck(this);
  }

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }
};

class QuicEpollConnectionHelperTest : public ::testing::Test {
 protected:
  QuicEpollConnectionHelperTest()
      : guid_(42),
        framer_(QuicSupportedVersions(), QuicTime::Zero(), false),
        send_algorithm_(new testing::StrictMock<MockSendAlgorithm>),
        helper_(&epoll_server_),
        connection_(guid_, IPEndPoint(), &helper_, &writer_),
        frame_(3, false, 0, MakeIOVector(kData)) {
    connection_.set_visitor(&visitor_);
    connection_.SetSendAlgorithm(send_algorithm_);
    epoll_server_.set_timeout_in_us(-1);
    EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _, _)).
        WillRepeatedly(Return(QuicTime::Delta::Zero()));
    EXPECT_CALL(*send_algorithm_, BandwidthEstimate()).WillRepeatedly(Return(
        QuicBandwidth::FromKBitsPerSecond(100)));
    EXPECT_CALL(*send_algorithm_, SmoothedRtt()).WillRepeatedly(Return(
        QuicTime::Delta::FromMilliseconds(100)));
    ON_CALL(*send_algorithm_, OnPacketSent(_, _, _, _, _))
        .WillByDefault(Return(true));
  }

  QuicPacket* ConstructDataPacket(QuicPacketSequenceNumber number,
                                  QuicFecGroupNumber fec_group) {
    QuicPacketHeader header;
    header.public_header.version_flag = false;
    header.public_header.reset_flag = false;
    header.fec_flag = false;
    header.entropy_flag = false;
    header.packet_sequence_number = number;
    header.is_in_fec_group = fec_group == 0 ? NOT_IN_FEC_GROUP : IN_FEC_GROUP;
    header.fec_group = fec_group;

    QuicFrames frames;
    frames.push_back(QuicFrame(&frame_));
    return framer_.BuildUnsizedDataPacket(header, frames).packet;
  }

  QuicGuid guid_;
  QuicFramer framer_;

  MockEpollServer epoll_server_;
  testing::StrictMock<MockSendAlgorithm>* send_algorithm_;
  QuicEpollConnectionHelper helper_;
  TestWriter writer_;
  TestConnection connection_;
  testing::StrictMock<MockConnectionVisitor> visitor_;

  QuicStreamFrame frame_;
};

TEST_F(QuicEpollConnectionHelperTest, DISABLED_TestRTORetransmission) {
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
      Return(QuicTime::Delta::Zero()));
  const int64 kDefaultRetransmissionTimeMs = 500;

  char buffer[] = "foo";
  const size_t packet_size =
      QuicPacketCreator::StreamFramePacketOverhead(
          framer_.version(), PACKET_8BYTE_GUID, kIncludeVersion,
          PACKET_1BYTE_SEQUENCE_NUMBER, NOT_IN_FEC_GROUP) +
      arraysize(buffer) - 1;

  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, 1, packet_size, NOT_RETRANSMISSION, _));
  EXPECT_CALL(*send_algorithm_, OnPacketAbandoned(1, packet_size));
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(Return(true));
  EXPECT_CALL(visitor_, HasPendingHandshake()).Times(AnyNumber());
  IOVector data;
  data.Append(buffer, 3);
  connection_.SendStreamData(1, data, 0, false, NULL);
  EXPECT_EQ(1u, writer_.header().packet_sequence_number);
  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, 2, packet_size, RTO_RETRANSMISSION, _));
  epoll_server_.AdvanceByAndCallCallbacks(kDefaultRetransmissionTimeMs * 1000);

  EXPECT_EQ(2u, writer_.header().packet_sequence_number);
}

TEST_F(QuicEpollConnectionHelperTest, InitialTimeout) {
  EXPECT_TRUE(connection_.connected());

  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, 1, _, NOT_RETRANSMISSION,
                                             HAS_RETRANSMITTABLE_DATA));
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillOnce(
      Return(QuicTime::Delta::FromMicroseconds(1)));
  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_CONNECTION_TIMED_OUT, !kFromPeer));
  epoll_server_.WaitForEventsAndExecuteCallbacks();
  EXPECT_FALSE(connection_.connected());
  EXPECT_EQ(kDefaultInitialTimeoutSecs * 1000000, epoll_server_.NowInUsec());
}

TEST_F(QuicEpollConnectionHelperTest, TimeoutAfterSend) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(0, epoll_server_.NowInUsec());

  // When we send a packet, the timeout will change to 5000 +
  // kDefaultInitialTimeoutSecs.
  epoll_server_.AdvanceBy(5000);
  EXPECT_EQ(5000, epoll_server_.NowInUsec());

  // Send an ack so we don't set the retransmission alarm.
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, 1, _, NOT_RETRANSMISSION,
                                             NO_RETRANSMITTABLE_DATA));
  connection_.SendAck();

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5000.  The alarm will reregister.
  epoll_server_.WaitForEventsAndExecuteCallbacks();
  EXPECT_EQ(kDefaultInitialTimeoutSecs * 1000000, epoll_server_.NowInUsec());

  // This time, we should time out.
  EXPECT_CALL(visitor_,
              OnConnectionClosed(QUIC_CONNECTION_TIMED_OUT, !kFromPeer));
  EXPECT_CALL(*send_algorithm_, OnPacketSent(_, 2, _, NOT_RETRANSMISSION,
                                             HAS_RETRANSMITTABLE_DATA));
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillOnce(
      Return(QuicTime::Delta::FromMicroseconds(1)));
  epoll_server_.WaitForEventsAndExecuteCallbacks();
  EXPECT_EQ(kDefaultInitialTimeoutSecs * 1000000 + 5000,
            epoll_server_.NowInUsec());
  EXPECT_FALSE(connection_.connected());
}

TEST_F(QuicEpollConnectionHelperTest, SendSchedulerDelayThenSend) {
  // Test that if we send a packet with a delay, it ends up queued.
  EXPECT_CALL(*send_algorithm_, RetransmissionDelay()).WillRepeatedly(
      Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(
      *send_algorithm_, TimeUntilSend(_, NOT_RETRANSMISSION, _, _)).WillOnce(
          Return(QuicTime::Delta::FromMicroseconds(1)));
  connection_.OnSerializedPacket(
      SerializedPacket(1, PACKET_6BYTE_SEQUENCE_NUMBER,
                       ConstructDataPacket(1, 0), 0,
                       new RetransmittableFrames));

  EXPECT_CALL(*send_algorithm_,
              OnPacketSent(_, 1, _, NOT_RETRANSMISSION, _));
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(*send_algorithm_,
              TimeUntilSend(_, NOT_RETRANSMISSION, _, _)).WillRepeatedly(
      Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(Return(true));
  EXPECT_CALL(visitor_, HasPendingHandshake()).Times(AnyNumber());
  epoll_server_.AdvanceByAndCallCallbacks(1);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
