// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DISPLAY_UTIL_H_
#define UI_AURA_DISPLAY_UTIL_H_

#include <string>

#include "ui/aura/aura_export.h"

namespace gfx {
class Display;
}

namespace aura {

// TODO(oshima): OBSOLETE. Eliminate this flag.
AURA_EXPORT void SetUseFullscreenHostWindow(bool use_fullscreen_window);
AURA_EXPORT bool UseFullscreenHostWindow();

// Creates a display from string spec. 100+200-1440x800 creates display
// whose size is 1440x800 at the location (100, 200) in screen's coordinates.
// The location can be omitted and be just "1440x800", which creates
// display at the origin of the screen. An empty string creates
// the display with default size.
//  The device scale factor can be specified by "*", like "1280x780*2",
// or will use the value of |gfx::Display::GetForcedDeviceScaleFactor()| if
// --force-device-scale-factor is specified.
// static gfx::Display CreateDisplayFromSpec(const std::string& spec);
AURA_EXPORT gfx::Display CreateDisplayFromSpec(const std::string& str);

}  // namespace aura

#endif  // UI_AURA_DISPLAY_UTIL_H_
