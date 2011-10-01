// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/stringprintf.h"
#include "crypto/symmetric_key.h"
#include "net/base/io_buffer.h"
#include "remoting/protocol/secure_p2p_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {
namespace protocol {

namespace {
class TestSocket : public net::Socket {
 public:
  TestSocket() {}

  // Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::OldCompletionCallback* callback) {
    std::string buffer = buffer_.front();
    buffer_.pop();

    memcpy(buf->data(), buffer.data(), buffer.length());
    int size = buffer.length();
    return size;
  }

  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::OldCompletionCallback* callback) {
    buffer_.push(std::string(buf->data(), buf_len));
    return buf_len;
  }

  virtual bool SetReceiveBufferSize(int32 size) {
    return true;
  }

  virtual bool SetSendBufferSize(int32 size) {
    return true;
  }

  std::string GetFrontBuffer() const {
    return buffer_.front();
  }

  void PushBuffer(std::string buffer) {
    buffer_.push(buffer);
  }

 private:
  std::queue<std::string> buffer_;

  DISALLOW_COPY_AND_ASSIGN(TestSocket);
};
}  // namespace

// AES-CTR is only implemented with NSS.
#if defined(USE_NSS)

TEST(SecureP2PSocketTest, WriteAndRead) {
  TestSocket* test_socket = new TestSocket();
  SecureP2PSocket secure_socket(test_socket, "1234567890123456");

  const std::string kWritePattern = "Hello world! This is a nice day.";
  scoped_refptr<net::IOBuffer> write_buf =
      new net::StringIOBuffer(kWritePattern);
  scoped_refptr<net::IOBuffer> read_buf = new net::IOBufferWithSize(2048);

  for (int i = 0; i < 5; ++i) {
    size_t written = secure_socket.Write(write_buf,
                                         kWritePattern.length(), NULL);
    EXPECT_EQ(kWritePattern.length(), written);
    EXPECT_EQ(kWritePattern.length() + 44,
              test_socket->GetFrontBuffer().length());

    std::string hex_packet;
    for (size_t j = 0; j < test_socket->GetFrontBuffer().length(); ++j) {
      base::StringAppendF(&hex_packet, "%02x",
                          (uint8)test_socket->GetFrontBuffer()[j]);
    }
    LOG(INFO) << hex_packet;

    size_t read = secure_socket.Read(read_buf, 2048, NULL);
    EXPECT_EQ(kWritePattern.length(), read);
    EXPECT_EQ(0, memcmp(kWritePattern.data(), read_buf->data(),
                        kWritePattern.length()));
  }
}

TEST(SecureP2PSocketTest, ReadRetry) {
  TestSocket* test_socket = new TestSocket();
  SecureP2PSocket secure_socket(test_socket, "1234567890123456");

  const std::string kWritePattern = "Hello world! This is a nice day.";
  scoped_refptr<net::IOBuffer> write_buf =
      new net::StringIOBuffer(kWritePattern);
  scoped_refptr<net::IOBuffer> read_buf = new net::IOBufferWithSize(2048);

  // Push some garbage to the socket.
  test_socket->PushBuffer("0");
  test_socket->PushBuffer("00");
  test_socket->PushBuffer("0000");
  test_socket->PushBuffer("00000000");
  test_socket->PushBuffer("0000000000000000");
  test_socket->PushBuffer("00000000000000000000000000000000");

  // Then write some real stuff.
  size_t written = secure_socket.Write(write_buf,
                                       kWritePattern.length(), NULL);
  EXPECT_EQ(kWritePattern.length(), written);

  size_t read = secure_socket.Read(read_buf, 2048, NULL);
  EXPECT_EQ(kWritePattern.length(), read);
  EXPECT_EQ(0, memcmp(kWritePattern.data(), read_buf->data(),
                      kWritePattern.length()));
}

#endif

}  // namespace protocol
}  // namespace remoting
