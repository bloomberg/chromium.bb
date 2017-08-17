// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_button.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/views/constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace message_center {

NotificationButton::NotificationButton(views::ButtonListener* listener)
    : views::Button(listener), icon_(NULL), title_(NULL) {
  SetFocusForPlatform();
  // Create a background so that it does not change when the MessageView
  // background changes to show touch feedback
  SetBackground(views::CreateSolidBackground(kNotificationBackgroundColor));
  set_notify_enter_exit_on_child(true);
  SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal,
      gfx::Insets(kButtonVerticalPadding,
                  message_center::kButtonHorizontalPadding),
      message_center::kButtonIconToTitlePadding));
  SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      message_center::kFocusBorderColor, gfx::Insets(1, 2, 2, 2)));
}

NotificationButton::~NotificationButton() {
}

void NotificationButton::SetIcon(const gfx::ImageSkia& image) {
  if (icon_ != NULL)
    delete icon_;  // This removes the icon from this view's children.
  if (image.isNull()) {
    icon_ = NULL;
  } else {
    icon_ = new views::ImageView();
    icon_->SetImageSize(gfx::Size(message_center::kNotificationButtonIconSize,
                                  message_center::kNotificationButtonIconSize));
    icon_->SetImage(image);
    icon_->SetHorizontalAlignment(views::ImageView::LEADING);
    icon_->SetVerticalAlignment(views::ImageView::LEADING);
    icon_->SetBorder(views::CreateEmptyBorder(
        message_center::kButtonIconTopPadding, 0, 0, 0));
    AddChildViewAt(icon_, 0);
  }
}

void NotificationButton::SetTitle(const base::string16& title) {
  if (title_ != NULL)
    delete title_;  // This removes the title from this view's children.
  if (title.empty()) {
    title_ = NULL;
  } else {
    title_ = new views::Label(title);
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetEnabledColor(message_center::kRegularTextColor);
    title_->SetBackgroundColor(kRegularTextBackgroundColor);
    title_->SetBorder(
        views::CreateEmptyBorder(kButtonTitleTopPadding, 0, 0, 0));
    AddChildView(title_);
  }
  SetAccessibleName(title);
}

gfx::Size NotificationButton::CalculatePreferredSize() const {
  return gfx::Size(message_center::kNotificationWidth,
                   message_center::kButtonHeight);
}

int NotificationButton::GetHeightForWidth(int width) const {
  return message_center::kButtonHeight;
}

void NotificationButton::OnFocus() {
  views::Button::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

void NotificationButton::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  // We disable view hierarchy change detection in the parent
  // because it resets the hoverstate, which we do not want
  // when we update the view to contain a new label or image.
  views::View::ViewHierarchyChanged(details);
}

void NotificationButton::StateChanged(ButtonState old_state) {
  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    SetBackground(views::CreateSolidBackground(
        message_center::kHoveredButtonBackgroundColor));
  } else {
    SetBackground(views::CreateSolidBackground(kNotificationBackgroundColor));
  }
}

}  // namespace message_center
