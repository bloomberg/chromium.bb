// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_client_stream.h"

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "net/quic/quic_utils.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/spdy_balsa_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::CryptoTestUtils;
using net::test::DefaultQuicConfig;
using net::test::MockConnection;
using net::test::MockConnectionHelper;
using net::test::SupportedVersions;
using net::test::kClientDataStreamId1;
using net::test::kServerDataStreamId1;
using net::test::kInitialSessionFlowControlWindowForTest;
using net::test::kInitialStreamFlowControlWindowForTest;
using net::test::ValueRestore;

using std::string;
using testing::StrictMock;
using testing::TestWithParam;

namespace net {
namespace tools {
namespace test {

class QuicClientPromisedInfoPeer {
 public:
  static QuicAlarm* GetAlarm(QuicClientPromisedInfo* promised_stream) {
    return promised_stream->cleanup_alarm_.get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicClientPromisedInfoPeer);
};

namespace {

class MockQuicClientSession : public QuicClientSession {
 public:
  explicit MockQuicClientSession(QuicConnection* connection,
                                 QuicPromisedByUrlMap* promised_by_url)
      : QuicClientSession(
            DefaultQuicConfig(),
            connection,
            QuicServerId("example.com", 80, PRIVACY_MODE_DISABLED),
            &crypto_config_,
            promised_by_url),
        crypto_config_(CryptoTestUtils::ProofVerifierForTesting()) {}
  ~MockQuicClientSession() override {}

  MOCK_METHOD1(CloseStream, void(QuicStreamId stream_id));

 private:
  QuicCryptoClientConfig crypto_config_;

  DISALLOW_COPY_AND_ASSIGN(MockQuicClientSession);
};

class QuicSpdyClientStreamTest : public ::testing::Test {
 public:
  QuicSpdyClientStreamTest()
      : connection_(
            new StrictMock<MockConnection>(&helper_, Perspective::IS_CLIENT)),
        session_(connection_, &promised_by_url_),
        body_("hello world") {
    session_.Initialize();

    headers_.SetResponseFirstlineFromStringPieces("HTTP/1.1", "200", "Ok");
    headers_.ReplaceOrAppendHeader("content-length", "11");

    headers_string_ =
        net::tools::SpdyBalsaUtils::SerializeResponseHeaders(headers_);

    stream_.reset(new QuicSpdyClientStream(kClientDataStreamId1, &session_));
  }

  class PromiseListener : public QuicClientPromisedInfo::Listener {
   public:
    PromiseListener() : have_promised_response_(false) {}

    void OnResponse() override { have_promised_response_ = true; }

    bool have_promised_response_;
  };

  MockConnectionHelper helper_;
  StrictMock<MockConnection>* connection_;
  QuicPromisedByUrlMap promised_by_url_;

  MockQuicClientSession session_;
  scoped_ptr<QuicSpdyClientStream> stream_;
  scoped_ptr<QuicSpdyClientStream> promised_stream_;
  BalsaHeaders headers_;
  string headers_string_;
  string body_;
  bool have_promised_response_;
};

TEST_F(QuicSpdyClientStreamTest, TestFraming) {
  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("200", stream_->headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_F(QuicSpdyClientStreamTest, TestFramingOnePacket) {
  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("200", stream_->headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_F(QuicSpdyClientStreamTest, DISABLED_TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());
  // The headers should parse successfully.
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ("200", stream_->headers().find(":status")->second);
  EXPECT_EQ(200, stream_->response_code());

  EXPECT_CALL(*connection_,
              SendRstStream(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD, 0));
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, large_body));

  EXPECT_NE(QUIC_STREAM_NO_ERROR, stream_->stream_error());
}

TEST_F(QuicSpdyClientStreamTest, TestNoBidirectionalStreaming) {
  QuicStreamFrame frame(kClientDataStreamId1, false, 3, StringPiece("asd"));

  EXPECT_FALSE(stream_->write_side_closed());
  stream_->OnStreamFrame(frame);
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicSpdyClientStreamTest, ReceivingTrailers) {
  // Test that receiving trailing headers, containing a final offset, results in
  // the stream being closed at that byte offset.
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_trailers, true);

  // Send headers as usual.
  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());

  // Send trailers before sending the body. Even though a FIN has been received
  // the stream should not be closed, as it does not yet have all the data bytes
  // promised by the final offset field.
  SpdyHeaderBlock trailers;
  trailers["trailer key"] = "trailer value";
  trailers[kFinalOffsetHeaderKey] = base::IntToString(body_.size());
  string trailers_string = SpdyUtils::SerializeUncompressedHeaders(trailers);
  stream_->OnStreamHeaders(trailers_string);
  stream_->OnStreamHeadersComplete(true, trailers_string.size());

  // Now send the body, which should close the stream as the FIN has been
  // received, as well as all data.
  EXPECT_CALL(session_, CloseStream(stream_->id()));
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
}

TEST_F(QuicSpdyClientStreamTest, ReceivingPromise) {
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_push_promise, true);

  // Receive promise
  SpdyHeaderBlock push_promise;
  push_promise[":path"] = "/bar";
  push_promise[":authority"] = "www.google.com";
  push_promise[":version"] = "HTTP/1.1";
  push_promise[":method"] = "GET";
  push_promise[":scheme"] = "https";

  string push_promise_string =
      SpdyUtils::SerializeUncompressedHeaders(push_promise);

  stream_->OnPromiseHeaders(push_promise_string);
  stream_->OnPromiseHeadersComplete(kServerDataStreamId1,
                                    push_promise_string.size());

  // Send headers as usual.
  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());

  // Verify that the promise is in the unclaimed streams map.
  string promise_url(SpdyUtils::GetUrlFromHeaderBlock(push_promise));
  EXPECT_NE(session_.GetPromisedByUrl(promise_url), nullptr);
}

TEST_F(QuicSpdyClientStreamTest, ReceivingPromiseCleanupAlarm) {
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_push_promise, true);

  // Receive promise
  SpdyHeaderBlock push_promise;
  push_promise[":path"] = "/bar";
  push_promise[":authority"] = "www.google.com";
  push_promise[":version"] = "HTTP/1.1";
  push_promise[":method"] = "GET";
  push_promise[":scheme"] = "https";

  string push_promise_string =
      SpdyUtils::SerializeUncompressedHeaders(push_promise);

  stream_->OnStreamHeaders(push_promise_string);
  stream_->OnPromiseHeadersComplete(kServerDataStreamId1,
                                    push_promise_string.size());

  // Send headers as usual.
  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());

  // Verify that the promise is in the unclaimed streams map.
  string promise_url(SpdyUtils::GetUrlFromHeaderBlock(push_promise));
  QuicPromisedByUrlMap::iterator it = promised_by_url_.find(promise_url);
  ASSERT_NE(it, promised_by_url_.end());

  // Verify that the promise is gone after the alarm fires.
  helper_.FireAlarm(QuicClientPromisedInfoPeer::GetAlarm(it->second));
  EXPECT_EQ(session_.GetPromisedById(kServerDataStreamId1), nullptr);
  EXPECT_EQ(session_.GetPromisedByUrl(promise_url), nullptr);
}

TEST_F(QuicSpdyClientStreamTest, ReceivingPromiseInvalidUrl) {
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_push_promise, true);

  // Receive promise - mising authority
  SpdyHeaderBlock push_promise;
  push_promise[":path"] = "/bar";
  push_promise[":version"] = "HTTP/1.1";
  push_promise[":method"] = "GET";
  push_promise[":scheme"] = "https";

  string push_promise_string =
      SpdyUtils::SerializeUncompressedHeaders(push_promise);
  stream_->OnStreamHeaders(push_promise_string);

  EXPECT_CALL(*connection_,
              SendRstStream(kServerDataStreamId1, QUIC_INVALID_PROMISE_URL, 0));

  stream_->OnPromiseHeadersComplete(kServerDataStreamId1,
                                    push_promise_string.size());

  // Verify that the promise was not created.
  string promise_url(SpdyUtils::GetUrlFromHeaderBlock(push_promise));
  EXPECT_EQ(session_.GetPromisedById(kServerDataStreamId1), nullptr);
  EXPECT_EQ(session_.GetPromisedByUrl(promise_url), nullptr);
}

TEST_F(QuicSpdyClientStreamTest, ReceivingPromiseUnauthorizedUrl) {
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_push_promise, true);

  // Receive promise - mismatched authority
  SpdyHeaderBlock push_promise;
  push_promise[":path"] = "/bar";
  push_promise[":authority"] = "mail.google.com";
  push_promise[":version"] = "HTTP/1.1";
  push_promise[":method"] = "GET";
  push_promise[":scheme"] = "https";

  string push_promise_string =
      SpdyUtils::SerializeUncompressedHeaders(push_promise);
  stream_->OnStreamHeaders(push_promise_string);

  EXPECT_CALL(*connection_, SendRstStream(kServerDataStreamId1,
                                          QUIC_UNAUTHORIZED_PROMISE_URL, 0));

  stream_->OnPromiseHeadersComplete(kServerDataStreamId1,
                                    push_promise_string.size());

  // Send response headers usual.
  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());

  // Ok now get the promise information.
  string promise_url(SpdyUtils::GetUrlFromHeaderBlock(push_promise));

  QuicClientPromisedInfo* promise = session_.GetPromisedByUrl(promise_url);
  ASSERT_NE(promise, nullptr);

  EXPECT_EQ("https://mail.google.com/bar",
            SpdyUtils::GetUrlFromHeaderBlock(*promise->request_headers()));

  promise->RejectUnauthorized();

  // Verify that the promise has been destroyed.
  EXPECT_EQ(session_.GetPromisedById(kServerDataStreamId1), nullptr);
  EXPECT_EQ(session_.GetPromisedByUrl(promise_url), nullptr);
}

TEST_F(QuicSpdyClientStreamTest, ReceivingPromiseVaryWaits) {
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_push_promise, true);

  // Receive promise
  SpdyHeaderBlock push_promise;
  push_promise[":path"] = "/bar";
  push_promise[":authority"] = "mail.google.com";
  push_promise[":version"] = "HTTP/1.1";
  push_promise[":method"] = "GET";
  push_promise[":scheme"] = "https";

  string push_promise_string =
      SpdyUtils::SerializeUncompressedHeaders(push_promise);
  stream_->OnStreamHeaders(push_promise_string);

  stream_->OnPromiseHeadersComplete(kServerDataStreamId1,
                                    push_promise_string.size());

  // Ok now get the promise information.
  string promise_url(SpdyUtils::GetUrlFromHeaderBlock(push_promise));
  QuicPromisedByUrlMap::iterator it = promised_by_url_.find(promise_url);
  ASSERT_NE(it, promised_by_url_.end());
  QuicClientPromisedInfo* promise = it->second;

  EXPECT_NE(promise->request_headers(), nullptr);
  EXPECT_EQ("https://mail.google.com/bar",
            SpdyUtils::GetUrlFromHeaderBlock(*promise->request_headers()));

  EXPECT_EQ(promise->response_headers(), nullptr);

  PromiseListener listener;
  EXPECT_FALSE(listener.have_promised_response_);
  promise->SetListener(&listener);
  QuicSpdyClientStream* promise_stream = static_cast<QuicSpdyClientStream*>(
      session_.GetStream(kServerDataStreamId1));
  promise_stream->OnStreamHeaders(headers_string_);
  promise_stream->OnStreamHeadersComplete(false, headers_string_.size());

  EXPECT_TRUE(listener.have_promised_response_);
  EXPECT_NE(promise->response_headers(), nullptr);
}

TEST_F(QuicSpdyClientStreamTest, ReceivingPromiseVaryNoWait) {
  ValueRestore<bool> old_flag(&FLAGS_quic_supports_push_promise, true);

  // Receive promise
  SpdyHeaderBlock push_promise;
  push_promise[":path"] = "/bar";
  push_promise[":authority"] = "mail.google.com";
  push_promise[":version"] = "HTTP/1.1";
  push_promise[":method"] = "GET";
  push_promise[":scheme"] = "https";

  string push_promise_string =
      SpdyUtils::SerializeUncompressedHeaders(push_promise);
  stream_->OnStreamHeaders(push_promise_string);

  stream_->OnPromiseHeadersComplete(kServerDataStreamId1,
                                    push_promise_string.size());

  QuicSpdyClientStream* promise_stream = static_cast<QuicSpdyClientStream*>(
      session_.GetStream(kServerDataStreamId1));

  promise_stream->OnStreamHeaders(headers_string_);
  promise_stream->OnStreamHeadersComplete(false, headers_string_.size());

  // Ok now get the promise information.
  string promise_url(SpdyUtils::GetUrlFromHeaderBlock(push_promise));
  QuicPromisedByUrlMap::iterator it = promised_by_url_.find(promise_url);
  ASSERT_NE(it, promised_by_url_.end());
  QuicClientPromisedInfo* promise = it->second;

  EXPECT_NE(promise->request_headers(), nullptr);
  EXPECT_EQ("https://mail.google.com/bar",
            SpdyUtils::GetUrlFromHeaderBlock(*promise->request_headers()));

  EXPECT_NE(promise->response_headers(), nullptr);
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
