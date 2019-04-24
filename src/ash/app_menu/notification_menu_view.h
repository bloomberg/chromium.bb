// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_

#include <deque>
#include <string>

#include "ash/app_menu/app_menu_export.h"
#include "ui/message_center/views/slide_out_controller.h"
#include "ui/views/view.h"

namespace message_center {
class Notification;
}

namespace views {
class MenuSeparator;
}

namespace ash {

class NotificationMenuHeaderView;
class NotificationOverflowView;
class NotificationItemView;

// A view inserted into a container MenuItemView which shows a
// NotificationItemView and a NotificationMenuHeaderView.
class APP_MENU_EXPORT NotificationMenuView : public views::View {
 public:
  // API for child views to interact with the NotificationMenuController.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Activates the notification corresponding with |notification_id| and
    // closes the menu.
    virtual void ActivateNotificationAndClose(
        const std::string& notification_id) = 0;

    // Called when an overflow view is added or remove.
    virtual void OnOverflowAddedOrRemoved() = 0;
  };

  NotificationMenuView(Delegate* notification_item_view_delegate,
                       message_center::SlideOutController::Delegate*
                           slide_out_controller_delegate,
                       const std::string& app_id);
  ~NotificationMenuView() override;

  // Whether |notifications_for_this_app_| is empty.
  bool IsEmpty() const;

  // Adds |notification| as a NotificationItemView, displacing the currently
  // displayed NotificationItemView, if it exists.
  void AddNotificationItemView(
      const message_center::Notification& notification);

  // Updates the NotificationItemView corresponding to |notification|, replacing
  // the contents if they have changed.
  void UpdateNotificationItemView(
      const message_center::Notification& notification);

  // Removes the NotificationItemView associated with |notification_id| and
  // if it is the currently displayed NotificationItemView, replaces it with
  // the next one if available. Also removes the notification from
  // |overflow_view_| if it exists there.
  void OnNotificationRemoved(const std::string& notification_id);

  // Gets the slide out layer, used to move the displayed NotificationItemView.
  ui::Layer* GetSlideOutLayer();

  // Gets the notification id of the displayed NotificationItemView.
  const std::string& GetDisplayedNotificationID();

  // views::View overrides:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;

 private:
  friend class NotificationMenuViewTestAPI;

  // Identifies the app for this menu.
  const std::string app_id_;

  // Owned by AppMenuModelAdapter.
  NotificationMenuView::Delegate* const notification_item_view_delegate_;

  // Owned by AppMenuModelAdapter.
  message_center::SlideOutController::Delegate* const
      slide_out_controller_delegate_;

  // The deque of NotificationItemViews. The front item in the deque is the view
  // which is shown.
  std::deque<std::unique_ptr<NotificationItemView>> notification_item_views_;

  // A double separator used to distinguish notifications from context menu
  // options. Owned by views hierarchy.
  views::MenuSeparator* double_separator_;

  // Holds the header and counter texts. Owned by views hierarchy.
  NotificationMenuHeaderView* const header_view_;

  // A view that shows icons of notifications for this app that are not being
  // shown.
  std::unique_ptr<NotificationOverflowView> overflow_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMenuView);
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_
