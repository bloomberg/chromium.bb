// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/chromium_socket_factory.h"

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/base/asyncpacketsocket.h"
#include "third_party/webrtc/base/socketaddress.h"

namespace remoting {
namespace protocol {

class ChromiumSocketFactoryTest : public testing::Test,
                                  public sigslot::has_slots<> {
 public:
  virtual void SetUp() OVERRIDE {
    socket_factory_.reset(new ChromiumPacketSocketFactory());

    socket_.reset(socket_factory_->CreateUdpSocket(
        rtc::SocketAddress("127.0.0.1", 0), 0, 0));
    ASSERT_TRUE(socket_.get() != NULL);
    EXPECT_EQ(socket_->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);
    socket_->SignalReadPacket.connect(
        this, &ChromiumSocketFactoryTest::OnPacket);
  }

  void OnPacket(rtc::AsyncPacketSocket* socket,
                const char* data, size_t size,
                const rtc::SocketAddress& address,
                const rtc::PacketTime& packet_time) {
    EXPECT_EQ(socket, socket_.get());
    last_packet_.assign(data, data + size);
    last_address_ = address;
    run_loop_.Quit();
  }

  void VerifyCanSendAndReceive(rtc::AsyncPacketSocket* sender) {
    // UDP packets may be lost, so we have to retry sending it more than once.
    const int kMaxAttempts = 3;
    const base::TimeDelta kAttemptPeriod = base::TimeDelta::FromSeconds(1);
    std::string test_packet("TEST PACKET");
    int attempts = 0;
    rtc::PacketOptions options;
    while (last_packet_.empty() && attempts++ < kMaxAttempts) {
      sender->SendTo(test_packet.data(), test_packet.size(),
                     socket_->GetLocalAddress(), options);
      message_loop_.PostDelayedTask(FROM_HERE, run_loop_.QuitClosure(),
                                    kAttemptPeriod);
      run_loop_.Run();
    }
    EXPECT_EQ(test_packet, last_packet_);
    EXPECT_EQ(sender->GetLocalAddress(), last_address_);
  }

 protected:
  base::MessageLoopForIO message_loop_;
  base::RunLoop run_loop_;

  scoped_ptr<rtc::PacketSocketFactory> socket_factory_;
  scoped_ptr<rtc::AsyncPacketSocket> socket_;

  std::string last_packet_;
  rtc::SocketAddress last_address_;
};

TEST_F(ChromiumSocketFactoryTest, SendAndReceive) {
  scoped_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(
          rtc::SocketAddress("127.0.0.1", 0), 0, 0));
  ASSERT_TRUE(sending_socket.get() != NULL);
  EXPECT_EQ(sending_socket->GetState(),
            rtc::AsyncPacketSocket::STATE_BOUND);

  VerifyCanSendAndReceive(sending_socket.get());
}

TEST_F(ChromiumSocketFactoryTest, SetOptions) {
  EXPECT_EQ(0, socket_->SetOption(rtc::Socket::OPT_SNDBUF, 4096));
  EXPECT_EQ(0, socket_->SetOption(rtc::Socket::OPT_RCVBUF, 4096));
}

TEST_F(ChromiumSocketFactoryTest, PortRange) {
  const int kMinPort = 12400;
  const int kMaxPort = 12410;
  socket_.reset(socket_factory_->CreateUdpSocket(
      rtc::SocketAddress("127.0.0.1", 0), kMaxPort, kMaxPort));
  ASSERT_TRUE(socket_.get() != NULL);
  EXPECT_EQ(socket_->GetState(), rtc::AsyncPacketSocket::STATE_BOUND);
  EXPECT_GE(socket_->GetLocalAddress().port(), kMinPort);
  EXPECT_LE(socket_->GetLocalAddress().port(), kMaxPort);
}

TEST_F(ChromiumSocketFactoryTest, TransientError) {
  scoped_ptr<rtc::AsyncPacketSocket> sending_socket(
      socket_factory_->CreateUdpSocket(
          rtc::SocketAddress("127.0.0.1", 0), 0, 0));
  std::string test_packet("TEST");

  // Try sending a packet to an IPv6 address from a socket that's bound to an
  // IPv4 address. This send is expected to fail, but the socket should still be
  // functional.
  sending_socket->SendTo(test_packet.data(), test_packet.size(),
                         rtc::SocketAddress("::1", 0),
                         rtc::PacketOptions());

  // Verify that socket is still usable.
  VerifyCanSendAndReceive(sending_socket.get());
}

}  // namespace protocol
}  // namespace remoting
