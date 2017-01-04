// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SHADOW_TYPES_H_
#define UI_WM_CORE_SHADOW_TYPES_H_

#include "ui/aura/window.h"
#include "ui/wm/wm_export.h"

namespace aura {
class Window;
}

namespace wm {

// Different types of drop shadows that can be drawn under a window by the
// shell. Used as a value for the kShadowTypeKey property. The integer value of
// each entry is directly used for determining the size of the shadow.
enum class ShadowElevation {
  NONE = 0,
  SMALL = 6,
  MEDIUM = 8,
  LARGE = 24,
};

WM_EXPORT void SetShadowElevation(aura::Window* window,
                                  ShadowElevation shadow_type);
WM_EXPORT ShadowElevation GetShadowElevation(aura::Window* window);

// A property key describing the drop shadow that should be displayed under the
// window.  If unset, no shadow is displayed.
extern const aura::WindowProperty<ShadowElevation>* const kShadowElevationKey;

}  // namespace wm

#endif  // UI_WM_CORE_SHADOW_TYPES_H_
