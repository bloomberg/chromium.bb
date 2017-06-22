// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/arc/notification/arc_notification_surface_impl.h"

#include "base/logging.h"
#include "components/exo/notification_surface.h"
#include "components/exo/surface.h"
#include "ui/aura/window.h"
#include "ui/views/controls/native/native_view_host.h"

namespace arc {

// Handles notification surface role of a given surface.
ArcNotificationSurfaceImpl::ArcNotificationSurfaceImpl(
    exo::NotificationSurface* surface)
    : surface_(surface) {
  DCHECK(surface);
}

gfx::Size ArcNotificationSurfaceImpl::GetSize() const {
  return surface_->GetSize();
}

void ArcNotificationSurfaceImpl::Attach(
    views::NativeViewHost* native_view_host) {
  DCHECK(!native_view_host_);
  DCHECK(native_view_host);
  native_view_host_ = native_view_host;
  native_view_host->Attach(surface_->window());
}

void ArcNotificationSurfaceImpl::Detach() {
  DCHECK(native_view_host_);
  DCHECK_EQ(surface_->window(), native_view_host_->native_view());
  native_view_host_->Detach();
  native_view_host_ = nullptr;
}

bool ArcNotificationSurfaceImpl::IsAttached() const {
  return native_view_host_;
}

aura::Window* ArcNotificationSurfaceImpl::GetWindow() const {
  return surface_->window();
}

aura::Window* ArcNotificationSurfaceImpl::GetContentWindow() const {
  DCHECK(surface_->window());
  DCHECK(!surface_->window()->children().empty());

  aura::Window* content_window = surface_->window()->children()[0];

  DCHECK(!content_window);
  return content_window;
}

const std::string& ArcNotificationSurfaceImpl::GetNotificationKey() const {
  return surface_->notification_key();
}

}  // namespace arc
