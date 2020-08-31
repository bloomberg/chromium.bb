// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_
#define ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_

#include "ash/system/tray/tray_bubble_base.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class UnifiedSystemTray;
class UnifiedMessageCenterView;

// Manages the bubble that contains UnifiedMessageCenterView.
// Shows the bubble on the constructor, and closes the bubble on the destructor.
class ASH_EXPORT UnifiedMessageCenterBubble : public TrayBubbleBase,
                                              public TrayBubbleView::Delegate,
                                              public views::ViewObserver,
                                              public views::WidgetObserver {
 public:
  explicit UnifiedMessageCenterBubble(UnifiedSystemTray* tray);
  ~UnifiedMessageCenterBubble() override;

  // We need the code to show the bubble explicitly separated from the
  // contructor. This is to prevent trigerring the TrayEventFilter from within
  // the constructor. Doing so can cause a crash when the TrayEventFilter tries
  // to reference the message center bubble before it is fully instantiated.
  void ShowBubble();

  // Calculate the height usable for the bubble.
  int CalculateAvailableHeight();

  // Collapse the bubble to only have the notification bar visible.
  void CollapseMessageCenter();

  // Expand the bubble to show all notifications.
  void ExpandMessageCenter();

  // Move the message center bubble to keep it on top of the quick settings
  // widget whenever the quick settings widget is resized.
  void UpdatePosition();

  // Inform message_center_view_ of focus being acquired.
  void FocusEntered(bool reverse);

  // Relinquish focus and transfer it to the quick settings widget.
  bool FocusOut(bool reverse);

  // Activate quick settings bubble. Used when the message center is going
  // invisible.
  void ActivateQuickSettingsBubble();

  // Move focus to the first notification.
  void FocusFirstNotification();

  // Returns true if notifications are shown.
  bool IsMessageCenterVisible();

  // Returns true if only StackedNotificationBar is visible.
  bool IsMessageCenterCollapsed();

  // TrayBubbleBase:
  TrayBackgroundView* GetTray() const override;
  TrayBubbleView* GetBubbleView() const override;
  views::Widget* GetBubbleWidget() const override;

  // TrayBubbleView::Delegate:
  base::string16 GetAccessibleNameForBubble() override;
  bool ShouldEnableExtraKeyboardAccessibility() override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  UnifiedMessageCenterView* message_center_view() {
    return message_center_view_;
  }

 private:
  class Border;

  UnifiedSystemTray* const tray_;
  std::unique_ptr<Border> border_;

  views::Widget* bubble_widget_ = nullptr;
  TrayBubbleView* bubble_view_ = nullptr;
  UnifiedMessageCenterView* message_center_view_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UnifiedMessageCenterBubble);
};

}  // namespace ash

#endif  // ASH_SYSTEM_MESSAGE_CENTER_UNIFIED_MESSAGE_CENTER_BUBBLE_H_
