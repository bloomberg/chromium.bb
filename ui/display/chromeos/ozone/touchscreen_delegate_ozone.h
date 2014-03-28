// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_OZONE_TOUCHSCREEN_DELEGATE_OZONE_H_
#define UI_DISPLAY_CHROMEOS_OZONE_TOUCHSCREEN_DELEGATE_OZONE_H_

#include "ui/display/chromeos/output_configurator.h"

namespace ui {

class TouchscreenDelegateOzone
    : public OutputConfigurator::TouchscreenDelegate {
 public:
  TouchscreenDelegateOzone();
  virtual ~TouchscreenDelegateOzone();

  // OutputConfigurator::TouchscreenDelegate overrides:
  virtual void AssociateTouchscreens(
      std::vector<OutputConfigurator::DisplayState>* outputs) OVERRIDE;
  virtual void ConfigureCTM(
      int touch_device_id,
      const OutputConfigurator::CoordinateTransformation& ctm) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchscreenDelegateOzone);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_OZONE_TOUCHSCREEN_DELEGATE_OZONE_H_
