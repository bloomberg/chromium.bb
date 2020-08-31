// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_control_buttons_view.h"

#include <memory>

#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace message_center {

const char NotificationControlButtonsView::kViewClassName[] =
    "NotificationControlButtonsView";

NotificationControlButtonsView::NotificationControlButtonsView(
    MessageView* message_view)
    : message_view_(message_view), icon_color_(gfx::kChromeIconGrey) {
  DCHECK(message_view);
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  // Do not stretch buttons as that would stretch their focus indicator.
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  // Use layer to change the opacity.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

NotificationControlButtonsView::~NotificationControlButtonsView() = default;

void NotificationControlButtonsView::ShowCloseButton(bool show) {
  if (show && !close_button_) {
    close_button_ = AddChildView(std::make_unique<PaddedButton>(this));
    close_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationCloseButtonIcon, icon_color_));
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
    close_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
    close_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    Layout();
  } else if (!show && close_button_) {
    DCHECK(Contains(close_button_));
    RemoveChildViewT(close_button_);
    close_button_ = nullptr;
  }
}

void NotificationControlButtonsView::ShowSettingsButton(bool show) {
  if (show && !settings_button_) {
    // Add the button next right to the snooze button.
    const int position = snooze_button_ ? 1 : 0;
    settings_button_ =
        AddChildViewAt(std::make_unique<PaddedButton>(this), position);
    settings_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationSettingsButtonIcon, icon_color_));
    settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    Layout();
  } else if (!show && settings_button_) {
    DCHECK(Contains(settings_button_));
    RemoveChildViewT(settings_button_);
    settings_button_ = nullptr;
  }
}

void NotificationControlButtonsView::ShowSnoozeButton(bool show) {
  if (show && !snooze_button_) {
    // Snooze button should appear as the first child.
    snooze_button_ = AddChildViewAt(std::make_unique<PaddedButton>(this), 0);
    snooze_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationSnoozeButtonIcon, icon_color_));
    snooze_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_NOTIFICATION_SNOOZE_BUTTON_TOOLTIP));
    snooze_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_NOTIFICATION_SNOOZE_BUTTON_TOOLTIP));
    snooze_button_->SetBackground(
        views::CreateSolidBackground(SK_ColorTRANSPARENT));
    Layout();
  } else if (!show && snooze_button_) {
    DCHECK(Contains(snooze_button_));
    RemoveChildViewT(snooze_button_);
    snooze_button_ = nullptr;
  }
}

void NotificationControlButtonsView::ShowButtons(bool show) {
  DCHECK(layer());
  // Manipulate the opacity instead of changing the visibility to keep the tab
  // order even when the view is invisible.
  layer()->SetOpacity(show ? 1. : 0.);
  set_can_process_events_within_subtree(show);
}

bool NotificationControlButtonsView::IsAnyButtonFocused() const {
  return (close_button_ && close_button_->HasFocus()) ||
         (settings_button_ && settings_button_->HasFocus()) ||
         (snooze_button_ && snooze_button_->HasFocus());
}

void NotificationControlButtonsView::SetButtonIconColors(SkColor color) {
  if (color == icon_color_)
    return;
  icon_color_ = color;

  if (close_button_) {
    close_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationCloseButtonIcon, icon_color_));
  }
  if (settings_button_) {
    settings_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationSettingsButtonIcon, icon_color_));
  }
  if (snooze_button_) {
    snooze_button_->SetImage(
        views::Button::STATE_NORMAL,
        gfx::CreateVectorIcon(kNotificationSnoozeButtonIcon, icon_color_));
  }
}

const char* NotificationControlButtonsView::GetClassName() const {
  return kViewClassName;
}

#if defined(OS_CHROMEOS)
void NotificationControlButtonsView::OnThemeChanged() {
  View::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_NotificationButtonBackground)));
}
#endif

void NotificationControlButtonsView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (close_button_ && sender == close_button_) {
    message_view_->OnCloseButtonPressed();
  } else if (settings_button_ && sender == settings_button_) {
    message_view_->OnSettingsButtonPressed(event);
  } else if (snooze_button_ && sender == snooze_button_) {
    message_view_->OnSnoozeButtonPressed(event);
  }
}

}  // namespace message_center
