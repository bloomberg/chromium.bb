// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge_lacros.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/notifications/notification_platform_bridge_delegate.h"
#include "chromeos/crosapi/mojom/message_center.mojom.h"
#include "chromeos/crosapi/mojom/notification.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using gfx::test::AreBitmapsEqual;
using gfx::test::AreImagesEqual;

namespace {

// Tracks user actions that would be passed into chrome's cross-platform
// notification subsystem.
class TestPlatformBridgeDelegate : public NotificationPlatformBridgeDelegate {
 public:
  TestPlatformBridgeDelegate() = default;
  TestPlatformBridgeDelegate(const TestPlatformBridgeDelegate&) = delete;
  TestPlatformBridgeDelegate& operator=(const TestPlatformBridgeDelegate&) =
      delete;
  ~TestPlatformBridgeDelegate() override = default;

  // NotificationPlatformBridgeDelegate:
  void HandleNotificationClosed(const std::string& id, bool by_user) override {
    ++closed_count_;
  }
  void HandleNotificationClicked(const std::string& id) override {
    ++clicked_count_;
  }
  void HandleNotificationButtonClicked(
      const std::string& id,
      int button_index,
      const base::Optional<base::string16>& reply) override {
    ++button_clicked_count_;
    last_button_index_ = button_index;
  }
  void HandleNotificationSettingsButtonClicked(const std::string& id) override {
    ++settings_button_clicked_count_;
  }
  void DisableNotification(const std::string& id) override {
    ++disabled_count_;
  }

  // Public because this is test code.
  int closed_count_ = 0;
  int clicked_count_ = 0;
  int button_clicked_count_ = 0;
  int last_button_index_ = -1;
  int settings_button_clicked_count_ = 0;
  int disabled_count_ = 0;
};

// Simulates MessageCenterAsh in ash-chrome.
class TestMessageCenter : public crosapi::mojom::MessageCenter {
 public:
  explicit TestMessageCenter(
      mojo::PendingReceiver<crosapi::mojom::MessageCenter> receiver)
      : receiver_(this, std::move(receiver)) {}
  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;
  ~TestMessageCenter() override = default;

  void DisplayNotification(
      crosapi::mojom::NotificationPtr notification,
      mojo::PendingRemote<crosapi::mojom::NotificationDelegate> delegate)
      override {
    ++display_count_;
    last_notification_ = std::move(notification);
    // Use unique_ptr to avoid having to deal with unbinding the remote.
    last_notification_delegate_remote_ =
        std::make_unique<mojo::Remote<crosapi::mojom::NotificationDelegate>>(
            std::move(delegate));
  }

  void CloseNotification(const std::string& id) override {
    ++close_count_;
    last_close_id_ = id;
  }

  void GetDisplayedNotifications(
      GetDisplayedNotificationsCallback callback) override {}

  int display_count_ = 0;
  std::string last_display_id_;
  crosapi::mojom::NotificationPtr last_notification_;
  std::unique_ptr<mojo::Remote<crosapi::mojom::NotificationDelegate>>
      last_notification_delegate_remote_;
  int close_count_ = 0;
  std::string last_close_id_;
  mojo::Receiver<crosapi::mojom::MessageCenter> receiver_;
};

class NotificationPlatformBridgeLacrosTest : public testing::Test {
 public:
  NotificationPlatformBridgeLacrosTest() = default;
  ~NotificationPlatformBridgeLacrosTest() override = default;

  // Public because this is test code.
  content::BrowserTaskEnvironment task_environment_;
  mojo::Remote<crosapi::mojom::MessageCenter> message_center_remote_;
  TestMessageCenter test_message_center_{
      message_center_remote_.BindNewPipeAndPassReceiver()};
  TestPlatformBridgeDelegate bridge_delegate_;
  NotificationPlatformBridgeLacros bridge_{&bridge_delegate_,
                                           &message_center_remote_};
};

TEST_F(NotificationPlatformBridgeLacrosTest, SerializationSimple) {
  // Create a message_center notification.
  message_center::RichNotificationData rich_data;
  rich_data.priority = 2;
  rich_data.never_timeout = true;
  base::Time now = base::Time::Now();
  rich_data.timestamp = now;
  rich_data.renotify = true;
  rich_data.accessible_name = ASCIIToUTF16("accessible_name");
  rich_data.fullscreen_visibility =
      message_center::FullscreenVisibility::OVER_USER;

  // Create badge and icon with both low DPI and high DPI versions.
  gfx::Image badge = gfx::test::CreateImage(1, 2);
  badge.AsImageSkia().AddRepresentation(
      gfx::ImageSkiaRep(gfx::test::CreateBitmap(2, 4), /*scale=*/2.0f));
  rich_data.small_image = badge;
  gfx::Image icon = gfx::test::CreateImage(3, 4);
  icon.AsImageSkia().AddRepresentation(
      gfx::ImageSkiaRep(gfx::test::CreateBitmap(6, 8), /*scale=*/2.0f));

  message_center::ButtonInfo button1;
  button1.title = ASCIIToUTF16("button1");
  message_center::ButtonInfo button2;
  button2.title = ASCIIToUTF16("button2");
  rich_data.buttons = {button1, button2};

  message_center::Notification ui_notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "test_id",
      ASCIIToUTF16("title"), ASCIIToUTF16("message"), icon,
      ASCIIToUTF16("display_source"), GURL("http://example.com/"),
      message_center::NotifierId(), rich_data, nullptr);

  // Show the notification.
  bridge_.Display(NotificationHandler::Type::TRANSIENT, /*profile=*/nullptr,
                  ui_notification, /*metadata=*/nullptr);
  message_center_remote_.FlushForTesting();
  EXPECT_EQ(1, test_message_center_.display_count_);

  // Fields were serialized properly.
  crosapi::mojom::Notification* last_notification =
      test_message_center_.last_notification_.get();
  EXPECT_EQ("test_id", last_notification->id);
  EXPECT_EQ(ASCIIToUTF16("title"), last_notification->title);
  EXPECT_EQ(ASCIIToUTF16("message"), last_notification->message);
  EXPECT_EQ(ASCIIToUTF16("display_source"), last_notification->display_source);
  EXPECT_EQ("http://example.com/", last_notification->origin_url->spec());
  EXPECT_EQ(2, last_notification->priority);
  EXPECT_TRUE(last_notification->require_interaction);
  EXPECT_EQ(now, last_notification->timestamp);
  EXPECT_EQ(ASCIIToUTF16("accessible_name"),
            last_notification->accessible_name);
  EXPECT_EQ(crosapi::mojom::FullscreenVisibility::kOverUser,
            last_notification->fullscreen_visibility);

  ASSERT_FALSE(last_notification->badge.isNull());
  EXPECT_TRUE(last_notification->badge.HasRepresentation(1.0f));
  EXPECT_TRUE(last_notification->badge.HasRepresentation(2.0f));
  EXPECT_TRUE(AreImagesEqual(badge, gfx::Image(last_notification->badge)));

  ASSERT_FALSE(last_notification->icon.isNull());
  EXPECT_TRUE(last_notification->icon.HasRepresentation(1.0f));
  EXPECT_TRUE(last_notification->icon.HasRepresentation(2.0f));
  EXPECT_TRUE(AreImagesEqual(icon, gfx::Image(last_notification->icon)));

  ASSERT_EQ(2u, last_notification->buttons.size());
  EXPECT_EQ(ASCIIToUTF16("button1"), last_notification->buttons[0]->title);
  EXPECT_EQ(ASCIIToUTF16("button2"), last_notification->buttons[1]->title);
}

TEST_F(NotificationPlatformBridgeLacrosTest, SerializationImage) {
  // Create a message_center notification.
  gfx::Image image = gfx::test::CreateImage(5, 6);
  message_center::RichNotificationData rich_data;
  rich_data.image = image;
  message_center::Notification ui_notification(
      message_center::NOTIFICATION_TYPE_IMAGE, "test_id", base::string16(),
      base::string16(), gfx::Image(), base::string16(), GURL(),
      message_center::NotifierId(), rich_data, nullptr);

  // Show the notification.
  bridge_.Display(NotificationHandler::Type::TRANSIENT, /*profile=*/nullptr,
                  ui_notification, /*metadata=*/nullptr);
  message_center_remote_.FlushForTesting();

  // Notification was shown with correct fields.
  crosapi::mojom::Notification* last_notification =
      test_message_center_.last_notification_.get();
  ASSERT_TRUE(last_notification);
  ASSERT_FALSE(last_notification->image.isNull());
  EXPECT_TRUE(AreImagesEqual(image, gfx::Image(last_notification->image)));
}

TEST_F(NotificationPlatformBridgeLacrosTest, SerializationList) {
  // Create a message_center notification.
  message_center::NotificationItem item1;
  item1.title = ASCIIToUTF16("title1");
  item1.message = ASCIIToUTF16("message1");
  message_center::NotificationItem item2;
  item2.title = ASCIIToUTF16("title2");
  item2.message = ASCIIToUTF16("message2");
  message_center::RichNotificationData rich_data;
  rich_data.items = {item1, item2};
  message_center::Notification ui_notification(
      message_center::NOTIFICATION_TYPE_MULTIPLE, "test_id", base::string16(),
      base::string16(), gfx::Image(), base::string16(), GURL(),
      message_center::NotifierId(), rich_data, nullptr);

  // Show the notification.
  bridge_.Display(NotificationHandler::Type::TRANSIENT, /*profile=*/nullptr,
                  ui_notification, /*metadata=*/nullptr);
  message_center_remote_.FlushForTesting();

  // Notification was shown with correct fields.
  crosapi::mojom::Notification* last_notification =
      test_message_center_.last_notification_.get();
  ASSERT_TRUE(last_notification);
  ASSERT_EQ(2u, last_notification->items.size());
  EXPECT_EQ(ASCIIToUTF16("title1"), last_notification->items[0]->title);
  EXPECT_EQ(ASCIIToUTF16("message1"), last_notification->items[0]->message);
  EXPECT_EQ(ASCIIToUTF16("title2"), last_notification->items[1]->title);
  EXPECT_EQ(ASCIIToUTF16("message2"), last_notification->items[1]->message);
}

TEST_F(NotificationPlatformBridgeLacrosTest, SerializationProgress) {
  // Create a message_center notification.
  message_center::RichNotificationData rich_data;
  rich_data.progress = 55;
  rich_data.progress_status = ASCIIToUTF16("status");
  message_center::Notification ui_notification(
      message_center::NOTIFICATION_TYPE_PROGRESS, "test_id", base::string16(),
      base::string16(), gfx::Image(), base::string16(), GURL(),
      message_center::NotifierId(), rich_data, nullptr);

  // Show the notification.
  bridge_.Display(NotificationHandler::Type::TRANSIENT, /*profile=*/nullptr,
                  ui_notification, /*metadata=*/nullptr);
  message_center_remote_.FlushForTesting();

  // Notification was shown with correct fields.
  crosapi::mojom::Notification* last_notification =
      test_message_center_.last_notification_.get();
  ASSERT_TRUE(last_notification);
  EXPECT_EQ(55, last_notification->progress);
  EXPECT_EQ(ASCIIToUTF16("status"), last_notification->progress_status);
}

TEST_F(NotificationPlatformBridgeLacrosTest, UserActions) {
  // Create a test notification.
  message_center::Notification ui_notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, "test_id", base::string16(),
      base::string16(), gfx::Image(), base::string16(), GURL(),
      message_center::NotifierId(), {}, nullptr);

  // Show the notification.
  bridge_.Display(NotificationHandler::Type::TRANSIENT, /*profile=*/nullptr,
                  ui_notification, /*metadata=*/nullptr);
  message_center_remote_.FlushForTesting();
  EXPECT_EQ(1, test_message_center_.display_count_);

  // Grab the last notification.
  crosapi::mojom::Notification* last_notification =
      test_message_center_.last_notification_.get();
  EXPECT_EQ("test_id", last_notification->id);

  // Grab the mojo::Remote<> for the last notification's delegate.
  ASSERT_TRUE(test_message_center_.last_notification_delegate_remote_);
  mojo::Remote<crosapi::mojom::NotificationDelegate>&
      notification_delegate_remote =
          *test_message_center_.last_notification_delegate_remote_;

  // Verify remote user actions are forwarded through to |bridge_delegate_|.
  notification_delegate_remote->OnNotificationClicked();
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate_.clicked_count_);

  notification_delegate_remote->OnNotificationButtonClicked(/*button_index=*/0);
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate_.button_clicked_count_);
  EXPECT_EQ(0, bridge_delegate_.last_button_index_);

  notification_delegate_remote->OnNotificationSettingsButtonClicked();
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate_.settings_button_clicked_count_);

  notification_delegate_remote->OnNotificationDisabled();
  notification_delegate_remote.FlushForTesting();
  EXPECT_EQ(1, bridge_delegate_.disabled_count_);

  // Close the notification.
  bridge_.Close(/*profile=*/nullptr, "test_id");
  message_center_remote_.FlushForTesting();
  EXPECT_EQ(1, test_message_center_.close_count_);
  EXPECT_EQ("test_id", test_message_center_.last_close_id_);
}

}  // namespace
