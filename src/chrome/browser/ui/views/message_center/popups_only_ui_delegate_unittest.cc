// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_center/popups_only_ui_delegate.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/popups_only_ui_controller.h"
#include "content/public/test/test_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/views/message_popup_collection.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotifierId;

namespace {

class PopupsOnlyUiDelegateTest : public views::test::WidgetTest {
 public:
  PopupsOnlyUiDelegateTest() {}
  ~PopupsOnlyUiDelegateTest() override {}

  void SetUp() override {
    views::test::WidgetTest::SetUp();
    test_views_delegate()->set_use_desktop_native_widgets(true);
    MessageCenter::Initialize();
  }

  void TearDown() override {
    MessageCenter::Get()->RemoveAllNotifications(
        false, MessageCenter::RemoveType::ALL);

    for (views::Widget* widget : GetAllWidgets())
      widget->CloseNow();
    MessageCenter::Shutdown();
    views::test::WidgetTest::TearDown();
  }

 protected:
  void AddNotification(const std::string& id) {
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id,
        base::ASCIIToUTF16("Test Web Notification"),
        base::ASCIIToUTF16("Notification message body."), gfx::Image(),
        base::ASCIIToUTF16("Some Chrome extension"),
        GURL("chrome-extension://abbccedd"),
        NotifierId(message_center::NotifierType::APPLICATION, id),
        message_center::RichNotificationData(), nullptr);

    MessageCenter::Get()->AddNotification(std::move(notification));
  }

  void UpdateNotification(const std::string& id) {
    auto notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_SIMPLE, id,
        base::ASCIIToUTF16("Updated Test Web Notification"),
        base::ASCIIToUTF16("Notification message body."), gfx::Image(),
        base::ASCIIToUTF16("Some Chrome extension"),
        GURL("chrome-extension://abbccedd"),
        NotifierId(message_center::NotifierType::APPLICATION, id),
        message_center::RichNotificationData(), nullptr);

    MessageCenter::Get()->UpdateNotification(id, std::move(notification));
  }

  void RemoveNotification(const std::string& id) {
    MessageCenter::Get()->RemoveNotification(id, false);
  }

  bool HasNotification(const std::string& id) {
    return !!MessageCenter::Get()->FindVisibleNotificationById(id);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PopupsOnlyUiDelegateTest);
};

TEST_F(PopupsOnlyUiDelegateTest, WebNotificationPopupBubble) {
  auto ui_controller = std::make_unique<PopupsOnlyUiController>();

  // Adding a notification should show the popup bubble.
  AddNotification("id1");
  EXPECT_TRUE(ui_controller->popups_visible());

  // Updating a notification should not hide the popup bubble.
  AddNotification("id2");
  UpdateNotification("id2");
  EXPECT_TRUE(ui_controller->popups_visible());

  // Removing the first notification should not hide the popup bubble.
  RemoveNotification("id1");
  EXPECT_TRUE(ui_controller->popups_visible());

  // Removing the visible notification should hide the popup bubble.
  RemoveNotification("id2");
  EXPECT_FALSE(ui_controller->popups_visible());

  ui_controller->delegate()->HidePopups();
}

TEST_F(PopupsOnlyUiDelegateTest, ManyPopupNotifications) {
  auto ui_controller = std::make_unique<PopupsOnlyUiController>();

  // Add the max visible popup notifications +1, ensure the correct num visible.
  size_t notifications_to_add =
      message_center::kMaxVisiblePopupNotifications + 1;
  for (size_t i = 0; i < notifications_to_add; ++i) {
    std::string id = base::StringPrintf("id%d", static_cast<int>(i));
    AddNotification(id);
  }
  EXPECT_TRUE(ui_controller->popups_visible());
  MessageCenter* message_center = MessageCenter::Get();
  EXPECT_EQ(notifications_to_add, message_center->NotificationCount());
  message_center::NotificationList::PopupNotifications popups =
      message_center->GetPopupNotifications();
  EXPECT_EQ(message_center::kMaxVisiblePopupNotifications, popups.size());
}

}  // namespace
