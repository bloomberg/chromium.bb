// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_types.h"

#include "ui/base/class_property.h"

DECLARE_EXPORTED_UI_CLASS_PROPERTY_TYPE(WM_EXPORT, ::wm::ShadowElevation);

namespace wm {

DEFINE_UI_CLASS_PROPERTY_KEY(ShadowElevation,
                             kShadowElevationKey,
                             ShadowElevation::DEFAULT);

void SetShadowElevation(aura::Window* window, ShadowElevation elevation) {
  window->SetProperty(kShadowElevationKey, elevation);
}

bool IsValidShadowElevation(int64_t value) {
  return value == int64_t(ShadowElevation::DEFAULT) ||
         value == int64_t(ShadowElevation::NONE) ||
         value == int64_t(ShadowElevation::SMALL) ||
         value == int64_t(ShadowElevation::MEDIUM) ||
         value == int64_t(ShadowElevation::LARGE);
}

}  // namespace wm
