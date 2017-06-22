// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_VIEW_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_VIEW_H_

#include "base/macros.h"
#include "ui/message_center/views/message_view.h"

namespace views {
class Painter;
}

namespace arc {

class ArcNotificationContentViewDelegate;

// View for custom notification with NOTIFICATION_TYPE_CUSTOM which hosts the
// ArcNotificationContentView which shows content of the notification.
class ArcNotificationView : public message_center::MessageView {
 public:
  static const char kViewClassName[];

  // |content_view| is a view to be hosted in this view.
  ArcNotificationView(
      std::unique_ptr<views::View> content_view,
      std::unique_ptr<ArcNotificationContentViewDelegate> delegate,
      message_center::MessageCenterController* controller,
      const message_center::Notification& notification);
  ~ArcNotificationView() override;

  // These method are called by the content view when focus handling is defered
  // to the content.
  void OnContentFocused();
  void OnContentBlured();

  // Overridden from MessageView:
  void SetDrawBackgroundAsActive(bool active) override;
  bool IsCloseButtonFocused() const override;
  void RequestFocusOnCloseButton() override;
  void UpdateControlButtonsVisibility() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::SlideOutController::Delegate:
  void OnSlideChanged() override;

  // Overridden from views::View:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool HasFocus() const override;
  void RequestFocus() override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void ChildPreferredSizeChanged(View* child) override;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;

 private:
  friend class ArcNotificationContentViewTest;
  friend class ArcNotificationViewTest;

  // The view for the custom content. Owned by view hierarchy.
  views::View* contents_view_ = nullptr;
  std::unique_ptr<ArcNotificationContentViewDelegate> contents_view_delegate_;

  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationView);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_VIEW_H_
