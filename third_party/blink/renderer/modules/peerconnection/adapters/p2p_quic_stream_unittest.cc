// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream.h"
#include "net/test/gtest_util.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/p2p_quic_stream_impl.h"
#include "third_party/blink/renderer/modules/peerconnection/adapters/test/mock_p2p_quic_stream_delegate.h"

namespace blink {

namespace {

using testing::_;
using testing::Invoke;

const quic::QuicStreamId kStreamId = 5;

}  // namespace

// Unit tests for the P2PQuicStream, using a mock QuicSession, which allows
// us to isolate testing the behaviors of reading and writing.
class P2PQuicStreamTest : public testing::Test {
 public:
  P2PQuicStreamTest()
      : connection_(
            new quic::test::MockQuicConnection(&connection_helper_,
                                               &alarm_factory_,
                                               quic::Perspective::IS_CLIENT)),
        session_(connection_) {
    session_.Initialize();
    stream_ = new P2PQuicStreamImpl(kStreamId, &session_);
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

TEST_F(P2PQuicStreamTest, StreamFinishSendsFinAndCanNoLongerWrite) {
  EXPECT_CALL(session_, WritevData(stream_, kStreamId, _, _, _))
      .WillOnce(Invoke(quic::test::MockQuicSession::ConsumeData));

  stream_->Finish();

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
// it has called Finish(), then the stream will close.
TEST_F(P2PQuicStreamTest, StreamClosedAfterReceivesFin) {
  EXPECT_CALL(session_, WritevData(stream_, kStreamId, _, _, _))
      .WillOnce(Invoke(quic::test::MockQuicSession::ConsumeData));
  stream_->Finish();
  EXPECT_FALSE(stream_->IsClosedForTesting());

  quic::QuicStreamFrame fin_frame(stream_->id(), /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);

  EXPECT_TRUE(stream_->reading_stopped());
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_TRUE(stream_->IsClosedForTesting());
}

// Tests that when a stream calls Finish() after receiving a stream frame with
// the FIN bit then the stream will close.
TEST_F(P2PQuicStreamTest, StreamClosedAfterFinish) {
  quic::QuicStreamFrame fin_frame(stream_->id(), /*fin=*/true, 0, 0);
  stream_->OnStreamFrame(fin_frame);
  EXPECT_FALSE(stream_->IsClosedForTesting());

  EXPECT_CALL(session_, WritevData(stream_, kStreamId, _, _, _))
      .WillOnce(Invoke(quic::test::MockQuicSession::ConsumeData));
  stream_->Finish();

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

}  // namespace blink
