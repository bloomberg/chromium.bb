// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/push_client_channel.h"

#include <cstddef>
#include <string>

#include "base/compiler_specific.h"
#include "jingle/notifier/listener/fake_push_client.h"
#include "jingle/notifier/listener/notification_defines.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class PushClientChannelTest : public ::testing::Test {
 protected:
  PushClientChannelTest()
      : fake_push_client_(new notifier::FakePushClient()),
        push_client_channel_(
            scoped_ptr<notifier::PushClient>(fake_push_client_)),
        connected_(false) {
    push_client_channel_.SetMessageReceiver(
        invalidation::NewPermanentCallback(
            this, &PushClientChannelTest::OnIncomingMessage));
    push_client_channel_.AddNetworkStatusReceiver(
        invalidation::NewPermanentCallback(
            this, &PushClientChannelTest::OnNetworkStatusChange));
    push_client_channel_.SetSystemResources(NULL);
  }

  virtual ~PushClientChannelTest() {}

  void OnIncomingMessage(std::string incoming_message) {
    last_message_ = incoming_message;
  }

  void OnNetworkStatusChange(bool connected) {
    connected_ = connected;
  }

  notifier::FakePushClient* fake_push_client_;
  PushClientChannel push_client_channel_;
  std::string last_message_;
  bool connected_;
};

const char kMessage[] = "message";
const char kServiceContext[] = "service context";
const int64 kSchedulingHash = 100;

// Encode a message with some context into a notification and then
// decode it.  The decoded info should match the original info.
TEST_F(PushClientChannelTest, EncodeDecode) {
  const notifier::Notification& notification =
      PushClientChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);
  std::string message;
  std::string service_context;
  int64 scheduling_hash = 0LL;
  EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
      notification, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Encode a message with no context into a notification and then
// decode it.  The decoded message should match the original message,
// but the context and hash should be untouched.
TEST_F(PushClientChannelTest, EncodeDecodeNoContext) {
  const notifier::Notification& notification =
      PushClientChannel::EncodeMessageForTest(
          kMessage, "", kSchedulingHash);
  std::string message;
  std::string service_context = kServiceContext;
  int64 scheduling_hash = kSchedulingHash + 1;
  EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
      notification, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash + 1, scheduling_hash);
}

// Decode an empty notification.  It should result in an empty message
// but should leave the context and hash untouched.
TEST_F(PushClientChannelTest, DecodeEmpty) {
  std::string message = kMessage;
  std::string service_context = kServiceContext;
  int64 scheduling_hash = kSchedulingHash;
  EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
      notifier::Notification(),
      &message, &service_context, &scheduling_hash));
  EXPECT_TRUE(message.empty());
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Try to decode a garbage notification.  It should leave all its
// arguments untouched and return false.
TEST_F(PushClientChannelTest, DecodeGarbage) {
  notifier::Notification notification;
  notification.data = "garbage";
  std::string message = kMessage;
  std::string service_context = kServiceContext;
  int64 scheduling_hash = kSchedulingHash;
  EXPECT_FALSE(PushClientChannel::DecodeMessageForTest(
      notification, &message, &service_context, &scheduling_hash));
  EXPECT_EQ(kMessage, message);
  EXPECT_EQ(kServiceContext, service_context);
  EXPECT_EQ(kSchedulingHash, scheduling_hash);
}

// Make sure the channel subscribes to the correct notifications
// channel on construction.
TEST_F(PushClientChannelTest, Subscriptions) {
  notifier::Subscription expected_subscription;
  expected_subscription.channel = "tango_raw";
  EXPECT_TRUE(notifier::SubscriptionListsEqual(
      fake_push_client_->subscriptions(),
      notifier::SubscriptionList(1, expected_subscription)));
}

// Call UpdateCredentials on the channel.  It should propagate it to
// the push client.
TEST_F(PushClientChannelTest, UpdateCredentials) {
  const char kEmail[] = "foo@bar.com";
  const char kToken[] = "token";
  EXPECT_TRUE(fake_push_client_->email().empty());
  EXPECT_TRUE(fake_push_client_->token().empty());
  push_client_channel_.UpdateCredentials(kEmail, kToken);
  EXPECT_EQ(kEmail, fake_push_client_->email());
  EXPECT_EQ(kToken, fake_push_client_->token());
}

// Call SendMessage on the channel.  It should propagate it to the
// push client.
TEST_F(PushClientChannelTest, SendMessage) {
  EXPECT_TRUE(fake_push_client_->sent_notifications().empty());
  push_client_channel_.SendMessage(kMessage);
  const notifier::Notification expected_notification =
      PushClientChannel::EncodeMessageForTest(
          kMessage,
          push_client_channel_.GetServiceContextForTest(),
          push_client_channel_.GetSchedulingHashForTest());
  ASSERT_EQ(1u, fake_push_client_->sent_notifications().size());
  EXPECT_TRUE(
      fake_push_client_->sent_notifications()[0].Equals(
          expected_notification));
}

// Simulate push client state changes on the push client.  It should
// propagate to the channel.
TEST_F(PushClientChannelTest, OnPushClientStateChange) {
  EXPECT_FALSE(connected_);
  fake_push_client_->EnableNotifications();
  EXPECT_TRUE(connected_);
  fake_push_client_->DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_FALSE(connected_);
  fake_push_client_->EnableNotifications();
  EXPECT_TRUE(connected_);
  fake_push_client_->DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);
  EXPECT_FALSE(connected_);
}

// Simulate an incoming notification.  It should be decoded properly
// by the channel.
TEST_F(PushClientChannelTest, OnIncomingNotification) {
  const notifier::Notification notification =
      PushClientChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);

  fake_push_client_->SimulateIncomingNotification(notification);
  EXPECT_EQ(kServiceContext,
            push_client_channel_.GetServiceContextForTest());
  EXPECT_EQ(kSchedulingHash,
            push_client_channel_.GetSchedulingHashForTest());
  EXPECT_EQ(kMessage, last_message_);
}

// Simulate an incoming notification with no receiver.  It should be
// dropped by the channel.
TEST_F(PushClientChannelTest, OnIncomingNotificationNoReceiver) {
  const notifier::Notification notification =
      PushClientChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);

  push_client_channel_.SetMessageReceiver(NULL);
  fake_push_client_->SimulateIncomingNotification(notification);
  EXPECT_TRUE(push_client_channel_.GetServiceContextForTest().empty());
  EXPECT_EQ(static_cast<int64>(0),
            push_client_channel_.GetSchedulingHashForTest());
  EXPECT_TRUE(last_message_.empty());
}

// Simulate an incoming garbage notification.  It should be dropped by
// the channel.
TEST_F(PushClientChannelTest, OnIncomingNotificationGarbage) {
  notifier::Notification notification;
  notification.data = "garbage";

  fake_push_client_->SimulateIncomingNotification(notification);
  EXPECT_TRUE(push_client_channel_.GetServiceContextForTest().empty());
  EXPECT_EQ(static_cast<int64>(0),
            push_client_channel_.GetSchedulingHashForTest());
  EXPECT_TRUE(last_message_.empty());
}

// Send a message, simulate an incoming message with context, and then
// send the same message again.  The first sent message should not
// have any context, but the second sent message should have the
// context from the incoming emssage.
TEST_F(PushClientChannelTest, PersistedMessageState) {
  push_client_channel_.SendMessage(kMessage);
  ASSERT_EQ(1u, fake_push_client_->sent_notifications().size());
  {
    std::string message;
    std::string service_context;
    int64 scheduling_hash = 0LL;
    EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
        fake_push_client_->sent_notifications()[0],
        &message, &service_context, &scheduling_hash));
    EXPECT_EQ(kMessage, message);
    EXPECT_TRUE(service_context.empty());
    EXPECT_EQ(0LL, scheduling_hash);
  }

  const notifier::Notification notification =
      PushClientChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);
  fake_push_client_->SimulateIncomingNotification(notification);

  push_client_channel_.SendMessage(kMessage);
  ASSERT_EQ(2u, fake_push_client_->sent_notifications().size());
  {
    std::string message;
    std::string service_context;
    int64 scheduling_hash = 0LL;
    EXPECT_TRUE(PushClientChannel::DecodeMessageForTest(
        fake_push_client_->sent_notifications()[1],
        &message, &service_context, &scheduling_hash));
    EXPECT_EQ(kMessage, message);
    EXPECT_EQ(kServiceContext, service_context);
    EXPECT_EQ(kSchedulingHash, scheduling_hash);
  }
}

}  // namespace
}  // namespace syncer
