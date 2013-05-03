// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_epoll_connection_helper.h"

#include "net/quic/crypto/crypto_protocol.h"
#include "net/quic/crypto/quic_decrypter.h"
#include "net/quic/crypto/quic_encrypter.h"
#include "net/quic/crypto/quic_random.h"
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
using testing::Return;

namespace net {
namespace tools {
namespace test {
namespace {

const char data1[] = "foo";
const bool kHasData = true;

class TestConnectionHelper : public QuicEpollConnectionHelper {
 public:
  TestConnectionHelper(int fd, EpollServer* eps)
      : QuicEpollConnectionHelper(fd, eps) {
  }

  virtual int WritePacketToWire(const QuicEncryptedPacket& packet,
                                int* error) OVERRIDE {
    QuicFramer framer(kQuicVersion1, QuicTime::Zero(), true);
    FramerVisitorCapturingFrames visitor;
    framer.set_visitor(&visitor);
    EXPECT_TRUE(framer.ProcessPacket(packet));
    header_ = *visitor.header();
    *error = 0;
    return packet.length();
  }

  QuicPacketHeader* header() { return &header_; }

 private:
  QuicPacketHeader header_;
};

class TestConnection : public QuicConnection {
 public:
  TestConnection(QuicGuid guid,
                 IPEndPoint address,
                 TestConnectionHelper* helper)
      : QuicConnection(guid, address, helper, false) {
  }

  void SendAck() {
    QuicConnectionPeer::SendAck(this);
  }

  void SetSendAlgorithm(SendAlgorithmInterface* send_algorithm) {
    QuicConnectionPeer::SetSendAlgorithm(this, send_algorithm);
  }

  using QuicConnection::SendOrQueuePacket;
};

class QuicConnectionHelperTest : public ::testing::Test {
 protected:
  QuicConnectionHelperTest()
      : guid_(42),
        framer_(kQuicVersion1, QuicTime::Zero(), false),
        send_algorithm_(new testing::StrictMock<MockSendAlgorithm>),
        helper_(new TestConnectionHelper(0, &epoll_server_)),
        connection_(guid_, IPEndPoint(), helper_),
        frame1_(1, false, 0, data1) {
    connection_.set_visitor(&visitor_);
    connection_.SetSendAlgorithm(send_algorithm_);
    epoll_server_.set_timeout_in_us(-1);
    EXPECT_CALL(*send_algorithm_, TimeUntilSend(_, _, _)).WillRepeatedly(Return(
        QuicTime::Delta::Zero()));
  }

  QuicPacket* ConstructDataPacket(QuicPacketSequenceNumber number,
                                  QuicFecGroupNumber fec_group) {
    header_.public_header.version_flag = false;
    header_.public_header.reset_flag = false;
    header_.fec_flag = false;
    header_.entropy_flag = false;
    header_.fec_entropy_flag = false;
    header_.packet_sequence_number = number;
    header_.fec_group = fec_group;

    QuicFrames frames;
    QuicFrame frame(&frame1_);
    frames.push_back(frame);
    return framer_.ConstructFrameDataPacket(header_, frames).packet;
  }

  QuicGuid guid_;
  QuicFramer framer_;

  MockEpollServer epoll_server_;
  testing::StrictMock<MockSendAlgorithm>* send_algorithm_;
  TestConnectionHelper* helper_;
  TestConnection connection_;
  testing::StrictMock<MockConnectionVisitor> visitor_;

  QuicPacketHeader header_;
  QuicStreamFrame frame1_;
};

TEST_F(QuicConnectionHelperTest, DISABLED_TestRetransmission) {
  //FLAGS_fake_packet_loss_percentage = 100;
  const int64 kDefaultRetransmissionTimeMs = 500;

  const char buffer[] = "foo";
  const size_t packet_size = GetPacketHeaderSize(kIncludeVersion) +
      QuicFramer::GetMinStreamFrameSize() + arraysize(buffer) - 1;
  EXPECT_CALL(*send_algorithm_,
              SentPacket(_, 1, packet_size, NOT_RETRANSMISSION));
  EXPECT_CALL(*send_algorithm_, AbandoningPacket(1, packet_size));
  connection_.SendStreamData(1, buffer, 0, false);
  EXPECT_EQ(1u, helper_->header()->packet_sequence_number);
  EXPECT_CALL(*send_algorithm_,
              SentPacket(_, 2, packet_size, IS_RETRANSMISSION));
  epoll_server_.AdvanceByAndCallCallbacks(kDefaultRetransmissionTimeMs * 1000);

  EXPECT_EQ(2u, helper_->header()->packet_sequence_number);
}

TEST_F(QuicConnectionHelperTest, InitialTimeout) {
  EXPECT_TRUE(connection_.connected());

  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, !kHasData));
  epoll_server_.WaitForEventsAndExecuteCallbacks();
  EXPECT_FALSE(connection_.connected());
  EXPECT_EQ(kDefaultTimeoutUs, epoll_server_.NowInUsec());
}

TEST_F(QuicConnectionHelperTest, TimeoutAfterSend) {
  EXPECT_TRUE(connection_.connected());
  EXPECT_EQ(0, epoll_server_.NowInUsec());

  // When we send a packet, the timeout will change to 5000 + kDefaultTimeout.
  epoll_server_.AdvanceBy(5000);
  EXPECT_EQ(5000, epoll_server_.NowInUsec());

  // Send an ack so we don't set the retransmission alarm.
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));
  connection_.SendAck();

  // The original alarm will fire.  We should not time out because we had a
  // network event at t=5000.  The alarm will reregister.
  epoll_server_.WaitForEventsAndExecuteCallbacks();
  EXPECT_EQ(kDefaultTimeoutUs, epoll_server_.NowInUsec());

  // This time, we should time out.
  EXPECT_CALL(visitor_, ConnectionClose(QUIC_CONNECTION_TIMED_OUT, false));
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 2, _, NOT_RETRANSMISSION));
  epoll_server_.WaitForEventsAndExecuteCallbacks();
  EXPECT_EQ(kDefaultTimeoutUs + 5000, epoll_server_.NowInUsec());
  EXPECT_FALSE(connection_.connected());
}

TEST_F(QuicConnectionHelperTest, SendSchedulerDelayThenSend) {
  // Test that if we send a packet with a delay, it ends up queued.
  QuicPacket* packet = ConstructDataPacket(1, 0);
  EXPECT_CALL(
      *send_algorithm_, TimeUntilSend(_, NOT_RETRANSMISSION, _)).WillOnce(
          testing::Return(QuicTime::Delta::FromMicroseconds(1)));
  connection_.SendOrQueuePacket(ENCRYPTION_NONE, 1, packet, 0,
                                HAS_RETRANSMITTABLE_DATA);
  EXPECT_CALL(*send_algorithm_, SentPacket(_, 1, _, NOT_RETRANSMISSION));
  EXPECT_EQ(1u, connection_.NumQueuedPackets());

  // Advance the clock to fire the alarm, and configure the scheduler
  // to permit the packet to be sent.
  EXPECT_CALL(
      *send_algorithm_, TimeUntilSend(_, NOT_RETRANSMISSION, _)).WillRepeatedly(
          testing::Return(QuicTime::Delta::Zero()));
  EXPECT_CALL(visitor_, OnCanWrite()).WillOnce(testing::Return(true));
  epoll_server_.AdvanceByAndCallCallbacks(1);
  EXPECT_EQ(0u, connection_.NumQueuedPackets());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
