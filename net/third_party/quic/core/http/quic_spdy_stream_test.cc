// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/http/quic_spdy_stream.h"

#include <memory>
#include <utility>

#include "net/third_party/quic/core/http/http_encoder.h"
#include "net/third_party/quic/core/http/spdy_utils.h"
#include "net/third_party/quic/core/quic_connection.h"
#include "net/third_party/quic/core/quic_stream_sequencer_buffer.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/core/quic_versions.h"
#include "net/third_party/quic/core/quic_write_blocked_list.h"
#include "net/third_party/quic/platform/api/quic_arraysize.h"
#include "net/third_party/quic/platform/api/quic_expect_bug.h"
#include "net/third_party/quic/platform/api/quic_map_util.h"
#include "net/third_party/quic/platform/api/quic_ptr_util.h"
#include "net/third_party/quic/platform/api/quic_string.h"
#include "net/third_party/quic/platform/api/quic_string_piece.h"
#include "net/third_party/quic/platform/api/quic_test.h"
#include "net/third_party/quic/platform/api/quic_text_utils.h"
#include "net/third_party/quic/test_tools/quic_flow_controller_peer.h"
#include "net/third_party/quic/test_tools/quic_session_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_session_peer.h"
#include "net/third_party/quic/test_tools/quic_spdy_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_stream_peer.h"
#include "net/third_party/quic/test_tools/quic_test_utils.h"

using spdy::kV3HighestPriority;
using spdy::kV3LowestPriority;
using spdy::SpdyHeaderBlock;
using spdy::SpdyPriority;
using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace quic {
namespace test {
namespace {

const bool kShouldProcessData = true;

class TestStream : public QuicSpdyStream {
 public:
  TestStream(QuicStreamId id,
             QuicSpdySession* session,
             bool should_process_data)
      : QuicSpdyStream(id, session, BIDIRECTIONAL),
        should_process_data_(should_process_data) {}
  ~TestStream() override = default;

  using QuicSpdyStream::set_ack_listener;
  using QuicStream::CloseWriteSide;
  using QuicStream::WriteOrBufferData;

  void OnBodyAvailable() override {
    if (!should_process_data_) {
      return;
    }
    char buffer[2048];
    struct iovec vec;
    vec.iov_base = buffer;
    vec.iov_len = QUIC_ARRAYSIZE(buffer);
    size_t bytes_read = Readv(&vec, 1);
    data_ += QuicString(buffer, bytes_read);
  }

  MOCK_METHOD1(WriteHeadersMock, void(bool fin));

  size_t WriteHeadersImpl(spdy::SpdyHeaderBlock header_block,
                          bool fin,
                          QuicReferenceCountedPointer<QuicAckListenerInterface>
                              ack_listener) override {
    saved_headers_ = std::move(header_block);
    WriteHeadersMock(fin);
    return 0;
  }

  const QuicString& data() const { return data_; }
  const spdy::SpdyHeaderBlock& saved_headers() const { return saved_headers_; }

 private:
  bool should_process_data_;
  spdy::SpdyHeaderBlock saved_headers_;
  QuicString data_;
};

class TestMockUpdateStreamSession : public MockQuicSpdySession {
 public:
  explicit TestMockUpdateStreamSession(QuicConnection* connection)
      : MockQuicSpdySession(connection) {}

  void UpdateStreamPriority(QuicStreamId id, SpdyPriority priority) override {
    EXPECT_EQ(id, expected_stream_->id());
    EXPECT_EQ(expected_priority_, priority);
    EXPECT_EQ(expected_priority_, expected_stream_->priority());
  }

  void SetExpectedStream(QuicSpdyStream* stream) { expected_stream_ = stream; }
  void SetExpectedPriority(SpdyPriority priority) {
    expected_priority_ = priority;
  }

 private:
  QuicSpdyStream* expected_stream_;
  SpdyPriority expected_priority_;
};

class QuicSpdyStreamTest : public QuicTestWithParam<ParsedQuicVersion> {
 public:
  QuicSpdyStreamTest() {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
    headers_["cookie"] =
        "__utma=208381060.1228362404.1372200928.1372200928.1372200928.1; "
        "__utmc=160408618; "
        "GX=DQAAAOEAAACWJYdewdE9rIrW6qw3PtVi2-d729qaa-74KqOsM1NVQblK4VhX"
        "hoALMsy6HOdDad2Sz0flUByv7etmo3mLMidGrBoljqO9hSVA40SLqpG_iuKKSHX"
        "RW3Np4bq0F0SDGDNsW0DSmTS9ufMRrlpARJDS7qAI6M3bghqJp4eABKZiRqebHT"
        "pMU-RXvTI5D5oCF1vYxYofH_l1Kviuiy3oQ1kS1enqWgbhJ2t61_SNdv-1XJIS0"
        "O3YeHLmVCs62O6zp89QwakfAWK9d3IDQvVSJzCQsvxvNIvaZFa567MawWlXg0Rh"
        "1zFMi5vzcns38-8_Sns; "
        "GA=v*2%2Fmem*57968640*47239936%2Fmem*57968640*47114716%2Fno-nm-"
        "yj*15%2Fno-cc-yj*5%2Fpc-ch*133685%2Fpc-s-cr*133947%2Fpc-s-t*1339"
        "47%2Fno-nm-yj*4%2Fno-cc-yj*1%2Fceft-as*1%2Fceft-nqas*0%2Fad-ra-c"
        "v_p%2Fad-nr-cv_p-f*1%2Fad-v-cv_p*859%2Fad-ns-cv_p-f*1%2Ffn-v-ad%"
        "2Fpc-t*250%2Fpc-cm*461%2Fpc-s-cr*722%2Fpc-s-t*722%2Fau_p*4"
        "SICAID=AJKiYcHdKgxum7KMXG0ei2t1-W4OD1uW-ecNsCqC0wDuAXiDGIcT_HA2o1"
        "3Rs1UKCuBAF9g8rWNOFbxt8PSNSHFuIhOo2t6bJAVpCsMU5Laa6lewuTMYI8MzdQP"
        "ARHKyW-koxuhMZHUnGBJAM1gJODe0cATO_KGoX4pbbFxxJ5IicRxOrWK_5rU3cdy6"
        "edlR9FsEdH6iujMcHkbE5l18ehJDwTWmBKBzVD87naobhMMrF6VvnDGxQVGp9Ir_b"
        "Rgj3RWUoPumQVCxtSOBdX0GlJOEcDTNCzQIm9BSfetog_eP_TfYubKudt5eMsXmN6"
        "QnyXHeGeK2UINUzJ-D30AFcpqYgH9_1BvYSpi7fc7_ydBU8TaD8ZRxvtnzXqj0RfG"
        "tuHghmv3aD-uzSYJ75XDdzKdizZ86IG6Fbn1XFhYZM-fbHhm3mVEXnyRW4ZuNOLFk"
        "Fas6LMcVC6Q8QLlHYbXBpdNFuGbuZGUnav5C-2I_-46lL0NGg3GewxGKGHvHEfoyn"
        "EFFlEYHsBQ98rXImL8ySDycdLEFvBPdtctPmWCfTxwmoSMLHU2SCVDhbqMWU5b0yr"
        "JBCScs_ejbKaqBDoB7ZGxTvqlrB__2ZmnHHjCr8RgMRtKNtIeuZAo ";
  }

  void Initialize(bool stream_should_process_data) {
    connection_ = new StrictMock<MockQuicConnection>(
        &helper_, &alarm_factory_, Perspective::IS_SERVER,
        SupportedVersions(GetParam()));
    session_ = QuicMakeUnique<StrictMock<MockQuicSpdySession>>(connection_);
    session_->Initialize();
    ON_CALL(*session_, WritevData(_, _, _, _, _))
        .WillByDefault(Invoke(MockQuicSession::ConsumeData));

    stream_ =
        new StrictMock<TestStream>(GetNthClientInitiatedBidirectionalId(0),
                                   session_.get(), stream_should_process_data);
    session_->ActivateStream(QuicWrapUnique(stream_));
    stream2_ =
        new StrictMock<TestStream>(GetNthClientInitiatedBidirectionalId(1),
                                   session_.get(), stream_should_process_data);
    session_->ActivateStream(QuicWrapUnique(stream2_));
  }

  QuicHeaderList ProcessHeaders(bool fin, const SpdyHeaderBlock& headers) {
    QuicHeaderList h = AsHeaderList(headers);
    stream_->OnStreamHeaderList(fin, h.uncompressed_header_bytes(), h);
    return h;
  }

  QuicStreamId GetNthClientInitiatedBidirectionalId(int n) {
    return GetNthClientInitiatedBidirectionalStreamId(
        connection_->transport_version(), n);
  }

  bool HasFrameHeader() const {
    return VersionHasDataFrameHeader(connection_->transport_version());
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;
  std::unique_ptr<MockQuicSpdySession> session_;

  // Owned by the |session_|.
  TestStream* stream_;
  TestStream* stream2_;

  SpdyHeaderBlock headers_;

  HttpEncoder encoder_;
};

INSTANTIATE_TEST_SUITE_P(Tests,
                         QuicSpdyStreamTest,
                         ::testing::ValuesIn(AllSupportedVersions()));

TEST_P(QuicSpdyStreamTest, ProcessHeaderList) {
  Initialize(kShouldProcessData);

  stream_->OnStreamHeadersPriority(kV3HighestPriority);
  ProcessHeaders(false, headers_);
  EXPECT_EQ("", stream_->data());
  EXPECT_FALSE(stream_->header_list().empty());
  EXPECT_FALSE(stream_->IsDoneReading());
}

TEST_P(QuicSpdyStreamTest, ProcessTooLargeHeaderList) {
  Initialize(kShouldProcessData);

  QuicHeaderList headers;
  stream_->OnStreamHeadersPriority(kV3HighestPriority);

  EXPECT_CALL(*session_,
              SendRstStream(stream_->id(), QUIC_HEADERS_TOO_LARGE, 0));
  stream_->OnStreamHeaderList(false, 1 << 20, headers);
  EXPECT_EQ(QUIC_HEADERS_TOO_LARGE, stream_->stream_error());
}

TEST_P(QuicSpdyStreamTest, ProcessHeaderListWithFin) {
  Initialize(kShouldProcessData);

  size_t total_bytes = 0;
  QuicHeaderList headers;
  for (auto p : headers_) {
    headers.OnHeader(p.first, p.second);
    total_bytes += p.first.size() + p.second.size();
  }
  stream_->OnStreamHeadersPriority(kV3HighestPriority);
  stream_->OnStreamHeaderList(true, total_bytes, headers);
  EXPECT_EQ("", stream_->data());
  EXPECT_FALSE(stream_->header_list().empty());
  EXPECT_FALSE(stream_->IsDoneReading());
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_P(QuicSpdyStreamTest, ParseHeaderStatusCode) {
  // A valid status code should be 3-digit integer. The first digit should be in
  // the range of [1, 5]. All the others are invalid.
  Initialize(kShouldProcessData);
  int status_code = 0;

  // Valid status codes.
  headers_[":status"] = "404";
  EXPECT_TRUE(stream_->ParseHeaderStatusCode(headers_, &status_code));
  EXPECT_EQ(404, status_code);

  headers_[":status"] = "100";
  EXPECT_TRUE(stream_->ParseHeaderStatusCode(headers_, &status_code));
  EXPECT_EQ(100, status_code);

  headers_[":status"] = "599";
  EXPECT_TRUE(stream_->ParseHeaderStatusCode(headers_, &status_code));
  EXPECT_EQ(599, status_code);

  // Invalid status codes.
  headers_[":status"] = "010";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "600";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "200 ok";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "2000";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "+200";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "+20";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "-10";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "-100";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  // Leading or trailing spaces are also invalid.
  headers_[":status"] = " 200";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "200 ";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = " 200 ";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));

  headers_[":status"] = "  ";
  EXPECT_FALSE(stream_->ParseHeaderStatusCode(headers_, &status_code));
}

TEST_P(QuicSpdyStreamTest, MarkHeadersConsumed) {
  Initialize(kShouldProcessData);

  QuicString body = "this is the body";
  QuicHeaderList headers = ProcessHeaders(false, headers_);
  EXPECT_EQ(headers, stream_->header_list());

  stream_->ConsumeHeaderList();
  EXPECT_EQ(QuicHeaderList(), stream_->header_list());
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndBody) {
  Initialize(kShouldProcessData);

  QuicString body = "this is the body";
  std::unique_ptr<char[]> buffer;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buffer);
  QuicString header = QuicString(buffer.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  EXPECT_EQ("", stream_->data());
  QuicHeaderList headers = ProcessHeaders(false, headers_);
  EXPECT_EQ(headers, stream_->header_list());
  stream_->ConsumeHeaderList();
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));
  stream_->OnStreamFrame(frame);
  EXPECT_EQ(QuicHeaderList(), stream_->header_list());
  EXPECT_EQ(body, stream_->data());
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndBodyFragments) {
  Initialize(kShouldProcessData);
  QuicString body = "this is the body";
  std::unique_ptr<char[]> buffer;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buffer);
  QuicString header = QuicString(buffer.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  for (size_t fragment_size = 1; fragment_size < data.size(); ++fragment_size) {
    Initialize(kShouldProcessData);
    QuicHeaderList headers = ProcessHeaders(false, headers_);
    ASSERT_EQ(headers, stream_->header_list());
    stream_->ConsumeHeaderList();
    for (size_t offset = 0; offset < data.size(); offset += fragment_size) {
      size_t remaining_data = data.size() - offset;
      QuicStringPiece fragment(data.data() + offset,
                               std::min(fragment_size, remaining_data));
      QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false,
                            offset, QuicStringPiece(fragment));
      stream_->OnStreamFrame(frame);
    }
    ASSERT_EQ(body, stream_->data()) << "fragment_size: " << fragment_size;
  }
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndBodyFragmentsSplit) {
  Initialize(kShouldProcessData);
  QuicString body = "this is the body";
  std::unique_ptr<char[]> buffer;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buffer);
  QuicString header = QuicString(buffer.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  for (size_t split_point = 1; split_point < data.size() - 1; ++split_point) {
    Initialize(kShouldProcessData);
    QuicHeaderList headers = ProcessHeaders(false, headers_);
    ASSERT_EQ(headers, stream_->header_list());
    stream_->ConsumeHeaderList();

    QuicStringPiece fragment1(data.data(), split_point);
    QuicStreamFrame frame1(GetNthClientInitiatedBidirectionalId(0), false, 0,
                           QuicStringPiece(fragment1));
    stream_->OnStreamFrame(frame1);

    QuicStringPiece fragment2(data.data() + split_point,
                              data.size() - split_point);
    QuicStreamFrame frame2(GetNthClientInitiatedBidirectionalId(0), false,
                           split_point, QuicStringPiece(fragment2));
    stream_->OnStreamFrame(frame2);

    ASSERT_EQ(body, stream_->data()) << "split_point: " << split_point;
  }
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndBodyReadv) {
  Initialize(!kShouldProcessData);

  QuicString body = "this is the body";
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  ProcessHeaders(false, headers_);
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));
  stream_->OnStreamFrame(frame);
  stream_->ConsumeHeaderList();

  char buffer[2048];
  ASSERT_LT(data.length(), QUIC_ARRAYSIZE(buffer));
  struct iovec vec;
  vec.iov_base = buffer;
  vec.iov_len = QUIC_ARRAYSIZE(buffer);

  size_t bytes_read = stream_->Readv(&vec, 1);
  EXPECT_EQ(body.length(), bytes_read);
  EXPECT_EQ(body, QuicString(buffer, bytes_read));
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndLargeBodySmallReadv) {
  Initialize(kShouldProcessData);
  QuicString body(12 * 1024, 'a');
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;
  ProcessHeaders(false, headers_);
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));
  stream_->OnStreamFrame(frame);
  stream_->ConsumeHeaderList();
  char buffer[2048];
  char buffer2[2048];
  struct iovec vec[2];
  vec[0].iov_base = buffer;
  vec[0].iov_len = QUIC_ARRAYSIZE(buffer);
  vec[1].iov_base = buffer2;
  vec[1].iov_len = QUIC_ARRAYSIZE(buffer2);
  size_t bytes_read = stream_->Readv(vec, 2);
  EXPECT_EQ(2048u * 2, bytes_read);
  EXPECT_EQ(body.substr(0, 2048), QuicString(buffer, 2048));
  EXPECT_EQ(body.substr(2048, 2048), QuicString(buffer2, 2048));
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndBodyMarkConsumed) {
  Initialize(!kShouldProcessData);

  QuicString body = "this is the body";
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  ProcessHeaders(false, headers_);
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));
  stream_->OnStreamFrame(frame);
  stream_->ConsumeHeaderList();

  struct iovec vec;

  EXPECT_EQ(1, stream_->GetReadableRegions(&vec, 1));
  EXPECT_EQ(body.length(), vec.iov_len);
  EXPECT_EQ(body, QuicString(static_cast<char*>(vec.iov_base), vec.iov_len));

  stream_->MarkConsumed(body.length());
  EXPECT_EQ(data.length(), stream_->flow_controller()->bytes_consumed());
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndConsumeMultipleBody) {
  Initialize(!kShouldProcessData);
  QuicString body1 = "this is body 1";
  QuicString body2 = "body 2";
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body1.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data1 = HasFrameHeader() ? header + body1 : body1;
  header_length = encoder_.SerializeDataFrameHeader(body2.length(), &buf);
  QuicString data2 = HasFrameHeader() ? header + body2 : body2;

  ProcessHeaders(false, headers_);
  QuicStreamFrame frame1(GetNthClientInitiatedBidirectionalId(0), false, 0,
                         QuicStringPiece(data1));
  QuicStreamFrame frame2(GetNthClientInitiatedBidirectionalId(0), false,
                         data1.length(), QuicStringPiece(data2));
  stream_->OnStreamFrame(frame1);
  stream_->OnStreamFrame(frame2);
  stream_->ConsumeHeaderList();

  stream_->MarkConsumed(body1.length() + body2.length());
  EXPECT_EQ(data1.length() + data2.length(),
            stream_->flow_controller()->bytes_consumed());
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersAndBodyIncrementalReadv) {
  Initialize(!kShouldProcessData);

  QuicString body = "this is the body";
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  ProcessHeaders(false, headers_);
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));
  stream_->OnStreamFrame(frame);
  stream_->ConsumeHeaderList();

  char buffer[1];
  struct iovec vec;
  vec.iov_base = buffer;
  vec.iov_len = QUIC_ARRAYSIZE(buffer);

  for (size_t i = 0; i < body.length(); ++i) {
    size_t bytes_read = stream_->Readv(&vec, 1);
    ASSERT_EQ(1u, bytes_read);
    EXPECT_EQ(body.data()[i], buffer[0]);
  }
}

TEST_P(QuicSpdyStreamTest, ProcessHeadersUsingReadvWithMultipleIovecs) {
  Initialize(!kShouldProcessData);

  QuicString body = "this is the body";
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  ProcessHeaders(false, headers_);
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));
  stream_->OnStreamFrame(frame);
  stream_->ConsumeHeaderList();

  char buffer1[1];
  char buffer2[1];
  struct iovec vec[2];
  vec[0].iov_base = buffer1;
  vec[0].iov_len = QUIC_ARRAYSIZE(buffer1);
  vec[1].iov_base = buffer2;
  vec[1].iov_len = QUIC_ARRAYSIZE(buffer2);

  for (size_t i = 0; i < body.length(); i += 2) {
    size_t bytes_read = stream_->Readv(vec, 2);
    ASSERT_EQ(2u, bytes_read) << i;
    ASSERT_EQ(body.data()[i], buffer1[0]) << i;
    ASSERT_EQ(body.data()[i + 1], buffer2[0]) << i;
  }
}

TEST_P(QuicSpdyStreamTest, StreamFlowControlBlocked) {
  testing::InSequence seq;
  // Tests that we send a BLOCKED frame to the peer when we attempt to write,
  // but are flow control blocked.
  Initialize(kShouldProcessData);

  // Set a small flow control limit.
  const uint64_t kWindow = 36;
  QuicFlowControllerPeer::SetSendWindowOffset(stream_->flow_controller(),
                                              kWindow);
  EXPECT_EQ(kWindow, QuicFlowControllerPeer::SendWindowOffset(
                         stream_->flow_controller()));

  // Try to send more data than the flow control limit allows.
  const uint64_t kOverflow = 15;
  QuicString body(kWindow + kOverflow, 'a');

  const uint64_t kHeaderLength = HasFrameHeader() ? 2 : 0;
  if (HasFrameHeader()) {
    EXPECT_CALL(*session_, WritevData(_, _, kHeaderLength, _, NO_FIN));
  }
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(kWindow - kHeaderLength, true)));
  EXPECT_CALL(*connection_, SendControlFrame(_));
  stream_->WriteOrBufferBody(body, false);

  // Should have sent as much as possible, resulting in no send window left.
  EXPECT_EQ(0u,
            QuicFlowControllerPeer::SendWindowSize(stream_->flow_controller()));

  // And we should have queued the overflowed data.
  EXPECT_EQ(kOverflow + kHeaderLength,
            QuicStreamPeer::SizeOfQueuedData(stream_));
}

TEST_P(QuicSpdyStreamTest, StreamFlowControlNoWindowUpdateIfNotConsumed) {
  // The flow control receive window decreases whenever we add new bytes to the
  // sequencer, whether they are consumed immediately or buffered. However we
  // only send WINDOW_UPDATE frames based on increasing number of bytes
  // consumed.

  // Don't process data - it will be buffered instead.
  Initialize(!kShouldProcessData);

  // Expect no WINDOW_UPDATE frames to be sent.
  EXPECT_CALL(*connection_, SendWindowUpdate(_, _)).Times(0);

  // Set a small flow control receive window.
  const uint64_t kWindow = 36;
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream_->flow_controller(),
                                                 kWindow);
  QuicFlowControllerPeer::SetMaxReceiveWindow(stream_->flow_controller(),
                                              kWindow);
  EXPECT_EQ(kWindow, QuicFlowControllerPeer::ReceiveWindowOffset(
                         stream_->flow_controller()));

  // Stream receives enough data to fill a fraction of the receive window.
  QuicString body(kWindow / 3, 'a');
  QuicByteCount header_length = 0;
  QuicString data;

  if (HasFrameHeader()) {
    std::unique_ptr<char[]> buffer;
    header_length = encoder_.SerializeDataFrameHeader(body.length(), &buffer);
    QuicString header = QuicString(buffer.get(), header_length);
    data = header + body;
  } else {
    data = body;
  }

  ProcessHeaders(false, headers_);

  QuicStreamFrame frame1(GetNthClientInitiatedBidirectionalId(0), false, 0,
                         QuicStringPiece(data));
  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(
      kWindow - (kWindow / 3) - header_length,
      QuicFlowControllerPeer::ReceiveWindowSize(stream_->flow_controller()));

  // Now receive another frame which results in the receive window being over
  // half full. This should all be buffered, decreasing the receive window but
  // not sending WINDOW_UPDATE.
  QuicStreamFrame frame2(GetNthClientInitiatedBidirectionalId(0), false,
                         kWindow / 3 + header_length, QuicStringPiece(data));
  stream_->OnStreamFrame(frame2);
  EXPECT_EQ(
      kWindow - (2 * kWindow / 3) - 2 * header_length,
      QuicFlowControllerPeer::ReceiveWindowSize(stream_->flow_controller()));
}

TEST_P(QuicSpdyStreamTest, StreamFlowControlWindowUpdate) {
  // Tests that on receipt of data, the stream updates its receive window offset
  // appropriately, and sends WINDOW_UPDATE frames when its receive window drops
  // too low.
  Initialize(kShouldProcessData);

  // Set a small flow control limit.
  const uint64_t kWindow = 36;
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream_->flow_controller(),
                                                 kWindow);
  QuicFlowControllerPeer::SetMaxReceiveWindow(stream_->flow_controller(),
                                              kWindow);
  EXPECT_EQ(kWindow, QuicFlowControllerPeer::ReceiveWindowOffset(
                         stream_->flow_controller()));

  // Stream receives enough data to fill a fraction of the receive window.
  QuicString body(kWindow / 3, 'a');
  QuicByteCount header_length = 0;
  QuicString data;

  if (HasFrameHeader()) {
    std::unique_ptr<char[]> buffer;
    header_length = encoder_.SerializeDataFrameHeader(body.length(), &buffer);
    QuicString header = QuicString(buffer.get(), header_length);
    data = header + body;
  } else {
    data = body;
  }

  ProcessHeaders(false, headers_);
  stream_->ConsumeHeaderList();

  QuicStreamFrame frame1(GetNthClientInitiatedBidirectionalId(0), false, 0,
                         QuicStringPiece(data));
  stream_->OnStreamFrame(frame1);
  EXPECT_EQ(
      kWindow - (kWindow / 3) - header_length,
      QuicFlowControllerPeer::ReceiveWindowSize(stream_->flow_controller()));

  // Now receive another frame which results in the receive window being over
  // half full.  This will trigger the stream to increase its receive window
  // offset and send a WINDOW_UPDATE. The result will be again an available
  // window of kWindow bytes.
  QuicStreamFrame frame2(GetNthClientInitiatedBidirectionalId(0), false,
                         kWindow / 3 + header_length, QuicStringPiece(data));
  EXPECT_CALL(*connection_, SendControlFrame(_));
  stream_->OnStreamFrame(frame2);
  EXPECT_EQ(kWindow, QuicFlowControllerPeer::ReceiveWindowSize(
                         stream_->flow_controller()));
}

TEST_P(QuicSpdyStreamTest, ConnectionFlowControlWindowUpdate) {
  // Tests that on receipt of data, the connection updates its receive window
  // offset appropriately, and sends WINDOW_UPDATE frames when its receive
  // window drops too low.
  Initialize(kShouldProcessData);

  // Set a small flow control limit for streams and connection.
  const uint64_t kWindow = 36;
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream_->flow_controller(),
                                                 kWindow);
  QuicFlowControllerPeer::SetMaxReceiveWindow(stream_->flow_controller(),
                                              kWindow);
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream2_->flow_controller(),
                                                 kWindow);
  QuicFlowControllerPeer::SetMaxReceiveWindow(stream2_->flow_controller(),
                                              kWindow);
  QuicFlowControllerPeer::SetReceiveWindowOffset(session_->flow_controller(),
                                                 kWindow);
  QuicFlowControllerPeer::SetMaxReceiveWindow(session_->flow_controller(),
                                              kWindow);

  // Supply headers to both streams so that they are happy to receive data.
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                              headers);
  stream_->ConsumeHeaderList();
  stream2_->OnStreamHeaderList(false, headers.uncompressed_header_bytes(),
                               headers);
  stream2_->ConsumeHeaderList();

  // Each stream gets a quarter window of data. This should not trigger a
  // WINDOW_UPDATE for either stream, nor for the connection.
  QuicByteCount header_length = 0;
  QuicString body;
  QuicString data;
  QuicString data2;
  QuicString body2(1, 'a');

  if (HasFrameHeader()) {
    body = QuicString(kWindow / 4 - 2, 'a');
    std::unique_ptr<char[]> buffer;
    header_length = encoder_.SerializeDataFrameHeader(body.length(), &buffer);
    QuicString header = QuicString(buffer.get(), header_length);
    data = header + body;
    std::unique_ptr<char[]> buffer2;
    QuicByteCount header_length2 =
        encoder_.SerializeDataFrameHeader(body2.length(), &buffer2);
    QuicString header2 = QuicString(buffer2.get(), header_length2);
    data2 = header2 + body2;
  } else {
    body = QuicString(kWindow / 4, 'a');
    data = body;
    data2 = body2;
  }

  QuicStreamFrame frame1(GetNthClientInitiatedBidirectionalId(0), false, 0,
                         QuicStringPiece(data));
  stream_->OnStreamFrame(frame1);
  QuicStreamFrame frame2(GetNthClientInitiatedBidirectionalId(1), false, 0,
                         QuicStringPiece(data));
  stream2_->OnStreamFrame(frame2);

  // Now receive a further single byte on one stream - again this does not
  // trigger a stream WINDOW_UPDATE, but now the connection flow control window
  // is over half full and thus a connection WINDOW_UPDATE is sent.
  EXPECT_CALL(*connection_, SendControlFrame(_));
  QuicStreamFrame frame3(GetNthClientInitiatedBidirectionalId(0), false,
                         body.length() + header_length, QuicStringPiece(data2));
  stream_->OnStreamFrame(frame3);
}

TEST_P(QuicSpdyStreamTest, StreamFlowControlViolation) {
  // Tests that on if the peer sends too much data (i.e. violates the flow
  // control protocol), then we terminate the connection.

  // Stream should not process data, so that data gets buffered in the
  // sequencer, triggering flow control limits.
  Initialize(!kShouldProcessData);

  // Set a small flow control limit.
  const uint64_t kWindow = 50;
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream_->flow_controller(),
                                                 kWindow);

  ProcessHeaders(false, headers_);

  // Receive data to overflow the window, violating flow control.
  QuicString body(kWindow + 1, 'a');
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  stream_->OnStreamFrame(frame);
}

TEST_P(QuicSpdyStreamTest, TestHandlingQuicRstStreamNoError) {
  Initialize(kShouldProcessData);
  ProcessHeaders(false, headers_);

  stream_->OnStreamReset(QuicRstStreamFrame(
      kInvalidControlFrameId, stream_->id(), QUIC_STREAM_NO_ERROR, 0));
  EXPECT_TRUE(stream_->write_side_closed());
  EXPECT_FALSE(stream_->reading_stopped());
}

TEST_P(QuicSpdyStreamTest, ConnectionFlowControlViolation) {
  // Tests that on if the peer sends too much data (i.e. violates the flow
  // control protocol), at the connection level (rather than the stream level)
  // then we terminate the connection.

  // Stream should not process data, so that data gets buffered in the
  // sequencer, triggering flow control limits.
  Initialize(!kShouldProcessData);

  // Set a small flow control window on streams, and connection.
  const uint64_t kStreamWindow = 50;
  const uint64_t kConnectionWindow = 10;
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream_->flow_controller(),
                                                 kStreamWindow);
  QuicFlowControllerPeer::SetReceiveWindowOffset(session_->flow_controller(),
                                                 kConnectionWindow);

  ProcessHeaders(false, headers_);

  // Send enough data to overflow the connection level flow control window.
  QuicString body(kConnectionWindow + 1, 'a');
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  EXPECT_LT(data.size(), kStreamWindow);
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), false, 0,
                        QuicStringPiece(data));

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  stream_->OnStreamFrame(frame);
}

TEST_P(QuicSpdyStreamTest, StreamFlowControlFinNotBlocked) {
  // An attempt to write a FIN with no data should not be flow control blocked,
  // even if the send window is 0.

  Initialize(kShouldProcessData);

  // Set a flow control limit of zero.
  QuicFlowControllerPeer::SetReceiveWindowOffset(stream_->flow_controller(), 0);
  EXPECT_EQ(0u, QuicFlowControllerPeer::ReceiveWindowOffset(
                    stream_->flow_controller()));

  // Send a frame with a FIN but no data. This should not be blocked.
  QuicString body = "";
  bool fin = true;

  EXPECT_CALL(*connection_,
              SendBlocked(GetNthClientInitiatedBidirectionalId(0)))
      .Times(0);
  EXPECT_CALL(*session_, WritevData(_, _, 0, _, FIN));

  stream_->WriteOrBufferBody(body, fin);
}

TEST_P(QuicSpdyStreamTest, ReceivingTrailersViaHeaderList) {
  // Test that receiving trailing headers from the peer via
  // OnStreamHeaderList() works, and can be read from the stream and consumed.
  Initialize(kShouldProcessData);

  // Receive initial headers.
  size_t total_bytes = 0;
  QuicHeaderList headers;
  for (const auto& p : headers_) {
    headers.OnHeader(p.first, p.second);
    total_bytes += p.first.size() + p.second.size();
  }

  stream_->OnStreamHeadersPriority(kV3HighestPriority);
  stream_->OnStreamHeaderList(/*fin=*/false, total_bytes, headers);
  stream_->ConsumeHeaderList();

  // Receive trailing headers.
  SpdyHeaderBlock trailers_block;
  trailers_block["key1"] = "value1";
  trailers_block["key2"] = "value2";
  trailers_block["key3"] = "value3";
  SpdyHeaderBlock trailers_block_with_final_offset = trailers_block.Clone();
  trailers_block_with_final_offset[kFinalOffsetHeaderKey] = "0";
  total_bytes = 0;
  QuicHeaderList trailers;
  for (const auto& p : trailers_block_with_final_offset) {
    trailers.OnHeader(p.first, p.second);
    total_bytes += p.first.size() + p.second.size();
  }
  stream_->OnStreamHeaderList(/*fin=*/true, total_bytes, trailers);

  // The trailers should be decompressed, and readable from the stream.
  EXPECT_TRUE(stream_->trailers_decompressed());
  EXPECT_EQ(trailers_block, stream_->received_trailers());

  // IsDoneReading() returns false until trailers marked consumed.
  EXPECT_FALSE(stream_->IsDoneReading());
  stream_->MarkTrailersConsumed();
  EXPECT_TRUE(stream_->IsDoneReading());
}

TEST_P(QuicSpdyStreamTest, ReceivingTrailersWithOffset) {
  // Test that when receiving trailing headers with an offset before response
  // body, stream is closed at the right offset.
  Initialize(kShouldProcessData);

  // Receive initial headers.
  QuicHeaderList headers = ProcessHeaders(false, headers_);
  stream_->ConsumeHeaderList();

  const QuicString body = "this is the body";
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  // Receive trailing headers.
  SpdyHeaderBlock trailers_block;
  trailers_block["key1"] = "value1";
  trailers_block["key2"] = "value2";
  trailers_block["key3"] = "value3";
  trailers_block[kFinalOffsetHeaderKey] =
      QuicTextUtils::Uint64ToString(data.size());

  QuicHeaderList trailers = ProcessHeaders(true, trailers_block);

  // The trailers should be decompressed, and readable from the stream.
  EXPECT_TRUE(stream_->trailers_decompressed());

  // The final offset trailer will be consumed by QUIC.
  trailers_block.erase(kFinalOffsetHeaderKey);
  EXPECT_EQ(trailers_block, stream_->received_trailers());

  // Consuming the trailers erases them from the stream.
  stream_->MarkTrailersConsumed();
  EXPECT_TRUE(stream_->FinishedReadingTrailers());

  EXPECT_FALSE(stream_->IsDoneReading());
  // Receive and consume body.
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), /*fin=*/false,
                        0, data);
  stream_->OnStreamFrame(frame);
  EXPECT_EQ(body, stream_->data());
  EXPECT_TRUE(stream_->IsDoneReading());
}

TEST_P(QuicSpdyStreamTest, ReceivingTrailersWithoutOffset) {
  // Test that receiving trailers without a final offset field is an error.
  Initialize(kShouldProcessData);

  // Receive initial headers.
  ProcessHeaders(false, headers_);
  stream_->ConsumeHeaderList();

  // Receive trailing headers, without kFinalOffsetHeaderKey.
  SpdyHeaderBlock trailers_block;
  trailers_block["key1"] = "value1";
  trailers_block["key2"] = "value2";
  trailers_block["key3"] = "value3";
  auto trailers = AsHeaderList(trailers_block);

  // Verify that the trailers block didn't contain a final offset.
  EXPECT_EQ("", trailers_block[kFinalOffsetHeaderKey].as_string());

  // Receipt of the malformed trailers will close the connection.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA, _, _))
      .Times(1);
  stream_->OnStreamHeaderList(/*fin=*/true,
                              trailers.uncompressed_header_bytes(), trailers);
}

TEST_P(QuicSpdyStreamTest, ReceivingTrailersWithoutFin) {
  // Test that received Trailers must always have the FIN set.
  Initialize(kShouldProcessData);

  // Receive initial headers.
  auto headers = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(/*fin=*/false,
                              headers.uncompressed_header_bytes(), headers);
  stream_->ConsumeHeaderList();

  // Receive trailing headers with FIN deliberately set to false.
  SpdyHeaderBlock trailers_block;
  trailers_block["foo"] = "bar";
  auto trailers = AsHeaderList(trailers_block);

  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA, _, _))
      .Times(1);
  stream_->OnStreamHeaderList(/*fin=*/false,
                              trailers.uncompressed_header_bytes(), trailers);
}

TEST_P(QuicSpdyStreamTest, ReceivingTrailersAfterHeadersWithFin) {
  // If headers are received with a FIN, no trailers should then arrive.
  Initialize(kShouldProcessData);

  // Receive initial headers with FIN set.
  ProcessHeaders(true, headers_);
  stream_->ConsumeHeaderList();

  // Receive trailing headers after FIN already received.
  SpdyHeaderBlock trailers_block;
  trailers_block["foo"] = "bar";
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA, _, _))
      .Times(1);
  ProcessHeaders(true, trailers_block);
}

TEST_P(QuicSpdyStreamTest, ReceivingTrailersAfterBodyWithFin) {
  // If body data are received with a FIN, no trailers should then arrive.
  Initialize(kShouldProcessData);

  // Receive initial headers without FIN set.
  ProcessHeaders(false, headers_);
  stream_->ConsumeHeaderList();

  // Receive body data, with FIN.
  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), /*fin=*/true,
                        0, "body");
  stream_->OnStreamFrame(frame);

  // Receive trailing headers after FIN already received.
  SpdyHeaderBlock trailers_block;
  trailers_block["foo"] = "bar";
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_INVALID_HEADERS_STREAM_DATA, _, _))
      .Times(1);
  ProcessHeaders(true, trailers_block);
}

TEST_P(QuicSpdyStreamTest, ClosingStreamWithNoTrailers) {
  // Verify that a stream receiving headers, body, and no trailers is correctly
  // marked as done reading on consumption of headers and body.
  Initialize(kShouldProcessData);

  // Receive and consume initial headers with FIN not set.
  auto h = AsHeaderList(headers_);
  stream_->OnStreamHeaderList(/*fin=*/false, h.uncompressed_header_bytes(), h);
  stream_->ConsumeHeaderList();

  // Receive and consume body with FIN set, and no trailers.
  QuicString body(1024, 'x');
  std::unique_ptr<char[]> buf;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buf);
  QuicString header = QuicString(buf.get(), header_length);
  QuicString data = HasFrameHeader() ? header + body : body;

  QuicStreamFrame frame(GetNthClientInitiatedBidirectionalId(0), /*fin=*/true,
                        0, data);
  stream_->OnStreamFrame(frame);

  EXPECT_TRUE(stream_->IsDoneReading());
}

TEST_P(QuicSpdyStreamTest, WritingTrailersSendsAFin) {
  // Test that writing trailers will send a FIN, as Trailers are the last thing
  // to be sent on a stream.
  Initialize(kShouldProcessData);

  // Write the initial headers, without a FIN.
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  stream_->WriteHeaders(SpdyHeaderBlock(), /*fin=*/false, nullptr);

  // Writing trailers implicitly sends a FIN.
  SpdyHeaderBlock trailers;
  trailers["trailer key"] = "trailer value";
  EXPECT_CALL(*stream_, WriteHeadersMock(true));
  stream_->WriteTrailers(std::move(trailers), nullptr);
  EXPECT_TRUE(stream_->fin_sent());
}

TEST_P(QuicSpdyStreamTest, WritingTrailersFinalOffset) {
  // Test that when writing trailers, the trailers that are actually sent to the
  // peer contain the final offset field indicating last byte of data.
  Initialize(kShouldProcessData);

  // Write the initial headers.
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  stream_->WriteHeaders(SpdyHeaderBlock(), /*fin=*/false, nullptr);

  // Write non-zero body data to force a non-zero final offset.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  QuicString body(1024, 'x');  // 1 kB
  QuicByteCount header_length = 0;
  if (HasFrameHeader()) {
    std::unique_ptr<char[]> buf;
    header_length = encoder_.SerializeDataFrameHeader(body.length(), &buf);
  }

  stream_->WriteOrBufferBody(body, false);

  // The final offset field in the trailing headers is populated with the
  // number of body bytes written (including queued bytes).
  SpdyHeaderBlock trailers;
  trailers["trailer key"] = "trailer value";
  SpdyHeaderBlock trailers_with_offset(trailers.Clone());
  trailers_with_offset[kFinalOffsetHeaderKey] =
      QuicTextUtils::Uint64ToString(body.length() + header_length);
  EXPECT_CALL(*stream_, WriteHeadersMock(true));
  stream_->WriteTrailers(std::move(trailers), nullptr);
  EXPECT_EQ(trailers_with_offset, stream_->saved_headers());
}

TEST_P(QuicSpdyStreamTest, WritingTrailersClosesWriteSide) {
  // Test that if trailers are written after all other data has been written
  // (headers and body), that this closes the stream for writing.
  Initialize(kShouldProcessData);

  // Write the initial headers.
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  stream_->WriteHeaders(SpdyHeaderBlock(), /*fin=*/false, nullptr);

  // Write non-zero body data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  const int kBodySize = 1 * 1024;  // 1 kB
  stream_->WriteOrBufferBody(QuicString(kBodySize, 'x'), false);
  EXPECT_EQ(0u, stream_->BufferedDataBytes());

  // Headers and body have been fully written, there is no queued data. Writing
  // trailers marks the end of this stream, and thus the write side is closed.
  EXPECT_CALL(*stream_, WriteHeadersMock(true));
  stream_->WriteTrailers(SpdyHeaderBlock(), nullptr);
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSpdyStreamTest, WritingTrailersWithQueuedBytes) {
  // Test that the stream is not closed for writing when trailers are sent
  // while there are still body bytes queued.
  testing::InSequence seq;
  Initialize(kShouldProcessData);

  // Write the initial headers.
  EXPECT_CALL(*stream_, WriteHeadersMock(false));
  stream_->WriteHeaders(SpdyHeaderBlock(), /*fin=*/false, nullptr);

  // Write non-zero body data, but only consume partially, ensuring queueing.
  const int kBodySize = 1 * 1024;  // 1 kB
  if (HasFrameHeader()) {
    EXPECT_CALL(*session_, WritevData(_, _, 3, _, NO_FIN));
  }
  EXPECT_CALL(*session_, WritevData(_, _, kBodySize, _, NO_FIN))
      .WillOnce(Return(QuicConsumedData(kBodySize - 1, false)));
  stream_->WriteOrBufferBody(QuicString(kBodySize, 'x'), false);
  EXPECT_EQ(1u, stream_->BufferedDataBytes());

  // Writing trailers will send a FIN, but not close the write side of the
  // stream as there are queued bytes.
  EXPECT_CALL(*stream_, WriteHeadersMock(true));
  stream_->WriteTrailers(SpdyHeaderBlock(), nullptr);
  EXPECT_TRUE(stream_->fin_sent());
  EXPECT_FALSE(stream_->write_side_closed());

  // Writing the queued bytes will close the write side of the stream.
  EXPECT_CALL(*session_, WritevData(_, _, 1, _, NO_FIN));
  stream_->OnCanWrite();
  EXPECT_TRUE(stream_->write_side_closed());
}

TEST_P(QuicSpdyStreamTest, WritingTrailersAfterFIN) {
  // EXPECT_QUIC_BUG tests are expensive so only run one instance of them.
  if (GetParam() != AllSupportedVersions()[0]) {
    return;
  }

  // Test that it is not possible to write Trailers after a FIN has been sent.
  Initialize(kShouldProcessData);

  // Write the initial headers, with a FIN.
  EXPECT_CALL(*stream_, WriteHeadersMock(true));
  stream_->WriteHeaders(SpdyHeaderBlock(), /*fin=*/true, nullptr);
  EXPECT_TRUE(stream_->fin_sent());

  // Writing Trailers should fail, as the FIN has already been sent.
  // populated with the number of body bytes written.
  EXPECT_QUIC_BUG(stream_->WriteTrailers(SpdyHeaderBlock(), nullptr),
                  "Trailers cannot be sent after a FIN");
}

TEST_P(QuicSpdyStreamTest, HeaderStreamNotiferCorrespondingSpdyStream) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  testing::InSequence s;
  QuicReferenceCountedPointer<MockAckListener> ack_listener1(
      new MockAckListener());
  QuicReferenceCountedPointer<MockAckListener> ack_listener2(
      new MockAckListener());
  stream_->set_ack_listener(ack_listener1);
  stream2_->set_ack_listener(ack_listener2);

  session_->headers_stream()->WriteOrBufferData("Header1", false,
                                                ack_listener1);
  stream_->WriteOrBufferBody("Test1", true);

  session_->headers_stream()->WriteOrBufferData("Header2", false,
                                                ack_listener2);
  stream2_->WriteOrBufferBody("Test2", false);

  QuicStreamFrame frame1(
      QuicUtils::GetHeadersStreamId(connection_->transport_version()), false, 0,
      "Header1");
  QuicString header = "";
  if (HasFrameHeader()) {
    std::unique_ptr<char[]> buffer;
    QuicByteCount header_length = encoder_.SerializeDataFrameHeader(5, &buffer);
    header = QuicString(buffer.get(), header_length);
  }
  QuicStreamFrame frame2(stream_->id(), true, 0, header + "Test1");
  QuicStreamFrame frame3(
      QuicUtils::GetHeadersStreamId(connection_->transport_version()), false, 7,
      "Header2");
  QuicStreamFrame frame4(stream2_->id(), false, 0, header + "Test2");

  EXPECT_CALL(*ack_listener1, OnPacketRetransmitted(7));
  session_->OnStreamFrameRetransmitted(frame1);

  EXPECT_CALL(*ack_listener1, OnPacketAcked(7, _));
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame1), QuicTime::Delta::Zero()));
  EXPECT_CALL(*ack_listener1, OnPacketAcked(5, _));
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame2), QuicTime::Delta::Zero()));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(7, _));
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame3), QuicTime::Delta::Zero()));
  EXPECT_CALL(*ack_listener2, OnPacketAcked(5, _));
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame4), QuicTime::Delta::Zero()));
}

TEST_P(QuicSpdyStreamTest, StreamBecomesZombieWithWriteThatCloses) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  QuicStreamPeer::CloseReadSide(stream_);
  // This write causes stream to be closed.
  stream_->WriteOrBufferBody("Test1", true);
  // stream_ has unacked data and should become zombie.
  EXPECT_TRUE(QuicContainsKey(QuicSessionPeer::zombie_streams(session_.get()),
                              stream_->id()));
  EXPECT_TRUE(QuicSessionPeer::closed_streams(session_.get()).empty());
}

TEST_P(QuicSpdyStreamTest, OnPriorityFrame) {
  Initialize(kShouldProcessData);
  stream_->OnPriorityFrame(kV3HighestPriority);
  EXPECT_EQ(kV3HighestPriority, stream_->priority());
}

TEST_P(QuicSpdyStreamTest, OnPriorityFrameAfterSendingData) {
  testing::InSequence seq;
  Initialize(kShouldProcessData);

  if (HasFrameHeader()) {
    EXPECT_CALL(*session_, WritevData(_, _, 2, _, NO_FIN));
  }
  EXPECT_CALL(*session_, WritevData(_, _, 4, _, FIN));
  stream_->WriteOrBufferBody("data", true);
  stream_->OnPriorityFrame(kV3HighestPriority);
  EXPECT_EQ(kV3HighestPriority, stream_->priority());
}

TEST_P(QuicSpdyStreamTest, SetPriorityBeforeUpdateStreamPriority) {
  MockQuicConnection* connection = new StrictMock<MockQuicConnection>(
      &helper_, &alarm_factory_, Perspective::IS_SERVER,
      SupportedVersions(GetParam()));
  std::unique_ptr<TestMockUpdateStreamSession> session(
      new StrictMock<TestMockUpdateStreamSession>(connection));
  auto stream = new StrictMock<TestStream>(
      GetNthClientInitiatedBidirectionalStreamId(
          session->connection()->transport_version(), 0),
      session.get(),
      /*should_process_data=*/true);
  session->ActivateStream(QuicWrapUnique(stream));

  // QuicSpdyStream::SetPriority() should eventually call UpdateStreamPriority()
  // on the session. Make sure stream->priority() returns the updated priority
  // if called within UpdateStreamPriority(). This expectation is enforced in
  // TestMockUpdateStreamSession::UpdateStreamPriority().
  session->SetExpectedStream(stream);
  session->SetExpectedPriority(kV3HighestPriority);
  stream->SetPriority(kV3HighestPriority);

  session->SetExpectedPriority(kV3LowestPriority);
  stream->SetPriority(kV3LowestPriority);
}

TEST_P(QuicSpdyStreamTest, StreamWaitsForAcks) {
  Initialize(kShouldProcessData);
  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);
  stream_->set_ack_listener(mock_ack_listener);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  // Stream is not waiting for acks initially.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // Send kData1.
  stream_->WriteOrBufferData("FooAndBar", false, nullptr);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(9, _));
  QuicByteCount newly_acked_length = 0;
  EXPECT_TRUE(stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  // Stream is not waiting for acks as all sent data is acked.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // Send kData2.
  stream_->WriteOrBufferData("FooAndBar", false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Send FIN.
  stream_->WriteOrBufferData("", true, nullptr);
  // Fin only frame is not stored in send buffer.
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());

  // kData2 is retransmitted.
  EXPECT_CALL(*mock_ack_listener, OnPacketRetransmitted(9));
  stream_->OnStreamFrameRetransmitted(9, 9, false);

  // kData2 is acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(9, _));
  EXPECT_TRUE(stream_->OnStreamFrameAcked(9, 9, false, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  // Stream is waiting for acks as FIN is not acked.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // FIN is acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(0, _));
  EXPECT_TRUE(stream_->OnStreamFrameAcked(18, 0, true, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_P(QuicSpdyStreamTest, StreamDataGetAckedMultipleTimes) {
  Initialize(kShouldProcessData);
  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);
  stream_->set_ack_listener(mock_ack_listener);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  // Send [0, 27) and fin.
  stream_->WriteOrBufferData("FooAndBar", false, nullptr);
  stream_->WriteOrBufferData("FooAndBar", false, nullptr);
  stream_->WriteOrBufferData("FooAndBar", true, nullptr);

  // Ack [0, 9), [5, 22) and [18, 26)
  // Verify [0, 9) 9 bytes are acked.
  QuicByteCount newly_acked_length = 0;
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(9, _));
  EXPECT_TRUE(stream_->OnStreamFrameAcked(0, 9, false, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  EXPECT_EQ(2u, QuicStreamPeer::SendBuffer(stream_).size());
  // Verify [9, 22) 13 bytes are acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(13, _));
  EXPECT_TRUE(stream_->OnStreamFrameAcked(5, 17, false, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Verify [22, 26) 4 bytes are acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(4, _));
  EXPECT_TRUE(stream_->OnStreamFrameAcked(18, 8, false, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // Ack [0, 27).
  // Verify [26, 27) 1 byte is acked.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(1, _));
  EXPECT_TRUE(stream_->OnStreamFrameAcked(26, 1, false, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // Ack Fin. Verify OnPacketAcked is called.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(0, _));
  EXPECT_TRUE(stream_->OnStreamFrameAcked(27, 0, true, QuicTime::Delta::Zero(),
                                          &newly_acked_length));
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_FALSE(stream_->IsWaitingForAcks());

  // Ack [10, 27) and fin.
  // No new data is acked, verify OnPacketAcked is not called.
  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(_, _)).Times(0);
  EXPECT_FALSE(stream_->OnStreamFrameAcked(
      10, 17, true, QuicTime::Delta::Zero(), &newly_acked_length));
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_FALSE(stream_->IsWaitingForAcks());
}

// HTTP/3 only.
TEST_P(QuicSpdyStreamTest, HeadersAckNotReportedWriteOrBufferBody) {
  Initialize(kShouldProcessData);
  if (!HasFrameHeader()) {
    return;
  }
  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);
  stream_->set_ack_listener(mock_ack_listener);
  QuicString body = "Test1";
  QuicString body2(100, 'x');

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  stream_->WriteOrBufferBody(body, false);
  stream_->WriteOrBufferBody(body2, true);

  std::unique_ptr<char[]> buffer;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buffer);
  QuicString header = QuicString(buffer.get(), header_length);

  header_length = encoder_.SerializeDataFrameHeader(body2.length(), &buffer);
  QuicString header2 = QuicString(buffer.get(), header_length);

  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(body.length(), _));
  QuicStreamFrame frame(stream_->id(), false, 0, header + body);
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame), QuicTime::Delta::Zero()));

  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(0, _));
  QuicStreamFrame frame2(stream_->id(), false, (header + body).length(),
                         header2);
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame2), QuicTime::Delta::Zero()));

  EXPECT_CALL(*mock_ack_listener, OnPacketAcked(body2.length(), _));
  QuicStreamFrame frame3(stream_->id(), true,
                         (header + body).length() + header2.length(), body2);
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame3), QuicTime::Delta::Zero()));

  EXPECT_TRUE(
      QuicSpdyStreamPeer::unacked_frame_headers_offsets(stream_).Empty());
}

// HTTP/3 only.
TEST_P(QuicSpdyStreamTest, HeadersAckNotReportedWriteBodySlices) {
  Initialize(kShouldProcessData);
  if (!HasFrameHeader()) {
    return;
  }
  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);
  stream_->set_ack_listener(mock_ack_listener);
  QuicString body = "Test1";
  QuicString body2(100, 'x');
  struct iovec body1_iov = {const_cast<char*>(body.data()), body.length()};
  struct iovec body2_iov = {const_cast<char*>(body2.data()), body2.length()};
  QuicMemSliceStorage storage(&body1_iov, 1,
                              helper_.GetStreamSendBufferAllocator(), 1024);
  QuicMemSliceStorage storage2(&body2_iov, 1,
                               helper_.GetStreamSendBufferAllocator(), 1024);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  stream_->WriteBodySlices(storage.ToSpan(), false);
  stream_->WriteBodySlices(storage2.ToSpan(), true);

  std::unique_ptr<char[]> buffer;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buffer);
  QuicString header = QuicString(buffer.get(), header_length);

  header_length = encoder_.SerializeDataFrameHeader(body2.length(), &buffer);
  QuicString header2 = QuicString(buffer.get(), header_length);

  EXPECT_CALL(*mock_ack_listener,
              OnPacketAcked(body.length() + body2.length(), _));
  QuicStreamFrame frame(stream_->id(), true, 0,
                        header + body + header2 + body2);
  EXPECT_TRUE(
      session_->OnFrameAcked(QuicFrame(frame), QuicTime::Delta::Zero()));

  EXPECT_TRUE(
      QuicSpdyStreamPeer::unacked_frame_headers_offsets(stream_).Empty());
}

// HTTP/3 only.
TEST_P(QuicSpdyStreamTest, HeaderBytesNotReportedOnRetransmission) {
  Initialize(kShouldProcessData);
  if (!HasFrameHeader()) {
    return;
  }
  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);
  stream_->set_ack_listener(mock_ack_listener);
  QuicString body = "Test1";
  QuicString body2(100, 'x');

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _)).Times(AtLeast(1));
  stream_->WriteOrBufferBody(body, false);
  stream_->WriteOrBufferBody(body2, true);

  std::unique_ptr<char[]> buffer;
  QuicByteCount header_length =
      encoder_.SerializeDataFrameHeader(body.length(), &buffer);
  QuicString header = QuicString(buffer.get(), header_length);

  header_length = encoder_.SerializeDataFrameHeader(body2.length(), &buffer);
  QuicString header2 = QuicString(buffer.get(), header_length);

  EXPECT_CALL(*mock_ack_listener, OnPacketRetransmitted(body.length()));
  QuicStreamFrame frame(stream_->id(), false, 0, header + body);
  session_->OnStreamFrameRetransmitted(frame);

  EXPECT_CALL(*mock_ack_listener, OnPacketRetransmitted(body2.length()));
  QuicStreamFrame frame2(stream_->id(), true, (header + body).length(),
                         header2 + body2);
  session_->OnStreamFrameRetransmitted(frame2);

  EXPECT_FALSE(
      QuicSpdyStreamPeer::unacked_frame_headers_offsets(stream_).Empty());
}

}  // namespace
}  // namespace test
}  // namespace quic
