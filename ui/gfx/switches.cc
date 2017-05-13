// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "ui/gfx/switches.h"

namespace switches {

#if defined(OS_WIN)
// Disables DirectWrite font rendering for general UI elements.
const char kDisableDirectWriteForUI[] = "disable-directwrite-for-ui";
#endif

#if defined(OS_MACOSX)
// Enables the HarfBuzz port of RenderText on Mac (it's already used only for
// text editing; this enables it for everything else).
const char kEnableHarfBuzzRenderText[] = "enable-harfbuzz-rendertext";
#endif

// Run in headless mode, i.e., without a UI or display server dependencies.
const char kHeadless[] = "headless";

// Convert rasterization and compositing inputs to the output color space
// before operating on them.
const char kEnableColorCorrectRendering[] = "enable-color-correct-rendering";

// Force all monitors to be treated as though they have the specified color
// profile. Accepted values are "srgb" and "generic-rgb" (currently used by Mac
// layout tests) and "bt2020-gamma18" (to be used by Mac layout tests in the
// future).
const char kForceColorProfile[] = "force-color-profile";

}  // namespace switches
