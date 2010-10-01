// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/jingle_glue/channel_socket_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/p2p/base/transportchannel.h"

using net::IOBuffer;

using testing::_;
using testing::Return;

namespace remoting {

namespace {
const int kBufferSize = 4096;
const char kTestData[] = "data";
const int kTestDataSize = 4;
const int kTestError = -32123;
}  // namespace

class MockTransportChannel : public cricket::TransportChannel {
 public:
  MockTransportChannel()
      : cricket::TransportChannel("", "") {
  }

  MOCK_METHOD2(SendPacket, int(const char *data, size_t len));
  MOCK_METHOD2(SetOption, int(talk_base::Socket::Option opt, int value));
  MOCK_METHOD0(GetError, int());
};

class TransportChannelSocketAdapterTest : public testing::Test {
 public:
  TransportChannelSocketAdapterTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          callback_(this, &TransportChannelSocketAdapterTest::Callback)),
        callback_result_(0) {
  }

 protected:
  virtual void SetUp() {
    target_.reset(new TransportChannelSocketAdapter(&channel_));
  }

  void Callback(int result) {
    callback_result_ = result;
  }

  MockTransportChannel channel_;
  scoped_ptr<TransportChannelSocketAdapter> target_;
  net::CompletionCallbackImpl<TransportChannelSocketAdapterTest> callback_;
  int callback_result_;
  MessageLoopForIO message_loop_;
};

// Verify that Read() returns net::ERR_IO_PENDING.
TEST_F(TransportChannelSocketAdapterTest, Read) {
  scoped_refptr<IOBuffer> buffer = new IOBuffer(kBufferSize);

  int result = target_->Read(buffer, kBufferSize, &callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  channel_.SignalReadPacket(&channel_, kTestData, kTestDataSize);
  EXPECT_EQ(kTestDataSize, callback_result_);
}

// Verify that Read() after Close() returns error.
TEST_F(TransportChannelSocketAdapterTest, ReadClose) {
  scoped_refptr<IOBuffer> buffer = new IOBuffer(kBufferSize);

  int result = target_->Read(buffer, kBufferSize, &callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  target_->Close(kTestError);
  EXPECT_EQ(kTestError, callback_result_);

  // All Read() calls after Close() should return the error.
  EXPECT_EQ(kTestError, target_->Read(buffer, kBufferSize, &callback_));
}

// Verify that Write sends the packet and returns correct result.
TEST_F(TransportChannelSocketAdapterTest, Write) {
  scoped_refptr<IOBuffer> buffer = new IOBuffer(kTestDataSize);

  EXPECT_CALL(channel_, SendPacket(buffer->data(), kTestDataSize))
      .WillOnce(Return(kTestDataSize));

  int result = target_->Write(buffer, kTestDataSize, &callback_);
  EXPECT_EQ(kTestDataSize, result);
}

// Verify that the message is still send if Write() is called while
// socket is not open yet, and that the callback is called.
TEST_F(TransportChannelSocketAdapterTest, WritePending) {
  scoped_refptr<IOBuffer> buffer = new IOBuffer(kTestDataSize);

  EXPECT_CALL(channel_, SendPacket(buffer->data(), kTestDataSize))
      .Times(2)
      .WillOnce(Return(SOCKET_ERROR))
      .WillOnce(Return(kTestDataSize));

  EXPECT_CALL(channel_, GetError())
      .WillOnce(Return(EWOULDBLOCK));

  int result = target_->Write(buffer, kTestDataSize, &callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  channel_.SignalWritableState(&channel_);
  EXPECT_EQ(kTestDataSize, callback_result_);
}

}  // namespace remoting
