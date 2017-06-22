// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_IMPL_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_IMPL_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "components/exo/notification_surface_manager.h"
#include "ui/arc/notification/arc_notification_surface_manager.h"

namespace arc {

class ArcNotificationSurface;
class ArcNotificationSurfaceImpl;

class ArcNotificationSurfaceManagerImpl
    : public ArcNotificationSurfaceManager,
      public exo::NotificationSurfaceManager {
 public:
  ArcNotificationSurfaceManagerImpl();
  ~ArcNotificationSurfaceManagerImpl() override;

  // ArcNotificationSurfaceManager:
  ArcNotificationSurface* GetArcSurface(
      const std::string& notification_id) const override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // exo::NotificationSurfaceManager:
  exo::NotificationSurface* GetSurface(
      const std::string& notification_id) const override;
  void AddSurface(exo::NotificationSurface* surface) override;
  void RemoveSurface(exo::NotificationSurface* surface) override;

 private:
  using NotificationSurfaceMap =
      std::unordered_map<std::string,
                         std::unique_ptr<ArcNotificationSurfaceImpl>>;
  NotificationSurfaceMap notification_surface_map_;

  base::ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationSurfaceManagerImpl);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_IMPL_H_
