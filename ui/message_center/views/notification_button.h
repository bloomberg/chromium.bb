// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_BUTTON_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_BUTTON_H_

#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}

namespace message_center {

// NotificationButtons render the action buttons of notifications.
class NotificationButton : public views::CustomButton {
 public:
  NotificationButton(views::ButtonListener* listener);
  ~NotificationButton() override;

  void SetIcon(const gfx::ImageSkia& icon);
  void SetTitle(const base::string16& title);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;

  // Overridden from views::CustomButton:
  void StateChanged(ButtonState old_state) override;

 private:
  views::ImageView* icon_;
  views::Label* title_;
  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(NotificationButton);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_BUTTON_H_
