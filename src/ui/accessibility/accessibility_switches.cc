// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_switches.h"

#include "base/command_line.h"

namespace switches {

// Shows additional checkboxes in Settings to enable Chrome OS accessibility
// features that haven't launched yet.
const char kEnableExperimentalAccessibilityFeatures[] =
    "enable-experimental-accessibility-features";

// Shows additional automatic click features that haven't launched yet.
const char kEnableExperimentalAccessibilityAutoclick[] =
    "enable-experimental-accessibility-autoclick";

// Shows setting to enable Switch Access before it has launched.
const char kEnableExperimentalAccessibilitySwitchAccess[] =
    "enable-experimental-accessibility-switch-access";

bool AreExperimentalAccessibilityFeaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityFeatures);
}

}  // namespace switches
