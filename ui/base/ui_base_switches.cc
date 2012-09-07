// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches.h"

namespace switches {

// Disable support for bezel touch.
const char kEnableBezelTouch[] = "enable-bezel-touch";

// Whether or not ImageSkiaOperations methods can scale one of images
// if they don't have the same scale factor.
const char kDisableScalingInImageSkiaOperations[] =
    "disable-scaling-in-image-skia-operations";

// Let text glyphs have X-positions that aren't snapped to the pixel grid in
// the browser UI.
const char kEnableBrowserTextSubpixelPositioning[] =
    "enable-browser-text-subpixel-positioning";

// Enable touch screen calibration.
const char kEnableTouchCalibration[] = "enable-touch-calibration";

// Enable support for touch event calibration in x direction.
const char kEnableTouchCalibrationX[] = "enable-touch-calibration-x";

// Enable support for touch events.
const char kEnableTouchEvents[] = "enable-touch-events";

// Enables the Views textfield on Windows.
const char kEnableViewsTextfield[] = "enable-views-textfield";

// Enable text glyphs to have X-positions that aren't snapped to the pixel grid
// in webkit renderers.
const char kEnableWebkitTextSubpixelPositioning[] =
    "enable-webkit-text-subpixel-positioning";

// Overrides the device scale factor for the browser UI and the
// contents.
const char kForceDeviceScaleFactor[] = "force-device-scale-factor";

// Generates a 2x version of resources for which no 2x version is available or
// the 2x version is of an incorrect size and applies a red mask to the
// resource. Resources for which hidpi is not supported because of software
// reasons will show up pixelated.
const char kHighlightMissing2xResources[] =
    "highlight-missing-2x-resources";

// The language file that we want to try to open. Of the form
// language[-country] where language is the 2 letter code from ISO-639.
const char kLang[] = "lang";

// Load the locale resources from the given path. When running on Mac/Unix the
// path should point to a locale.pak file.
const char kLocalePak[] = "locale_pak";

// Disables the new appearance for checkboxes and radio buttons.
const char kOldCheckboxStyle[]           = "old-checkbox-style";

// Disable ui::MessageBox. This is useful when running as part of scripts that
// do not have a user interface.
const char kNoMessageBox[] = "no-message-box";

// Enables UI changes that make it easier to use with a touchscreen.
// WARNING: Do not check this flag directly when deciding what UI to draw,
// instead you must call ui::GetDisplayLayout
const char kTouchOptimizedUI[]              = "touch-optimized-ui";

// The values the kTouchOptimizedUI switch may have, as in
// "--touch-optimized-ui=disabled".
//   auto: Enabled on monitors which have touchscreen support (default).
const char kTouchOptimizedUIAuto[] = "auto";
//   enabled: always optimized for touch (even if no touch support).
const char kTouchOptimizedUIEnabled[] = "enabled";
//   disabled: never optimized for touch.
const char kTouchOptimizedUIDisabled[] = "disabled";

#if defined(OS_MACOSX)
const char kDisableCompositedCoreAnimationPlugins[] =
    "disable-composited-core-animation-plugins";
// Disables using core animation in plugins. This is triggered when accelerated
// compositing is disabled. See http://crbug.com/122430
const char kDisableCoreAnimationPlugins[] =
    "disable-core-animation-plugins";
#endif

#if defined(TOOLKIT_VIEWS) && defined(OS_LINUX)
// Tells chrome to interpret events from these devices as touch events. Only
// available with XInput 2 (i.e. X server 1.8 or above). The id's of the
// devices can be retrieved from 'xinput list'.
const char kTouchDevices[] = "touch-devices";
#endif

}  // namespace switches
