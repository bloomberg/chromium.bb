// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_spdy_server_stream.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/spdy_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/tools/epoll_server/epoll_server.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/spdy_balsa_utils.h"
#include "net/tools/quic/test_tools/quic_in_memory_cache_peer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using net::test::MockConnection;
using net::test::MockHelper;
using net::test::MockQuicSpdySession;
using net::test::SupportedVersions;
using net::test::kInitialSessionFlowControlWindowForTest;
using net::test::kInitialStreamFlowControlWindowForTest;
using std::string;
using testing::_;
using testing::AnyNumber;
using testing::Invoke;
using testing::InvokeArgument;
using testing::InSequence;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;

namespace net {
namespace tools {
namespace test {

class QuicSpdyServerStreamPeer : public QuicSpdyServerStream {
 public:
  QuicSpdyServerStreamPeer(QuicStreamId stream_id, QuicSpdySession* session)
      : QuicSpdyServerStream(stream_id, session) {}

  using QuicSpdyServerStream::SendResponse;
  using QuicSpdyServerStream::SendErrorResponse;

  SpdyHeaderBlock* mutable_headers() {
    return &request_headers_;
  }

  static void SendResponse(QuicSpdyServerStream* stream) {
    stream->SendResponse();
  }

  static void SendErrorResponse(QuicSpdyServerStream* stream) {
    stream->SendErrorResponse();
  }

  static const string& body(QuicSpdyServerStream* stream) {
    return stream->body_;
  }

  static const int content_length(QuicSpdyServerStream* stream) {
    return stream->content_length_;
  }

  static const SpdyHeaderBlock& headers(QuicSpdyServerStream* stream) {
    return stream->request_headers_;
  }
};

namespace {

class QuicSpdyServerStreamTest : public ::testing::Test {
 public:
  QuicSpdyServerStreamTest()
      : connection_(
            new StrictMock<MockConnection>(&helper_, Perspective::IS_SERVER)),
        session_(connection_),
        body_("hello world") {
    SpdyHeaderBlock request_headers;
    request_headers[":host"] = "";
    request_headers[":authority"] = "";
    request_headers[":path"] = "/";
    request_headers[":method"] = "POST";
    request_headers[":version"] = "HTTP/1.1";
    request_headers["content-length"] = "11";

    headers_string_ =
        net::SpdyUtils::SerializeUncompressedHeaders(request_headers);

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    session_.config()->SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindowForTest);
    session_.config()->SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindowForTest);
    stream_.reset(new QuicSpdyServerStreamPeer(3, &session_));

    QuicInMemoryCachePeer::ResetForTests();

    string host = "";
    string path = "/foo";
    SpdyHeaderBlock response_headers;
    StringPiece body("Yum");
    QuicInMemoryCache::GetInstance()->AddResponse(host, path, response_headers,
                                                  body);
  }

  ~QuicSpdyServerStreamTest() override {
    QuicInMemoryCachePeer::ResetForTests();
  }

  const string& StreamBody() {
    return QuicSpdyServerStreamPeer::body(stream_.get());
  }

  StringPiece StreamHeadersValue(const string& key) {
    return (*stream_->mutable_headers())[key];
  }

  SpdyHeaderBlock response_headers_;
  MockHelper helper_;
  StrictMock<MockConnection>* connection_;
  StrictMock<MockQuicSpdySession> session_;
  scoped_ptr<QuicSpdyServerStreamPeer> stream_;
  string headers_string_;
  string body_;
};

QuicConsumedData ConsumeAllData(
    QuicStreamId /*id*/,
    const QuicIOVector& data,
    QuicStreamOffset /*offset*/,
    bool fin,
    FecProtection /*fec_protection_*/,
    QuicAckListenerInterface* /*ack_notifier_delegate*/) {
  return QuicConsumedData(data.total_length, fin);
}

TEST_F(QuicSpdyServerStreamTest, TestFraming) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));
  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
  EXPECT_EQ(body_, StreamBody());
}

TEST_F(QuicSpdyServerStreamTest, TestFramingOnePacket) {
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));

  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
  EXPECT_EQ(body_, StreamBody());
}

TEST_F(QuicSpdyServerStreamTest, TestFramingExtraData) {
  string large_body = "hello world!!!!!!";

  // We'll automatically write out an error (headers + body)
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(AnyNumber()).
      WillRepeatedly(Invoke(ConsumeAllData));

  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, /*offset=*/0, body_));
  // Content length is still 11.  This will register as an error and we won't
  // accept the bytes.
  stream_->OnStreamFrame(
      QuicStreamFrame(stream_->id(), /*fin=*/false, body_.size(), large_body));
  EXPECT_EQ("11", StreamHeadersValue("content-length"));
  EXPECT_EQ("/", StreamHeadersValue(":path"));
  EXPECT_EQ("POST", StreamHeadersValue(":method"));
}

TEST_F(QuicSpdyServerStreamTest, TestSendResponse) {
  SpdyHeaderBlock* request_headers = stream_->mutable_headers();
  (*request_headers)[":path"] = "/foo";
  (*request_headers)[":authority"] = "";
  (*request_headers)[":version"] = "HTTP/1.1";
  (*request_headers)[":method"] = "GET";

  response_headers_[":version"] = "HTTP/1.1";
  response_headers_[":status"] = "200 OK";
  response_headers_["content-length"] = "3";

  InSequence s;
  EXPECT_CALL(session_, WritevData(kHeadersStreamId, _, 0, false, _, nullptr));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(1).
      WillOnce(Return(QuicConsumedData(3, true)));

  QuicSpdyServerStreamPeer::SendResponse(stream_.get());
  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicSpdyServerStreamTest, TestSendErrorResponse) {
  response_headers_[":version"] = "HTTP/1.1";
  response_headers_[":status"] = "500 Server Error";
  response_headers_["content-length"] = "3";

  InSequence s;
  EXPECT_CALL(session_, WritevData(kHeadersStreamId, _, 0, false, _, nullptr));
  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _)).Times(1).
      WillOnce(Return(QuicConsumedData(3, true)));

  QuicSpdyServerStreamPeer::SendErrorResponse(stream_.get());
  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicSpdyServerStreamTest, InvalidMultipleContentLength) {
  SpdyHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  request_headers["content-length"] = StringPiece("11\00012", 5);

  headers_string_ = SpdyUtils::SerializeUncompressedHeaders(request_headers);

  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(ConsumeAllData));

  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());

  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicSpdyServerStreamTest, InvalidLeadingNullContentLength) {
  SpdyHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  request_headers["content-length"] = StringPiece("\00012", 3);

  headers_string_ = SpdyUtils::SerializeUncompressedHeaders(request_headers);

  EXPECT_CALL(session_, WritevData(_, _, _, _, _, _))
      .Times(AnyNumber())
      .WillRepeatedly(Invoke(ConsumeAllData));

  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());

  EXPECT_TRUE(stream_->read_side_closed());
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_F(QuicSpdyServerStreamTest, ValidMultipleContentLength) {
  SpdyHeaderBlock request_headers;
  // \000 is a way to write the null byte when followed by a literal digit.
  request_headers["content-length"] = StringPiece("11\00011", 5);

  headers_string_ = SpdyUtils::SerializeUncompressedHeaders(request_headers);

  stream_->OnStreamHeaders(headers_string_);
  stream_->OnStreamHeadersComplete(false, headers_string_.size());

  EXPECT_EQ(11, QuicSpdyServerStreamPeer::content_length(stream_.get()));
  EXPECT_FALSE(stream_->read_side_closed());
  EXPECT_FALSE(stream_->write_side_closed());
}

TEST_F(QuicSpdyServerStreamTest, InvalidHeadersWithFin) {
  char arr[] = {
    0x3a, 0x68, 0x6f, 0x73,  // :hos
    0x74, 0x00, 0x00, 0x00,  // t...
    0x00, 0x00, 0x00, 0x00,  // ....
    0x07, 0x3a, 0x6d, 0x65,  // .:me
    0x74, 0x68, 0x6f, 0x64,  // thod
    0x00, 0x00, 0x00, 0x03,  // ....
    0x47, 0x45, 0x54, 0x00,  // GET.
    0x00, 0x00, 0x05, 0x3a,  // ...:
    0x70, 0x61, 0x74, 0x68,  // path
    0x00, 0x00, 0x00, 0x04,  // ....
    0x2f, 0x66, 0x6f, 0x6f,  // /foo
    0x00, 0x00, 0x00, 0x07,  // ....
    0x3a, 0x73, 0x63, 0x68,  // :sch
    0x65, 0x6d, 0x65, 0x00,  // eme.
    0x00, 0x00, 0x00, 0x00,  // ....
    0x00, 0x00, 0x08, 0x3a,  // ...:
    0x76, 0x65, 0x72, 0x73,  // vers
    '\x96', 0x6f, 0x6e, 0x00,  // <i(69)>on.
    0x00, 0x00, 0x08, 0x48,  // ...H
    0x54, 0x54, 0x50, 0x2f,  // TTP/
    0x31, 0x2e, 0x31,        // 1.1
  };
  StringPiece data(arr, arraysize(arr));
  QuicStreamFrame frame(stream_->id(), true, 0, data);
  // Verify that we don't crash when we get a invalid headers in stream frame.
  stream_->OnStreamFrame(frame);
}

}  // namespace
}  // namespace test
}  // namespace tools
}  // namespace net
