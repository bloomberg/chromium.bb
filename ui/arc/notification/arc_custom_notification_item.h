// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_
#define UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_

#include "base/macros.h"
#include "ui/arc/notification/arc_notification_item.h"
#include "ui/arc/notification/arc_notification_surface_manager.h"

namespace arc {

class ArcCustomNotificationItem
    : public ArcNotificationItem,
      public ArcNotificationSurfaceManager::Observer {
 public:
  ArcCustomNotificationItem(ArcNotificationManager* manager,
                            message_center::MessageCenter* message_center,
                            const std::string& notification_key,
                            const AccountId& profile_id);
  ~ArcCustomNotificationItem() override;

  void UpdateWithArcNotificationData(
      const mojom::ArcNotificationData& data) override;

 private:
  // ArcNotificationSurfaceManager::Observer:
  void OnNotificationSurfaceAdded(exo::NotificationSurface* surface) override;
  void OnNotificationSurfaceRemoved(exo::NotificationSurface* surface) override;

  DISALLOW_COPY_AND_ASSIGN(ArcCustomNotificationItem);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_CUSTOM_NOTIFICATION_ITEM_H_
