// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/media/media_notification_container.h"
#include "ash/media/media_notification_view.h"
#include "ui/message_center/views/message_view.h"

namespace ash {

class MediaNotificationItem;

// MediaNotificationContainerImpl will show up as a custom notification. It will
// show the currently playing media and provide playback controls. There will
// also be control buttons (e.g. close) in the top right corner that will hide
// and show if the notification is hovered.
class ASH_EXPORT MediaNotificationContainerImpl
    : public message_center::MessageView,
      public MediaNotificationContainer {
 public:
  explicit MediaNotificationContainerImpl(
      const message_center::Notification& notification,
      base::WeakPtr<MediaNotificationItem> item);
  ~MediaNotificationContainerImpl() override;

  // message_center::MessageView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override;
  void SetExpanded(bool expanded) override;
  void UpdateCornerRadius(int top_radius, int bottom_radius) override;

  // MediaNotificationContainer:
  void OnExpanded(bool expanded) override;

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  void UpdateControlButtonsVisibilityWithNotification(
      const message_center::Notification& notification);

  // View containing close and settings buttons.
  std::unique_ptr<message_center::NotificationControlButtonsView>
      control_buttons_view_;

  MediaNotificationView view_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationContainerImpl);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_CONTAINER_IMPL_H_
