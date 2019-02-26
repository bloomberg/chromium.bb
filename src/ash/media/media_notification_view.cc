// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_view.h"

#include "ash/media/media_notification_constants.h"
#include "ash/media/media_notification_controller.h"
#include "ash/shell.h"
#include "components/vector_icons/vector_icons.h"
#include "services/media_session/public/mojom/media_session.mojom.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/style/typography.h"

namespace ash {

using media_session::mojom::MediaSessionAction;

namespace {

// Dimensions.
constexpr gfx::Insets kButtonRowPadding(0, 12, 16, 12);
constexpr int kMediaButtonIconSize = 32;

SkColor GetMediaNotificationColor(const views::View& view) {
  return views::style::GetColor(view, views::style::CONTEXT_LABEL,
                                views::style::STYLE_PRIMARY);
}

}  // namespace

MediaNotificationView::MediaNotificationView(
    const message_center::Notification& notification)
    : message_center::MessageView(notification) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0));

  // |controls_button_view_| has the common notification control buttons.
  control_buttons_view_ =
      std::make_unique<message_center::NotificationControlButtonsView>(this);
  control_buttons_view_->set_owned_by_client();

  // |header_row_| contains app_icon, app_name, control buttons, etc.
  header_row_ = new message_center::NotificationHeaderView(
      control_buttons_view_.get(), this);
  header_row_->SetExpandButtonEnabled(false);
  header_row_->SetAppName(
      message_center::MessageCenter::Get()->GetSystemNotificationAppName());
  AddChildView(header_row_);

  // |button_row_| contains the buttons for controlling playback.
  button_row_ = new views::View();
  auto* button_row_layout =
      button_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, kButtonRowPadding, 16));
  button_row_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  button_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  AddChildView(button_row_);

  CreateMediaButton(vector_icons::kMediaPreviousTrackIcon,
                    MediaSessionAction::kPreviousTrack);
  CreateMediaButton(vector_icons::kMediaSeekBackwardIcon,
                    MediaSessionAction::kSeekBackward);

  // |play_pause_button_| toggles playback.
  play_pause_button_ = views::CreateVectorToggleImageButton(this);
  play_pause_button_->set_tag(static_cast<int>(MediaSessionAction::kPlay));
  SkColor play_button_color = GetMediaNotificationColor(*play_pause_button_);
  views::SetImageFromVectorIcon(play_pause_button_,
                                vector_icons::kPlayArrowIcon,
                                kMediaButtonIconSize, play_button_color);
  views::SetToggledImageFromVectorIcon(play_pause_button_,
                                       vector_icons::kPauseIcon,
                                       kMediaButtonIconSize, play_button_color);
  button_row_->AddChildView(play_pause_button_);

  CreateMediaButton(vector_icons::kMediaSeekForwardIcon,
                    MediaSessionAction::kSeekForward);
  CreateMediaButton(vector_icons::kMediaNextTrackIcon,
                    MediaSessionAction::kNextTrack);

  // TODO(beccahughes): Add remaining UI for notification.

  UpdateControlButtonsVisibilityWithNotification(notification);
  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);

  Shell::Get()->media_notification_controller()->SetView(this);
}

MediaNotificationView::~MediaNotificationView() {
  Shell::Get()->media_notification_controller()->SetView(nullptr);
}

void MediaNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  MessageView::UpdateWithNotification(notification);

  UpdateControlButtonsVisibilityWithNotification(notification);
  Layout();
  SchedulePaint();
}

message_center::NotificationControlButtonsView*
MediaNotificationView::GetControlButtonsView() const {
  return control_buttons_view_.get();
}

void MediaNotificationView::UpdateControlButtonsVisibility() {
  const bool target_visibility =
      (IsMouseHovered() || control_buttons_view_->IsCloseButtonFocused() ||
       control_buttons_view_->IsSettingsButtonFocused()) &&
      (GetMode() != Mode::SETTING);

  control_buttons_view_->SetVisible(target_visibility);
}

void MediaNotificationView::OnMouseEvent(ui::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_EXITED:
      UpdateControlButtonsVisibility();
      break;
    default:
      break;
  }

  View::OnMouseEvent(event);
}

void MediaNotificationView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (sender->parent() == button_row_) {
    message_center::MessageCenter::Get()->ClickOnNotificationButton(
        notification_id(), sender->tag());
  }
}

void MediaNotificationView::UpdateWithMediaSessionInfo(
    const media_session::mojom::MediaSessionInfoPtr& session_info) {
  bool playing = session_info->playback_state ==
                 media_session::mojom::MediaPlaybackState::kPlaying;
  play_pause_button_->SetToggled(playing);
  play_pause_button_->set_tag(
      playing ? static_cast<int>(MediaSessionAction::kPause)
              : static_cast<int>(MediaSessionAction::kPlay));
}

void MediaNotificationView::UpdateControlButtonsVisibilityWithNotification(
    const message_center::Notification& notification) {
  // Media notifications do not use the settings and snooze buttons.
  DCHECK(!notification.should_show_settings_button());
  DCHECK(!notification.should_show_snooze_button());

  control_buttons_view_->ShowCloseButton(!notification.pinned());
  UpdateControlButtonsVisibility();
}

void MediaNotificationView::CreateMediaButton(const gfx::VectorIcon& icon,
                                              MediaSessionAction action) {
  views::ImageButton* button = views::CreateVectorImageButton(this);
  button->set_tag(static_cast<int>(action));
  views::SetImageFromVectorIcon(button, icon, kMediaButtonIconSize,
                                GetMediaNotificationColor(*button));
  button_row_->AddChildView(button);
}

}  // namespace ash
