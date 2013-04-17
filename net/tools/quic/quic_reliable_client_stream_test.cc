// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_reliable_client_stream.h"

#include "base/strings/string_number_conversions.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/flip_server/epoll_server.h"
#include "net/tools/quic/quic_client_session.h"
#include "net/tools/quic/quic_spdy_client_stream.h"
#include "net/tools/quic/spdy_utils.h"
#include "net/tools/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::TestWithParam;

namespace net {
namespace tools {
namespace test {
namespace {

class QuicClientStreamTest : public ::testing::Test {
 public:
  QuicClientStreamTest()
      : session_("localhost", config_,
                 new MockConnection(1, IPEndPoint(), 0, &eps_, false),
                 &crypto_config_),
        body_("hello world") {
    config_.SetDefaults();
    crypto_config_.SetDefaults();

    headers_.SetResponseFirstlineFromStringPieces("HTTP/1.1", "200", "Ok");
    headers_.ReplaceOrAppendHeader("content-length", "11");

    headers_string_ = SpdyUtils::SerializeResponseHeaders(headers_);
    stream_.reset(new QuicSpdyClientStream(3, &session_));
  }

  EpollServer eps_;
  QuicClientSession session_;
  scoped_ptr<QuicReliableClientStream> stream_;
  BalsaHeaders headers_;
  string headers_string_;
  string body_;
  QuicConfig config_;
  QuicCryptoClientConfig crypto_config_;
};

TEST_F(QuicClientStreamTest, TestFraming) {
  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  EXPECT_EQ(body_.size(),
            stream_->ProcessData(body_.c_str(), body_.size()));
  EXPECT_EQ(200u, stream_->headers().parsed_response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_F(QuicClientStreamTest, TestFramingOnePacket) {
  string message = headers_string_ + body_;

  EXPECT_EQ(message.size(), stream_->ProcessData(
      message.c_str(), message.size()));
  EXPECT_EQ(200u, stream_->headers().parsed_response_code());
  EXPECT_EQ(body_, stream_->data());
}

TEST_F(QuicClientStreamTest, TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  EXPECT_EQ(headers_string_.size(), stream_->ProcessData(
      headers_string_.c_str(), headers_string_.size()));
  // The headers should parse successfully.
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(200u, stream_->headers().parsed_response_code());

  stream_->ProcessData(large_body.c_str(), large_body.size());
  stream_->TerminateFromPeer(true);

  EXPECT_NE(QUIC_STREAM_NO_ERROR, stream_->stream_error());
}

TEST_F(QuicClientStreamTest, TestNoBidirectionalStreaming) {
  QuicStreamFrame frame(3, false, 3, "asd");

  EXPECT_FALSE(stream_->write_side_closed());
  EXPECT_TRUE(stream_->OnStreamFrame(frame));
  EXPECT_TRUE(stream_->write_side_closed());
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net

