// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/display/display_switches.h"

namespace switches {

// TODO(rjkroege): Some of these have an "ash" prefix. When ChromeOS startup
// scripts have been updated, the leading "ash" prefix should be removed.

// Overrides the device scale factor for the browser UI and the contents.
const char kForceDeviceScaleFactor[] = "force-device-scale-factor";

// Specifies the layout mode and offsets for the secondary display for
// testing. The format is "<t|r|b|l>,<offset>" where t=TOP, r=RIGHT,
// b=BOTTOM and L=LEFT. For example, 'r,-100' means the secondary display
// is positioned on the right with -100 offset. (above than primary)
const char kSecondaryDisplayLayout[] = "secondary-display-layout";

// Enables software based mirroring.
const char kEnableSoftwareMirroring[] = "ash-enable-software-mirroring";

// Uses the 1st display in --ash-host-window-bounds as internal display.
// This is for debugging on linux desktop.
const char kUseFirstDisplayAsInternal[] = "ash-use-first-display-as-internal";

// Sets a window size, optional position, and optional scale factor.
// "1024x768" creates a window of size 1024x768.
// "100+200-1024x768" positions the window at 100,200.
// "1024x768*2" sets the scale factor to 2 for a high DPI display.
// "800,0+800-800x800" for two displays at 800x800 resolution.
// "800,0+800-800x800,0+1600-800x800" for three displays at 800x800 resolution.
const char kHostWindowBounds[] = "ash-host-window-bounds";

#if defined(OS_CHROMEOS)
const char kDisableDisplayColorCalibration[] =
    "disable-display-color-calibration";

// Enables unified desktop mode.
const char kEnableUnifiedDesktop[] = "ash-enable-unified-desktop";
#endif

}  // namespace switches
