// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/notifier/p2p_notifier.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "jingle/notifier/base/fake_base_task.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/push_client.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/notifier/mock_sync_notifier_observer.h"
#include "sync/syncable/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {

namespace {

using ::testing::_;
using ::testing::Mock;
using ::testing::StrictMock;

class P2PNotifierTest : public testing::Test {
 protected:
  P2PNotifierTest() {
    notifier_options_.request_context_getter =
        new TestURLRequestContextGetter(message_loop_.message_loop_proxy());
  }

  virtual ~P2PNotifierTest() {}

  virtual void SetUp() OVERRIDE {
    p2p_notifier_.reset(new P2PNotifier(notifier_options_, NOTIFY_OTHERS));
    p2p_notifier_->AddObserver(&mock_observer_);
  }

  virtual void TearDown() OVERRIDE {
    message_loop_.RunAllPending();
    p2p_notifier_->RemoveObserver(&mock_observer_);
    p2p_notifier_.reset();
    message_loop_.RunAllPending();
  }

  syncable::ModelTypePayloadMap MakePayloadMap(
      syncable::ModelTypeSet types) {
    return syncable::ModelTypePayloadMapFromEnumSet(types, "");
  }

  // The sockets created by the XMPP code expect an IO loop.
  MessageLoopForIO message_loop_;
  notifier::NotifierOptions notifier_options_;
  scoped_ptr<P2PNotifier> p2p_notifier_;
  StrictMock<MockSyncNotifierObserver> mock_observer_;
  notifier::FakeBaseTask fake_base_task_;
};

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

TEST_F(P2PNotifierTest, NotificationsBasic) {
  syncable::ModelTypeSet enabled_types(
      syncable::BOOKMARKS, syncable::PREFERENCES);

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(MakePayloadMap(enabled_types),
                                     REMOTE_NOTIFICATION));

  p2p_notifier_->ReflectSentNotificationsForTest();

  p2p_notifier_->SetUniqueId("sender");
  p2p_notifier_->UpdateCredentials("foo@bar.com", "fake_token");
  p2p_notifier_->UpdateEnabledTypes(enabled_types);

  p2p_notifier_->SimulateConnectForTest(fake_base_task_.AsWeakPtr());

  // Sent with target NOTIFY_OTHERS so should not be propagated to
  // |mock_observer_|.
  {
    syncable::ModelTypeSet changed_types(
        syncable::THEMES, syncable::APPS);
    p2p_notifier_->SendNotification(changed_types);
  }
}

TEST_F(P2PNotifierTest, SendNotificationData) {
  syncable::ModelTypeSet enabled_types(
      syncable::BOOKMARKS, syncable::PREFERENCES);

  syncable::ModelTypeSet changed_types(
      syncable::THEMES, syncable::APPS);

  const syncable::ModelTypePayloadMap& changed_payload_map =
      MakePayloadMap(changed_types);

  EXPECT_CALL(mock_observer_, OnNotificationStateChange(true));
  EXPECT_CALL(mock_observer_,
              OnIncomingNotification(MakePayloadMap(enabled_types),
                                     REMOTE_NOTIFICATION));

  p2p_notifier_->ReflectSentNotificationsForTest();

  p2p_notifier_->SetUniqueId("sender");
  p2p_notifier_->UpdateCredentials("foo@bar.com", "fake_token");
  p2p_notifier_->UpdateEnabledTypes(enabled_types);

  p2p_notifier_->SimulateConnectForTest(fake_base_task_.AsWeakPtr());

  message_loop_.RunAllPending();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(P2PNotificationData());

  message_loop_.RunAllPending();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_SELF, changed_types));

  message_loop_.RunAllPending();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_SELF, changed_types));

  message_loop_.RunAllPending();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_SELF, syncable::ModelTypeSet()));

  message_loop_.RunAllPending();

  // Should be dropped.
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_OTHERS, changed_types));

  message_loop_.RunAllPending();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_OTHERS, changed_types));

  message_loop_.RunAllPending();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_OTHERS, syncable::ModelTypeSet()));

  message_loop_.RunAllPending();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender", NOTIFY_ALL, changed_types));

  message_loop_.RunAllPending();

  // Should be propagated.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  EXPECT_CALL(mock_observer_, OnIncomingNotification(changed_payload_map,
                                                     REMOTE_NOTIFICATION));
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_ALL, changed_types));

  message_loop_.RunAllPending();

  // Should be dropped.
  Mock::VerifyAndClearExpectations(&mock_observer_);
  p2p_notifier_->SendNotificationDataForTest(
      P2PNotificationData("sender2", NOTIFY_ALL, syncable::ModelTypeSet()));

  message_loop_.RunAllPending();
}

}  // namespace

}  // namespace sync_notifier
