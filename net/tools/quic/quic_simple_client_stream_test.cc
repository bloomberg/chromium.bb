// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_client_stream.h"

#include "base/strings/string_number_conversions.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/tools/quic/quic_simple_client_session.h"
#include "net/tools/quic/spdy_utils.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::test::DefaultQuicConfig;
using net::test::MockConnection;
using net::test::SupportedVersions;
using std::string;
using testing::StrictMock;
using testing::TestWithParam;

namespace net {
namespace tools {
namespace test {
namespace {

class QuicSimpleClientStreamTest : public TestWithParam<QuicVersion> {
 public:
  QuicSimpleClientStreamTest()
      : connection_(
            new StrictMock<MockConnection>(Perspective::IS_CLIENT,
                                           SupportedVersions(GetParam()))),
        session_(DefaultQuicConfig(), connection_),
        headers_(new HttpResponseHeaders("")),
        body_("hello world") {
    session_.InitializeSession(
        QuicServerId("example.com", 80, false, PRIVACY_MODE_DISABLED),
        &crypto_config_);

    headers_->ReplaceStatusLine("HTTP/1.1 200 Ok");
    headers_->AddHeader("content-length: 11");

    SpdyHeaderBlock header_block;
    CreateSpdyHeadersFromHttpResponse(*headers_, SPDY3, &header_block);
    headers_string_ = SpdyUtils::SerializeUncompressedHeaders(header_block);

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    session_.config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_.config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    stream_.reset(new QuicSimpleClientStream(3, &session_));
  }

  StrictMock<MockConnection>* connection_;
  QuicSimpleClientSession session_;
  scoped_ptr<QuicSimpleClientStream> stream_;
  scoped_refptr<HttpResponseHeaders> headers_;
  string headers_string_;
  string body_;
  QuicCryptoClientConfig crypto_config_;
};

INSTANTIATE_TEST_CASE_P(Tests, QuicSimpleClientStreamTest,
                        ::testing::ValuesIn(QuicSupportedVersions()));

TEST_P(QuicSimpleClientStreamTest, TestFraming) {
  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  EXPECT_EQ(body_.size(),
            stream_->ProcessData(body_.c_str(), body_.size()));
  EXPECT_EQ(200, stream_->headers()->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_P(QuicSimpleClientStreamTest, TestFramingOnePacket) {
  string message = headers_string_ + body_;

  EXPECT_EQ(message.size(), stream_->ProcessData(
      message.c_str(), message.size()));
  EXPECT_EQ(200, stream_->headers()->response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_P(QuicSimpleClientStreamTest, DISABLED_TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  // The headers should parse successfully.
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(200, stream_->headers()->response_code());

  EXPECT_CALL(*connection_,
              SendRstStream(stream_->id(), QUIC_BAD_APPLICATION_PAYLOAD, 0));
  stream_->ProcessData(large_body.c_str(), large_body.size());

  EXPECT_NE(QUIC_STREAM_NO_ERROR, stream_->stream_error());
}

TEST_P(QuicSimpleClientStreamTest, TestNoBidirectionalStreaming) {
  QuicStreamFrame frame(3, false, 3, MakeIOVector("asd"));

  EXPECT_FALSE(stream_->write_side_closed());
  stream_->OnStreamFrame(frame);
  EXPECT_TRUE(stream_->write_side_closed());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
