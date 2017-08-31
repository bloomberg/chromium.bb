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
#include "ui/views/controls/button/label_button.h"
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
class ItemView;
class LargeImageContainerView;
}

// This class is needed in addition to LabelButton mainly becuase we want to set
// visible_opacity of InkDropHighlight.
// This button capitalizes the given label string.
class NotificationButtonMD : public views::LabelButton {
 public:
  NotificationButtonMD(views::ButtonListener* listener,
                       const base::string16& text);
  ~NotificationButtonMD() override;

  void SetText(const base::string16& text) override;
  const char* GetClassName() const override;

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

  SkColor enabled_color_for_testing() { return label()->enabled_color(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationButtonMD);
};

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
  NotificationControlButtonsView* GetControlButtonsView() const override;
  bool IsExpanded() const override;
  void SetExpanded(bool expanded) override;

  // views::ViewTargeterDelegate:
  views::View* TargetForRect(views::View* root, const gfx::Rect& rect) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NotificationViewMDTest, CreateOrUpdateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewMDTest, TestIconSizing);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewMDTest, UpdateButtonsStateTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewMDTest, UpdateButtonCountTest);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewMDTest, ExpandLongMessage);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewMDTest, TestAccentColor);
  FRIEND_TEST_ALL_PREFIXES(NotificationViewMDTest, UseImageAsIcon);

  friend class NotificationViewMDTest;

  void UpdateControlButtonsVisibilityWithNotification(
      const Notification& notification);

  void CreateOrUpdateViews(const Notification& notification);

  void CreateOrUpdateContextTitleView(const Notification& notification);
  void CreateOrUpdateTitleView(const Notification& notification);
  void CreateOrUpdateMessageView(const Notification& notification);
  void CreateOrUpdateCompactTitleMessageView(const Notification& notification);
  void CreateOrUpdateProgressBarView(const Notification& notification);
  void CreateOrUpdateProgressStatusView(const Notification& notification);
  void CreateOrUpdateListItemViews(const Notification& notification);
  void CreateOrUpdateIconView(const Notification& notification);
  void CreateOrUpdateSmallIconView(const Notification& notification);
  void CreateOrUpdateImageView(const Notification& notification);
  void CreateOrUpdateActionButtonViews(const Notification& notification);

  bool IsExpandable();
  void ToggleExpanded();
  void UpdateViewForExpandedState(bool expanded);

  // View containing close and settings buttons
  std::unique_ptr<NotificationControlButtonsView> control_buttons_view_;

  // Whether this notification is expanded or not.
  bool expanded_ = false;

  // Whether hiding icon on the right side when expanded.
  bool hide_icon_on_expanded_ = false;

  // Number of total list items in the given Notification class.
  int list_items_count_ = 0;

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
  views::Label* status_view_ = nullptr;
  ProportionalImageView* icon_view_ = nullptr;
  LargeImageContainerView* image_container_view_ = nullptr;
  std::vector<NotificationButtonMD*> action_buttons_;
  std::vector<ItemView*> item_views_;
  views::ProgressBar* progress_bar_view_ = nullptr;
  CompactTitleMessageView* compact_title_message_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(NotificationViewMD);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_NOTIFICATION_VIEW_MD_H_
