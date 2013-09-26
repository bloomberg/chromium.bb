// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ui_base_switches.h"

namespace switches {

// Disables use of DWM composition for top level windows.
const char kDisableDwmComposition[] = "disable-dwm-composition";

// Disables touch adjustment.
const char kDisableTouchAdjustment[] = "disable-touch-adjustment";

// Disables touch event based drag and drop.
const char kDisableTouchDragDrop[] = "disable-touch-drag-drop";

// Disables controls that support touch base text editing.
const char kDisableTouchEditing[] = "disable-touch-editing";

// Disables the Views textfield on Windows.
const char kDisableViewsTextfield[] = "disable-views-textfield";

// Enables touch event based drag and drop.
const char kEnableTouchDragDrop[] = "enable-touch-drag-drop";

// Enables controls that support touch base text editing.
const char kEnableTouchEditing[] = "enable-touch-editing";

// Enables the Views textfield on Windows.
const char kEnableViewsTextfield[] = "enable-views-textfield";

// If a resource is requested at a scale factor at which it is not available
// or the resource is the incorrect size (based on the size of the 1x resource),
// generates the missing resource and applies a red mask to the generated
// resource. Resources for which hidpi is not supported because of software
// reasons will show up pixelated.
const char kHighlightMissingScaledResources[] =
    "highlight-missing-scaled-resources";

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
// WARNING: Do not check this flag directly when deciding what UI to draw,
// instead you must call ui::GetDisplayLayout
const char kTouchOptimizedUI[] = "touch-optimized-ui";

// The values the kTouchOptimizedUI switch may have, as in
// "--touch-optimized-ui=disabled".
//   auto: Enabled on monitors which have touchscreen support (default).
const char kTouchOptimizedUIAuto[] = "auto";
//   enabled: always optimized for touch (even if no touch support).
const char kTouchOptimizedUIEnabled[] = "enabled";
//   disabled: never optimized for touch.
const char kTouchOptimizedUIDisabled[] = "disabled";

// Enables touch events on the side bezels.
const char kTouchSideBezels[] = "touch-side-bezels";

#if defined(USE_XI2_MT)
// The calibration factors given as "<left>,<right>,<top>,<bottom>".
const char kTouchCalibration[] = "touch-calibration";
#endif

}  // namespace switches
