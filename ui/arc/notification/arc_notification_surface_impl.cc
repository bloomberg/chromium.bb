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
  native_view_ = base::MakeUnique<aura::Window>(nullptr);
  native_view_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  native_view_->set_owned_by_parent(false);
  native_view_->Init(ui::LAYER_NOT_DRAWN);
  native_view_->AddChild(surface_->host_window());
  native_view_->Show();
}

ArcNotificationSurfaceImpl::~ArcNotificationSurfaceImpl() = default;

gfx::Size ArcNotificationSurfaceImpl::GetSize() const {
  return surface_->GetContentSize();
}

void ArcNotificationSurfaceImpl::Attach(
    views::NativeViewHost* native_view_host) {
  DCHECK(!native_view_host_);
  DCHECK(native_view_host);
  native_view_host_ = native_view_host;
  native_view_host->Attach(native_view_.get());
}

void ArcNotificationSurfaceImpl::Detach() {
  DCHECK(native_view_host_);
  DCHECK_EQ(native_view_.get(), native_view_host_->native_view());
  native_view_host_->Detach();
  native_view_host_ = nullptr;
}

bool ArcNotificationSurfaceImpl::IsAttached() const {
  return native_view_host_;
}

views::NativeViewHost* ArcNotificationSurfaceImpl::GetAttachedHost() const {
  return native_view_host_;
}

aura::Window* ArcNotificationSurfaceImpl::GetWindow() const {
  return native_view_.get();
}

aura::Window* ArcNotificationSurfaceImpl::GetContentWindow() const {
  DCHECK(surface_->host_window());
  return surface_->host_window();
}

const std::string& ArcNotificationSurfaceImpl::GetNotificationKey() const {
  return surface_->notification_key();
}

}  // namespace arc
