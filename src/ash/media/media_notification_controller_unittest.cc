// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_controller.h"

#include <memory>

#include "ash/media/media_notification_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "ui/message_center/message_center.h"

namespace ash {

using media_session::mojom::AudioFocusRequestState;

namespace {

bool IsMediaNotificationShown() {
  return message_center::MessageCenter::Get()->FindVisibleNotificationById(
      kMediaSessionNotificationId);
}

int GetVisibleNotificationCount() {
  return message_center::MessageCenter::Get()->GetVisibleNotifications().size();
}

int GetPopupNotificationCount() {
  return message_center::MessageCenter::Get()->GetPopupNotifications().size();
}

}  // namespace

class MediaNotificationControllerTest : public AshTestBase {
 public:
  MediaNotificationControllerTest() = default;
  ~MediaNotificationControllerTest() override = default;

  // AshTestBase
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMediaSessionNotification);

    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationControllerTest);
};

TEST_F(MediaNotificationControllerTest, OnActiveSessionChanged) {
  EXPECT_FALSE(IsMediaNotificationShown());
  EXPECT_EQ(0, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());

  Shell::Get()->media_notification_controller()->OnActiveSessionChanged(
      AudioFocusRequestState::New());
  EXPECT_TRUE(IsMediaNotificationShown());
  EXPECT_EQ(1, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());

  Shell::Get()->media_notification_controller()->OnActiveSessionChanged(
      AudioFocusRequestState::New());
  EXPECT_TRUE(IsMediaNotificationShown());
  EXPECT_EQ(1, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());

  Shell::Get()->media_notification_controller()->OnActiveSessionChanged(
      nullptr);
  EXPECT_FALSE(IsMediaNotificationShown());
  EXPECT_EQ(0, GetVisibleNotificationCount());
  EXPECT_EQ(0, GetPopupNotificationCount());
}

TEST_F(MediaNotificationControllerTest, OnActiveSessionChanged_Noop) {
  EXPECT_FALSE(IsMediaNotificationShown());

  Shell::Get()->media_notification_controller()->OnActiveSessionChanged(
      nullptr);
  EXPECT_FALSE(IsMediaNotificationShown());
}

TEST_F(MediaNotificationControllerTest, NotificationHasCustomViewType) {
  EXPECT_FALSE(IsMediaNotificationShown());

  Shell::Get()->media_notification_controller()->OnActiveSessionChanged(
      AudioFocusRequestState::New());
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          kMediaSessionNotificationId);
  EXPECT_TRUE(notification);

  EXPECT_EQ(kMediaSessionNotificationCustomViewType,
            notification->custom_view_type());
}

}  // namespace ash
