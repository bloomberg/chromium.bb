// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {

// Extract dark mode settings from |settings| and modify them as needed
// based on |layout_object|.
DarkModeSettings CORE_EXPORT
BuildDarkModeSettings(const Settings& settings,
                      const LayoutObject& layout_object);

// Determine whether the page with the provided |root_layout_object| should have
// its colors inverted, based on the provided |policy|.
//
// This method does not check whether Dark Mode is enabled overall.
bool CORE_EXPORT
ShouldApplyDarkModeFilterToPage(DarkModePagePolicy policy,
                                const LayoutObject& root_layout_object);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ACCESSIBILITY_APPLY_DARK_MODE_H_
