// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/jingle_glue/stream_socket_adapter.h"
#include "remoting/jingle_glue/jingle_glue_mock_objects.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/p2p/base/transportchannel.h"

using net::IOBuffer;

using testing::_;
using testing::Return;
using testing::SetArgumentPointee;

namespace remoting {

namespace {
const int kBufferSize = 4096;
const int kTestDataSize = 4;
const int kTestError = -32123;
}  // namespace

class StreamSocketAdapterTest : public testing::Test {
 public:
  StreamSocketAdapterTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &StreamSocketAdapterTest::Callback)),
        callback_result_(0) {
    stream_ = new MockStream();
    target_.reset(new StreamSocketAdapter(stream_));
  }

 protected:
  void Callback(int result) {
    callback_result_ = result;
  }

  // |stream_| must be allocated on the heap, because StreamSocketAdapter
  // owns the object and it will free it in the end.
  MockStream* stream_;
  scoped_ptr<StreamSocketAdapter> target_;
  net::CompletionCallbackImpl<StreamSocketAdapterTest> callback_;
  int callback_result_;
  MessageLoopForIO message_loop_;
};

// Verify that Read() calls Read() in stream.
TEST_F(StreamSocketAdapterTest, Read) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kBufferSize));

  EXPECT_CALL(*stream_, Read(buffer->data(), kBufferSize, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kTestDataSize),
                      Return(talk_base::SR_SUCCESS)));

  int result = target_->Read(buffer, kBufferSize, &callback_);
  EXPECT_EQ(kTestDataSize, result);
  EXPECT_EQ(0, callback_result_);
}

// Verify that read callback is called for pending reads.
TEST_F(StreamSocketAdapterTest, ReadPending) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kBufferSize));

  EXPECT_CALL(*stream_, Read(buffer->data(), kBufferSize, _, _))
      .Times(2)
      .WillOnce(Return(talk_base::SR_BLOCK))
      .WillOnce(DoAll(SetArgumentPointee<2>(kTestDataSize),
                      Return(talk_base::SR_SUCCESS)));

  int result = target_->Read(buffer, kBufferSize, &callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  stream_->SignalEvent(stream_, talk_base::SE_READ, 0);
  EXPECT_EQ(kTestDataSize, callback_result_);
}

// Verify that Read() returns error after Close().
TEST_F(StreamSocketAdapterTest, ReadClose) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kBufferSize));

  EXPECT_CALL(*stream_, Read(buffer->data(), kBufferSize, _, _))
      .WillOnce(Return(talk_base::SR_BLOCK));

  int result = target_->Read(buffer, kBufferSize, &callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  EXPECT_CALL(*stream_, Close());
  target_->Close(kTestError);
  EXPECT_EQ(kTestError, callback_result_);

  // All Read() calls after Close() should return the error.
  EXPECT_EQ(kTestError, target_->Read(buffer, kBufferSize, &callback_));
}

// Verify that Write() calls stream's Write() and returns result.
TEST_F(StreamSocketAdapterTest, Write) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kTestDataSize));

  EXPECT_CALL(*stream_, Write(buffer->data(), kTestDataSize, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(kTestDataSize),
                      Return(talk_base::SR_SUCCESS)));

  int result = target_->Write(buffer, kTestDataSize, &callback_);
  EXPECT_EQ(kTestDataSize, result);
  EXPECT_EQ(0, callback_result_);
}

// Verify that write callback is called for pending writes.
TEST_F(StreamSocketAdapterTest, WritePending) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kTestDataSize));

  EXPECT_CALL(*stream_, Write(buffer->data(), kTestDataSize, _, _))
      .Times(2)
      .WillOnce(Return(talk_base::SR_BLOCK))
      .WillOnce(DoAll(SetArgumentPointee<2>(kTestDataSize),
                      Return(talk_base::SR_SUCCESS)));

  int result = target_->Write(buffer, kTestDataSize, &callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  stream_->SignalEvent(stream_, talk_base::SE_WRITE, 0);
  EXPECT_EQ(kTestDataSize, callback_result_);
}

// Verify that Write() returns error after Close().
TEST_F(StreamSocketAdapterTest, WriteClose) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kTestDataSize));

  EXPECT_CALL(*stream_, Write(buffer->data(), kTestDataSize, _, _))
      .WillOnce(Return(talk_base::SR_BLOCK));

  int result = target_->Write(buffer, kTestDataSize, &callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  EXPECT_CALL(*stream_, Close());
  target_->Close(kTestError);
  EXPECT_EQ(kTestError, callback_result_);

  // All Write() calls after Close() should return the error.
  EXPECT_EQ(kTestError, target_->Write(buffer, kTestError, &callback_));
}

}  // namespace remoting
