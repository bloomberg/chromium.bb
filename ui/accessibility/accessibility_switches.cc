// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/accessibility_switches.h"

#include "base/command_line.h"
#include "build/build_config.h"

namespace switches {

// Shows additional checkboxes in Settings to enable Chrome OS accessibility
// features that haven't launched yet.
const char kEnableExperimentalAccessibilityFeatures[] =
    "enable-experimental-accessibility-features";

// Shows additional automatic click features that haven't launched yet.
const char kEnableExperimentalAccessibilityAutoclick[] =
    "enable-experimental-accessibility-autoclick";

// Enables language detection on in-page text content which is then exposed to
// accessibility technology such as screen readers.
const char kEnableExperimentalAccessibilityLanguageDetection[] =
    "enable-experimental-accessibility-language-detection";

// Shows setting to enable Switch Access before it has launched.
const char kEnableExperimentalAccessibilitySwitchAccess[] =
    "enable-experimental-accessibility-switch-access";

// Enables language switching feature that hasn't launched yet.
const char kEnableExperimentalAccessibilityChromeVoxLanguageSwitching[] =
    "enable-experimental-accessibility-chromevox-language-switching";

bool AreExperimentalAccessibilityFeaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityFeatures);
}

bool IsExperimentalAccessibilityLanguageDetectionEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalAccessibilityLanguageDetection);
}

#if defined(OS_WIN)
// Toggles between IAccessible and UI Automation platform API.
const char kEnableExperimentalUIAutomation[] =
    "enable-experimental-ui-automation";
#endif

bool IsExperimentalAccessibilityPlatformUIAEnabled() {
#if defined(OS_WIN)
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kEnableExperimentalUIAutomation);
#else
  return false;
#endif
}

}  // namespace switches
