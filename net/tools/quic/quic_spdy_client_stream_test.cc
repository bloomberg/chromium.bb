// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_client_stream.h"

#include <memory>

#include "base/macros.h"
#include "net/quic/core/quic_utils.h"
#include "net/quic/core/spdy_utils.h"
#include "net/quic/platform/api/quic_logging.h"
#include "net/quic/platform/api/quic_socket_address.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/platform/api/quic_text_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_spdy_session_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_spdy_client_session.h"

using base::IntToString;
using std::string;
using testing::StrictMock;
using testing::TestWithParam;

namespace net {
namespace test {

namespace {

class MockQuicSpdyClientSession : public QuicSpdyClientSession {
 public:
  explicit MockQuicSpdyClientSession(
      QuicConnection* connection,
      QuicClientPushPromiseIndex* push_promise_index)
      : QuicSpdyClientSession(
            DefaultQuicConfig(),
            connection,
            QuicServerId("example.com", 443, PRIVACY_MODE_DISABLED),
            &crypto_config_,
            push_promise_index),
        crypto_config_(crypto_test_utils::ProofVerifierForTesting()) {}
  ~MockQuicSpdyClientSession() override {}

  MOCK_METHOD1(CloseStream, void(QuicStreamId stream_id));

 private:
  QuicCryptoClientConfig crypto_config_;

  DISALLOW_COPY_AND_ASSIGN(MockQuicSpdyClientSession);
};

class QuicSpdyClientStreamTest : public QuicTest {
 public:
  class StreamVisitor;

  QuicSpdyClientStreamTest()
      : connection_(new StrictMock<MockQuicConnection>(&helper_,
                                                       &alarm_factory_,
                                                       Perspective::IS_CLIENT)),
        session_(connection_, &push_promise_index_),
        body_("hello world") {
    session_.Initialize();

    headers_[":status"] = "200";
    headers_["content-length"] = "11";

    stream_.reset(new QuicSpdyClientStream(
        QuicSpdySessionPeer::GetNthClientInitiatedStreamId(session_, 0),
        &session_));
    stream_visitor_.reset(new StreamVisitor());
    stream_->set_visitor(stream_visitor_.get());
  }

  class StreamVisitor : public QuicSpdyClientStream::Visitor {
    void OnClose(QuicSpdyStream* stream) override {
      QUIC_DVLOG(1) << "stream " << stream->id();
    }
  };

  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  StrictMock<MockQuicConnection>* connection_;
  QuicClientPushPromiseIndex push_promise_index_;

  MockQuicSpdyClientSession session_;
  std::unique_ptr<QuicSpdyClientStream> stream_;
  std::unique_ptr<StreamVisitor> stream_visitor_;
  SpdyHeaderBlock headers_;
  string body_;
};

TEST_F(QuicSpdyClientStreamTest, TestReceivingIllegalResponseStatusCode) {
  headers_[":status"] = "200 ok";

  EXPECT_CALL(*connection_,
              SendRstStream(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD, 0));
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  EXPECT_EQ(QUIC_BAD_APPLICATION_PAYLOAD, stream_->stream_error());
}

TEST_F(QuicSpdyClientStreamTest, TestFraming) {
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_F(QuicSpdyClientStreamTest, TestFraming100Continue) {
  headers_[":status"] = "100";
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("100", stream_->preliminary_headers().find(":status")->second);
  EXPECT_EQ(0u, stream_->response_headers().size());
  EXPECT_EQ(100, stream_->response_code());
  EXPECT_EQ("", stream_->data());
}

TEST_F(QuicSpdyClientStreamTest, TestFramingOnePacket) {
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_F(QuicSpdyClientStreamTest, DISABLED_TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  // The headers should parse successfully.
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ("200", stream_->response_headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());

  EXPECT_CALL(*connection_,
              SendRstStream(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD, 0));
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, large_body));

  EXPECT_NE(QUIC_STREAM_NO_ERROR, stream_->stream_error());
}

TEST_F(QuicSpdyClientStreamTest, ReceivingTrailers) {
  // Test that receiving trailing headers, containing a final offset, results in
  // the stream being closed at that byte offset.
  // Send headers as usual.
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);

  // Send trailers before sending the body. Even though a FIN has been received
  // the stream should not be closed, as it does not yet have all the data bytes
  // promised by the final offset field.
  SpdyHeaderBlock trailer_block;
  trailer_block["trailer key"] = "trailer value";
  trailer_block[kFinalOffsetHeaderKey] =
      QuicTextUtils::Uint64ToString(body_.size());
  auto trailers = AsHeaderList(trailer_block);
  stream_->OnStreamHeaderList(true, trailers.uncompressed_header_bytes(),
                              trailers);

  // Now send the body, which should close the stream as the FIN has been
  // received, as well as all data.
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_TRUE(stream_->reading_stopped());
}

}  // namespace
}  // namespace test
}  // namespace net
