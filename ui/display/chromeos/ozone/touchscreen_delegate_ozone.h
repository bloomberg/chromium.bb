// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_OZONE_TOUCHSCREEN_DELEGATE_OZONE_H_
#define UI_DISPLAY_CHROMEOS_OZONE_TOUCHSCREEN_DELEGATE_OZONE_H_

#include "ui/display/chromeos/display_configurator.h"

namespace ui {

class TouchscreenDelegateOzone
    : public DisplayConfigurator::TouchscreenDelegate {
 public:
  TouchscreenDelegateOzone();
  virtual ~TouchscreenDelegateOzone();

  // DisplayConfigurator::TouchscreenDelegate overrides:
  virtual void AssociateTouchscreens(
      std::vector<DisplayConfigurator::DisplayState>* outputs) OVERRIDE;
  virtual void ConfigureCTM(
      int touch_device_id,
      const DisplayConfigurator::CoordinateTransformation& ctm) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchscreenDelegateOzone);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_OZONE_TOUCHSCREEN_DELEGATE_OZONE_H_
