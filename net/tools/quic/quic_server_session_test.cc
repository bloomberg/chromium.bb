// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_server_session.h"

#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_connection.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_data_stream_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_spdy_server_stream.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using __gnu_cxx::vector;
using net::test::MockConnection;
using net::test::QuicConnectionPeer;
using net::test::QuicDataStreamPeer;
using testing::_;
using testing::StrictMock;

namespace net {
namespace tools {
namespace test {

class QuicServerSessionPeer {
 public:
  static QuicDataStream* GetIncomingReliableStream(
      QuicServerSession* s, QuicStreamId id) {
    return s->GetIncomingReliableStream(id);
  }
  static QuicDataStream* GetDataStream(QuicServerSession* s, QuicStreamId id) {
    return s->GetDataStream(id);
  }
};

class CloseOnDataStream : public QuicDataStream {
 public:
  CloseOnDataStream(QuicStreamId id, QuicSession* session)
      : QuicDataStream(id, session) {
  }

  virtual bool OnStreamFrame(const QuicStreamFrame& frame) OVERRIDE {
    session()->MarkDecompressionBlocked(1, id());
    session()->CloseStream(id());
    return true;
  }

  virtual uint32 ProcessData(const char* data, uint32 data_len) OVERRIDE {
    return 0;
  }
};

class TestQuicQuicServerSession : public QuicServerSession {
 public:
  TestQuicQuicServerSession(const QuicConfig& config,
                            QuicConnection* connection,
                            QuicSessionOwner* owner)
      : QuicServerSession(config, connection, owner),
        close_stream_on_data_(false) {
  }

  virtual QuicDataStream* CreateIncomingDataStream(
      QuicStreamId id) OVERRIDE {
    if (!ShouldCreateIncomingDataStream(id)) {
      return NULL;
    }
    if (close_stream_on_data_) {
      return new CloseOnDataStream(id, this);
    } else {
      return new QuicSpdyServerStream(id, this);
    }
  }

  void CloseStreamOnData() {
    close_stream_on_data_ = true;
  }

 private:
  bool close_stream_on_data_;
};

namespace {

class QuicServerSessionTest : public ::testing::Test {
 protected:
  QuicServerSessionTest()
      : crypto_config_(QuicCryptoServerConfig::TESTING,
                       QuicRandom::GetInstance()) {
    config_.SetDefaults();
    config_.set_max_streams_per_connection(3, 3);

    connection_ = new MockConnection(true);
    session_.reset(new TestQuicQuicServerSession(
        config_, connection_, &owner_));
    session_->InitializeSession(crypto_config_);
    visitor_ = QuicConnectionPeer::GetVisitor(connection_);
  }

  void MarkHeadersReadForStream(QuicStreamId id) {
    QuicDataStream* stream = QuicServerSessionPeer::GetDataStream(
        session_.get(), id);
    ASSERT_TRUE(stream != NULL);
    QuicDataStreamPeer::SetHeadersDecompressed(stream, true);
  }

  StrictMock<MockQuicSessionOwner> owner_;
  MockConnection* connection_;
  QuicConfig config_;
  QuicCryptoServerConfig crypto_config_;
  scoped_ptr<TestQuicQuicServerSession> session_;
  QuicConnectionVisitorInterface* visitor_;
};

TEST_F(QuicServerSessionTest, CloseStreamDueToReset) {
  // Open a stream, then reset it.
  // Send two bytes of payload to open it.
  QuicStreamFrame data1(3, false, 0, MakeIOVector("HT"));
  vector<QuicStreamFrame> frames;
  frames.push_back(data1);
  EXPECT_TRUE(visitor_->OnStreamFrames(frames));
  EXPECT_EQ(1u, session_->GetNumOpenStreams());

  // Pretend we got full headers, so we won't trigger the 'unrecoverable
  // compression context' state.
  MarkHeadersReadForStream(3);

  // Send a reset.
  QuicRstStreamFrame rst1(3, QUIC_STREAM_NO_ERROR);
  visitor_->OnRstStream(rst1);
  EXPECT_EQ(0u, session_->GetNumOpenStreams());

  // Send the same two bytes of payload in a new packet.
  EXPECT_TRUE(visitor_->OnStreamFrames(frames));

  // The stream should not be re-opened.
  EXPECT_EQ(0u, session_->GetNumOpenStreams());
}

TEST_F(QuicServerSessionTest, NeverOpenStreamDueToReset) {
  // Send a reset.
  QuicRstStreamFrame rst1(3, QUIC_STREAM_NO_ERROR);
  visitor_->OnRstStream(rst1);
  EXPECT_EQ(0u, session_->GetNumOpenStreams());

  // Send two bytes of payload.
  QuicStreamFrame data1(3, false, 0, MakeIOVector("HT"));
  vector<QuicStreamFrame> frames;
  frames.push_back(data1);

  // When we get data for the closed stream, it implies the far side has
  // compressed some headers.  As a result we're going to bail due to
  // unrecoverable compression context state.
  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_STREAM_RST_BEFORE_HEADERS_DECOMPRESSED));
  EXPECT_FALSE(visitor_->OnStreamFrames(frames));

  // The stream should never be opened, now that the reset is received.
  EXPECT_EQ(0u, session_->GetNumOpenStreams());
}

TEST_F(QuicServerSessionTest, GoOverPrematureClosedStreamLimit) {
  QuicStreamFrame data1(3, false, 0, MakeIOVector("H"));
  vector<QuicStreamFrame> frames;
  frames.push_back(data1);

  // Set up the stream such that it's open in OnPacket, but closes half way
  // through while on the decompression blocked list.
  session_->CloseStreamOnData();

  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_STREAM_RST_BEFORE_HEADERS_DECOMPRESSED));
  EXPECT_FALSE(visitor_->OnStreamFrames(frames));
}

TEST_F(QuicServerSessionTest, AcceptClosedStream) {
  vector<QuicStreamFrame> frames;
  // Send (empty) compressed headers followed by two bytes of data.
  frames.push_back(
      QuicStreamFrame(3, false, 0, MakeIOVector("\1\0\0\0\0\0\0\0HT")));
  frames.push_back(
      QuicStreamFrame(5, false, 0, MakeIOVector("\2\0\0\0\0\0\0\0HT")));
  EXPECT_TRUE(visitor_->OnStreamFrames(frames));

  // Pretend we got full headers, so we won't trigger the 'unercoverable
  // compression context' state.
  MarkHeadersReadForStream(3);

  // Send a reset.
  QuicRstStreamFrame rst(3, QUIC_STREAM_NO_ERROR);
  visitor_->OnRstStream(rst);

  // If we were tracking, we'd probably want to reject this because it's data
  // past the reset point of stream 3.  As it's a closed stream we just drop the
  // data on the floor, but accept the packet because it has data for stream 5.
  frames.clear();
  frames.push_back(QuicStreamFrame(3, false, 2, MakeIOVector("TP")));
  frames.push_back(QuicStreamFrame(5, false, 2, MakeIOVector("TP")));
  EXPECT_TRUE(visitor_->OnStreamFrames(frames));
}

TEST_F(QuicServerSessionTest, MaxNumConnections) {
  EXPECT_EQ(0u, session_->GetNumOpenStreams());
  EXPECT_TRUE(
      QuicServerSessionPeer::GetIncomingReliableStream(session_.get(), 3));
  EXPECT_TRUE(
      QuicServerSessionPeer::GetIncomingReliableStream(session_.get(), 5));
  EXPECT_TRUE(
      QuicServerSessionPeer::GetIncomingReliableStream(session_.get(), 7));
  EXPECT_FALSE(
      QuicServerSessionPeer::GetIncomingReliableStream(session_.get(), 9));
}

TEST_F(QuicServerSessionTest, MaxNumConnectionsImplicit) {
  EXPECT_EQ(0u, session_->GetNumOpenStreams());
  EXPECT_TRUE(
      QuicServerSessionPeer::GetIncomingReliableStream(session_.get(), 3));
  // Implicitly opens two more streams before 9.
  EXPECT_FALSE(
      QuicServerSessionPeer::GetIncomingReliableStream(session_.get(), 9));
}

TEST_F(QuicServerSessionTest, GetEvenIncomingError) {
  // Incoming streams on the server session must be odd.
  EXPECT_EQ(NULL,
            QuicServerSessionPeer::GetIncomingReliableStream(
                session_.get(), 2));
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
