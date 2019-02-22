// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/notification.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace exo {
namespace {

using NotificationTest = test::ExoTestBase;

void Close(int* close_call_count, bool by_user) {
  (*close_call_count)++;
}

TEST_F(NotificationTest, CloseCallback) {
  auto* message_center = message_center::MessageCenter::Get();

  // Clear all notifications.
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);

  // Params for test notification.
  const std::string title = "TEST title";
  const std::string message = "TEST message";
  const std::string display_source = "TEST display_source";
  const std::string notification_id = "exo-notification.test";
  const std::string notifier_id = "exo-notification-test";

  // For the close callback.
  int close_call_count = 0;

  Notification notification(
      title, message, display_source, notification_id, notifier_id,
      base::BindRepeating(&Close, base::Unretained(&close_call_count)));

  EXPECT_EQ(close_call_count, 0);

  EXPECT_NE(nullptr,
            message_center->FindVisibleNotificationById(notification_id));

  // Closes notification.
  notification.Close();

  EXPECT_EQ(nullptr,
            message_center->FindVisibleNotificationById(notification_id));

  // Expected to be called once
  EXPECT_EQ(close_call_count, 1);

  // Clear all notifications.
  message_center->RemoveAllNotifications(
      false /* by_user */, message_center::MessageCenter::RemoveType::ALL);
}

}  // namespace
}  // namespace exo
