// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme_dark_aura.h"

#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"

namespace ui {

NativeThemeDarkAura* NativeThemeDarkAura::instance() {
  CR_DEFINE_STATIC_LOCAL(NativeThemeDarkAura, s_native_theme, ());
  return &s_native_theme;
}

SkColor NativeThemeDarkAura::GetSystemColor(ColorId color_id) const {
  if (!ui::MaterialDesignController::IsModeMaterial())
    return NativeThemeAura::GetSystemColor(color_id);

  static const SkColor kLinkEnabledColor = gfx::kGoogleBlue300;

  static const SkColor kTextfieldDefaultColor = SK_ColorWHITE;
  static const SkColor kTextfieldDefaultBackground =
      SkColorSetRGB(0x62, 0x62, 0x62);
  static const SkColor kTextfieldSelectionBackgroundFocused =
      SkColorSetA(gfx::kGoogleBlue700, 0xCC);

  static const SkColor kResultsTableNormalBackground =
      SkColorSetRGB(0x28, 0x28, 0x28);
  static const SkColor kResultsTableText = SK_ColorWHITE;
  static const SkColor kResultsTableDimmedText =
      SkColorSetA(kResultsTableText, 0x80);

  switch (color_id) {
    // Button
    case kColorId_CallToActionColor:
      return kLinkEnabledColor;

    // Link
    case kColorId_LinkEnabled:
    case kColorId_LinkPressed:
      return kLinkEnabledColor;

    // Textfield
    case kColorId_TextfieldDefaultColor:
    case kColorId_TextfieldSelectionColor:
      return kTextfieldDefaultColor;
    case kColorId_TextfieldDefaultBackground:
      return kTextfieldDefaultBackground;
    case kColorId_TextfieldSelectionBackgroundFocused:
      return kTextfieldSelectionBackgroundFocused;

    // Results Tables
    case kColorId_ResultsTableNormalBackground:
      return kResultsTableNormalBackground;
    case kColorId_ResultsTableNormalText:
    case kColorId_ResultsTableHoveredText:
    case kColorId_ResultsTableSelectedText:
    case kColorId_ResultsTableNormalHeadline:
    case kColorId_ResultsTableHoveredHeadline:
    case kColorId_ResultsTableSelectedHeadline:
      return kResultsTableText;
    case kColorId_ResultsTableNormalDimmedText:
    case kColorId_ResultsTableHoveredDimmedText:
    case kColorId_ResultsTableSelectedDimmedText:
      return kResultsTableDimmedText;

    // Intentional pass-throughs to NativeThemeAura.
    case kColorId_ResultsTableHoveredBackground:
    case kColorId_ResultsTableSelectedBackground:
    case kColorId_ResultsTableNormalUrl:
    case kColorId_ResultsTableHoveredUrl:
    case kColorId_ResultsTableSelectedUrl:
      return NativeThemeAura::GetSystemColor(color_id);

    // Any other color is not defined and shouldn't be used in a dark theme.
    default:
      return gfx::kPlaceholderColor;
  }
}

NativeThemeDarkAura::NativeThemeDarkAura() {}

NativeThemeDarkAura::~NativeThemeDarkAura() {}

}  // namespace ui
