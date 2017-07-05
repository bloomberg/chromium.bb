// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

// This value should be the same as the duration of reveal animation of
// the settings view of an Android notification.
constexpr int kBackgroundColorChangeDuration = 360;

// The initial background color of the view.
constexpr SkColor kInitialBackgroundColor =
    message_center::kControlButtonBackgroundColor;

}  // anonymous namespace

namespace message_center {

const char NotificationControlButtonsView::kViewClassName[] =
    "NotificationControlButtonsView";

NotificationControlButtonsView::NotificationControlButtonsView(
    MessageView* message_view)
    : message_view_(message_view),
      bgcolor_origin_(kInitialBackgroundColor),
      bgcolor_target_(kInitialBackgroundColor) {
  DCHECK(message_view);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal));
  SetBackground(views::CreateSolidBackground(kInitialBackgroundColor));
}

NotificationControlButtonsView::~NotificationControlButtonsView() = default;

void NotificationControlButtonsView::ShowCloseButton(bool show) {
  if (show && !close_button_) {
    close_button_ = new message_center::PaddedButton(this);
    close_button_->SetImage(views::CustomButton::STATE_NORMAL,
                            message_center::GetCloseIcon());
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
    close_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
    close_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    // Add the button at the last.
    DCHECK_LE(child_count(), 1);
    AddChildView(close_button_);
  } else if (!show && close_button_) {
    RemoveChildView(close_button_);
    close_button_ = nullptr;
  }
}

void NotificationControlButtonsView::ShowSettingsButton(bool show) {
  if (show && !settings_button_) {
    settings_button_ = new message_center::PaddedButton(this);
    settings_button_->SetImage(views::CustomButton::STATE_NORMAL,
                               message_center::GetSettingsIcon());
    settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));

    // Add the button at the first.
    DCHECK_LE(child_count(), 1);
    AddChildViewAt(settings_button_, 0);
  } else if (!show && close_button_) {
    RemoveChildView(settings_button_);
    settings_button_ = nullptr;
  }
}

void NotificationControlButtonsView::SetBackgroundColor(
    const SkColor& target_bgcolor) {
  if (background()->get_color() != target_bgcolor) {
    bgcolor_origin_ = background()->get_color();
    bgcolor_target_ = target_bgcolor;

    if (bgcolor_animation_)
      bgcolor_animation_->End();
    bgcolor_animation_.reset(new gfx::LinearAnimation(this));
    bgcolor_animation_->SetDuration(kBackgroundColorChangeDuration);
    bgcolor_animation_->Start();
  }
}

void NotificationControlButtonsView::RequestFocusOnCloseButton() {
  if (close_button_)
    close_button_->RequestFocus();
}

bool NotificationControlButtonsView::IsCloseButtonFocused() const {
  return close_button_ && close_button_->HasFocus();
}

bool NotificationControlButtonsView::IsSettingsButtonFocused() const {
  return settings_button_ && settings_button_->HasFocus();
}

const char* NotificationControlButtonsView::GetClassName() const {
  return kViewClassName;
}

void NotificationControlButtonsView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (close_button_ && sender == close_button_) {
    message_view_->OnCloseButtonPressed();
  } else if (settings_button_ && sender == settings_button_) {
    message_view_->OnSettingsButtonPressed();
  }
}

void NotificationControlButtonsView::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, bgcolor_animation_.get());

  const SkColor color = gfx::Tween::ColorValueBetween(
      animation->GetCurrentValue(), bgcolor_origin_, bgcolor_target_);
  SetBackground(views::CreateSolidBackground(color));
  SchedulePaint();
}

void NotificationControlButtonsView::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, bgcolor_animation_.get());
  bgcolor_animation_.reset();
  bgcolor_origin_ = bgcolor_target_;
}

void NotificationControlButtonsView::AnimationCanceled(
    const gfx::Animation* animation) {
  // The animation is never cancelled explicitly.
  NOTREACHED();

  bgcolor_origin_ = bgcolor_target_;
  SetBackground(views::CreateSolidBackground(bgcolor_target_));
  SchedulePaint();
}

}  // namespace message_center
