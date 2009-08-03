/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "import/cross/memory_buffer.h"
#include "import/cross/threaded_stream_processor.h"
#include "tests/common/win/testing_common.h"
#include "tests/common/cross/test_utils.h"

using test_utils::ReadFile;

namespace o3d {

class ThreadedStreamProcessorTest : public testing::Test {
 protected:
};

class TestReceiver : public StreamProcessor {
 public:
  explicit TestReceiver(size_t size) : closed_(false), success_(false) {
    buffer_.Allocate(size);
    stream_.Assign(buffer_, size);
  }

  virtual Status ProcessBytes(MemoryReadStream *input_stream,
                              size_t bytes_to_process) {
    const uint8 *p = input_stream->GetDirectMemoryPointer();
    input_stream->Skip(bytes_to_process);

    size_t bytes_written = stream_.Write(p, bytes_to_process);
    EXPECT_EQ(bytes_written, bytes_to_process);

    return SUCCESS;
  }

  virtual void Close(bool success) {
    closed_ = true;
    success_ = success;
  }

  uint8 *GetResultBuffer() { return buffer_; }
  size_t GetResultLength() const { return stream_.GetStreamPosition(); }

  bool closed() const {
    return closed_;
  }

  bool success() const {
    return success_;
  }

 private:
  MemoryBuffer<uint8> buffer_;
  MemoryWriteStream stream_;
  bool closed_;
  bool success_;
};

TEST_F(ThreadedStreamProcessorTest, CanForwardZeroBuffers) {
  TestReceiver receiver(1000);
  ThreadedStreamProcessor processor(&receiver);
  processor.Close(true);
  processor.StopThread();

  EXPECT_EQ(0, receiver.GetResultLength());
  EXPECT_TRUE(receiver.closed());
  EXPECT_TRUE(receiver.success());
}

TEST_F(ThreadedStreamProcessorTest, CanForwardOneBufferOfZeroBytes) {
  uint8 buffer[5] = { 1, 2, 3, 4, 5 };
  TestReceiver receiver(1000);

  ThreadedStreamProcessor processor(&receiver);
  MemoryReadStream stream(buffer, sizeof(buffer));
  processor.ProcessBytes(&stream, 0);
  processor.Close(true);
  processor.StopThread();

  EXPECT_EQ(0, receiver.GetResultLength());
  EXPECT_TRUE(receiver.closed());
  EXPECT_TRUE(receiver.success());
}

TEST_F(ThreadedStreamProcessorTest, CanForwardOneBufferFullOfBytes) {
  uint8 buffer[5] = { 1, 2, 3, 4, 5 };
  TestReceiver receiver(1000);

  ThreadedStreamProcessor processor(&receiver);
  MemoryReadStream stream(buffer, sizeof(buffer));
  processor.ProcessBytes(&stream, sizeof(buffer));
  processor.Close(true);
  processor.StopThread();

  EXPECT_EQ(sizeof(buffer), receiver.GetResultLength());
  EXPECT_EQ(0, memcmp(buffer, receiver.GetResultBuffer(), sizeof(buffer)));
  EXPECT_TRUE(receiver.closed());
  EXPECT_TRUE(receiver.success());
}

TEST_F(ThreadedStreamProcessorTest, CanForwardTwoBuffersFullOfBytes) {
  uint8 buffer[5] = { 1, 2, 3, 4, 5 };
  TestReceiver receiver(1000);

  ThreadedStreamProcessor processor(&receiver);
  MemoryReadStream stream(buffer, sizeof(buffer));
  processor.ProcessBytes(&stream, 2);
  processor.ProcessBytes(&stream, 3);
  processor.Close(true);
  processor.StopThread();

  EXPECT_EQ(sizeof(buffer), receiver.GetResultLength());
  EXPECT_EQ(0, memcmp(buffer, receiver.GetResultBuffer(), sizeof(buffer)));
  EXPECT_TRUE(receiver.closed());
  EXPECT_TRUE(receiver.success());
}

TEST_F(ThreadedStreamProcessorTest, PassesThroughStreamFailed) {
  TestReceiver receiver(1000);
  ThreadedStreamProcessor processor(&receiver);
  processor.Close(false);
  processor.StopThread();

  EXPECT_TRUE(receiver.closed());
  EXPECT_FALSE(receiver.success());
}

}  // namespace o3d
