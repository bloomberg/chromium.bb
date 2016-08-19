// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/display/display_switches.h"

namespace switches {

// Overrides the device scale factor for the browser UI and the contents.
const char kForceDeviceScaleFactor[] = "force-device-scale-factor";

// Specifies the layout mode and offsets for the secondary display for
// testing. The format is "<t|r|b|l>,<offset>" where t=TOP, r=RIGHT,
// b=BOTTOM and L=LEFT. For example, 'r,-100' means the secondary display
// is positioned on the right with -100 offset. (above than primary)
const char kSecondaryDisplayLayout[] = "secondary-display-layout";

#if defined(OS_CHROMEOS)
const char kDisableDisplayColorCalibration[] =
    "disable-display-color-calibration";
#endif

}  // namespace switches
