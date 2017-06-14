// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace message_center {

namespace {

constexpr int kHeaderHeight = 28;
constexpr int kAppIconSize = 12;
constexpr int kExpandIconSize = 12;
constexpr gfx::Insets kHeaderPadding(0, 12, 0, 2);
constexpr int kHeaderHorizontalSpacing = 2;
constexpr int kAppInfoConatainerTopPadding = 12;

}  // namespace

NotificationHeaderView::NotificationHeaderView(views::ButtonListener* listener)
    : views::CustomButton(listener) {
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kHeaderPadding, kHeaderHorizontalSpacing);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout);

  views::View* app_info_container = new views::View();
  views::BoxLayout* app_info_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           gfx::Insets(kAppInfoConatainerTopPadding, 0, 0, 0),
                           kHeaderHorizontalSpacing);
  app_info_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  app_info_container->SetLayoutManager(app_info_layout);
  AddChildView(app_info_container);

  // App icon view
  app_icon_view_ = new views::ImageView();
  app_icon_view_->SetImageSize(gfx::Size(kAppIconSize, kAppIconSize));
  app_info_container->AddChildView(app_icon_view_);

  // App name view
  const gfx::FontList& font_list = views::Label().font_list().Derive(
      -2, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  app_name_view_ = new views::Label(base::string16());
  app_name_view_->SetFontList(font_list);
  app_name_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  app_info_container->AddChildView(app_name_view_);

  // Expand button view
  expand_button_ = new views::ImageButton(listener);
  expand_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(kNotificationExpandMoreIcon, kExpandIconSize,
                            gfx::kChromeIconGrey));
  expand_button_->SetFocusForPlatform();
  expand_button_->SetFocusPainter(views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1, 2, 2, 2)));
  app_info_container->AddChildView(expand_button_);

  // Spacer between left-aligned views and right-aligned views
  views::View* spacer = new views::View;
  spacer->SetPreferredSize(gfx::Size(1, kHeaderHeight));
  AddChildView(spacer);
  layout->SetFlexForView(spacer, 1);

  // Settings button view
  settings_button_ = new PaddedButton(listener);
  settings_button_->SetImage(views::Button::STATE_NORMAL, GetSettingsIcon());
  settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
  AddChildView(settings_button_);

  // Close button view
  close_button_ = new PaddedButton(listener);
  close_button_->SetImage(views::Button::STATE_NORMAL, GetCloseIcon());
  close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
  AddChildView(close_button_);
}

void NotificationHeaderView::SetAppIcon(const gfx::ImageSkia& img) {
  app_icon_view_->SetImage(img);
}

void NotificationHeaderView::SetAppName(const base::string16& name) {
  app_name_view_->SetText(name);
}

void NotificationHeaderView::SetExpandButtonEnabled(bool enabled) {
  expand_button_->SetVisible(enabled);
}

void NotificationHeaderView::SetExpanded(bool expanded) {
  expand_button_->SetImage(
      views::Button::STATE_NORMAL,
      gfx::CreateVectorIcon(
          expanded ? kNotificationExpandLessIcon : kNotificationExpandMoreIcon,
          kExpandIconSize, gfx::kChromeIconGrey));
}

void NotificationHeaderView::SetSettingsButtonEnabled(bool enabled) {
  if (settings_button_enabled_ != enabled) {
    settings_button_enabled_ = enabled;
    UpdateControlButtonsVisibility();
  }
}

void NotificationHeaderView::SetCloseButtonEnabled(bool enabled) {
  if (close_button_enabled_ != enabled) {
    close_button_enabled_ = enabled;
    UpdateControlButtonsVisibility();
  }
}

void NotificationHeaderView::SetControlButtonsVisible(bool visible) {
  if (is_control_buttons_visible_ != visible) {
    is_control_buttons_visible_ = visible;
    UpdateControlButtonsVisibility();
  }
}

bool NotificationHeaderView::IsExpandButtonEnabled() {
  return expand_button_->visible();
}

bool NotificationHeaderView::IsSettingsButtonEnabled() {
  return settings_button_enabled_;
}

bool NotificationHeaderView::IsCloseButtonEnabled() {
  return close_button_enabled_;
}

void NotificationHeaderView::UpdateControlButtonsVisibility() {
  settings_button_->SetVisible(settings_button_enabled_ &&
                               is_control_buttons_visible_);
  close_button_->SetVisible(close_button_enabled_ &&
                            is_control_buttons_visible_);
  Layout();
}

}  // namespace message_center
