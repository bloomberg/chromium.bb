// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/color/chrome_color_provider_utils.h"

#include <string>

#include "base/containers/fixed_flat_map.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "ui/base/buildflags.h"

#include "ui/color/color_id_map_macros.inc"

std::string ChromeColorIdName(ui::ColorId color_id) {
  static constexpr const auto color_id_map =
      base::MakeFixedFlatMap<ui::ColorId, const char*>({CHROME_COLOR_IDS});
  auto* i = color_id_map.find(color_id);
  if (i != color_id_map.cend())
    return {i->second};
  NOTREACHED();
  return "<invalid>";
}

color_utils::HSL GetThemeTint(int id,
                              const ui::ColorProviderManager::Key& key) {
#if !BUILDFLAG(IS_ANDROID)
  color_utils::HSL hsl;
  if (key.custom_theme && key.custom_theme->GetTint(id, &hsl))
    return hsl;
  return ThemeProperties::GetDefaultTint(
      id, false, key.color_mode == ui::ColorProviderManager::ColorMode::kDark);
#else
  return {-1, -1, -1};
#endif  // !BUILDFLAG(IS_ANDROID)
}

#include "ui/color/color_id_map_macros.inc"

SkColor GetToolbarTopSeparatorColor(SkColor toolbar_color,
                                    SkColor frame_color) {
  constexpr float kContrastRatio = 2.0f;

  // Used to generate the initial alpha blended separator color.
  const auto generate_separator_color = [&]() {
    // In most cases, if the tab is lighter than the frame, we darken the frame;
    // if the tab is darker than the frame, we lighten the frame.
    // However, if the frame is already very dark or very light, respectively,
    // this won't contrast sufficiently with the frame color, so we'll need to
    // reverse when we're lightening and darkening.
    SkColor separator_color = SK_ColorWHITE;
    if (color_utils::GetRelativeLuminance(toolbar_color) >=
        color_utils::GetRelativeLuminance(frame_color)) {
      separator_color = color_utils::GetColorWithMaxContrast(separator_color);
    }

    {
      const auto result = color_utils::BlendForMinContrast(
          frame_color, frame_color, separator_color, kContrastRatio);
      if (color_utils::GetContrastRatio(result.color, frame_color) >=
          kContrastRatio) {
        return SkColorSetA(separator_color, result.alpha);
      }
    }

    separator_color = color_utils::GetColorWithMaxContrast(separator_color);

    // If the above call failed to create sufficient contrast, the frame color
    // is already very dark or very light.  Since separators are only used when
    // the tab has low contrast against the frame, the tab color is similarly
    // very dark or very light, just not quite as much so as the frame color.
    // Blend towards the opposite separator color, and compute the contrast
    // against the tab instead of the frame to ensure both contrasts hit the
    // desired minimum.
    const auto result = color_utils::BlendForMinContrast(
        frame_color, toolbar_color, separator_color, kContrastRatio);
    return SkColorSetA(separator_color, result.alpha);
  };

  // The vertical tab separator might show through the stroke if the stroke
  // color is translucent. To prevent this, always use an opaque stroke color.
  return color_utils::GetResultingPaintColor(generate_separator_color(),
                                             frame_color);
}

bool ShouldApplyHighContrastColors(const ui::ColorProviderManager::Key& key) {
  // Only apply custom high contrast handling on platforms where we are not
  // using the system theme for high contrast.
#if BUILDFLAG(USE_GTK) || BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
  return false;
#else
  return key.contrast_mode == ui::ColorProviderManager::ContrastMode::kHigh;
#endif
}
