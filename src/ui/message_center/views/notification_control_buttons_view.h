
// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/views/view.h"

namespace message_center {

class MessageView;

class MESSAGE_CENTER_EXPORT NotificationControlButtonsView
    : public views::View {
 public:
  METADATA_HEADER(NotificationControlButtonsView);

  explicit NotificationControlButtonsView(MessageView* message_view = nullptr);
  NotificationControlButtonsView(const NotificationControlButtonsView&) =
      delete;
  NotificationControlButtonsView& operator=(
      const NotificationControlButtonsView&) = delete;
  ~NotificationControlButtonsView() override;

  // Change the visibility of the close button. True to show, false to hide.
  void ShowCloseButton(bool show);
  // Change the visibility of the settings button. True to show, false to hide.
  void ShowSettingsButton(bool show);
  // Change the visibility of the settings button. True to show, false to hide.
  // Default: hidden.
  void ShowSnoozeButton(bool show);
  // Change the visibility of all buttons. True to show, false to hide.
  void ShowButtons(bool show);

  // Return the focus status of any button. True if the focus is on any button,
  // false otherwise.
  bool IsAnyButtonFocused() const;

  // Sets the icon color for the close, settings, and snooze buttons.
  void SetButtonIconColors(SkColor color);

  // Sets the background color to ensure proper readability.
  void SetBackgroundColor(SkColor color);

  void SetMessageView(MessageView* message_view);

  // Methods for retrieving the control buttons directly.
  PaddedButton* close_button() { return close_button_; }
  PaddedButton* settings_button() { return settings_button_; }
  PaddedButton* snooze_button() { return snooze_button_; }

 private:
  // Updates the button icon colors to the value of DetermineButtonIconColor().
  void UpdateButtonIconColors();

  // Determines the button icon color to use given |icon_color_| and
  // |background_color_| ensuring readability.
  SkColor DetermineButtonIconColor() const;

  raw_ptr<MessageView> message_view_;

  raw_ptr<PaddedButton> close_button_ = nullptr;
  raw_ptr<PaddedButton> settings_button_ = nullptr;
  raw_ptr<PaddedButton> snooze_button_ = nullptr;

  // The color used for the close, settings, and snooze icons.
  SkColor icon_color_;
  // The background color for readability of the icons.
  SkColor background_color_ = SK_ColorTRANSPARENT;
};

BEGIN_VIEW_BUILDER(MESSAGE_CENTER_EXPORT,
                   NotificationControlButtonsView,
                   views::View)
VIEW_BUILDER_PROPERTY(MessageView*, MessageView)
VIEW_BUILDER_PROPERTY(SkColor, ButtonIconColors)
END_VIEW_BUILDER

}  // namespace message_center

DEFINE_VIEW_BUILDER(MESSAGE_CENTER_EXPORT,
                    message_center::NotificationControlButtonsView)

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_CONTROL_BUTTONS_VIEW_H_
