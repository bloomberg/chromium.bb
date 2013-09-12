// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_tray.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_types.h"

namespace message_center {
namespace {

class MockDelegate : public MessageCenterTrayDelegate {
 public:
  MockDelegate()
      : show_popups_success_(true),
        show_message_center_success_(true) {}
  virtual ~MockDelegate() {}
  virtual void OnMessageCenterTrayChanged() OVERRIDE {}
  virtual bool ShowPopups() OVERRIDE {
    return show_message_center_success_;
  }
  virtual void HidePopups() OVERRIDE {}
  virtual bool ShowMessageCenter() OVERRIDE {
    return show_popups_success_;
  }
  virtual void HideMessageCenter() OVERRIDE {}
  virtual bool ShowNotifierSettings() OVERRIDE {
    return false;
  }

  virtual MessageCenterTray* GetMessageCenterTray() OVERRIDE {
    return NULL;
  }

  bool show_popups_success_;
  bool show_message_center_success_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class MessageCenterTrayTest : public testing::Test {
 public:
  MessageCenterTrayTest() {}
  virtual ~MessageCenterTrayTest() {}

  virtual void SetUp() {
    MessageCenter::Initialize();
    delegate_.reset(new MockDelegate);
    message_center_ = MessageCenter::Get();
    message_center_tray_.reset(
        new MessageCenterTray(delegate_.get(), message_center_));
  }

  virtual void TearDown() {
    message_center_tray_.reset();
    delegate_.reset();
    message_center_ = NULL;
    MessageCenter::Shutdown();
  }

 protected:
  void AddNotification(const std::string& id) {
    scoped_ptr<Notification> notification(
        new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                         id,
                         ASCIIToUTF16("Test Web Notification"),
                         ASCIIToUTF16("Notification message body."),
                         gfx::Image(),
                         ASCIIToUTF16("www.test.org"),
                         NotifierId(),
                         message_center::RichNotificationData(),
                         NULL /* delegate */));
    message_center_->AddNotification(notification.Pass());
  }
  scoped_ptr<MockDelegate> delegate_;
  scoped_ptr<MessageCenterTray> message_center_tray_;
  MessageCenter* message_center_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MessageCenterTrayTest);
};

}  // namespace

TEST_F(MessageCenterTrayTest, BasicMessageCenter) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ToggleMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->ToggleMessageCenterBubble();

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

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_TRUE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  // The notification is queued if it's added when message center is visible.
  AddNotification("MessageCenterClosesPopups2");

  message_center_tray_->ShowPopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_TRUE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  // The queued notification appears as a popup.
  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ShowMessageCenterBubble();
  message_center_tray_->HideMessageCenterBubble();
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

TEST_F(MessageCenterTrayTest, MessageCenterReopenPopupsForSystemPriority) {
  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  scoped_ptr<Notification> notification(
      new Notification(message_center::NOTIFICATION_TYPE_SIMPLE,
                       "MessageCenterReopnPopupsForSystemPriority",
                       ASCIIToUTF16("Test Web Notification"),
                       ASCIIToUTF16("Notification message body."),
                       gfx::Image(),
                       ASCIIToUTF16("www.test.org"),
                       NotifierId(),
                       message_center::RichNotificationData(),
                       NULL /* delegate */));
  notification->SetSystemPriority();
  message_center_->AddNotification(notification.Pass());

  ASSERT_TRUE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  bool shown = message_center_tray_->ShowMessageCenterBubble();
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

  bool shown = message_center_tray_->ShowMessageCenterBubble();
  EXPECT_FALSE(shown);

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HideMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->ToggleMessageCenterBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());

  message_center_tray_->HidePopupBubble();

  ASSERT_FALSE(message_center_tray_->popups_visible());
  ASSERT_FALSE(message_center_tray_->message_center_visible());
}

}  // namespace message_center
