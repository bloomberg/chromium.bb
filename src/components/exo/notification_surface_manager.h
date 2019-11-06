// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_NOTIFICATION_SURFACE_MANAGER_H_
#define COMPONENTS_EXO_NOTIFICATION_SURFACE_MANAGER_H_

#include <string>

namespace exo {

class NotificationSurface;

class NotificationSurfaceManager {
 public:
  // Gets the NotificationSurface associated with the given notification id.
  // Returns nullptr if no NotificationSurface is associated with the id.
  virtual NotificationSurface* GetSurface(
      const std::string& notification_key) const = 0;

  // Adds a NotificationSurface to the manager.
  virtual void AddSurface(NotificationSurface* surface) = 0;

  // Removes a NotificationSurface from the manager.
  virtual void RemoveSurface(NotificationSurface* surface) = 0;

 protected:
  virtual ~NotificationSurfaceManager() {}
};

}  // namespace exo

#endif  // COMPONENTS_EXO_NOTIFICATION_SURFACE_MANAGER_H_
