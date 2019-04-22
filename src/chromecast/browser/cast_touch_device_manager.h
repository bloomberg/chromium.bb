// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_TOUCH_DEVICE_MANAGER_H_
#define CHROMECAST_BROWSER_CAST_TOUCH_DEVICE_MANAGER_H_

#include <inttypes.h>

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace chromecast {
namespace shell {

// Manages touchscreen->display mapping for cast browser.
class CastTouchDeviceManager : public ui::InputDeviceEventObserver {
 public:
  explicit CastTouchDeviceManager();
  ~CastTouchDeviceManager() override;

  // ui::InputDeviceEventObserver:
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

  void OnDisplayConfigured(int64_t display_id,
                           display::Display::Rotation rotation,
                           const gfx::Rect& native_bounds_in_pixel);

 private:
  void UpdateTouchscreenConfiguration();

  int64_t display_id_;
  display::Display::Rotation display_rotation_;
  gfx::Rect native_display_bounds_in_pixel_;

  DISALLOW_COPY_AND_ASSIGN(CastTouchDeviceManager);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_TOUCH_DEVICE_MANAGER_H_
