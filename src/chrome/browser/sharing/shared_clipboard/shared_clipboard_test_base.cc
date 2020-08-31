// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_test_base.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/proto/sharing_message.pb.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/message_center/public/cpp/notification.h"

SharedClipboardTestBase::SharedClipboardTestBase()
    : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

SharedClipboardTestBase::~SharedClipboardTestBase() = default;

void SharedClipboardTestBase::SetUp() {
  notification_tester_ =
      std::make_unique<NotificationDisplayServiceTester>(&profile_);
  sharing_service_ = std::make_unique<MockSharingService>();
  ui::TestClipboard::CreateForCurrentThread();
}

void SharedClipboardTestBase::TearDown() {
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

chrome_browser_sharing::SharingMessage SharedClipboardTestBase::CreateMessage(
    const std::string& guid,
    const std::string& device_name) {
  chrome_browser_sharing::SharingMessage message;
  message.set_sender_guid(guid);
  message.set_sender_device_name(device_name);
  return message;
}

std::string SharedClipboardTestBase::GetClipboardText() {
  base::string16 text;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &text);
  return base::UTF16ToUTF8(text);
}

SkBitmap SharedClipboardTestBase::GetClipboardImage() {
  return ui::clipboard_test_util::ReadImage(
      ui::Clipboard::GetForCurrentThread());
}

bool SharedClipboardTestBase::HasImageNotification() {
  auto notifications = notification_tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::SHARING);
  if (notifications.size() != 1u)
    return false;

  return notifications[0].type() == message_center::NOTIFICATION_TYPE_IMAGE;
}

bool SharedClipboardTestBase::HasProgressNotification() {
  auto notifications = notification_tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::SHARING);
  if (notifications.size() != 1u)
    return false;

  return notifications[0].type() == message_center::NOTIFICATION_TYPE_PROGRESS;
}

message_center::Notification SharedClipboardTestBase::GetNotification() {
  auto notifications = notification_tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::SHARING);
  EXPECT_EQ(notifications.size(), 1u);

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_SIMPLE, notification.type());

  return notification;
}

message_center::Notification
SharedClipboardTestBase::GetProgressNotification() {
  auto notifications = notification_tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::SHARING);
  EXPECT_EQ(notifications.size(), 1u);

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_PROGRESS, notification.type());

  return notification;
}

message_center::Notification SharedClipboardTestBase::GetImageNotification() {
  auto notifications = notification_tester_->GetDisplayedNotificationsForType(
      NotificationHandler::Type::SHARING);
  EXPECT_EQ(notifications.size(), 1u);

  const message_center::Notification& notification = notifications[0];
  EXPECT_EQ(message_center::NOTIFICATION_TYPE_IMAGE, notification.type());

  return notification;
}
