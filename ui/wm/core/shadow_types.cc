// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_types.h"

#include "ui/base/class_property.h"

DEFINE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_CORE_EXPORT, ::wm::ShadowElevation);

namespace wm {

DEFINE_UI_CLASS_PROPERTY_KEY(ShadowElevation,
                             kShadowElevationKey,
                             ShadowElevation::DEFAULT);

void SetShadowElevation(aura::Window* window, ShadowElevation elevation) {
  window->SetProperty(kShadowElevationKey, elevation);
}

bool IsValidShadowElevation(int64_t value) {
  switch (static_cast<ShadowElevation>(value)) {
    case ShadowElevation::DEFAULT:
    case ShadowElevation::NONE:
    case ShadowElevation::TINY:
    case ShadowElevation::SMALL:
    case ShadowElevation::MEDIUM:
    case ShadowElevation::LARGE:
      return true;
  }
  return false;
}

}  // namespace wm
