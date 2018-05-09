// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_ozone.h"

#include "ui/gfx/geometry/dip_util.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

namespace views {

DesktopScreenOzone::DesktopScreenOzone() {
  // TODO(msisov): Turn this hack into a real fetching of displays snapshot,
  // which will be used to create a real display::Display based on the
  // information from snapshots.
  float device_scale_factor = 1.f;
  if (display::Display::HasForceDeviceScaleFactor())
    device_scale_factor = display::Display::GetForcedDeviceScaleFactor();

  gfx::Size scaled_size =
      gfx::ConvertSizeToDIP(device_scale_factor, gfx::Size(800, 600));

  const int64_t display_id = 1;
  display::Display display(display_id);
  display.set_bounds(gfx::Rect(scaled_size));
  display.set_work_area(gfx::Rect(0, 0, 800, 600));
  display.set_device_scale_factor(device_scale_factor);

  ProcessDisplayChanged(display, true /* is_primary */);
}

DesktopScreenOzone::~DesktopScreenOzone() = default;

//////////////////////////////////////////////////////////////////////////////

display::Screen* CreateDesktopScreen() {
  return new DesktopScreenOzone;
}

}  // namespace views
