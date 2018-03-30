// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_VIEW_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_VIEW_H_

#include "base/macros.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/message_center/views/message_view.h"

namespace views {
class Painter;
}

namespace arc {

class ArcNotificationContentViewDelegate;

// View for custom notification with NOTIFICATION_TYPE_CUSTOM which hosts the
// ArcNotificationContentView which shows content of the notification.
class ArcNotificationView : public message_center::MessageView,
                            public ArcNotificationItem::Observer {
 public:
  static const char kMessageViewSubClassName[];

  // |content_view| is a view to be hosted in this view.
  ArcNotificationView(ArcNotificationItem* item,
                      std::unique_ptr<views::View> content_view,
                      std::unique_ptr<ArcNotificationContentViewDelegate>
                          contents_view_delegate,
                      const message_center::Notification& notification);
  ~ArcNotificationView() override;

  // These method are called by the content view when focus handling is defered
  // to the content.
  void OnContentFocused();
  void OnContentBlured();

  // Overridden from MessageView:
  void UpdateWithNotification(
      const message_center::Notification& notification) override;
  void SetDrawBackgroundAsActive(bool active) override;
  bool IsCloseButtonFocused() const override;
  void RequestFocusOnCloseButton() override;
  void UpdateControlButtonsVisibility() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  message_center::NotificationControlButtonsView* GetControlButtonsView()
      const override;
  bool IsExpanded() const override;
  void SetExpanded(bool expanded) override;
  bool IsAutoExpandingAllowed() const override;
  bool IsManuallyExpandedOrCollapsed() const override;
  void OnContainerAnimationStarted() override;
  void OnContainerAnimationEnded() override;
  const char* GetMessageViewSubClassName() const final;

  // views::SlideOutController::Delegate:
  void OnSlideChanged() override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool HasFocus() const override;
  void RequestFocus() override;
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void ChildPreferredSizeChanged(View* child) override;
  bool HandleAccessibleAction(const ui::AXActionData& action) override;

  // ArcNotificationItem::Observer
  void OnItemDestroying() override;
  void OnItemUpdated() override;

 private:
  friend class ArcNotificationContentViewTest;
  friend class ArcNotificationViewTest;
  friend class ArcAccessibilityHelperBridgeTest;

  // TODO(yoshiki): Mmove this to message_center::MessageView.
  void UpdateControlButtonsVisibilityWithNotification(
      const message_center::Notification& notification);

  ArcNotificationItem* item_;

  // The view for the custom content. Owned by view hierarchy.
  views::View* contents_view_ = nullptr;
  std::unique_ptr<ArcNotificationContentViewDelegate> contents_view_delegate_;

  std::unique_ptr<views::Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationView);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_VIEW_H_
