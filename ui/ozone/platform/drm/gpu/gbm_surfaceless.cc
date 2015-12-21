// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

#include <utility>

#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/scanout_buffer.h"

namespace ui {

GbmSurfaceless::GbmSurfaceless(scoped_ptr<DrmWindowProxy> window,
                               GbmSurfaceFactory* surface_manager)
    : window_(std::move(window)), surface_manager_(surface_manager) {
  surface_manager_->RegisterSurface(window_->widget(), this);
}

GbmSurfaceless::~GbmSurfaceless() {
  surface_manager_->UnregisterSurface(window_->widget());
}

void GbmSurfaceless::QueueOverlayPlane(const OverlayPlane& plane) {
  planes_.push_back(plane);
}

intptr_t GbmSurfaceless::GetNativeWindow() {
  NOTREACHED();
  return 0;
}

bool GbmSurfaceless::ResizeNativeWindow(const gfx::Size& viewport_size) {
  return true;
}

bool GbmSurfaceless::OnSwapBuffers() {
  NOTREACHED();
  return false;
}

void GbmSurfaceless::OnSwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  window_->SchedulePageFlip(planes_, callback);
  planes_.clear();
}

scoped_ptr<gfx::VSyncProvider> GbmSurfaceless::CreateVSyncProvider() {
  return make_scoped_ptr(new DrmVSyncProvider(window_.get()));
}

bool GbmSurfaceless::IsUniversalDisplayLinkDevice() {
  return planes_.empty() ? false : planes_[0].buffer->RequiresGlFinish();
}

}  // namespace ui
