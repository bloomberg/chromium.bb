// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_deflate_stream.h"

#include <stdint.h>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/websockets/websocket_deflater.h"
#include "net/websockets/websocket_frame.h"
#include "net/websockets/websocket_inflater.h"
#include "net/websockets/websocket_stream.h"
#include "net/websockets/websocket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

typedef ::testing::MockFunction<void(int)> MockCallback;  // NOLINT
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

typedef uint32_t FrameFlag;
const FrameFlag kNoFlag = 0;
const FrameFlag kFinal = 1;
const FrameFlag kReserved1 = 2;
// We don't define values for other flags because we don't need them.

// The value must equal to the value of the corresponding
// constant in websocket_deflate_stream.cc
const size_t kChunkSize = 4 * 1024;
const int kWindowBits = 15;

scoped_refptr<IOBuffer> ToIOBuffer(const std::string& s) {
  scoped_refptr<IOBuffer> buffer = new IOBuffer(s.size());
  memcpy(buffer->data(), s.data(), s.size());
  return buffer;
}

std::string ToString(IOBufferWithSize* buffer) {
  return std::string(buffer->data(), buffer->size());
}

std::string ToString(const scoped_refptr<IOBufferWithSize>& buffer) {
  return ToString(buffer.get());
}

std::string ToString(IOBuffer* buffer, size_t size) {
  return std::string(buffer->data(), size);
}

std::string ToString(const scoped_refptr<IOBuffer>& buffer, size_t size) {
  return ToString(buffer.get(), size);
}

std::string ToString(WebSocketFrame* frame) {
  return frame->data ? ToString(frame->data, frame->header.payload_length) : "";
}

void AppendTo(ScopedVector<WebSocketFrame>* frames,
              WebSocketFrameHeader::OpCode opcode,
              FrameFlag flag,
              const std::string& data) {
  scoped_ptr<WebSocketFrame> frame(new WebSocketFrame(opcode));
  frame->header.final = (flag & kFinal);
  frame->header.reserved1 = (flag & kReserved1);
  frame->data = ToIOBuffer(data);
  frame->header.payload_length = data.size();
  frames->push_back(frame.release());
}

void AppendTo(ScopedVector<WebSocketFrame>* frames,
              WebSocketFrameHeader::OpCode opcode,
              FrameFlag flag) {
  scoped_ptr<WebSocketFrame> frame(new WebSocketFrame(opcode));
  frame->header.final = (flag & kFinal);
  frame->header.reserved1 = (flag & kReserved1);
  frames->push_back(frame.release());
}

class MockWebSocketStream : public WebSocketStream {
 public:
  MOCK_METHOD2(ReadFrames, int(ScopedVector<WebSocketFrame>*,
                               const CompletionCallback&));
  MOCK_METHOD2(WriteFrames, int(ScopedVector<WebSocketFrame>*,
                                const CompletionCallback&));
  MOCK_METHOD0(Close, void());
  MOCK_CONST_METHOD0(GetSubProtocol, std::string());
  MOCK_CONST_METHOD0(GetExtensions, std::string());
};

class WebSocketDeflateStreamTest : public ::testing::Test {
 public:
  WebSocketDeflateStreamTest()
      : mock_stream_(NULL) {
    mock_stream_ = new testing::StrictMock<MockWebSocketStream>;
    deflate_stream_.reset(new WebSocketDeflateStream(
        scoped_ptr<WebSocketStream>(mock_stream_)));
  }
  virtual ~WebSocketDeflateStreamTest() {}

 protected:
  scoped_ptr<WebSocketDeflateStream> deflate_stream_;
  // |mock_stream_| will be deleted when |deflate_stream_| is destroyed.
  MockWebSocketStream* mock_stream_;
};

// ReadFrameStub is a stub for WebSocketStream::ReadFrames.
// It returns |result_| and |frames_to_output_| to the caller and
// saves parameters to |frames_passed_| and |callback_|.
class ReadFramesStub {
 public:
  explicit ReadFramesStub(int result) : result_(result) {}

  ReadFramesStub(int result, ScopedVector<WebSocketFrame>* frames_to_output)
      : result_(result) {
    frames_to_output_.swap(*frames_to_output);
  }

  int Call(ScopedVector<WebSocketFrame>* frames,
           const CompletionCallback& callback) {
    DCHECK(frames->empty());
    frames_passed_ = frames;
    callback_ = callback;
    frames->swap(frames_to_output_);
    return result_;
  }

  int result() const { return result_; }
  const CompletionCallback callback() const { return callback_; }
  ScopedVector<WebSocketFrame>* frames_passed() {
    return frames_passed_;
  }

 private:
  int result_;
  CompletionCallback callback_;
  ScopedVector<WebSocketFrame> frames_to_output_;
  ScopedVector<WebSocketFrame>* frames_passed_;
};

// WriteFrameStub is a stub for WebSocketStream::WriteFrames.
// It returns |result_| and |frames_| to the caller and
// saves |callback| parameter to |callback_|.
class WriteFramesStub {
 public:
  explicit WriteFramesStub(int result) : result_(result) {}

  int Call(ScopedVector<WebSocketFrame>* frames,
           const CompletionCallback& callback) {
    for (size_t i = 0; i < frames->size(); ++i) {
      frames_.push_back((*frames)[i]);
    }
    frames->weak_clear();
    callback_ = callback;
    return result_;
  }

  int result() const { return result_; }
  const CompletionCallback callback() const { return callback_; }
  ScopedVector<WebSocketFrame>* frames() { return &frames_; }

 private:
  int result_;
  CompletionCallback callback_;
  ScopedVector<WebSocketFrame> frames_;
};

TEST_F(WebSocketDeflateStreamTest, ReadFailedImmediately) {
  ScopedVector<WebSocketFrame> frames;
  CompletionCallback callback;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Return(ERR_FAILED));
  }
  EXPECT_EQ(ERR_FAILED, deflate_stream_->ReadFrames(&frames, callback));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedFrameImmediately) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "hello");
  ReadFramesStub stub(OK, &frames_to_output);
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  CompletionCallback callback;
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedFrameAsync) {
  ReadFramesStub stub(ERR_IO_PENDING);
  ScopedVector<WebSocketFrame> frames;
  MockCallback mock_callback, checkpoint;
  CompletionCallback callback =
      base::Bind(&MockCallback::Call, base::Unretained(&mock_callback));

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(mock_callback, Call(OK));
  }
  ASSERT_EQ(ERR_IO_PENDING, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(0u, frames.size());

  checkpoint.Call(0);

  AppendTo(stub.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "hello");
  stub.callback().Run(OK);
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadFailedAsync) {
  ReadFramesStub stub(ERR_IO_PENDING);
  ScopedVector<WebSocketFrame> frames;
  MockCallback mock_callback, checkpoint;
  CompletionCallback callback =
      base::Bind(&MockCallback::Call, base::Unretained(&mock_callback));

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(mock_callback, Call(ERR_FAILED));
  }
  ASSERT_EQ(ERR_IO_PENDING, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(0u, frames.size());

  checkpoint.Call(0);

  AppendTo(stub.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "hello");
  stub.callback().Run(ERR_FAILED);
  ASSERT_EQ(0u, frames.size());
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedFrameImmediately) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedFrameAsync) {
  ReadFramesStub stub(ERR_IO_PENDING);
  MockCallback mock_callback, checkpoint;
  CompletionCallback callback =
      base::Bind(&MockCallback::Call, base::Unretained(&mock_callback));
  ScopedVector<WebSocketFrame> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(mock_callback, Call(OK));
  }
  ASSERT_EQ(ERR_IO_PENDING, deflate_stream_->ReadFrames(&frames, callback));

  checkpoint.Call(0);

  AppendTo(stub.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7));
  stub.callback().Run(OK);

  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadCompressedFrameFragmentImmediatelyButInflaterReturnsPending) {
  ScopedVector<WebSocketFrame> frames_to_output;
  const std::string data1("\xf2", 1);
  const std::string data2("\x48\xcd\xc9\xc9\x07\x00", 6);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  ReadFramesStub stub1(OK, &frames_to_output), stub2(ERR_IO_PENDING);
  MockCallback mock_callback, checkpoint;
  CompletionCallback callback =
      base::Bind(&MockCallback::Call, base::Unretained(&mock_callback));
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub1, &ReadFramesStub::Call))
        .WillOnce(Invoke(&stub2, &ReadFramesStub::Call));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(mock_callback, Call(OK));
  }
  ASSERT_EQ(ERR_IO_PENDING, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(0u, frames.size());

  AppendTo(stub2.frames_passed(),
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           data2);

  checkpoint.Call(0);
  stub2.callback().Run(OK);

  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadInvalidCompressedPayload) {
  const std::string data("\xf2\x48\xcdINVALID", 10);
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           data);
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(ERR_WS_PROTOCOL_ERROR,
            deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(0u, frames.size());
}

TEST_F(WebSocketDeflateStreamTest, MergeMultipleFramesInReadFrames) {
  const std::string data1("\xf2\x48\xcd", 3);
  const std::string data2("\xc9\xc9\x07\x00", 4);
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal,
           data2);
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedEmptyFrames) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kNoFlag);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal);
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_FALSE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedEmptyFrames) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           std::string("\x02\x00", 1));
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal);
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadCompressedFrameFollowedByEmptyFrame) {
  const std::string data("\xf2\x48\xcd\xc9\xc9\x07\x00", 7);
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal);
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(1u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[0]));
}

TEST_F(WebSocketDeflateStreamTest, ReadControlFrameBetweenDataFrames) {
  const std::string data1("\xf2\x48\xcd", 3);
  const std::string data2("\xc9\xc9\x07\x00", 4);
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  AppendTo(&frames_to_output, WebSocketFrameHeader::kOpCodePing, kFinal);
  AppendTo(&frames_to_output, WebSocketFrameHeader::kOpCodeText, kFinal, data2);
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodePing, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("Hello", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest, SplitToMultipleFramesInReadFrames) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  deflater.Initialize(kWindowBits);
  const size_t kSize = kChunkSize * 3;
  const std::string original_data(kSize, 'a');
  deflater.AddBytes(original_data.data(), original_data.size());
  deflater.Finish();

  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeBinary,
           kFinal | kReserved1,
           ToString(deflater.GetOutput(deflater.CurrentOutputSize())));

  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }

  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(3u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeBinary, frames[0]->header.opcode);
  EXPECT_FALSE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ(kChunkSize, static_cast<size_t>(frames[0]->header.payload_length));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[1]->header.opcode);
  EXPECT_FALSE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ(kChunkSize, static_cast<size_t>(frames[1]->header.payload_length));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeContinuation,
            frames[2]->header.opcode);
  EXPECT_TRUE(frames[2]->header.final);
  EXPECT_FALSE(frames[2]->header.reserved1);
  EXPECT_EQ(kChunkSize, static_cast<size_t>(frames[2]->header.payload_length));
  EXPECT_EQ(original_data,
            ToString(frames[0]) + ToString(frames[1]) + ToString(frames[2]));
}

TEST_F(WebSocketDeflateStreamTest,
       Reserved1TurnsOnDuringReadingCompressedContinuationFrame) {
  const std::string data1("\xf2\x48\xcd", 3);
  const std::string data2("\xc9\xc9\x07\x00", 4);
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kReserved1,
           data1);
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal | kReserved1,
           data2);
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(ERR_WS_PROTOCOL_ERROR,
            deflate_stream_->ReadFrames(&frames, callback));
}

TEST_F(WebSocketDeflateStreamTest,
       Reserved1TurnsOnDuringReadingUncompressedContinuationFrame) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kNoFlag,
           "hello");
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeContinuation,
           kFinal | kReserved1,
           "world");
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(ERR_WS_PROTOCOL_ERROR,
            deflate_stream_->ReadFrames(&frames, callback));
}

TEST_F(WebSocketDeflateStreamTest, ReadCompressedMessages) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string(
               "\x4a\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x31\x04\x00", 13));
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string("\x4a\x86\x33\x8d\x00\x00", 6));
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("compressed1", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("compressed2", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest, ReadUncompressedMessages) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed1");
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed2");
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("uncompressed1", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("uncompressed2", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadCompressedMessageThenUncompressedMessage) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string(
               "\x4a\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x01\x00", 12));
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed");
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("compressed", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("uncompressed", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest,
       ReadUncompressedMessageThenCompressedMessage) {
  ScopedVector<WebSocketFrame> frames_to_output;
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal,
           "uncompressed");
  AppendTo(&frames_to_output,
           WebSocketFrameHeader::kOpCodeText,
           kFinal | kReserved1,
           std::string(
               "\x4a\xce\xcf\x2d\x28\x4a\x2d\x2e\x4e\x4d\x01\x00", 12));
  ReadFramesStub stub(OK, &frames_to_output);
  CompletionCallback callback;
  ScopedVector<WebSocketFrame> frames;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, ReadFrames(&frames, _))
        .WillOnce(Invoke(&stub, &ReadFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->ReadFrames(&frames, callback));
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[0]->header.opcode);
  EXPECT_TRUE(frames[0]->header.final);
  EXPECT_FALSE(frames[0]->header.reserved1);
  EXPECT_EQ("uncompressed", ToString(frames[0]));
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames[1]->header.opcode);
  EXPECT_TRUE(frames[1]->header.final);
  EXPECT_FALSE(frames[1]->header.reserved1);
  EXPECT_EQ("compressed", ToString(frames[1]));
}

TEST_F(WebSocketDeflateStreamTest, WriteEmpty) {
  ScopedVector<WebSocketFrame> frames;
  CompletionCallback callback;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFrames(&frames, _)).Times(0);
  }
  EXPECT_EQ(OK, deflate_stream_->WriteFrames(&frames, callback));
}

TEST_F(WebSocketDeflateStreamTest, WriteFailedImmediately) {
  ScopedVector<WebSocketFrame> frames;
  CompletionCallback callback;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFrames(&frames, _))
        .WillOnce(Return(ERR_FAILED));
  }

  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "hello");
  EXPECT_EQ(ERR_FAILED, deflate_stream_->WriteFrames(&frames, callback));
}

TEST_F(WebSocketDeflateStreamTest, WriteFrameImmediately) {
  ScopedVector<WebSocketFrame> frames;
  CompletionCallback callback;
  WriteFramesStub stub(OK);
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFrames(_, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->WriteFrames(&frames, callback));
  const ScopedVector<WebSocketFrame>& frames_passed = *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[0]));
}

TEST_F(WebSocketDeflateStreamTest, WriteFrameAsync) {
  WriteFramesStub stub(ERR_IO_PENDING);
  MockCallback mock_callback, checkpoint;
  CompletionCallback callback =
      base::Bind(&MockCallback::Call, base::Unretained(&mock_callback));
  ScopedVector<WebSocketFrame> frames;
  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFrames(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
    EXPECT_CALL(checkpoint, Call(0));
    EXPECT_CALL(mock_callback, Call(OK));
  }
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal, "Hello");
  ASSERT_EQ(ERR_IO_PENDING, deflate_stream_->WriteFrames(&frames, callback));

  checkpoint.Call(0);
  stub.callback().Run(OK);

  const ScopedVector<WebSocketFrame>& frames_passed = *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[0]));
}

TEST_F(WebSocketDeflateStreamTest, WriteControlFrameBetweenDataFrames) {
  ScopedVector<WebSocketFrame> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kNoFlag, "Hel");
  AppendTo(&frames, WebSocketFrameHeader::kOpCodePing, kFinal);
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeContinuation, kFinal, "lo");
  WriteFramesStub stub(OK);
  CompletionCallback callback;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFrames(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->WriteFrames(&frames, callback));
  const ScopedVector<WebSocketFrame>& frames_passed = *stub.frames();
  ASSERT_EQ(2u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodePing, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_FALSE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[1]->header.opcode);
  EXPECT_TRUE(frames_passed[1]->header.final);
  EXPECT_TRUE(frames_passed[1]->header.reserved1);
  EXPECT_EQ(std::string("\xf2\x48\xcd\xc9\xc9\x07\x00", 7),
            ToString(frames_passed[1]));
}

TEST_F(WebSocketDeflateStreamTest, WriteEmptyMessage) {
  ScopedVector<WebSocketFrame> frames;
  AppendTo(&frames, WebSocketFrameHeader::kOpCodeText, kFinal);
  WriteFramesStub stub(OK);
  CompletionCallback callback;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFrames(&frames, _))
        .WillOnce(Invoke(&stub, &WriteFramesStub::Call));
  }
  ASSERT_EQ(OK, deflate_stream_->WriteFrames(&frames, callback));
  const ScopedVector<WebSocketFrame>& frames_passed = *stub.frames();
  ASSERT_EQ(1u, frames_passed.size());
  EXPECT_EQ(WebSocketFrameHeader::kOpCodeText, frames_passed[0]->header.opcode);
  EXPECT_TRUE(frames_passed[0]->header.final);
  EXPECT_TRUE(frames_passed[0]->header.reserved1);
  EXPECT_EQ(std::string("\x02\x00", 2), ToString(frames_passed[0]));
}

TEST_F(WebSocketDeflateStreamTest, LargeDeflatedFramesShouldBeSplit) {
  WebSocketDeflater deflater(WebSocketDeflater::TAKE_OVER_CONTEXT);
  LinearCongruentialGenerator lcg(133);
  WriteFramesStub stub(OK);
  CompletionCallback callback;
  const size_t size = 1024;

  {
    InSequence s;
    EXPECT_CALL(*mock_stream_, WriteFrames(_, _))
        .WillRepeatedly(Invoke(&stub, &WriteFramesStub::Call));
  }
  ScopedVector<WebSocketFrame> total_compressed_frames;

  deflater.Initialize(kWindowBits);
  while (true) {
    bool is_final = (total_compressed_frames.size() >= 2);
    ScopedVector<WebSocketFrame> frames;
    std::string data;
    for (size_t i = 0; i < size; ++i)
      data += static_cast<char>(lcg.Generate());
    deflater.AddBytes(data.data(), data.size());
    FrameFlag flag = is_final ? kFinal : kNoFlag;
    AppendTo(&frames, WebSocketFrameHeader::kOpCodeBinary, flag, data);
    ASSERT_EQ(OK, deflate_stream_->WriteFrames(&frames, callback));
    for (size_t i = 0; i < stub.frames()->size(); ++i)
      total_compressed_frames.push_back((*stub.frames())[i]);
    stub.frames()->weak_clear();
    if (is_final)
      break;
  }
  deflater.Finish();
  std::string total_deflated;
  for (size_t i = 0; i < total_compressed_frames.size(); ++i) {
    WebSocketFrame* frame = total_compressed_frames[i];
    const WebSocketFrameHeader& header = frame->header;
    if (i > 0) {
      EXPECT_EQ(header.kOpCodeContinuation, header.opcode);
      EXPECT_FALSE(header.reserved1);
    } else {
      EXPECT_EQ(header.kOpCodeBinary, header.opcode);
      EXPECT_TRUE(header.reserved1);
    }
    const bool is_final_frame = (i + 1 == total_compressed_frames.size());
    EXPECT_EQ(is_final_frame, header.final);
    if (!is_final_frame)
      EXPECT_GT(header.payload_length, 0ul);
    total_deflated += ToString(frame);
  }
  EXPECT_EQ(total_deflated,
            ToString(deflater.GetOutput(deflater.CurrentOutputSize())));
}

}  // namespace

}  // namespace net
