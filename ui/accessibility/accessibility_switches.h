// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Define all the command-line switches used by ui/accessibility.
#ifndef UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
#define UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_

#include "ui/accessibility/ax_export.h"

namespace switches {

AX_EXPORT extern const char kEnableExperimentalAccessibilityFeatures[];
AX_EXPORT extern const char kEnableExperimentalAccessibilityAutoclick[];
AX_EXPORT extern const char kEnableExperimentalAccessibilityLabels[];
AX_EXPORT extern const char kEnableExperimentalAccessibilityLanguageDetection[];
AX_EXPORT extern const char kEnableExperimentalAccessibilitySwitchAccess[];
AX_EXPORT extern const char
    kEnableExperimentalAccessibilityChromeVoxLanguageSwitching[];

// Returns true if experimental accessibility features are enabled.
AX_EXPORT bool AreExperimentalAccessibilityFeaturesEnabled();

// Returns true if experimental accessibility language detection is enabled.
AX_EXPORT bool AreExperimentalAccessibilityLanguageDetectionEnabled();
}  // namespace switches

#endif  // UI_ACCESSIBILITY_ACCESSIBILITY_SWITCHES_H_
