// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/switches.h"

namespace switches {

// When allowed, the ImageSkia looks up the resource pack with the closest
// available scale factor instead of the actual device scale factor and then
// rescale on ImageSkia side.
// In Windows: default is allowed. Specify --disallow-... to prevent this.
// Other platforms: default is not allowed. Specify --allow-... to do this.
const char kAllowArbitraryScaleFactorInImageSkia[] =
    "allow-arbitrary-scale-factor-in-image-skia";

const char kDisallowArbitraryScaleFactorInImageSkia[] =
    "disallow-arbitrary-scale-factor-in-image-skia";

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
