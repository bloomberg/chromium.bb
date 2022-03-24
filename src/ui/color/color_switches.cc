// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/color_switches.h"

#include "build/build_config.h"

namespace switches {

#if defined(OS_WIN)
// Use the system accent color as the Chrome UI accent color.
const char kPervasiveSystemAccentColor[] = "pervasive-system-accent-color";
#endif

}  // namespace switches
