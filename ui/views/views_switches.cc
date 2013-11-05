// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/views_switches.h"

#include "base/command_line.h"

namespace views {
namespace switches {

// Please keep alphabetized.

// Specifies if a heuristic should be used to determine the most probable
// target of a gesture, where the touch region is represented by a rectangle.
const char kViewsUseRectBasedTargeting[] = "views-use-rect-based-targeting";

bool UseRectBasedTargeting() {
  return CommandLine::ForCurrentProcess()->
      HasSwitch(kViewsUseRectBasedTargeting);
}

}  // namespace switches
}  // namespace views
