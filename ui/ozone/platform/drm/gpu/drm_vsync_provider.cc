// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"

#include "base/time/time.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_controller.h"

namespace ui {

DrmVSyncProvider::DrmVSyncProvider(DrmWindow* window_delegate)
    : window_delegate_(window_delegate) {
}

DrmVSyncProvider::~DrmVSyncProvider() {
}

void DrmVSyncProvider::GetVSyncParameters(const UpdateVSyncCallback& callback) {
  HardwareDisplayController* controller = window_delegate_->GetController();
  if (!controller)
    return;

  // The value is invalid, so we can't update the parameters.
  if (controller->GetTimeOfLastFlip() == 0 ||
      controller->get_mode().vrefresh == 0)
    return;

  // Stores the time of the last refresh.
  base::TimeTicks timebase =
      base::TimeTicks::FromInternalValue(controller->GetTimeOfLastFlip());
  // Stores the refresh rate.
  base::TimeDelta interval =
      base::TimeDelta::FromSeconds(1) / controller->get_mode().vrefresh;

  callback.Run(timebase, interval);
}

}  // namespace ui
