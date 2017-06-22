// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_IMPL_H_
#define UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_IMPL_H_

#include "ui/arc/notification/arc_notification_surface.h"

namespace exo {
class NotificationSurface;
}

namespace arc {

// Handles notification surface role of a given surface.
class ArcNotificationSurfaceImpl : public ArcNotificationSurface {
 public:
  explicit ArcNotificationSurfaceImpl(exo::NotificationSurface* surface);

  // ArcNotificationSurface overrides:
  gfx::Size GetSize() const override;
  aura::Window* GetWindow() const override;
  aura::Window* GetContentWindow() const override;
  const std::string& GetNotificationKey() const override;
  void Attach(views::NativeViewHost* native_view_host) override;
  void Detach() override;
  bool IsAttached() const override;

  exo::NotificationSurface* surface() const { return surface_; }

 private:
  exo::NotificationSurface* surface_;
  views::NativeViewHost* native_view_host_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ArcNotificationSurfaceImpl);
};

}  // namespace arc

#endif  // UI_ARC_NOTIFICATION_ARC_NOTIFICATION_SURFACE_IMPL_H_
