// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_BUTTON_BAR_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_BUTTON_BAR_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view.h"

namespace views {
class Label;
class ToggleImageButton;
}

namespace message_center {

class MessageCenter;
class MessageCenterView;
class NotifierSettingsProvider;

// MessageCenterButtonBar is the class that shows the content outside the main
// notification area - the label (or NotifierGroup switcher) and the buttons.
class MessageCenterButtonBar : public views::View,
                               public views::ButtonListener {
 public:
  MessageCenterButtonBar(MessageCenterView* message_center_view,
                         MessageCenter* message_center,
                         NotifierSettingsProvider* notifier_settings_provider,
                         bool settings_initially_visible,
                         const base::string16& title);
  ~MessageCenterButtonBar() override;

  // Enables or disables all of the buttons in the center.  This is used to
  // prevent user clicks during the close-all animation.
  void SetSettingsAndQuietModeButtonsEnabled(bool enabled);

  // Sometimes we shouldn't see the close-all button.
  void SetCloseAllButtonEnabled(bool enabled);

  MESSAGE_CENTER_EXPORT views::Button* GetCloseAllButtonForTest() const;
  MESSAGE_CENTER_EXPORT views::Button* GetSettingsButtonForTest() const;
  MESSAGE_CENTER_EXPORT views::Button* GetQuietModeButtonForTest() const;

  // Sometimes we shouldn't see the back arrow (not in settings).
  void SetBackArrowVisible(bool visible);

  // Update the label of the title.
  void SetTitle(const base::string16& title);

  void SetButtonsVisible(bool visible);

 private:
  // Updates the layout manager which can have differing configuration
  // depending on the visibility of different parts of the button bar.
  void ViewVisibilityChanged();

  // Overridden from views::View:
  void ChildVisibilityChanged(views::View* child) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  MessageCenterView* message_center_view() const {
    return message_center_view_;
  }
  MessageCenter* message_center() const { return message_center_; }

  MessageCenterView* message_center_view_;  // Weak reference.
  MessageCenter* message_center_;           // Weak reference.

  // |close_bubble_button_| closes the message center bubble. This is required
  // for desktop Linux because the status icon doesn't toggle the bubble, and
  // close-on-deactivation is off. This is a tentative solution. Once pkotwicz
  // Fixes the problem of focus-follow-mouse, close-on-deactivation will be
  // back and this field will be removed. See crbug.com/319516.
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  views::ImageButton* close_bubble_button_;
#endif

  // Sub-views of the button bar.
  views::ToggleImageButton* title_arrow_;
  views::Label* notification_label_;
  views::View* button_container_;
  views::ToggleImageButton* close_all_button_;
  views::ToggleImageButton* settings_button_;
  views::ToggleImageButton* quiet_mode_button_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterButtonBar);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_CENTER_BUTTON_BAR_H_
