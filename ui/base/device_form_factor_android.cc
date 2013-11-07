// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/device_form_factor.h"

#include "base/command_line.h"
#include "ui_base_switches.h"

namespace ui {

DeviceFormFactor GetDeviceFormFactor() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kTabletUI))
    return DEVICE_FORM_FACTOR_TABLET;
  return DEVICE_FORM_FACTOR_PHONE;
}

}  // namespace ui
