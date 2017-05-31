// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_MD_H_
#define UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_MD_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/views/message_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

namespace views {
class BoxLayout;
class ImageView;
}

namespace message_center {

class BoundedLabel;
class NotificationButton;

// View that displays all current types of notification (web, basic, image, and
// list) except the custom notification. Future notification types may be
// handled by other classes, in which case instances of those classes would be
// returned by the Create() factory method below.
class MESSAGE_CENTER_EXPORT NotificationViewMD
    : public MessageView,
      public views::ButtonListener,
      public views::ViewTargeterDelegate {
 public:
  NotificationViewMD(MessageCenterController* controller,
                     const Notification& notification);
  ~NotificationViewMD() override;

  // Overridden from views::View:
  void Layout() override;
  void OnFocus() override;
  void ScrollRectToVisible(const gfx::Rect& rect) override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // Overridden from MessageView:
  void UpdateWithNotification(const Notification& notification) override;
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;
  bool IsCloseButtonFocused() const override;
  void RequestFocusOnCloseButton() override;
  void UpdateControlButtonsVisibility() override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

 private:
  void CreateOrUpdateViews(const Notification& notification);

  void CreateOrUpdateContextTitleView(const Notification& notification);
  void CreateOrUpdateTitleView(const Notification& notification);
  void CreateOrUpdateMessageView(const Notification& notification);
  void CreateOrUpdateProgressBarView(const Notification& notification);
  void CreateOrUpdateListItemViews(const Notification& notification);
  void CreateOrUpdateIconView(const Notification& notification);
  void CreateOrUpdateSmallIconView(const Notification& notification);
  void CreateOrUpdateImageView(const Notification& notification);
  void CreateOrUpdateActionButtonViews(const Notification& notification);
  void CreateOrUpdateCloseButtonView(const Notification& notification);
  void CreateOrUpdateSettingsButtonView(const Notification& notification);

  // Describes whether the view should display a hand pointer or not.
  bool clickable_;

  // Views in the top view
  views::BoxLayout* layout_ = nullptr;

  // Views in the top view
  views::View* top_view_ = nullptr;
  BoundedLabel* context_title_view_ = nullptr;

  // Views in the main view
  views::View* main_view_ = nullptr;
  BoundedLabel* title_view_ = nullptr;
  BoundedLabel* message_view_ = nullptr;

  // Views in the bottom view
  views::View* bottom_view_ = nullptr;

  // Views in the floating controller
  std::unique_ptr<views::ImageButton> settings_button_;
  std::unique_ptr<views::ImageButton> close_button_;
  std::unique_ptr<views::ImageView> small_image_view_;

  std::vector<NotificationButton*> action_buttons_;

  DISALLOW_COPY_AND_ASSIGN(NotificationViewMD);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_MD_H_
