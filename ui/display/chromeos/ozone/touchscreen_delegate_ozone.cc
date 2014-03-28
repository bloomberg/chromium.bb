// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/ozone/touchscreen_delegate_ozone.h"

namespace ui {

TouchscreenDelegateOzone::TouchscreenDelegateOzone() {}

TouchscreenDelegateOzone::~TouchscreenDelegateOzone() {}

void TouchscreenDelegateOzone::AssociateTouchscreens(
    std::vector<OutputConfigurator::DisplayState>* outputs) {
  NOTIMPLEMENTED();
}

void TouchscreenDelegateOzone::ConfigureCTM(
    int touch_device_id,
    const OutputConfigurator::CoordinateTransformation& ctm) {
  NOTIMPLEMENTED();
}

}  // namespace ui
