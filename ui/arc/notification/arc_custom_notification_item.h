// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_
#define UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_surface_manager.h"

namespace arc {

class ArcCustomNotificationItem
    : public ArcNotificationItem,
      public ArcNotificationSurfaceManager::Observer {
 public:
  class Observer {
   public:
    // Invoked when the notification data for this item has changed.
    virtual void OnItemDestroying() = 0;

    // Invoked when the pinned stated is changed.
    virtual void OnItemPinnedChanged() = 0;

    // Invoked when the notification surface for this item is gone.
    virtual void OnItemNotificationSurfaceRemoved() = 0;

   protected:
    virtual ~Observer() = default;
  };

  ArcCustomNotificationItem(ArcNotificationManager* manager,
                            message_center::MessageCenter* message_center,
                            const std::string& notification_key,
                            const AccountId& profile_id);
  ~ArcCustomNotificationItem() override;

  void UpdateWithArcNotificationData(
      const mojom::ArcNotificationData& data) override;

  void CloseFromCloseButton();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  bool pinned() const { return pinned_; }

 private:
  // ArcNotificationSurfaceManager::Observer:
  void OnNotificationSurfaceAdded(exo::NotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(exo::NotificationSurface* surface) override;

  bool pinned_ = false;
  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomNotificationItem);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_
