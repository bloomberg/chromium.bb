// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_H_

#include <string>

#include "base/macros.h"

namespace arc {

class ArcNotificationSurface;

// Keeps track of NotificationSurface.
class ArcNotificationSurfaceManager {
 public:
  class Observer {
   public:
    // Invoked when a notification surface is added to the registry.
    virtual void OnNotificationSurfaceAdded(
        ArcNotificationSurface* surface) = 0;

    // Invoked when a notification surface is removed from the registry.
    virtual void OnNotificationSurfaceRemoved(
        ArcNotificationSurface* surface) = 0;

   protected:
    virtual ~Observer() = default;
  };
  static ArcNotificationSurfaceManager* Get();

  virtual ~ArcNotificationSurfaceManager();

  virtual ArcNotificationSurface* GetArcSurface(
      const std::string& notification_id) const = 0;
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  ArcNotificationSurfaceManager();

 private:
  static ArcNotificationSurfaceManager* instance_;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationSurfaceManager);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_MANAGER_H_
