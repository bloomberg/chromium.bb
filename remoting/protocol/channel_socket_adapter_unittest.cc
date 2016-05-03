// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_socket_adapter.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/p2p/base/transportchannel.h"

using net::IOBuffer;

using testing::_;
using testing::Return;

namespace remoting {
namespace protocol {

namespace {
const int kBufferSize = 4096;
const char kTestData[] = "data";
const int kTestDataSize = 4;
const int kTestError = -32123;
}  // namespace

class MockTransportChannel : public cricket::TransportChannel {
 public:
  MockTransportChannel() : cricket::TransportChannel(std::string(), 0) {
    set_writable(true);
  }

  MOCK_METHOD4(SendPacket, int(const char* data,
                               size_t len,
                               const rtc::PacketOptions& options,
                               int flags));
  MOCK_METHOD2(SetOption, int(rtc::Socket::Option opt, int value));
  MOCK_METHOD0(GetError, int());
  MOCK_CONST_METHOD0(GetIceRole, cricket::IceRole());
  MOCK_METHOD1(GetStats, bool(cricket::ConnectionInfos* infos));
  MOCK_CONST_METHOD0(IsDtlsActive, bool());
  MOCK_CONST_METHOD1(GetSslRole, bool(rtc::SSLRole* role));
  MOCK_METHOD1(SetSrtpCiphers, bool(const std::vector<std::string>& ciphers));
  MOCK_METHOD1(GetSrtpCipher, bool(std::string* cipher));
  MOCK_METHOD1(GetSslCipher, bool(std::string* cipher));
  MOCK_CONST_METHOD0(GetLocalCertificate,
                     rtc::scoped_refptr<rtc::RTCCertificate>());

  // This can't be a real mock method because gmock doesn't support move-only
  // return values.
  std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate()
      const override {
    EXPECT_TRUE(false);  // Never called.
    return nullptr;
  }

  MOCK_METHOD6(ExportKeyingMaterial,
               bool(const std::string& label,
                    const uint8_t* context,
                    size_t context_len,
                    bool use_context,
                    uint8_t* result,
                    size_t result_len));
};

class TransportChannelSocketAdapterTest : public testing::Test {
 public:
  TransportChannelSocketAdapterTest()
      : callback_(base::Bind(&TransportChannelSocketAdapterTest::Callback,
                             base::Unretained(this))),
        callback_result_(0) {
  }

 protected:
  void SetUp() override {
    target_.reset(new TransportChannelSocketAdapter(&channel_));
  }

  void Callback(int result) {
    callback_result_ = result;
  }

  MockTransportChannel channel_;
  std::unique_ptr<TransportChannelSocketAdapter> target_;
  net::CompletionCallback callback_;
  int callback_result_;
  base::MessageLoopForIO message_loop_;
};

// Verify that Read() returns net::ERR_IO_PENDING.
TEST_F(TransportChannelSocketAdapterTest, Read) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kBufferSize));

  int result = target_->Recv(buffer.get(), kBufferSize, callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  channel_.SignalReadPacket(&channel_, kTestData, kTestDataSize,
                            rtc::CreatePacketTime(0), 0);
  EXPECT_EQ(kTestDataSize, callback_result_);
}

// Verify that Read() after Close() returns error.
TEST_F(TransportChannelSocketAdapterTest, ReadClose) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kBufferSize));

  int result = target_->Recv(buffer.get(), kBufferSize, callback_);
  ASSERT_EQ(net::ERR_IO_PENDING, result);

  target_->Close(kTestError);
  EXPECT_EQ(kTestError, callback_result_);

  // All Recv() calls after Close() should return the error.
  EXPECT_EQ(kTestError, target_->Recv(buffer.get(), kBufferSize, callback_));
}

// Verify that Send sends the packet and returns correct result.
TEST_F(TransportChannelSocketAdapterTest, Send) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kTestDataSize));

  EXPECT_CALL(channel_, SendPacket(buffer->data(), kTestDataSize, _, 0))
      .WillOnce(Return(kTestDataSize));

  int result = target_->Send(buffer.get(), kTestDataSize, callback_);
  EXPECT_EQ(kTestDataSize, result);
}

// Verify that the message is still sent if Send() is called while
// socket is not open yet. The result is the packet is lost.
TEST_F(TransportChannelSocketAdapterTest, SendPending) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(kTestDataSize));

  EXPECT_CALL(channel_, SendPacket(buffer->data(), kTestDataSize, _, 0))
      .Times(1)
      .WillOnce(Return(SOCKET_ERROR));

  EXPECT_CALL(channel_, GetError())
      .WillOnce(Return(EWOULDBLOCK));

  int result = target_->Send(buffer.get(), kTestDataSize, callback_);
  ASSERT_EQ(net::OK, result);
}

}  // namespace protocol
}  // namespace remoting
