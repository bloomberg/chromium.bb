// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/slidable_message_view.h"

#include "ui/message_center/public/cpp/features.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_swipe_control_view.h"
#include "ui/views/background.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace message_center {

SlidableMessageView::SlidableMessageView(MessageView* message_view)
    : message_view_(message_view) {
  SetFocusBehavior(views::View::FocusBehavior::NEVER);

  // Draw on its own layer to allow bound animation.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetBackground(views::CreateSolidBackground(kSwipeControlBackgroundColor));

  SetLayoutManager(std::make_unique<views::FillLayout>());

  control_view_ = std::make_unique<NotificationSwipeControlView>();
  AddChildView(control_view_.get());
  control_view_->AddObserver(this);

  message_view_->AddSlideObserver(this);
  AddChildView(message_view);
}

SlidableMessageView::~SlidableMessageView() = default;

void SlidableMessageView::OnSlideChanged(const std::string& notification_id) {
  float gesture_amount = message_view_->GetSlideAmount();
  if (gesture_amount == 0) {
    control_view_->HideButtons();
  } else {
    NotificationSwipeControlView::ButtonPosition button_position =
        gesture_amount < 0 ? NotificationSwipeControlView::ButtonPosition::RIGHT
                           : NotificationSwipeControlView::ButtonPosition::LEFT;
    NotificationControlButtonsView* buttons =
        message_view_->GetControlButtonsView();
    bool has_settings_button = buttons->settings_button();
    bool has_snooze_button = buttons->snooze_button();
    control_view_->ShowButtons(button_position, has_settings_button,
                               has_snooze_button);
  }
}

void SlidableMessageView::OnSettingsButtonPressed(const ui::Event& event) {
  message_view_->CloseSwipeControl();
  message_view_->OnSettingsButtonPressed(event);
  control_view_->HideButtons();
}

void SlidableMessageView::OnSnoozeButtonPressed(const ui::Event& event) {
  message_view_->OnSnoozeButtonPressed(event);
  control_view_->HideButtons();
}

void SlidableMessageView::CloseSwipeControl() {
  message_view_->CloseSwipeControl();
}

void SlidableMessageView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
  InvalidateLayout();
}

void SlidableMessageView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
  InvalidateLayout();
}

void SlidableMessageView::UpdateWithNotification(
    const Notification& notification) {
  message_view_->UpdateWithNotification(notification);
}

gfx::Size SlidableMessageView::CalculatePreferredSize() const {
  return message_view_->GetPreferredSize();
}

int SlidableMessageView::GetHeightForWidth(int width) const {
  return message_view_->GetHeightForWidth(width);
}

void SlidableMessageView::UpdateCornerRadius(int top_radius,
                                             int bottom_radius) {
  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<NotificationBackgroundPainter>(
          top_radius, bottom_radius, kSwipeControlBackgroundColor)));
  SchedulePaint();
}

// static
SlidableMessageView* SlidableMessageView::GetFromMessageView(
    MessageView* message_view) {
  DCHECK(message_view);
  DCHECK(message_view->parent());
  DCHECK_EQ(std::string(SlidableMessageView::kViewClassName),
            message_view->parent()->GetClassName());
  return static_cast<SlidableMessageView*>(message_view->parent());
}

}  // namespace message_center
