// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/dri/dri_vsync_provider.h"

#include "base/time/time.h"
#include "ui/gfx/ozone/dri/hardware_display_controller.h"

namespace gfx {

DriVSyncProvider::DriVSyncProvider(HardwareDisplayController* controller)
    : controller_(controller) {}

DriVSyncProvider::~DriVSyncProvider() {}

void DriVSyncProvider::GetVSyncParameters(const UpdateVSyncCallback& callback) {
  // The value is invalid, so we can't update the parameters.
  if (controller_->get_time_of_last_flip() == 0)
    return;

  // Stores the time of the last refresh.
  base::TimeTicks timebase =
      base::TimeTicks::FromInternalValue(controller_->get_time_of_last_flip());
  // Stores the refresh rate.
  base::TimeDelta interval =
      base::TimeDelta::FromSeconds(1) / controller_->get_mode().vrefresh;

  callback.Run(timebase, interval);
}

}  // namespace gfx
