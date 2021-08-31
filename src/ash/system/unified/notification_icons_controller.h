// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
#define ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/unified/unified_system_tray_model.h"
#include "base/scoped_observation.h"
#include "ui/message_center/message_center_observer.h"

namespace message_center {
class Notification;
}  // namespace message_center

namespace ash {

class UnifiedSystemTray;
class TrayContainer;
class TrayItemView;
class NotificationCounterView;
class QuietModeView;
class SeparatorTrayItemView;

// Tray item view for notification icon shown in the tray.
class ASH_EXPORT NotificationIconTrayItemView : public TrayItemView {
 public:
  explicit NotificationIconTrayItemView(Shelf* shelf);
  ~NotificationIconTrayItemView() override;
  NotificationIconTrayItemView(const NotificationIconTrayItemView&) = delete;
  NotificationIconTrayItemView& operator=(const NotificationIconTrayItemView&) =
      delete;

  // Set the image and tooltip for the view according to the notification.
  void SetNotification(message_center::Notification* notification);

  // Reset notification pointer, id, image and tooltip text.
  void Reset();

  // Returns a string describing the current state for accessibility.
  const std::u16string& GetAccessibleNameString() const;

  const std::string& GetNotificationId() const;

  // TrayItemView:
  void HandleLocaleChange() override;
  const char* GetClassName() const override;

 private:
  // Store the id to make sure we still have it when notification is removed and
  // goes out of scope.
  std::string notification_id_;
};

// Controller for notification icons in UnifiedSystemTray button. The icons will
// be displayed in medium or large screen size and only for important
// notifications.
class ASH_EXPORT NotificationIconsController
    : public UnifiedSystemTrayModel::Observer,
      public message_center::MessageCenterObserver,
      public SessionObserver {
 public:
  explicit NotificationIconsController(UnifiedSystemTray* tray);
  ~NotificationIconsController() override;
  NotificationIconsController(const NotificationIconsController&) = delete;
  NotificationIconsController& operator=(const NotificationIconsController&) =
      delete;

  // Initialize the view by adding items to the container of the tray.
  void AddNotificationTrayItems(TrayContainer* tray_container);

  // Returns true if any item in `tray_items_` is containing a notification.
  bool TrayItemHasNotification() const;

  // Returns the number of notification icons showing in |tray_items_|.
  size_t TrayNotificationIconsCount() const;

  // Returns true if we should not show notification related items in tray (e.g.
  // during quiet mode, screen lock, etc.).
  bool ShouldShowNotificationItemsInTray();

  // Returns a string describing the current state for accessibility.
  std::u16string GetAccessibleNameString() const;

  // Update notification indicators, including counters and quiet mode view.
  void UpdateNotificationIndicators();

  // UnifiedSystemTrayModel::Observer:
  void OnSystemTrayButtonSizeChanged(
      UnifiedSystemTrayModel::SystemTrayButtonSize system_tray_size) override;

  // message_center::MessageCenterObserver:
  void OnNotificationAdded(const std::string& id) override;
  void OnNotificationRemoved(const std::string& id, bool by_user) override;
  void OnNotificationUpdated(const std::string& id) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  std::vector<NotificationIconTrayItemView*> tray_items() {
    return tray_items_;
  }

  NotificationCounterView* notification_counter_view() {
    return notification_counter_view_;
  }

  QuietModeView* quiet_mode_view() { return quiet_mode_view_; }

  bool icons_view_visible() const { return icons_view_visible_; }

 private:
  friend class NotificationIconsControllerTest;

  // Iterate through the notifications in message center and update the icons
  // shown accordingly.
  void UpdateNotificationIcons();

  // If the notification with given id is currently shown in tray, returns the
  // pointer to that tray item. Otherwise, returns a null pointer.
  NotificationIconTrayItemView* GetNotificationIconShownInTray(
      const std::string& id);

  // Contains notification icon tray items that are added to tray container. All
  // items are owned by views hierarchy.
  std::vector<NotificationIconTrayItemView*> tray_items_;

  // Points to the first item that is available to use among the notification
  // icons tray item. All the items in previous index are used and visible.
  size_t first_unused_item_index_ = 0;

  // Indicates if the notification icons view is set to be shown. Currently, we
  // show the icon view in medium or large screen size.
  bool icons_view_visible_ = false;

  UnifiedSystemTray* tray_;

  NotificationCounterView* notification_counter_view_ = nullptr;
  QuietModeView* quiet_mode_view_ = nullptr;
  SeparatorTrayItemView* separator_ = nullptr;

  base::ScopedObservation<UnifiedSystemTrayModel,
                          UnifiedSystemTrayModel::Observer>
      system_tray_model_observation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_NOTIFICATION_ICONS_CONTROLLER_H_
