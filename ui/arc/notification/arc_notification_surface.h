// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_H_

#include <string>

#include "base/macros.h"
#include "ui/gfx/geometry/size.h"

namespace aura {
class Window;
}

namespace views {
class NativeViewHost;
}

namespace arc {

// Handles notification surface role.
class ArcNotificationSurface {
 public:
  ArcNotificationSurface() = default;
  virtual ~ArcNotificationSurface() = default;

  // Returns the content size of the notification surface.
  virtual gfx::Size GetSize() const = 0;

  // Returns the window of NotificationSurface.
  virtual aura::Window* GetWindow() const = 0;

  // Returns the window of the inner Surface.
  virtual aura::Window* GetContentWindow() const = 0;

  // Returns the notification key which is associated with Android notification.
  virtual const std::string& GetNotificationKey() const = 0;

  // Attaches the surface window to the native host.
  virtual void Attach(views::NativeViewHost* native_view_host) = 0;

  // Detaches the surface window from the native host. Please ensure that a
  // surface window is attached before calling this method.
  virtual void Detach() = 0;

  // True if the surface window is attached to the native host. False otherwise.
  virtual bool IsAttached() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcNotificationSurface);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_H_
