// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/switches.h"

namespace switches {

// Let text glyphs have X-positions that aren't snapped to the pixel grid in
// the browser UI.
const char kEnableBrowserTextSubpixelPositioning[] =
    "enable-browser-text-subpixel-positioning";

// Enable text glyphs to have X-positions that aren't snapped to the pixel grid
// in webkit renderers.
const char kEnableWebkitTextSubpixelPositioning[] =
    "enable-webkit-text-subpixel-positioning";

// Overrides the device scale factor for the browser UI and the contents.
const char kForceDeviceScaleFactor[] = "force-device-scale-factor";

// Enables/Disables High DPI support (windows)
const char kHighDPISupport[] = "high-dpi-support";

}  // namespace switches
