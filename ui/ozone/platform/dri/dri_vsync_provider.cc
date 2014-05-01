// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_vsync_provider.h"

#include "base/time/time.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace ui {

DriVSyncProvider::DriVSyncProvider(DriSurfaceFactory* factory,
                                   gfx::AcceleratedWidget widget)
    : factory_(factory), widget_(widget) {}

DriVSyncProvider::~DriVSyncProvider() {}

void DriVSyncProvider::GetVSyncParameters(const UpdateVSyncCallback& callback) {
  if (!factory_->IsWidgetValid(widget_))
    return;

  HardwareDisplayController* controller =
      factory_->GetControllerForWidget(widget_);
  // The value is invalid, so we can't update the parameters.
  if (controller->get_time_of_last_flip() == 0 ||
      controller->get_mode().vrefresh == 0)
    return;

  // Stores the time of the last refresh.
  base::TimeTicks timebase =
      base::TimeTicks::FromInternalValue(controller->get_time_of_last_flip());
  // Stores the refresh rate.
  base::TimeDelta interval =
      base::TimeDelta::FromSeconds(1) / controller->get_mode().vrefresh;

  callback.Run(timebase, interval);
}

}  // namespace ui
