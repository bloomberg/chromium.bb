// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

namespace ui {

namespace {

void PostedSwapResult(const SwapCompletionCallback& callback,
                      gfx::SwapResult result) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, result));
}

}  // namespace

GbmSurfaceless::GbmSurfaceless(DrmWindow* window,
                               DrmDeviceManager* drm_device_manager,
                               GbmSurfaceFactory* surface_manager)
    : window_(window),
      drm_device_manager_(drm_device_manager),
      surface_manager_(surface_manager) {
  surface_manager_->RegisterSurface(window_->GetAcceleratedWidget(), this);
}

GbmSurfaceless::~GbmSurfaceless() {
  surface_manager_->UnregisterSurface(window_->GetAcceleratedWidget());
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

bool GbmSurfaceless::OnSwapBuffersAsync(
    const SwapCompletionCallback& callback) {
  // Wrap the callback and post the result such that everything using the
  // callback doesn't need to worry about re-entrancy.
  window_->SchedulePageFlip(planes_, base::Bind(&PostedSwapResult, callback));
  planes_.clear();
  return true;
}

scoped_ptr<gfx::VSyncProvider> GbmSurfaceless::CreateVSyncProvider() {
  return make_scoped_ptr(new DrmVSyncProvider(window_));
}

bool GbmSurfaceless::IsUniversalDisplayLinkDevice() {
  if (!drm_device_manager_)
    return false;
  scoped_refptr<DrmDevice> drm_primary =
      drm_device_manager_->GetDrmDevice(gfx::kNullAcceleratedWidget);
  DCHECK(drm_primary);

  HardwareDisplayController* controller = window_->GetController();
  if (!controller)
    return false;
  scoped_refptr<DrmDevice> drm = controller->GetAllocationDrmDevice();
  DCHECK(drm);

  return drm_primary != drm;
}

}  // namespace ui
