// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches.h"

namespace switches {

// The default device scale factor to apply to the browser UI and
// the contents in the absence of a viewport meta tag.
const char kDefaultDeviceScaleFactor[] = "default-device-scale-factor";

// Enable touch screen calibration.
const char kDisableTouchCalibration[] = "disable-touch-calibration";

// Let text glyphs have X-positions that aren't snapped to the pixel grid.
const char kEnableTextSubpixelPositioning[] =
    "enable-text-subpixel-positioning";

// Enable support for touch events.
const char kEnableTouchEvents[] = "enable-touch-events";

// The language file that we want to try to open. Of the form
// language[-country] where language is the 2 letter code from ISO-639.
const char kLang[] = "lang";

// Load the locale resources from the given path. When running on Mac/Unix the
// path should point to a locale.pak file.
const char kLocalePak[] = "locale_pak";

// Disable ui::MessageBox. This is useful when running as part of scripts that
// do not have a user interface.
const char kNoMessageBox[] = "no-message-box";

// Enables UI changes that make it easier to use with a touchscreen.
const char kTouchOptimizedUI[] = "touch-optimized-ui";


#if defined(OS_MACOSX)
const char kDisableCompositedCoreAnimationPlugins[] =
    "disable-composited-core-animation-plugins";
#endif

}  // namespace switches
