// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"

#include "base/time/time.h"
#include "ui/ozone/platform/drm/gpu/crtc_controller.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

namespace ui {

DrmVSyncProvider::DrmVSyncProvider(DrmWindow* window) : window_(window) {
}

DrmVSyncProvider::~DrmVSyncProvider() {
}

void DrmVSyncProvider::GetVSyncParameters(const UpdateVSyncCallback& callback) {
  HardwareDisplayController* controller = window_->GetController();
  if (!controller)
    return;

  // If we're in mirror mode the 2 CRTCs should have similar modes with the same
  // refresh rates.
  CrtcController* crtc = controller->crtc_controllers()[0];
  // The value is invalid, so we can't update the parameters.
  if (controller->GetTimeOfLastFlip() == 0 || crtc->mode().vrefresh == 0)
    return;

  // Stores the time of the last refresh.
  base::TimeTicks timebase =
      base::TimeTicks::FromInternalValue(controller->GetTimeOfLastFlip());
  // Stores the refresh rate.
  base::TimeDelta interval =
      base::TimeDelta::FromSeconds(1) / crtc->mode().vrefresh;

  callback.Run(timebase, interval);
}

}  // namespace ui
