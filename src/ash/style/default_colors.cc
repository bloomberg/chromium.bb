// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/default_colors.h"

#include "ash/public/cpp/ash_features.h"

namespace ash {

SkColor DeprecatedGetShieldLayerColor(AshColorProvider::ShieldLayerType type,
                                      SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetShieldLayerColor(type);
}

SkColor DeprecatedGetBackgroundColor(SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetBackgroundColor();
}

SkColor DeprecatedGetBaseLayerColor(AshColorProvider::BaseLayerType type,
                                    SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetBaseLayerColor(type);
}

SkColor DeprecatedGetControlsLayerColor(
    AshColorProvider::ControlsLayerType type,
    SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetControlsLayerColor(type);
}

SkColor DeprecatedGetContentLayerColor(AshColorProvider::ContentLayerType type,
                                       SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetContentLayerColor(type);
}

}  // namespace ash
