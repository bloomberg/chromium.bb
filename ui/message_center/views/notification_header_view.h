// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_

#include "base/macros.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/controls/button/custom_button.h"

namespace views {
class ImageButton;
class ImageView;
class Label;
}

namespace message_center {

class NotificationHeaderView : public views::CustomButton {
 public:
  NotificationHeaderView(views::ButtonListener* listener);
  void SetAppIcon(const gfx::ImageSkia& img);
  void SetAppName(const base::string16& name);
  void SetExpandButtonEnabled(bool enabled);
  void SetExpanded(bool expanded);
  void SetSettingsButtonEnabled(bool enabled);
  void SetCloseButtonEnabled(bool enabled);
  void SetControlButtonsVisible(bool visible);
  bool IsExpandButtonEnabled();
  bool IsSettingsButtonEnabled();
  bool IsCloseButtonEnabled();
  bool IsCloseButtonFocused();

  views::ImageButton* expand_button() { return expand_button_; }
  views::ImageButton* settings_button() { return settings_button_; }
  views::ImageButton* close_button() { return close_button_; }

 private:
  void UpdateControlButtonsVisibility();

  views::Label* app_name_view_ = nullptr;
  views::ImageView* app_icon_view_ = nullptr;
  views::ImageButton* expand_button_ = nullptr;
  PaddedButton* settings_button_ = nullptr;
  PaddedButton* close_button_ = nullptr;

  bool settings_button_enabled_ = false;
  bool close_button_enabled_ = false;
  bool is_control_buttons_visible_ = false;

  DISALLOW_COPY_AND_ASSIGN(NotificationHeaderView);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_HEADER_VIEW_H_
