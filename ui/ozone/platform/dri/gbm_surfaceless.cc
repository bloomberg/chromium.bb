// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/gbm_surfaceless.h"

#include "ui/ozone/platform/dri/dri_vsync_provider.h"
#include "ui/ozone/platform/dri/dri_window_delegate.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/drm_device_manager.h"
#include "ui/ozone/platform/dri/gbm_buffer.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace ui {

GbmSurfaceless::GbmSurfaceless(DriWindowDelegate* window_delegate,
                               DrmDeviceManager* drm_device_manager)
    : window_delegate_(window_delegate),
      drm_device_manager_(drm_device_manager) {
}

GbmSurfaceless::~GbmSurfaceless() {}

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
  HardwareDisplayController* controller = window_delegate_->GetController();
  if (!controller) {
    callback.Run();
    return true;
  }

  return controller->SchedulePageFlip(callback);
}

scoped_ptr<gfx::VSyncProvider> GbmSurfaceless::CreateVSyncProvider() {
  return make_scoped_ptr(new DriVSyncProvider(window_delegate_));
}

bool GbmSurfaceless::IsUniversalDisplayLinkDevice() {
  if (!drm_device_manager_)
    return false;
  scoped_refptr<DriWrapper> drm_primary =
      drm_device_manager_->GetDrmDevice(gfx::kNullAcceleratedWidget);
  DCHECK(drm_primary);

  HardwareDisplayController* controller = window_delegate_->GetController();
  if (!controller)
    return false;
  scoped_refptr<DriWrapper> drm = controller->GetAllocationDriWrapper();
  DCHECK(drm);

  return drm_primary != drm;
}

}  // namespace ui
