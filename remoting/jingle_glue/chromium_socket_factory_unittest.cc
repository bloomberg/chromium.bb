// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/jingle_glue/chromium_socket_factory.h"

#include "base/message_loop.h"
#include "base/run_loop.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/base/asyncpacketsocket.h"
#include "third_party/libjingle/source/talk/base/socketaddress.h"

namespace remoting {

class ChromiumSocketFactoryTest : public testing::Test,
                                  public sigslot::has_slots<> {
 public:
  virtual void SetUp() OVERRIDE {
    socket_factory_.reset(new ChromiumPacketSocketFactory());

    socket_.reset(socket_factory_->CreateUdpSocket(
        talk_base::SocketAddress("127.0.0.1", 0), 0, 0));
    ASSERT_TRUE(socket_.get() != NULL);
    EXPECT_EQ(socket_->GetState(), talk_base::AsyncPacketSocket::STATE_BOUND);
    socket_->SignalReadPacket.connect(
        this, &ChromiumSocketFactoryTest::OnPacket);
  }

  void OnPacket(talk_base::AsyncPacketSocket* socket,
                const char* data, size_t size,
                const talk_base::SocketAddress& address) {
    EXPECT_EQ(socket, socket_.get());
    last_packet_.assign(data, data + size);
    last_address_ = address;
    run_loop_.Quit();
  }

 protected:
  base::MessageLoopForIO message_loop_;
  base::RunLoop run_loop_;

  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;
  scoped_ptr<talk_base::AsyncPacketSocket> socket_;

  std::string last_packet_;
  talk_base::SocketAddress last_address_;
};

TEST_F(ChromiumSocketFactoryTest, SendAndReceive) {
  // UDP packets may be lost, so we have to retry sending it more than once.
  const int kMaxAttempts = 3;
  const base::TimeDelta kAttemptPeriod = base::TimeDelta::FromSeconds(1);

  scoped_ptr<talk_base::AsyncPacketSocket> sending_socket;
  talk_base::SocketAddress address;

  sending_socket.reset(socket_factory_->CreateUdpSocket(
      talk_base::SocketAddress("127.0.0.1", 0), 0, 0));
  ASSERT_TRUE(sending_socket.get() != NULL);
  EXPECT_EQ(sending_socket->GetState(),
            talk_base::AsyncPacketSocket::STATE_BOUND);
  address = sending_socket->GetLocalAddress();

  std::string test_packet("TEST PACKET");
  int attempts = 0;
  while (last_packet_.empty() && attempts++ < kMaxAttempts) {
    sending_socket->SendTo(test_packet.data(), test_packet.size(),
                           socket_->GetLocalAddress());
    message_loop_.PostDelayedTask(FROM_HERE, run_loop_.QuitClosure(),
                                  kAttemptPeriod);
    run_loop_.Run();
  }
  EXPECT_EQ(test_packet, last_packet_);
  EXPECT_EQ(address, last_address_);
}

TEST_F(ChromiumSocketFactoryTest, SetOptions) {
  EXPECT_EQ(0, socket_->SetOption(talk_base::Socket::OPT_SNDBUF, 4096));
  EXPECT_EQ(0, socket_->SetOption(talk_base::Socket::OPT_RCVBUF, 4096));
}

TEST_F(ChromiumSocketFactoryTest, PortRange) {
  const int kMinPort = 12400;
  const int kMaxPort = 12410;
  socket_.reset(socket_factory_->CreateUdpSocket(
      talk_base::SocketAddress("127.0.0.1", 0), kMaxPort, kMaxPort));
  ASSERT_TRUE(socket_.get() != NULL);
  EXPECT_EQ(socket_->GetState(), talk_base::AsyncPacketSocket::STATE_BOUND);
  EXPECT_GE(socket_->GetLocalAddress().port(), kMinPort);
  EXPECT_LE(socket_->GetLocalAddress().port(), kMaxPort);
}

}  // namespace remoting
