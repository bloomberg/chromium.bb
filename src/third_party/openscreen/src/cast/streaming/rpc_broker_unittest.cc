// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/rpc_broker.h"

#include <memory>
#include <vector>

#include "cast/streaming/remoting.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace openscreen {
namespace cast {

namespace {

class FakeMessager {
 public:
  void OnReceivedRpc(const RpcMessage& message) {
    received_rpc_ = message;
    received_count_++;
  }

  void OnSentRpc(const std::vector<uint8_t>& message) {
    EXPECT_TRUE(sent_rpc_.ParseFromArray(message.data(), message.size()));
    sent_count_++;
  }

  int received_count() const { return received_count_; }
  const RpcMessage& received_rpc() const { return received_rpc_; }

  int sent_count() const { return sent_count_; }
  const RpcMessage& sent_rpc() const { return sent_rpc_; }

  void set_handle(RpcBroker::Handle handle) { handle_ = handle; }
  RpcBroker::Handle handle() { return handle_; }

 private:
  RpcMessage received_rpc_;
  int received_count_ = 0;

  RpcMessage sent_rpc_;
  int sent_count_ = 0;

  RpcBroker::Handle handle_ = -1;
};

}  // namespace

class RpcBrokerTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_messager_ = std::make_unique<FakeMessager>();
    ASSERT_FALSE(fake_messager_->received_count());

    rpc_broker_ = std::make_unique<RpcBroker>(
        [p = fake_messager_.get()](std::vector<uint8_t> message) {
          p->OnSentRpc(message);
        });

    const auto handle = rpc_broker_->GetUniqueHandle();
    fake_messager_->set_handle(handle);
    rpc_broker_->RegisterMessageReceiverCallback(
        handle, [p = fake_messager_.get()](const RpcMessage& message) {
          p->OnReceivedRpc(message);
        });
  }

  std::unique_ptr<FakeMessager> fake_messager_;
  std::unique_ptr<RpcBroker> rpc_broker_;
};

TEST_F(RpcBrokerTest, TestProcessMessageFromRemoteRegistered) {
  RpcMessage rpc;
  rpc.set_handle(fake_messager_->handle());
  rpc_broker_->ProcessMessageFromRemote(rpc);
  ASSERT_EQ(1, fake_messager_->received_count());
}

TEST_F(RpcBrokerTest, TestProcessMessageFromRemoteUnregistered) {
  RpcMessage rpc;
  rpc_broker_->UnregisterMessageReceiverCallback(fake_messager_->handle());
  rpc_broker_->ProcessMessageFromRemote(rpc);
  ASSERT_EQ(0, fake_messager_->received_count());
}

TEST_F(RpcBrokerTest, CanSendMultipleMessages) {
  for (int i = 0; i < 10; ++i) {
    rpc_broker_->SendMessageToRemote(RpcMessage{});
  }
  EXPECT_EQ(10, fake_messager_->sent_count());
}

TEST_F(RpcBrokerTest, SendMessageCallback) {
  // Send message for RPC broker to process.
  RpcMessage sent_rpc;
  sent_rpc.set_handle(fake_messager_->handle());
  sent_rpc.set_proc(RpcMessage::RPC_R_SETVOLUME);
  sent_rpc.set_double_value(2.3);
  rpc_broker_->SendMessageToRemote(sent_rpc);

  // Check if received message is identical to the one sent earlier.
  ASSERT_EQ(1, fake_messager_->sent_count());
  const RpcMessage& message = fake_messager_->sent_rpc();
  ASSERT_EQ(fake_messager_->handle(), message.handle());
  ASSERT_EQ(RpcMessage::RPC_R_SETVOLUME, message.proc());
  ASSERT_EQ(2.3, message.double_value());
}

TEST_F(RpcBrokerTest, ProcessMessageWithRegisteredHandle) {
  // Send message for RPC broker to process.
  RpcMessage sent_rpc;
  sent_rpc.set_handle(fake_messager_->handle());
  sent_rpc.set_proc(RpcMessage::RPC_R_SETVOLUME);
  sent_rpc.set_double_value(3.4);
  rpc_broker_->ProcessMessageFromRemote(sent_rpc);

  // Checks if received message is identical to the one sent earlier.
  ASSERT_EQ(1, fake_messager_->received_count());
  const RpcMessage& received_rpc = fake_messager_->received_rpc();
  ASSERT_EQ(fake_messager_->handle(), received_rpc.handle());
  ASSERT_EQ(RpcMessage::RPC_R_SETVOLUME, received_rpc.proc());
  ASSERT_EQ(3.4, received_rpc.double_value());
}

TEST_F(RpcBrokerTest, ProcessMessageWithUnregisteredHandle) {
  // Send message for RPC broker to process.
  RpcMessage sent_rpc;
  RpcBroker::Handle different_handle = fake_messager_->handle() + 1;
  sent_rpc.set_handle(different_handle);
  sent_rpc.set_proc(RpcMessage::RPC_R_SETVOLUME);
  sent_rpc.set_double_value(4.5);
  rpc_broker_->ProcessMessageFromRemote(sent_rpc);

  // We shouldn't have gotten the message since the handle is different.
  ASSERT_EQ(0, fake_messager_->received_count());
}

TEST_F(RpcBrokerTest, Registration) {
  const auto handle = fake_messager_->handle();
  ASSERT_TRUE(rpc_broker_->IsRegisteredForTesting(handle));

  rpc_broker_->UnregisterMessageReceiverCallback(handle);
  ASSERT_FALSE(rpc_broker_->IsRegisteredForTesting(handle));
}

}  // namespace cast
}  // namespace openscreen
