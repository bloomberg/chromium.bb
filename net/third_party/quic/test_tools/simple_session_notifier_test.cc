// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/test_tools/simple_session_notifier.h"

#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/test_tools/quic_connection_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

using testing::_;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

class MockQuicConnectionWithSendStreamData : public MockQuicConnection {
 public:
  MockQuicConnectionWithSendStreamData(MockQuicConnectionHelper* helper,
                                       MockAlarmFactory* alarm_factory,
                                       Perspective perspective)
      : MockQuicConnection(helper, alarm_factory, perspective) {}

  MOCK_METHOD4(SendStreamData,
               QuicConsumedData(QuicStreamId id,
                                size_t write_length,
                                QuicStreamOffset offset,
                                StreamSendingState state));
};

class SimpleSessionNotifierTest : public QuicTest {
 public:
  SimpleSessionNotifierTest()
      : connection_(&helper_, &alarm_factory_, Perspective::IS_CLIENT),
        notifier_(&connection_) {
    connection_.set_visitor(&visitor_);
    QuicConnectionPeer::SetSessionDecidesWhatToWrite(&connection_);
    EXPECT_FALSE(notifier_.WillingToWrite());
    EXPECT_EQ(0u, notifier_.StreamBytesSent());
    EXPECT_FALSE(notifier_.HasBufferedStreamData());
  }

  bool ControlFrameConsumed(const QuicFrame& frame) {
    DeleteFrame(&const_cast<QuicFrame&>(frame));
    return true;
  }

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnectionVisitor visitor_;
  StrictMock<MockQuicConnectionWithSendStreamData> connection_;
  SimpleSessionNotifier notifier_;
};

TEST_F(SimpleSessionNotifierTest, WriteOrBufferData) {
  InSequence s;
  EXPECT_CALL(connection_, SendStreamData(3, 1024, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(1024, false)));
  notifier_.WriteOrBufferData(3, 1024, NO_FIN);
  EXPECT_EQ(0u, notifier_.StreamBytesToSend());
  EXPECT_CALL(connection_, SendStreamData(5, 512, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(512, false)));
  notifier_.WriteOrBufferData(5, 512, NO_FIN);
  EXPECT_FALSE(notifier_.WillingToWrite());
  // Connection is blocked.
  EXPECT_CALL(connection_, SendStreamData(5, 512, 512, FIN))
      .WillOnce(Return(QuicConsumedData(256, false)));
  notifier_.WriteOrBufferData(5, 512, FIN);
  EXPECT_TRUE(notifier_.WillingToWrite());
  EXPECT_EQ(1792u, notifier_.StreamBytesSent());
  EXPECT_EQ(256u, notifier_.StreamBytesToSend());
  EXPECT_TRUE(notifier_.HasBufferedStreamData());

  // New data cannot be sent as connection is blocked.
  EXPECT_CALL(connection_, SendStreamData(7, 1024, 0, FIN)).Times(0);
  notifier_.WriteOrBufferData(7, 1024, FIN);
  EXPECT_EQ(1792u, notifier_.StreamBytesSent());
}

TEST_F(SimpleSessionNotifierTest, WriteOrBufferRstStream) {
  InSequence s;
  EXPECT_CALL(connection_, SendStreamData(5, 1024, 0, FIN))
      .WillOnce(Return(QuicConsumedData(1024, true)));
  notifier_.WriteOrBufferData(5, 1024, FIN);

  // Reset stream 5 with no error.
  EXPECT_CALL(connection_, SendControlFrame(_))
      .WillRepeatedly(
          Invoke(this, &SimpleSessionNotifierTest::ControlFrameConsumed));
  notifier_.WriteOrBufferRstStream(5, QUIC_STREAM_NO_ERROR, 1024);
  // Verify stream 5 is waiting for acks.
  EXPECT_TRUE(notifier_.StreamIsWaitingForAcks(5));

  // Reset stream 5 with error.
  notifier_.WriteOrBufferRstStream(5, QUIC_ERROR_PROCESSING_STREAM, 1024);
  EXPECT_FALSE(notifier_.StreamIsWaitingForAcks(5));
}

TEST_F(SimpleSessionNotifierTest, NeuterUnencryptedData) {
  InSequence s;
  // Send crypto data [0, 1024) in ENCRYPTION_NONE.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_NONE);
  EXPECT_CALL(connection_, SendStreamData(1, 1024, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(1024, false)));
  notifier_.WriteOrBufferData(1, 1024, NO_FIN);
  // Send crypto data [1024, 2048) in ENCRYPTION_INITIAL.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  EXPECT_CALL(connection_, SendStreamData(1, 1024, 1024, NO_FIN))
      .WillOnce(Return(QuicConsumedData(1024, false)));
  notifier_.WriteOrBufferData(1, 1024, NO_FIN);
  // Ack [1024, 2048).
  QuicStreamFrame stream_frame(1, false, 1024, 1024);
  notifier_.OnFrameAcked(QuicFrame(&stream_frame), QuicTime::Delta::Zero());
  EXPECT_TRUE(notifier_.StreamIsWaitingForAcks(1));
  // Neuters unencrypted data.
  notifier_.NeuterUnencryptedData();
  EXPECT_FALSE(notifier_.StreamIsWaitingForAcks(1));
}

TEST_F(SimpleSessionNotifierTest, OnCanWrite) {
  InSequence s;
  // Send crypto data [0, 1024) in ENCRYPTION_NONE.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_NONE);
  EXPECT_CALL(connection_, SendStreamData(1, 1024, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(1024, false)));
  notifier_.WriteOrBufferData(1, 1024, NO_FIN);
  // Send crypto data [1024, 2048) in ENCRYPTION_INITIAL.
  connection_.SetDefaultEncryptionLevel(ENCRYPTION_INITIAL);
  EXPECT_CALL(connection_, SendStreamData(1, 1024, 1024, NO_FIN))
      .WillOnce(Return(QuicConsumedData(1024, false)));
  notifier_.WriteOrBufferData(1, 1024, NO_FIN);
  // Send stream 3 [0, 1024) and connection is blocked.
  EXPECT_CALL(connection_, SendStreamData(3, 1024, 0, FIN))
      .WillOnce(Return(QuicConsumedData(512, false)));
  notifier_.WriteOrBufferData(3, 1024, FIN);
  // Send stream 5 [0, 1024).
  EXPECT_CALL(connection_, SendStreamData(5, _, _, _)).Times(0);
  notifier_.WriteOrBufferData(5, 1024, NO_FIN);
  // Reset stream 5 with error.
  EXPECT_CALL(connection_, SendControlFrame(_)).Times(0);
  notifier_.WriteOrBufferRstStream(5, QUIC_ERROR_PROCESSING_STREAM, 1024);

  // Lost crypto data [500, 1500) and stream 3 [0, 512).
  QuicStreamFrame frame1(1, false, 500, 1000);
  QuicStreamFrame frame2(3, false, 0, 512);
  notifier_.OnFrameLost(QuicFrame(&frame1));
  notifier_.OnFrameLost(QuicFrame(&frame2));

  // Connection becomes writable.
  // Lost crypto data gets retransmitted as [500, 1024) and [1024, 1500), as
  // they are in different encryption levels.
  EXPECT_CALL(connection_, SendStreamData(1, 524, 500, NO_FIN))
      .WillOnce(Return(QuicConsumedData(524, false)));
  EXPECT_CALL(connection_, SendStreamData(1, 476, 1024, NO_FIN))
      .WillOnce(Return(QuicConsumedData(476, false)));
  // Lost stream 3 data gets retransmitted.
  EXPECT_CALL(connection_, SendStreamData(3, 512, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(512, false)));
  // Buffered control frames get sent.
  EXPECT_CALL(connection_, SendControlFrame(_))
      .WillOnce(Invoke(this, &SimpleSessionNotifierTest::ControlFrameConsumed));
  // Buffered stream 3 data [512, 1024) gets sent.
  EXPECT_CALL(connection_, SendStreamData(3, 512, 512, FIN))
      .WillOnce(Return(QuicConsumedData(512, true)));
  notifier_.OnCanWrite();
  EXPECT_FALSE(notifier_.WillingToWrite());
}

TEST_F(SimpleSessionNotifierTest, RetransmitFrames) {
  InSequence s;
  // Send stream 3 data [0, 10) and fin.
  EXPECT_CALL(connection_, SendStreamData(3, 10, 0, FIN))
      .WillOnce(Return(QuicConsumedData(10, true)));
  notifier_.WriteOrBufferData(3, 10, FIN);
  QuicStreamFrame frame1(3, true, 0, 10);
  // Send stream 5 [0, 10) and fin.
  EXPECT_CALL(connection_, SendStreamData(5, 10, 0, FIN))
      .WillOnce(Return(QuicConsumedData(10, true)));
  notifier_.WriteOrBufferData(5, 10, FIN);
  QuicStreamFrame frame2(5, true, 0, 10);
  // Reset stream 5 with no error.
  EXPECT_CALL(connection_, SendControlFrame(_))
      .WillOnce(Invoke(this, &SimpleSessionNotifierTest::ControlFrameConsumed));
  notifier_.WriteOrBufferRstStream(5, QUIC_STREAM_NO_ERROR, 10);

  // Ack stream 3 [3, 7), and stream 5 [8, 10).
  QuicStreamFrame ack_frame1(3, false, 3, 4);
  QuicStreamFrame ack_frame2(5, false, 8, 2);
  notifier_.OnFrameAcked(QuicFrame(&ack_frame1), QuicTime::Delta::Zero());
  notifier_.OnFrameAcked(QuicFrame(&ack_frame2), QuicTime::Delta::Zero());
  EXPECT_FALSE(notifier_.WillingToWrite());

  // Force to send.
  QuicRstStreamFrame rst_stream(1, 5, QUIC_STREAM_NO_ERROR, 10);
  QuicFrames frames;
  frames.push_back(QuicFrame(&frame2));
  frames.push_back(QuicFrame(&rst_stream));
  frames.push_back(QuicFrame(&frame1));
  // stream 5 data [0, 8), fin only are retransmitted.
  EXPECT_CALL(connection_, SendStreamData(5, 8, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(8, false)));
  EXPECT_CALL(connection_, SendStreamData(5, 0, 10, FIN))
      .WillOnce(Return(QuicConsumedData(0, true)));
  // rst_stream is retransmitted.
  EXPECT_CALL(connection_, SendControlFrame(_))
      .WillOnce(Invoke(this, &SimpleSessionNotifierTest::ControlFrameConsumed));
  // stream 3 data [0, 3) is retransmitted and connection is blocked.
  EXPECT_CALL(connection_, SendStreamData(3, 3, 0, NO_FIN))
      .WillOnce(Return(QuicConsumedData(2, false)));
  notifier_.RetransmitFrames(frames, RTO_RETRANSMISSION);
  EXPECT_FALSE(notifier_.WillingToWrite());
}

}  // namespace
}  // namespace test
}  // namespace quic
