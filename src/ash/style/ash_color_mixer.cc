// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/ash_color_mixer.h"

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"

namespace ash {

void AddAshColorMixer(ui::ColorProvider* provider,
                      const ui::ColorProviderManager::Key& key) {
  if (!features::IsDarkLightModeEnabled())
    return;

  auto* ash_color_provider = AshColorProvider::Get();
  ui::ColorMixer& mixer = provider->AddMixer();
  mixer[ui::kColorAshSystemUIMenuBackground] = {
      ash_color_provider->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80)};
  mixer[ui::kColorAshSystemUIMenuIcon] = {
      ash_color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)};

  auto [color, opacity] = ash_color_provider->GetInkDropBaseColorAndOpacity();
  mixer[ui::kColorAshSystemUIMenuItemBackgroundSelected] = {
      SkColorSetA(color, opacity * SK_AlphaOPAQUE)};
  mixer[ui::kColorAshSystemUIMenuSeparator] = {
      ash_color_provider->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kSeparatorColor)};
}

}  // namespace ash
