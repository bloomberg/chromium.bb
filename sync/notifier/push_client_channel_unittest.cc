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

class PushClientChannelTest
    : public ::testing::Test,
      public SyncNetworkChannel::Observer {
 protected:
  PushClientChannelTest()
      : fake_push_client_(new notifier::FakePushClient()),
        push_client_channel_(
            scoped_ptr<notifier::PushClient>(fake_push_client_)),
        last_invalidator_state_(DEFAULT_INVALIDATION_ERROR) {
    push_client_channel_.AddObserver(this);
    push_client_channel_.SetMessageReceiver(
        invalidation::NewPermanentCallback(
            this, &PushClientChannelTest::OnIncomingMessage));
    push_client_channel_.SetSystemResources(NULL);
  }

  virtual ~PushClientChannelTest() {
    push_client_channel_.RemoveObserver(this);
  }

  virtual void OnNetworkChannelStateChanged(
      InvalidatorState invalidator_state) OVERRIDE {
    last_invalidator_state_ = invalidator_state;
  }

  void OnIncomingMessage(std::string incoming_message) {
    last_message_ = incoming_message;
  }

  notifier::FakePushClient* fake_push_client_;
  PushClientChannel push_client_channel_;
  std::string last_message_;
  InvalidatorState last_invalidator_state_;
};

const char kMessage[] = "message";
const char kServiceContext[] = "service context";
const int64 kSchedulingHash = 100;

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

// Simulate push client state changes on the push client.  It should
// propagate to the channel.
TEST_F(PushClientChannelTest, OnPushClientStateChange) {
  EXPECT_EQ(DEFAULT_INVALIDATION_ERROR, last_invalidator_state_);
  fake_push_client_->EnableNotifications();
  EXPECT_EQ(INVALIDATIONS_ENABLED, last_invalidator_state_);
  fake_push_client_->DisableNotifications(
      notifier::TRANSIENT_NOTIFICATION_ERROR);
  EXPECT_EQ(TRANSIENT_INVALIDATION_ERROR, last_invalidator_state_);
  fake_push_client_->DisableNotifications(
      notifier::NOTIFICATION_CREDENTIALS_REJECTED);
  EXPECT_EQ(INVALIDATION_CREDENTIALS_REJECTED, last_invalidator_state_);
}

// Call SendMessage on the channel.  It should propagate it to the
// push client.
TEST_F(PushClientChannelTest, SendMessage) {
  EXPECT_TRUE(fake_push_client_->sent_notifications().empty());
  push_client_channel_.SendMessage(kMessage);
  ASSERT_EQ(1u, fake_push_client_->sent_notifications().size());
}

// Simulate an incoming notification.  It should be decoded properly
// by the channel.
TEST_F(PushClientChannelTest, OnIncomingNotification) {
  notifier::Notification notification;
  notification.data =
      PushClientChannel::EncodeMessageForTest(
          kMessage, kServiceContext, kSchedulingHash);

  fake_push_client_->SimulateIncomingNotification(notification);
  EXPECT_EQ(kServiceContext,
            push_client_channel_.GetServiceContextForTest());
  EXPECT_EQ(kSchedulingHash,
            push_client_channel_.GetSchedulingHashForTest());
  EXPECT_EQ(kMessage, last_message_);
}

}  // namespace
}  // namespace syncer
