// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_HIGH_CONTRAST_CHECK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_HIGH_CONTRAST_CHECK_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/graphics/high_contrast_settings.h"

namespace blink {

// TODO(https://crbug.com/925949): Move this to high_contrast_settings.h.
enum class HighContrastPagePolicy {
  // Apply high-contrast filter to all frames, regardless of content.
  kFilterAll,
  // Apply high-contrast filter to frames based on background color.
  kFilterByBackground,
};

// Extract high contrast settings from |settings| and modify them as needed
// based on |layout_object|.
HighContrastSettings CORE_EXPORT
BuildHighContrastSettings(const Settings& settings,
                          const LayoutObject& layout_object);

// Determine whether the page with the provided |root_layout_object| should have
// its colors inverted, based on the provided |policy|.
//
// This method does not check whether High Contrast Mode is enabled overall.
bool CORE_EXPORT
ShouldApplyHighContrastFilterToPage(HighContrastPagePolicy policy,
                                    const LayoutObject& root_layout_object);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_HIGH_CONTRAST_CHECK_H_
