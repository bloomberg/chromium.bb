// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/p2p_notifier.h"

#include <cstddef>

#include "jingle/notifier/listener/fake_push_client.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

class P2PNotifierTest : public testing::Test {
 protected:
  P2PNotifierTest()
      : fake_push_client_(new notifier::FakePushClient()),
        p2p_notifier_(
            scoped_ptr<notifier::PushClient>(fake_push_client_),
            NOTIFY_OTHERS),
        next_sent_notification_to_reflect_(0) {
    p2p_notifier_.AddObserver(&mock_observer_);
  }

  virtual ~P2PNotifierTest() {
    p2p_notifier_.RemoveObserver(&mock_observer_);
  }

  syncable::ModelTypePayloadMap MakePayloadMap(
      syncable::ModelTypeSet types) {
    return syncable::ModelTypePayloadMapFromEnumSet(types, "");
  }

  // Simulate receiving all the notifications we sent out since last
  // time this was called.
  void ReflectSentNotifications() {
    const std::vector<notifier::Notification>& sent_notifications =
        fake_push_client_->sent_notifications();
    for(size_t i = next_sent_notification_to_reflect_;
        i < sent_notifications.size(); ++i) {
      p2p_notifier_.OnIncomingNotification(sent_notifications[i]);
    }
    next_sent_notification_to_reflect_ = sent_notifications.size();
  }

  // Owned by |p2p_notifier_|.
  notifier::FakePushClient* fake_push_client_;
  P2PNotifier p2p_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;

 private:
  size_t next_sent_notification_to_reflect_;
};

// Make sure the P2PNotificationTarget <-> string conversions work.
TEST_F(P2PNotifierTest, P2PNotificationTarget) {
  for (int i = FIRST_NOTIFICATION_TARGET;
       i <= LAST_NOTIFICATION_TARGET; ++i) {
    P2PNotificationTarget target = static_cast<P2PNotificationTarget>(i);
    const std::string& target_str = P2PNotificationTargetToString(target);
    EXPECT_FALSE(target_str.empty());
    EXPECT_EQ(target, P2PNotificationTargetFromString(target_str));
  }
  EXPECT_EQ(NOTIFY_SELF, P2PNotificationTargetFromString("unknown"));
}

// Make sure notification targeting works correctly.
TEST_F(P2PNotifierTest, P2PNotificationDataIsTargeted) {
  {
    const P2PNotificationData notification_data(
        "sender", NOTIFY_SELF, syncable::ModelTypeSet());
    EXPECT_TRUE(notification_data.IsTargeted("sender"));
    EXPECT_FALSE(notification_data.IsTargeted("other1"));
    EXPECT_FALSE(notification_data.IsTargeted("other2"));
  }
  {
    const P2PNotificationData notification_data(
        "sender", NOTIFY_OTHERS, syncable::ModelTypeSet());
    EXPECT_FALSE(notification_data.IsTargeted("sender"));
    EXPECT_TRUE(notification_data.IsTargeted("other1"));
    EXPECT_TRUE(notification_data.IsTargeted("other2"));
  }
  {
    const P2PNotificationData notification_data(
        "sender", NOTIFY_ALL, syncable::ModelTypeSet());
    EXPECT_TRUE(notification_data.IsTargeted("sender"));
    EXPECT_TRUE(notification_data.IsTargeted("other1"));
    EXPECT_TRUE(notification_data.IsTargeted("other2"));
  }
}

// Make sure the P2PNotificationData <-> string conversions work for a
// default-constructed P2PNotificationData.
TEST_F(P2PNotifierTest, P2PNotificationDataDefault) {
  const P2PNotificationData notification_data;
  EXPECT_TRUE(notification_data.IsTargeted(""));
  EXPECT_FALSE(notification_data.IsTargeted("other1"));
  EXPECT_FALSE(notification_data.IsTargeted("other2"));
  EXPECT_TRUE(notification_data.GetChangedTypes().Empty());
  const std::string& notification_data_str = notification_data.ToString();
  EXPECT_EQ(
      "{\"changedTypes\":[],\"notificationType\":\"notifySelf\","
      "\"senderId\":\"\"}", notification_data_str);

  P2PNotificationData notification_data_parsed;
  EXPECT_TRUE(notification_data_parsed.ResetFromString(notification_data_str));
  EXPECT_TRUE(notification_data.Equals(notification_data_parsed));
}

// Make sure the P2PNotificationData <-> string conversions work for a
// non-default-constructed P2PNotificationData.
TEST_F(P2PNotifierTest, P2PNotificationDataNonDefault) {
  const syncable::ModelTypeSet changed_types(
      syncable::BOOKMARKS, syncable::THEMES);
  const P2PNotificationData notification_data(
      "sender", NOTIFY_ALL, changed_types);
  EXPECT_TRUE(notification_data.IsTargeted("sender"));
  EXPECT_TRUE(notification_data.IsTargeted("other1"));
  EXPECT_TRUE(notification_data.IsTargeted("other2"));
  EXPECT_TRUE(notification_data.GetChangedTypes().Equals(changed_types));
  const std::string& notification_data_str = notification_data.ToString();
  EXPECT_EQ(
      "{\"changedTypes\":[\"Bookmarks\",\"Themes\"],"
      "\"notificationType\":\"notifyAll\","
      "\"senderId\":\"sender\"}", notification_data_str);

  P2PNotificationData notification_data_parsed;
  EXPECT_TRUE(notification_data_parsed.ResetFromString(notification_data_str));
  EXPECT_TRUE(notification_data.Equals(notification_data_parsed));
}

// Set up the P2PNotifier, simulate a successful connection, and send
// a notification with the default target (NOTIFY_OTHERS).  The
// observer should receive only a notification from the call to
// UpdateEnabledTypes().
TEST_F(P2PNotifierTest, NotificationsBasic) {
  syncable::ModelTypeSet enabled_types(
      syncable::BOOKMARKS, syncable::PREFERENCES);

  EXPECT_CALL(mock_observer_, OnNotificationsEnabled());
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(MakePayloadMap(enabled_types),
                                     REMOTE_NOTIFICATION));

  p2p_notifier_.SetUniqueId("sender");

  const char kEmail[] = "foo@bar.com";
  const char kToken[] = "token";
  p2p_notifier_.UpdateCredentials(kEmail, kToken);
  {
    notifier::Subscription expected_subscription;
    expected_subscription.channel = kSyncP2PNotificationChannel;
    expected_subscription.from = kEmail;
    EXPECT_TRUE(notifier::SubscriptionListsEqual(
        fake_push_client_->subscriptions(),
        notifier::SubscriptionList(1, expected_subscription)));
  }
  EXPECT_EQ(kEmail, fake_push_client_->email());
  EXPECT_EQ(kToken, fake_push_client_->token());

  p2p_notifier_.UpdateEnabledTypes(enabled_types);

  ReflectSentNotifications();
  fake_push_client_->EnableNotifications();

  // Sent with target NOTIFY_OTHERS so should not be propagated to
  // |mock_observer_|.
  {
    syncable::ModelTypeSet changed_types(
        syncable::THEMES, syncable::APPS);
    p2p_notifier_.SendNotification(changed_types);
  }

  ReflectSentNotifications();
}

// Set up the P2PNotifier and send out notifications with various
// target settings.  The notifications received by the observer should
// be consistent with the target settings.
TEST_F(P2PNotifierTest, SendNotificationData) {
  syncable::ModelTypeSet enabled_types(
      syncable::BOOKMARKS, syncable::PREFERENCES);

  syncable::ModelTypeSet changed_types(
      syncable::THEMES, syncable::APPS);

  const syncable::ModelTypePayloadMap& changed_payload_map =
      MakePayloadMap(changed_types);

  EXPECT_CALL(mock_observer_, OnNotificationsEnabled());
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(MakePayloadMap(enabled_types),
                                     REMOTE_NOTIFICATION));

  p2p_notifier_.SetUniqueId("sender");
  p2p_notifier_.UpdateCredentials("foo@bar.com", "fake_token");
  p2p_notifier_.UpdateEnabledTypes(enabled_types);

  ReflectSentNotifications();
  fake_push_client_->EnableNotifications();

  ReflectSentNotifications();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_.SendNotificationDataForTest(P2PNotificationData());

  ReflectSentNotifications();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_SELF, changed_types));

  ReflectSentNotifications();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_SELF, changed_types));

  ReflectSentNotifications();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_SELF, syncable::ModelTypeSet()));

  ReflectSentNotifications();

  // Should be dropped.
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_OTHERS, changed_types));

  ReflectSentNotifications();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_OTHERS, changed_types));

  ReflectSentNotifications();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_OTHERS, syncable::ModelTypeSet()));

  ReflectSentNotifications();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_ALL, changed_types));

  ReflectSentNotifications();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_ALL, changed_types));

  ReflectSentNotifications();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_.SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_ALL, syncable::ModelTypeSet()));

  ReflectSentNotifications();
}

}  // namespace

}  // namespace syncer
