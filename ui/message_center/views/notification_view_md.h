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
#include "ui/views/view_targeter_delegate.h"

namespace views {
class Label;
class LabelButton;
class ProgressBar;
}

namespace message_center {

class BoundedLabel;
class NotificationHeaderView;
class ProportionalImageView;

namespace {
class CompactTitleMessageView;
}

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
  void OnMouseMoved(const ui::MouseEvent& event) override;
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
  void CreateOrUpdateCompactTitleMessageView(const Notification& notification);
  void CreateOrUpdateProgressBarView(const Notification& notification);
  void CreateOrUpdateListItemViews(const Notification& notification);
  void CreateOrUpdateIconView(const Notification& notification);
  void CreateOrUpdateSmallIconView(const Notification& notification);
  void CreateOrUpdateImageView(const Notification& notification);
  void CreateOrUpdateActionButtonViews(const Notification& notification);
  void CreateOrUpdateCloseButtonView(const Notification& notification);
  void CreateOrUpdateSettingsButtonView(const Notification& notification);

  bool IsExpandable();
  void ToggleExpanded();
  void UpdateViewForExpandedState(bool expanded);

  // Whether this notification is expanded or not.
  bool expanded_ = false;

  // Describes whether the view should display a hand pointer or not.
  bool clickable_;

  // Container views directly attached to this view.
  NotificationHeaderView* header_row_ = nullptr;
  views::View* content_row_ = nullptr;
  views::View* actions_row_ = nullptr;

  // Containers for left and right side on |content_row_|
  views::View* left_content_ = nullptr;
  views::View* right_content_ = nullptr;

  // Views which are dinamicallly created inside view hierarchy.
  views::Label* title_view_ = nullptr;
  BoundedLabel* message_view_ = nullptr;
  ProportionalImageView* icon_view_ = nullptr;
  views::View* image_container_ = nullptr;
  ProportionalImageView* image_view_ = nullptr;
  std::vector<views::LabelButton*> action_buttons_;
  std::vector<views::View*> item_views_;
  views::ProgressBar* progress_bar_view_ = nullptr;
  CompactTitleMessageView* compact_title_message_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NotificationViewMD);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_MD_H_
