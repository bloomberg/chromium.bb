// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

#include <utility>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"

using base::ASCIIToUTF16;

namespace message_center {
namespace {

class TestNotificationDelegate : public message_center::NotificationDelegate {
 public:
  TestNotificationDelegate() = default;

 private:
  ~TestNotificationDelegate() override = default;

  DISALLOW_COPY_AND_ASSIGN(TestNotificationDelegate);
};

class MockDelegate : public MessageCenterTrayDelegate {
 public:
  MockDelegate() {}
  ~MockDelegate() override {}
  void OnMessageCenterTrayChanged() override {}
  bool ShowPopups() override { return show_message_center_success_; }
  void HidePopups() override {}
  bool ShowMessageCenter(bool show_by_click) override {
    return show_popups_success_;
  }
  void HideMessageCenter() override {}
  bool ShowNotifierSettings() override { return true; }

  MessageCenterTray* GetMessageCenterTray() override { return nullptr; }

  bool show_popups_success_ = true;
  bool show_message_center_success_ = true;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

}  // namespace

class MessageCenterTrayTest : public testing::Test {
 public:
  MessageCenterTrayTest() {}
  ~MessageCenterTrayTest() override {}

  void SetUp() override {
    MessageCenter::Initialize();
    delegate_.reset(new MockDelegate);
    message_center_ = MessageCenter::Get();
    message_center_tray_.reset(
        new MessageCenterTray(delegate_.get(), message_center_));
  }

  void TearDown() override {
    message_center_tray_.reset();
    delegate_.reset();
    message_center_ = NULL;
    MessageCenter::Shutdown();
  }

 protected:
  NotifierId DummyNotifierId() {
    return NotifierId();
  }

  Notification* AddNotification(const std::string& id) {
    return AddNotification(id, DummyNotifierId());
  }

  Notification* AddNotification(const std::string& id, NotifierId notifier_id) {
    std::unique_ptr<Notification> notification(
        new Notification(message_center::NOTIFICATION_TYPE_SIMPLE, id,
                         ASCIIToUTF16("Test Web Notification"),
                         ASCIIToUTF16("Notification message body."),
                         gfx::Image(), ASCIIToUTF16("www.test.org"), GURL(),
                         notifier_id, message_center::RichNotificationData(),
                         new TestNotificationDelegate()));
    Notification* notification_ptr = notification.get();
    message_center_->AddNotification(std::move(notification));
    return notification_ptr;
  }
  std::unique_ptr<MockDelegate> delegate_;
  std::unique_ptr<MessageCenterTray> message_center_tray_;
  MessageCenter* message_center_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterTrayTest);
};

TEST_F(MessageCenterTrayTest, BasicMessageCenter) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown =
      message_center_tray_->ShowMessageCenterBubble(false /* show_by_click */);
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble(false /* show_by_click */);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, BasicPopup) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  AddNotification("BasicPopup");

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HidePopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, MessageCenterClosesPopups) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  AddNotification("MessageCenterClosesPopups");

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown =
      message_center_tray_->ShowMessageCenterBubble(false /* show_by_click */);
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  // The notification is queued if it's added when message center is visible.
  AddNotification("MessageCenterClosesPopups2");

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  // There is no queued notification.
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble(false /* show_by_click */);
  message_center_tray_->HideMessageCenterBubble();
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, MessageCenterReopenPopupsForSystemPriority) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      "MessageCenterReopnPopupsForSystemPriority",
      ASCIIToUTF16("Test Web Notification"),
      ASCIIToUTF16("Notification message body."), gfx::Image(),
      ASCIIToUTF16("www.test.org"), GURL(), DummyNotifierId(),
      message_center::RichNotificationData(), NULL /* delegate */));
  notification->SetSystemPriority();
  message_center_->AddNotification(std::move(notification));

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown =
      message_center_tray_->ShowMessageCenterBubble(false /* show_by_click */);
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, ShowBubbleFails) {
  // Now the delegate will signal that it was unable to show a bubble.
  delegate_->show_popups_success_ = false;
  delegate_->show_message_center_success_ = false;

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  AddNotification("ShowBubbleFails");

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown =
      message_center_tray_->ShowMessageCenterBubble(false /* show_by_click */);
  EXPECT_FALSE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble(false /* show_by_click */);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HidePopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, ContextMenuTestWithMessageCenter) {
  const std::string id1 = "id1";
  const std::string id2 = "id2";
  const std::string id3 = "id3";
  Notification* notification1 = AddNotification(id1);

  base::string16 display_source = ASCIIToUTF16("www.test.org");
  NotifierId notifier_id = DummyNotifierId();

  NotifierId notifier_id2(NotifierId::APPLICATION, "sample-app");
  std::unique_ptr<Notification> notification(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, id2,
      ASCIIToUTF16("Test Web Notification"),
      ASCIIToUTF16("Notification message body."), gfx::Image(), display_source,
      GURL(), notifier_id2, message_center::RichNotificationData(),
      new TestNotificationDelegate()));
  message_center_->AddNotification(std::move(notification));

  AddNotification(id3);

  std::unique_ptr<ui::MenuModel> model(
      message_center_tray_->CreateNotificationMenuModel(*notification1));
#if defined(OS_CHROMEOS)
  EXPECT_EQ(2, model->GetItemCount());

  // The second item is to open the settings.
  EXPECT_TRUE(model->IsEnabledAt(0));
  EXPECT_TRUE(model->IsEnabledAt(1));
  model->ActivatedAt(1);
  EXPECT_TRUE(message_center_tray_->message_center_visible());
#else
  EXPECT_EQ(1, model->GetItemCount());
#endif

  message_center_tray_->HideMessageCenterBubble();

  // The first item is to disable notifications from the notifier id. It also
  // removes the notification.
  EXPECT_EQ(3u, message_center_->GetVisibleNotifications().size());
  model->ActivatedAt(0);
  EXPECT_EQ(2u, message_center_->GetVisibleNotifications().size());
}

}  // namespace message_center
