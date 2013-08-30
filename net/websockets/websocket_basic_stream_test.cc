// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Tests for WebSocketBasicStream. Note that we do not attempt to verify that
// frame parsing itself functions correctly, as that is covered by the
// WebSocketFrameParser tests.

#include "net/websockets/websocket_basic_stream.h"

#include "base/basictypes.h"
#include "base/port.h"
#include "net/base/capturing_net_log.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

// TODO(ricea): Add tests for
// - Empty frames (data & control)
// - Non-NULL masking key
// - A frame larger than kReadBufferSize;

const char kSampleFrame[] = "\x81\x06Sample";
const size_t kSampleFrameSize = arraysize(kSampleFrame) - 1;
const char kPartialLargeFrame[] =
    "\x81\x7F\x00\x00\x00\x00\x7F\xFF\xFF\xFF"
    "chromiunum ad pasco per loca insanis pullum manducat frumenti";
const size_t kPartialLargeFrameSize = arraysize(kPartialLargeFrame) - 1;
const size_t kLargeFrameHeaderSize = 10;
const size_t kLargeFrameDeclaredPayloadSize = 0x7FFFFFFF;
const char kMultipleFrames[] = "\x81\x01X\x81\x01Y\x81\x01Z";
const size_t kMultipleFramesSize = arraysize(kMultipleFrames) - 1;
// This frame encodes a payload length of 7 in two bytes, which is always
// invalid.
const char kInvalidFrame[] = "\x81\x7E\x00\x07Invalid";
const size_t kInvalidFrameSize = arraysize(kInvalidFrame) - 1;
const char kWriteFrame[] = "\x81\x85\x00\x00\x00\x00Write";
const size_t kWriteFrameSize = arraysize(kWriteFrame) - 1;
const WebSocketMaskingKey kNulMaskingKey = {{'\0', '\0', '\0', '\0'}};

// Generates a ScopedVector<WebSocketFrameChunk> which will have a wire format
// matching kWriteFrame.
ScopedVector<WebSocketFrameChunk> GenerateWriteFrame() {
  scoped_ptr<WebSocketFrameChunk> chunk(new WebSocketFrameChunk);
  const size_t payload_size =
      kWriteFrameSize - (WebSocketFrameHeader::kBaseHeaderSize +
                         WebSocketFrameHeader::kMaskingKeyLength);
  chunk->data = new IOBufferWithSize(payload_size);
  memcpy(chunk->data->data(),
         kWriteFrame + kWriteFrameSize - payload_size,
         payload_size);
  chunk->final_chunk = true;
  scoped_ptr<WebSocketFrameHeader> header(
      new WebSocketFrameHeader(WebSocketFrameHeader::kOpCodeText));
  header->final = true;
  header->masked = true;
  header->payload_length = payload_size;
  chunk->header = header.Pass();
  ScopedVector<WebSocketFrameChunk> chunks;
  chunks.push_back(chunk.release());
  return chunks.Pass();
}

// A masking key generator function which generates the identity mask,
// ie. "\0\0\0\0".
WebSocketMaskingKey GenerateNulMaskingKey() { return kNulMaskingKey; }

// Base class for WebSocketBasicStream test fixtures.
class WebSocketBasicStreamTest : public ::testing::Test {
 protected:
  scoped_ptr<WebSocketBasicStream> stream_;
  CapturingNetLog net_log_;
};

// A fixture for tests which only perform normal socket operations.
class WebSocketBasicStreamSocketTest : public WebSocketBasicStreamTest {
 protected:
  WebSocketBasicStreamSocketTest()
      : histograms_("a"), pool_(1, 1, &histograms_, &factory_) {}

  virtual ~WebSocketBasicStreamSocketTest() {
    // stream_ has a reference to socket_data_ (via MockTCPClientSocket) and so
    // should be destroyed first.
    stream_.reset();
  }

  scoped_ptr<ClientSocketHandle> MakeTransportSocket(MockRead reads[],
                                                     size_t reads_count,
                                                     MockWrite writes[],
                                                     size_t writes_count) {
    socket_data_.reset(
        new StaticSocketDataProvider(reads, reads_count, writes, writes_count));
    socket_data_->set_connect_data(MockConnect(SYNCHRONOUS, OK));
    factory_.AddSocketDataProvider(socket_data_.get());

    scoped_ptr<ClientSocketHandle> transport_socket(new ClientSocketHandle);
    scoped_refptr<MockTransportSocketParams> params;
    transport_socket->Init("a",
                           params,
                           MEDIUM,
                           CompletionCallback(),
                           &pool_,
                           bound_net_log_.bound());
    return transport_socket.Pass();
  }

  void SetHttpReadBuffer(const char* data, size_t size) {
    http_read_buffer_ = new GrowableIOBuffer;
    http_read_buffer_->SetCapacity(size);
    memcpy(http_read_buffer_->data(), data, size);
    http_read_buffer_->set_offset(size);
  }

  void CreateStream(MockRead reads[],
                    size_t reads_count,
                    MockWrite writes[],
                    size_t writes_count) {
    stream_ = WebSocketBasicStream::CreateWebSocketBasicStreamForTesting(
        MakeTransportSocket(reads, reads_count, writes, writes_count),
        http_read_buffer_,
        sub_protocol_,
        extensions_,
        &GenerateNulMaskingKey);
  }

  template <size_t N>
  void CreateReadOnly(MockRead (&reads)[N]) {
    CreateStream(reads, N, NULL, 0);
  }

  template <size_t N>
  void CreateWriteOnly(MockWrite (&writes)[N]) {
    CreateStream(NULL, 0, writes, N);
  }

  void CreateNullStream() { CreateStream(NULL, 0, NULL, 0); }

  scoped_ptr<SocketDataProvider> socket_data_;
  MockClientSocketFactory factory_;
  ClientSocketPoolHistograms histograms_;
  MockTransportClientSocketPool pool_;
  CapturingBoundNetLog(bound_net_log_);
  ScopedVector<WebSocketFrameChunk> frame_chunks_;
  TestCompletionCallback cb_;
  scoped_refptr<GrowableIOBuffer> http_read_buffer_;
  std::string sub_protocol_;
  std::string extensions_;
};

TEST_F(WebSocketBasicStreamSocketTest, ConstructionWorks) {
  CreateNullStream();
}

TEST_F(WebSocketBasicStreamSocketTest, SyncReadWorks) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, kSampleFrame, kSampleFrameSize)};
  CreateReadOnly(reads);
  int result = stream_->ReadFrames(&frame_chunks_, cb_.callback());
  EXPECT_EQ(OK, result);
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->header);
  EXPECT_EQ(GG_UINT64_C(6), frame_chunks_[0]->header->payload_length);
  EXPECT_TRUE(frame_chunks_[0]->header->final);
  EXPECT_TRUE(frame_chunks_[0]->final_chunk);
}

TEST_F(WebSocketBasicStreamSocketTest, AsyncReadWorks) {
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame, kSampleFrameSize)};
  CreateReadOnly(reads);
  int result = stream_->ReadFrames(&frame_chunks_, cb_.callback());
  ASSERT_EQ(ERR_IO_PENDING, result);
  EXPECT_EQ(OK, cb_.WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->header);
  EXPECT_EQ(GG_UINT64_C(6), frame_chunks_[0]->header->payload_length);
  // Don't repeat all the tests from SyncReadWorks; just enough to be sure the
  // frame was really read.
}

// ReadFrames will not return a frame whose header has not been wholly received.
TEST_F(WebSocketBasicStreamSocketTest, HeaderFragmentedSync) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kSampleFrame, 1),
      MockRead(SYNCHRONOUS, kSampleFrame + 1, kSampleFrameSize - 1)};
  CreateReadOnly(reads);
  int result = stream_->ReadFrames(&frame_chunks_, cb_.callback());
  ASSERT_EQ(OK, result);
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->header);
  EXPECT_EQ(GG_UINT64_C(6), frame_chunks_[0]->header->payload_length);
}

// The same behaviour applies to asynchronous reads.
TEST_F(WebSocketBasicStreamSocketTest, HeaderFragmentedAsync) {
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame, 1),
                      MockRead(ASYNC, kSampleFrame + 1, kSampleFrameSize - 1)};
  CreateReadOnly(reads);
  int result = stream_->ReadFrames(&frame_chunks_, cb_.callback());
  ASSERT_EQ(ERR_IO_PENDING, result);
  EXPECT_EQ(OK, cb_.WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->header);
  EXPECT_EQ(GG_UINT64_C(6), frame_chunks_[0]->header->payload_length);
}

// If it receives an incomplete header in a synchronous call, then has to wait
// for the rest of the frame, ReadFrames will return ERR_IO_PENDING.
TEST_F(WebSocketBasicStreamSocketTest, HeaderFragmentedSyncAsync) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, kSampleFrame, 1),
                      MockRead(ASYNC, kSampleFrame + 1, kSampleFrameSize - 1)};
  CreateReadOnly(reads);
  int result = stream_->ReadFrames(&frame_chunks_, cb_.callback());
  ASSERT_EQ(ERR_IO_PENDING, result);
  EXPECT_EQ(OK, cb_.WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->header);
  EXPECT_EQ(GG_UINT64_C(6), frame_chunks_[0]->header->payload_length);
}

// An extended header should also return ERR_IO_PENDING if it is not completely
// received.
TEST_F(WebSocketBasicStreamSocketTest, FragmentedLargeHeader) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kPartialLargeFrame, kLargeFrameHeaderSize - 1),
      MockRead(SYNCHRONOUS, ERR_IO_PENDING)};
  CreateReadOnly(reads);
  EXPECT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
}

// A frame that does not arrive in a single read should arrive in chunks.
TEST_F(WebSocketBasicStreamSocketTest, LargeFrameFirstChunk) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kPartialLargeFrame, kPartialLargeFrameSize)};
  CreateReadOnly(reads);
  EXPECT_EQ(OK, stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->header);
  EXPECT_EQ(kLargeFrameDeclaredPayloadSize,
            frame_chunks_[0]->header->payload_length);
  EXPECT_TRUE(frame_chunks_[0]->header->final);
  EXPECT_FALSE(frame_chunks_[0]->final_chunk);
  EXPECT_EQ(kPartialLargeFrameSize - kLargeFrameHeaderSize,
            static_cast<size_t>(frame_chunks_[0]->data->size()));
}

// If only the header arrives, we should get a zero-byte chunk.
TEST_F(WebSocketBasicStreamSocketTest, HeaderOnlyChunk) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kPartialLargeFrame, kLargeFrameHeaderSize)};
  CreateReadOnly(reads);
  EXPECT_EQ(OK, stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  ASSERT_EQ(1U, frame_chunks_.size());
  EXPECT_FALSE(frame_chunks_[0]->final_chunk);
  EXPECT_TRUE(frame_chunks_[0]->data.get() == NULL);
}

// The second and subsequent chunks of a frame have no header.
TEST_F(WebSocketBasicStreamSocketTest, LargeFrameTwoChunks) {
  static const size_t kChunkSize = 16;
  MockRead reads[] = {
      MockRead(ASYNC, kPartialLargeFrame, kChunkSize),
      MockRead(ASYNC, kPartialLargeFrame + kChunkSize, kChunkSize)};
  CreateReadOnly(reads);
  TestCompletionCallback cb[2];

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb[0].callback()));
  EXPECT_EQ(OK, cb[0].WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->header);

  frame_chunks_.clear();
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb[1].callback()));
  EXPECT_EQ(OK, cb[1].WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_FALSE(frame_chunks_[0]->header);
}

// Only the final chunk of a frame has final_chunk set.
TEST_F(WebSocketBasicStreamSocketTest, OnlyFinalChunkIsFinal) {
  static const size_t kFirstChunkSize = 4;
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame, kFirstChunkSize),
                      MockRead(ASYNC,
                               kSampleFrame + kFirstChunkSize,
                               kSampleFrameSize - kFirstChunkSize)};
  CreateReadOnly(reads);
  TestCompletionCallback cb[2];

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb[0].callback()));
  EXPECT_EQ(OK, cb[0].WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_FALSE(frame_chunks_[0]->final_chunk);

  frame_chunks_.clear();
  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb[1].callback()));
  EXPECT_EQ(OK, cb[1].WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->final_chunk);
}

// Multiple frames that arrive together should be parsed correctly.
TEST_F(WebSocketBasicStreamSocketTest, ThreeFramesTogether) {
  MockRead reads[] = {
      MockRead(SYNCHRONOUS, kMultipleFrames, kMultipleFramesSize)};
  CreateReadOnly(reads);

  ASSERT_EQ(OK, stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  ASSERT_EQ(3U, frame_chunks_.size());
  EXPECT_TRUE(frame_chunks_[0]->final_chunk);
  EXPECT_TRUE(frame_chunks_[1]->final_chunk);
  EXPECT_TRUE(frame_chunks_[2]->final_chunk);
}

// ERR_CONNECTION_CLOSED must be returned on close.
TEST_F(WebSocketBasicStreamSocketTest, SyncClose) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, "", 0)};
  CreateReadOnly(reads);

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketTest, AsyncClose) {
  MockRead reads[] = {MockRead(ASYNC, "", 0)};
  CreateReadOnly(reads);

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(ERR_CONNECTION_CLOSED, cb_.WaitForResult());
}

// The result should be the same if the socket returns
// ERR_CONNECTION_CLOSED. This is not expected to happen on an established
// connection; a Read of size 0 is the expected behaviour. The key point of this
// test is to confirm that ReadFrames() behaviour is identical in both cases.
TEST_F(WebSocketBasicStreamSocketTest, SyncCloseWithErr) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED)};
  CreateReadOnly(reads);

  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketTest, AsyncCloseWithErr) {
  MockRead reads[] = {MockRead(ASYNC, ERR_CONNECTION_CLOSED)};
  CreateReadOnly(reads);

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(ERR_CONNECTION_CLOSED, cb_.WaitForResult());
}

TEST_F(WebSocketBasicStreamSocketTest, SyncErrorsPassedThrough) {
  // ERR_INSUFFICIENT_RESOURCES here represents an arbitrary error that
  // WebSocketBasicStream gives no special handling to.
  MockRead reads[] = {MockRead(SYNCHRONOUS, ERR_INSUFFICIENT_RESOURCES)};
  CreateReadOnly(reads);

  EXPECT_EQ(ERR_INSUFFICIENT_RESOURCES,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketTest, AsyncErrorsPassedThrough) {
  MockRead reads[] = {MockRead(ASYNC, ERR_INSUFFICIENT_RESOURCES)};
  CreateReadOnly(reads);

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(ERR_INSUFFICIENT_RESOURCES, cb_.WaitForResult());
}

// If we get a frame followed by a close, we should receive them separately.
TEST_F(WebSocketBasicStreamSocketTest, CloseAfterFrame) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, kSampleFrame, kSampleFrameSize),
                      MockRead(SYNCHRONOUS, "", 0)};
  CreateReadOnly(reads);

  EXPECT_EQ(OK, stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(1U, frame_chunks_.size());
  frame_chunks_.clear();
  EXPECT_EQ(ERR_CONNECTION_CLOSED,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
}

// Synchronous close after an async frame header is handled by a different code
// path.
TEST_F(WebSocketBasicStreamSocketTest, AsyncCloseAfterIncompleteHeader) {
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame, 1U),
                      MockRead(SYNCHRONOUS, "", 0)};
  CreateReadOnly(reads);

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  ASSERT_EQ(ERR_CONNECTION_CLOSED, cb_.WaitForResult());
}

// When Stream::Read returns ERR_CONNECTION_CLOSED we get the same result via a
// slightly different code path.
TEST_F(WebSocketBasicStreamSocketTest, AsyncErrCloseAfterIncompleteHeader) {
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame, 1U),
                      MockRead(SYNCHRONOUS, ERR_CONNECTION_CLOSED)};
  CreateReadOnly(reads);

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  ASSERT_EQ(ERR_CONNECTION_CLOSED, cb_.WaitForResult());
}

// If there was a frame read at the same time as the response headers (and the
// handshake succeeded), then we should parse it.
TEST_F(WebSocketBasicStreamSocketTest, HttpReadBufferIsUsed) {
  SetHttpReadBuffer(kSampleFrame, kSampleFrameSize);
  CreateNullStream();

  EXPECT_EQ(OK, stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->data);
  EXPECT_EQ(6, frame_chunks_[0]->data->size());
}

// Check that a frame whose header partially arrived at the end of the response
// headers works correctly.
TEST_F(WebSocketBasicStreamSocketTest, PartialFrameHeaderInHttpResponse) {
  SetHttpReadBuffer(kSampleFrame, 1);
  MockRead reads[] = {MockRead(ASYNC, kSampleFrame + 1, kSampleFrameSize - 1)};
  CreateReadOnly(reads);

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(OK, cb_.WaitForResult());
  ASSERT_EQ(1U, frame_chunks_.size());
  ASSERT_TRUE(frame_chunks_[0]->data);
  EXPECT_EQ(6, frame_chunks_[0]->data->size());
  ASSERT_TRUE(frame_chunks_[0]->header);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText,
            frame_chunks_[0]->header->opcode);
}

// Check that an invalid frame results in an error.
TEST_F(WebSocketBasicStreamSocketTest, SyncInvalidFrame) {
  MockRead reads[] = {MockRead(SYNCHRONOUS, kInvalidFrame, kInvalidFrameSize)};
  CreateReadOnly(reads);

  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
}

TEST_F(WebSocketBasicStreamSocketTest, AsyncInvalidFrame) {
  MockRead reads[] = {MockRead(ASYNC, kInvalidFrame, kInvalidFrameSize)};
  CreateReadOnly(reads);

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->ReadFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(ERR_WS_PROTOCOL_ERROR, cb_.WaitForResult());
}

// Check that writing a frame all at once works.
TEST_F(WebSocketBasicStreamSocketTest, WriteAtOnce) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, kWriteFrame, kWriteFrameSize)};
  CreateWriteOnly(writes);
  frame_chunks_ = GenerateWriteFrame();

  EXPECT_EQ(OK, stream_->WriteFrames(&frame_chunks_, cb_.callback()));
}

// Check that completely async writing works.
TEST_F(WebSocketBasicStreamSocketTest, AsyncWriteAtOnce) {
  MockWrite writes[] = {MockWrite(ASYNC, kWriteFrame, kWriteFrameSize)};
  CreateWriteOnly(writes);
  frame_chunks_ = GenerateWriteFrame();

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->WriteFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(OK, cb_.WaitForResult());
}

// Check that writing a frame to an extremely full kernel buffer (so that it
// ends up being sent in bits) works. The WriteFrames() callback should not be
// called until all parts have been written.
TEST_F(WebSocketBasicStreamSocketTest, WriteInBits) {
  MockWrite writes[] = {MockWrite(SYNCHRONOUS, kWriteFrame, 4),
                        MockWrite(ASYNC, kWriteFrame + 4, 4),
                        MockWrite(ASYNC, kWriteFrame + 8, kWriteFrameSize - 8)};
  CreateWriteOnly(writes);
  frame_chunks_ = GenerateWriteFrame();

  ASSERT_EQ(ERR_IO_PENDING,
            stream_->WriteFrames(&frame_chunks_, cb_.callback()));
  EXPECT_EQ(OK, cb_.WaitForResult());
}

TEST_F(WebSocketBasicStreamSocketTest, GetExtensionsWorks) {
  extensions_ = "inflate-uuencode";
  CreateNullStream();

  EXPECT_EQ("inflate-uuencode", stream_->GetExtensions());
}

TEST_F(WebSocketBasicStreamSocketTest, GetSubProtocolWorks) {
  sub_protocol_ = "cyberchat";
  CreateNullStream();

  EXPECT_EQ("cyberchat", stream_->GetSubProtocol());
}

}  // namespace
}  // namespace net
