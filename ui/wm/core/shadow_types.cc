// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_types.h"

#include "ui/aura/window_property.h"

DECLARE_WINDOW_PROPERTY_TYPE(wm::ShadowElevation);

namespace wm {

void SetShadowElevation(aura::Window* window, ShadowElevation shadow_type) {
  window->SetProperty(kShadowElevationKey, shadow_type);
}

ShadowElevation GetShadowElevation(aura::Window* window) {
  return window->GetProperty(kShadowElevationKey);
}

DEFINE_WINDOW_PROPERTY_KEY(ShadowElevation,
                           kShadowElevationKey,
                           ShadowElevation::NONE);

}  // namespace wm
