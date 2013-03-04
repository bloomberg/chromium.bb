// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_QUIET_MODE_BUBBLE_H_
#define UI_MESSAGE_CENTER_VIEWS_QUIET_MODE_BUBBLE_H_

#include "base/string16.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/button/button.h"

namespace views {
class BubbleDelegateView;
class View;
class Widget;
}

namespace message_center {
class NotificationList;

// The bubble for the quiet mode selection. Note that this isn't TrayBubbleView
// or MessageBubbleBase because its UI is slightly different.
class MESSAGE_CENTER_EXPORT QuietModeBubble : public views::ButtonListener {
 public:
  QuietModeBubble(views::View* anchor_view,
                  gfx::NativeView parent_window,
                  NotificationList* notification_list);
  virtual ~QuietModeBubble();

  // Close the quiet mode bubble.
  void Close();

  // Returns the widget for the bubble.
  views::Widget* GetBubbleWidget();

 private:
  // Initialize the contents of the bubble.
  void InitializeBubbleContents();

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  NotificationList* notification_list_;
  views::BubbleDelegateView* bubble_;

  // Buttons. Used in ButtonPressed() to check which button is pressed.
  views::Button* quiet_mode_;
  views::Button* quiet_mode_1hour_;
  views::Button* quiet_mode_1day_;

  DISALLOW_COPY_AND_ASSIGN(QuietModeBubble);
};

}  // namespace messge_center

#endif  // UI_MESSAGE_CENTER_VIEWS_QUIET_MODE_BUBBLE_H_
