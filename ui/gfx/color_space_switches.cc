// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/color_space_switches.h"
#include "build/build_config.h"

namespace features {

const base::Feature kColorCorrectRendering{"ColorCorrectRendering",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features

namespace switches {

// Force all monitors to be treated as though they have the specified color
// profile. Accepted values are "srgb" and "generic-rgb" (currently used by Mac
// layout tests) and "color-spin-gamma24" (used by layout tests).
const char kForceColorProfile[] = "force-color-profile";

}  // namespace switches
