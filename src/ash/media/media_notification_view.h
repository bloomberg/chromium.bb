// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MEDIA_MEDIA_NOTIFICATION_VIEW_H_
#define ASH_MEDIA_MEDIA_NOTIFICATION_VIEW_H_

#include "ash/ash_export.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace media_session {
enum class MediaSessionAction;
}  // namespace media_session

namespace message_center {
class NotificationHeaderView;
}  // namespace message_center

namespace views {
class ToggleImageButton;
class View;
}  // namespace views

namespace ash {

// MediaNotificationView will show up as a custom notification. It will show the
// currently playing media and provide playback controls. There will also be
// control buttons (e.g. close) in the top right corner that will hide and show
// if the notification is hovered.
class ASH_EXPORT MediaNotificationView : public message_center::MessageView,
                                         public views::ButtonListener {
 public:
  explicit MediaNotificationView(
      const message_center::Notification& notification);
  ~MediaNotificationView() override;

  // message_center::MessageView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override;
  void UpdateControlButtonsVisibility() override;

  // views::View:
  void OnMouseEvent(ui::MouseEvent* event) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  void UpdateWithMediaSessionInfo(
      const media_session::mojom::MediaSessionInfoPtr& session_info);

 private:
  friend class MediaNotificationViewTest;

  void UpdateControlButtonsVisibilityWithNotification(
      const message_center::Notification& notification);

  // Creates an image button with |icon| and adds it to |button_row_|. When
  // clicked it will trigger |action| on the sesssion.
  void CreateMediaButton(const gfx::VectorIcon& icon,
                         media_session::mojom::MediaSessionAction action);

  // View containing close and settings buttons.
  std::unique_ptr<message_center::NotificationControlButtonsView>
      control_buttons_view_;

  // Container views directly attached to this view.
  message_center::NotificationHeaderView* header_row_ = nullptr;
  views::View* button_row_ = nullptr;
  views::ToggleImageButton* play_pause_button_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationView);
};

}  // namespace ash

#endif  // ASH_MEDIA_MEDIA_NOTIFICATION_VIEW_H_
