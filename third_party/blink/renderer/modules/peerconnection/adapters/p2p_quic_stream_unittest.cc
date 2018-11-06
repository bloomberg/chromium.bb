// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quic/core/quic_data_writer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_stream_delegate.h"

namespace blink {

namespace {

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;

const uint32_t kWriteBufferSize = 1024;
const quic::QuicStreamId kStreamId = 5;
const std::string kSomeData = "howdy";

}  // namespace

// Unit tests for the P2PQuicStream, using a mock QuicSession, which allows
// us to isolate testing the behaviors of reading a writing.
class P2PQuicStreamTest : public testing::Test {
 public:
  P2PQuicStreamTest()
      : connection_(
            new quic::test::MockQuicConnection(&connection_helper_,
                                               &alarm_factory_,
                                               quic::Perspective::IS_CLIENT)),
        session_(connection_) {
    session_.Initialize();
    stream_ = new P2PQuicStreamImpl(kStreamId, &session_, kWriteBufferSize);
    stream_->SetDelegate(&delegate_);
    // The session takes the ownership of the stream.
    session_.ActivateStream(std::unique_ptr<P2PQuicStreamImpl>(stream_));
    // DCHECKS get hit when the clock is at 0.
    connection_helper_.AdvanceTime(quic::QuicTime::Delta::FromSeconds(1));
  }

  ~P2PQuicStreamTest() override {}

  quic::test::MockQuicConnectionHelper connection_helper_;
  quic::test::MockAlarmFactory alarm_factory_;
  // Owned by the |session_|.
  quic::test::MockQuicConnection* connection_;
  // The MockQuicSession allows us to see data that is being written and control
  // whether the data is being "sent across" or blocked.
  quic::test::MockQuicSession session_;
  MockP2PQuicStreamDelegate delegate_;
  // Owned by |session_|.
  P2PQuicStreamImpl* stream_;
};

TEST_F(P2PQuicStreamTest, StreamSendsFinAndCanNoLongerWrite) {
  EXPECT_CALL(session_, WritevData(stream_, kStreamId, _, _, _))
      .WillOnce(Invoke(quic::test::MockQuicSession::ConsumeData));

  stream_->WriteData({}, /*fin=*/true);

  EXPECT_TRUE(stream_->fin_sent());
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_FALSE(stream_->reading_stopped());
}

TEST_F(P2PQuicStreamTest, StreamResetSendsRst) {
  EXPECT_CALL(session_, SendRstStream(kStreamId, _, _));
  stream_->Reset();
  EXPECT_TRUE(stream_->rst_sent());
}

// Tests that when a stream receives a stream frame with the FIN bit set it
// will fire the appropriate callback and close the stream for reading.
TEST_F(P2PQuicStreamTest, StreamOnStreamFrameWithFin) {
  EXPECT_CALL(delegate_, OnRemoteFinish());

  quic::QuicStreamFrame fin_frame(kStreamId, /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_FALSE(stream_->write_side_closed());
}

// Tests that when a stream receives a stream frame with the FIN bit set after
// it has written the FIN bit, then the stream will close.
TEST_F(P2PQuicStreamTest, StreamClosedAfterSendingThenReceivingFin) {
  EXPECT_CALL(session_, WritevData(stream_, kStreamId, _, _, _))
      .WillOnce(Invoke(quic::test::MockQuicSession::ConsumeData));
  stream_->WriteData({}, /*fin=*/true);
  EXPECT_FALSE(stream_->IsClosedForTesting());

  quic::QuicStreamFrame fin_frame(stream_->id(), /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that when a stream writes a FIN bit after receiving a stream frame with
// the FIN bit then the stream will close.
TEST_F(P2PQuicStreamTest, StreamClosedAfterReceivingThenSendingFin) {
  quic::QuicStreamFrame fin_frame(stream_->id(), /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);
  EXPECT_FALSE(stream_->IsClosedForTesting());

  EXPECT_CALL(session_, WritevData(stream_, kStreamId, _, _, _))
      .WillOnce(Invoke(quic::test::MockQuicSession::ConsumeData));

  stream_->WriteData({}, /*fin=*/true);

  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that when a stream writes some data with the FIN bit set, and receives
// data with the FIN bit set it will become closed.
TEST_F(P2PQuicStreamTest, StreamClosedAfterWritingAndReceivingDataWithFin) {
  EXPECT_CALL(session_, WritevData(stream_, kStreamId,
                                   /*write_length=*/kSomeData.size(), _, _))
      .WillOnce(Invoke(quic::test::MockQuicSession::ConsumeData));
  stream_->WriteData(std::vector<uint8_t>(kSomeData.begin(), kSomeData.end()),
                     /*fin=*/true);
  EXPECT_FALSE(stream_->IsClosedForTesting());

  quic::QuicStreamFrame fin_frame_with_data(stream_->id(), /*fin=*/true, 0,
                                            kSomeData);
  stream_->OnStreamFrame(fin_frame_with_data);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that when a stream receives a RST_STREAM frame it will fire the
// appropriate callback and the stream will become closed.
TEST_F(P2PQuicStreamTest, StreamClosedAfterReceivingReset) {
  EXPECT_CALL(delegate_, OnRemoteReset());

  quic::QuicRstStreamFrame rst_frame(quic::kInvalidControlFrameId, kStreamId,
                                     quic::QUIC_STREAM_CANCELLED, 0);
  stream_->OnStreamReset(rst_frame);

  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that data written to the P2PQuicStream will appropriately get written
// to the underlying QUIC library.
TEST_F(P2PQuicStreamTest, StreamWritesData) {
  EXPECT_CALL(session_, WritevData(stream_, kStreamId,
                                   /*write_length=*/kSomeData.size(), _, _))
      .WillOnce(Invoke([](quic::QuicStream* stream, quic::QuicStreamId id,
                          size_t write_length, quic::QuicStreamOffset offset,
                          quic::StreamSendingState state) {
        // quic::QuicSession::WritevData does not pass the data. The data is
        // saved to the stream, so we must grab it before it's consumed, in
        // order to check that it's what was written.
        std::string data_consumed_by_quic(write_length, 'a');
        quic::QuicDataWriter writer(write_length, &data_consumed_by_quic[0],
                                    quic::NETWORK_BYTE_ORDER);
        stream->WriteStreamData(offset, write_length, &writer);

        EXPECT_EQ(kSomeData, data_consumed_by_quic);
        EXPECT_EQ(quic::StreamSendingState::NO_FIN, state);
        return quic::QuicConsumedData(
            write_length, state != quic::StreamSendingState::NO_FIN);
      }));
  EXPECT_CALL(delegate_, OnWriteDataConsumed(kSomeData.size()));

  stream_->WriteData(std::vector<uint8_t>(kSomeData.begin(), kSomeData.end()),
                     /*fin=*/false);
}

// Tests that data written to the P2PQuicStream will appropriately get written
// to the underlying QUIC library with the FIN bit set.
TEST_F(P2PQuicStreamTest, StreamWritesDataWithFin) {
  EXPECT_CALL(session_, WritevData(stream_, kStreamId,
                                   /*write_length=*/kSomeData.size(), _, _))
      .WillOnce(Invoke([](quic::QuicStream* stream, quic::QuicStreamId id,
                          size_t write_length, quic::QuicStreamOffset offset,
                          quic::StreamSendingState state) {
        // WritevData does not pass the data. The data is saved to the stream,
        // so we must grab it before it's consumed, in order to check that it's
        // what was written.
        std::string data_consumed_by_quic(write_length, 'a');
        quic::QuicDataWriter writer(write_length, &data_consumed_by_quic[0],
                                    quic::NETWORK_BYTE_ORDER);
        stream->WriteStreamData(offset, write_length, &writer);

        EXPECT_EQ(kSomeData, data_consumed_by_quic);
        EXPECT_EQ(quic::StreamSendingState::FIN, state);
        return quic::QuicConsumedData(
            write_length, state != quic::StreamSendingState::NO_FIN);
      }));
  EXPECT_CALL(delegate_, OnWriteDataConsumed(kSomeData.size()));

  stream_->WriteData(std::vector<uint8_t>(kSomeData.begin(), kSomeData.end()),
                     /*fin=*/true);
}

// Tests that when written data is not consumed by QUIC (due to buffering),
// the OnWriteDataConsumed will not get fired.
TEST_F(P2PQuicStreamTest, StreamWritesDataAndNotConsumedByQuic) {
  EXPECT_CALL(delegate_, OnWriteDataConsumed(_)).Times(0);
  EXPECT_CALL(session_, WritevData(stream_, kStreamId,
                                   /*write_length=*/kSomeData.size(), _, _))
      .WillOnce(Invoke([](quic::QuicStream* stream, quic::QuicStreamId id,
                          size_t write_length, quic::QuicStreamOffset offset,
                          quic::StreamSendingState state) {
        // We mock that the QUIC library is not consuming the data, meaning it's
        // being buffered. In this case, the OnWriteDataConsumed() callback
        // should not be called.
        return quic::QuicConsumedData(/*bytes_consumed=*/0,
                                      quic::StreamSendingState::NO_FIN);
      }));

  stream_->WriteData(std::vector<uint8_t>(kSomeData.begin(), kSomeData.end()),
                     /*fin=*/true);
}

// Tests that OnWriteDataConsumed() is fired with the amount consumed by QUIC.
// This tests the case when amount consumed by QUIC is less than what is written
// with P2PQuicStream::WriteData. This can happen when QUIC is receiving back
// pressure from the receive side, and its "send window" is smaller than the
// amount attempted to be written.
TEST_F(P2PQuicStreamTest, StreamWritesDataAndPartiallyConsumedByQuic) {
  size_t amount_consumed_by_quic = 2;
  EXPECT_CALL(delegate_, OnWriteDataConsumed(amount_consumed_by_quic));
  EXPECT_CALL(session_, WritevData(stream_, kStreamId,
                                   /*write_length=*/kSomeData.size(), _, _))
      .WillOnce(Invoke([&amount_consumed_by_quic](
                           quic::QuicStream* stream, quic::QuicStreamId id,
                           size_t write_length, quic::QuicStreamOffset offset,
                           quic::StreamSendingState state) {
        // We mock that the QUIC library is only consuming some of the data,
        // meaning the rest is being buffered.
        return quic::QuicConsumedData(
            /*bytes_consumed=*/amount_consumed_by_quic,
            quic::StreamSendingState::NO_FIN);
      }));

  stream_->WriteData(std::vector<uint8_t>(kSomeData.begin(), kSomeData.end()),
                     /*fin=*/true);
}

}  // namespace blink
