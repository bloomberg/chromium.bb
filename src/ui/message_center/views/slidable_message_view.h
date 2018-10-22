// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_SLIDABLE_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_SLIDABLE_MESSAGE_VIEW_H_

#include "ui/message_center/views/message_view.h"
#include "ui/views/view.h"

namespace message_center {

class MESSAGE_CENTER_EXPORT SlidableMessageView
    : public views::View,
      public MessageView::SlideObserver {
 public:
  SlidableMessageView(message_center::MessageView* message_view);
  ~SlidableMessageView() override;

  MessageView* GetMessageView() const { return message_view_; }

  // MessageView::SlideObserver
  void OnSlideChanged(const std::string& notification_id) override;

  void SetExpanded(bool expanded) {
    return message_view_->SetExpanded(expanded);
  }

  bool IsExpanded() const { return message_view_->IsExpanded(); }

  bool IsAutoExpandingAllowed() const {
    return message_view_->IsAutoExpandingAllowed();
  }

  bool IsCloseButtonFocused() const {
    return message_view_->IsCloseButtonFocused();
  }

  bool IsManuallyExpandedOrCollapsed() const {
    return message_view_->IsManuallyExpandedOrCollapsed();
  }

  void SetManuallyExpandedOrCollapsed(bool value) {
    return message_view_->SetManuallyExpandedOrCollapsed(value);
  }

  // Updates this view with the new data contained in the notification.
  void UpdateWithNotification(const Notification& notification);

  std::string notification_id() const {
    return message_view_->notification_id();
  }

  MessageView::Mode GetMode() const { return message_view_->GetMode(); }

  void CloseSwipeControl();

  void StartDismissAnimation();

  // views::View
  void ChildPreferredSizeChanged(views::View* child) override;
  void ChildVisibilityChanged(views::View* child) override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;

 private:
  MessageView* message_view_;
  // TODO(crbug.com/840497): Add child view containing settings and snooze
  // buttons.
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_SLIDABLE_MESSAGE_VIEW_H_
