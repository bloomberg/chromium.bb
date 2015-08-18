// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/fallback_theme.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/base/resource/material_design/material_design_controller.h"
#include "ui/gfx/color_utils.h"
#include "ui/native_theme/common_theme.h"

#define COLOR_FOR_MODE(ARRAY, MODE)  \
  DCHECK_LT(MODE, arraysize(ARRAY)); \
  return ARRAY[MODE];

namespace ui {

FallbackTheme::FallbackTheme() {
}

FallbackTheme::~FallbackTheme() {
}

SkColor FallbackTheme::GetSystemColor(ColorId color_id) const {
  // This implementation returns hardcoded colors.

  size_t mode = ui::MaterialDesignController::IsModeMaterial();

  static const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
  // Windows:
  static const SkColor kWindowBackgroundColor = SK_ColorWHITE;
  // Dialogs:
  static const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);
  // FocusableBorder:
  static const SkColor kFocusedBorderColor = SkColorSetRGB(0x4D, 0x90, 0xFE);
  static const SkColor kUnfocusedBorderColor = SkColorSetRGB(0xD9, 0xD9, 0xD9);
  // Button:
  static const SkColor kButtonBackgroundColor = SkColorSetRGB(0xDE, 0xDE, 0xDE);
  static const SkColor kButtonEnabledColor = SkColorSetRGB(0x22, 0x22, 0x22);
  static const SkColor kButtonHighlightColor = SkColorSetRGB(0, 0, 0);
  static const SkColor kButtonHoverColor = kButtonEnabledColor;
  // Label:
  static const SkColor kLabelEnabledColor = kButtonEnabledColor;
  static const SkColor kLabelBackgroundColor = SK_ColorWHITE;
  // Textfield:
  static const SkColor kTextfieldDefaultColor = SK_ColorBLACK;
  static const SkColor kTextfieldDefaultBackground = SK_ColorWHITE;
  static const SkColor kTextfieldReadOnlyColor = SK_ColorDKGRAY;
  static const SkColor kTextfieldReadOnlyBackground = SK_ColorWHITE;
  static const SkColor kTextfieldSelectionBackgroundFocused =
      SkColorSetARGB(0x54, 0x60, 0xA8, 0xEB);
  static const SkColor kTextfieldSelectionColor =
      color_utils::AlphaBlend(SK_ColorBLACK,
          kTextfieldSelectionBackgroundFocused, 0xdd);
  // Tooltip
  static const SkColor kTooltipBackground = 0xFFFFFFCC;
  static const SkColor kTooltipTextColor = kLabelEnabledColor;
  // Tree
  static const SkColor kTreeBackground = SK_ColorWHITE;
  static const SkColor kTreeTextColor = SK_ColorBLACK;
  static const SkColor kTreeSelectedTextColor = SK_ColorBLACK;
  static const SkColor kTreeSelectionBackgroundColor =
      SkColorSetRGB(0xEE, 0xEE, 0xEE);
  static const SkColor kTreeArrowColor = SkColorSetRGB(0x7A, 0x7A, 0x7A);
  // Table
  static const SkColor kTableBackground = SK_ColorWHITE;
  static const SkColor kTableTextColor = SK_ColorBLACK;
  static const SkColor kTableSelectedTextColor = SK_ColorBLACK;
  static const SkColor kTableSelectionBackgroundColor =
      SkColorSetRGB(0xEE, 0xEE, 0xEE);
  static const SkColor kTableGroupingIndicatorColor =
      SkColorSetRGB(0xCC, 0xCC, 0xCC);
  // Results Tables
  static const SkColor kResultsTableTextMD = SK_ColorBLACK;
  static const SkColor kResultsTableDimmedTextMD =
      SkColorSetRGB(0x64, 0x64, 0x64);
  static const SkColor kResultsTableUrlMD = SkColorSetRGB(0x33, 0x67, 0xD6);
  static const SkColor kResultsTableHoveredBackground[] = {
      color_utils::AlphaBlend(kTextfieldSelectionBackgroundFocused,
                              kTextfieldDefaultBackground, 0x40),
      SkColorSetARGB(0x0D, 0x00, 0x00, 0x00)};
  static const SkColor kResultsTableSelectedBackground[] = {
      kTextfieldSelectionBackgroundFocused,
      SkColorSetARGB(0x15, 0x00, 0x00, 0x00)};
  static const SkColor kResultsTableNormalText[] = {
      color_utils::AlphaBlend(SK_ColorBLACK, kTextfieldDefaultBackground, 0xDD),
      kResultsTableTextMD};
  static const SkColor kResultsTableHoveredText[] = {
      color_utils::AlphaBlend(
          SK_ColorBLACK, kResultsTableHoveredBackground[mode], 0xDD),
      kResultsTableTextMD};
  static const SkColor kResultsTableSelectedText[] = {
      color_utils::AlphaBlend(
          SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xDD),
      kResultsTableTextMD};
  static const SkColor kResultsTableNormalDimmedText[] = {
      color_utils::AlphaBlend(SK_ColorBLACK, kTextfieldDefaultBackground, 0xBB),
      kResultsTableDimmedTextMD};
  static const SkColor kResultsTableHoveredDimmedText[] = {
      color_utils::AlphaBlend(
          SK_ColorBLACK, kResultsTableHoveredBackground[mode], 0xBB),
      kResultsTableDimmedTextMD};
  static const SkColor kResultsTableSelectedDimmedText[] = {
      color_utils::AlphaBlend(
          SK_ColorBLACK, kTextfieldSelectionBackgroundFocused, 0xBB),
      kResultsTableDimmedTextMD};
  static const SkColor kResultsTableNormalUrl[] = {
      kTextfieldSelectionColor,
      kResultsTableUrlMD};
  static const SkColor kResultsTableSelectedOrHoveredUrl[] = {
      SkColorSetARGB(0xff, 0x0b, 0x80, 0x43),
      kResultsTableUrlMD};
  static const SkColor kResultsTableNormalDivider = color_utils::AlphaBlend(
      kResultsTableNormalText[mode], kTextfieldDefaultBackground, 0x34);
  static const SkColor kResultsTableHoveredDivider = color_utils::AlphaBlend(
      kResultsTableHoveredText[mode],
      kResultsTableHoveredBackground[mode], 0x34);
  static const SkColor kResultsTabSelectedDivider = color_utils::AlphaBlend(
      kResultsTableSelectedText[mode],
      kTextfieldSelectionBackgroundFocused, 0x34);
  const SkColor kPositiveTextColor = SkColorSetRGB(0x0b, 0x80, 0x43);
  const SkColor kNegativeTextColor = SkColorSetRGB(0xc5, 0x39, 0x29);
  static const SkColor kResultsTablePositiveText[] = {
      color_utils::AlphaBlend(
          kPositiveTextColor, kTextfieldDefaultBackground, 0xDD),
      kPositiveTextColor};
  static const SkColor kResultsTablePositiveHoveredText[] = {
      color_utils::AlphaBlend(
          kPositiveTextColor, kResultsTableHoveredBackground[mode], 0xDD),
      kPositiveTextColor};
  static const SkColor kResultsTablePositiveSelectedText[] = {
      color_utils::AlphaBlend(
          kPositiveTextColor, kTextfieldSelectionBackgroundFocused, 0xDD),
      kPositiveTextColor};
  static const SkColor kResultsTableNegativeText[] = {
      color_utils::AlphaBlend(
          kNegativeTextColor, kTextfieldDefaultBackground, 0xDD),
      kNegativeTextColor};
  static const SkColor kResultsTableNegativeHoveredText[] = {
      color_utils::AlphaBlend(
          kNegativeTextColor, kResultsTableHoveredBackground[mode], 0xDD),
      kNegativeTextColor};
  static const SkColor kResultsTableNegativeSelectedText[] = {
      color_utils::AlphaBlend(
          kNegativeTextColor, kTextfieldSelectionBackgroundFocused, 0xDD),
      kNegativeTextColor};

  SkColor color;
  if (CommonThemeGetSystemColor(color_id, &color))
    return color;

  switch (color_id) {
    // Windows
    case kColorId_WindowBackground:
      return kWindowBackgroundColor;

    // Dialogs
    case kColorId_DialogBackground:
      return kDialogBackgroundColor;

    // FocusableBorder
    case kColorId_FocusedBorderColor:
      return kFocusedBorderColor;
    case kColorId_UnfocusedBorderColor:
      return kUnfocusedBorderColor;

    // Button
    case kColorId_ButtonBackgroundColor:
      return kButtonBackgroundColor;
    case kColorId_ButtonEnabledColor:
      return kButtonEnabledColor;
    case kColorId_ButtonHighlightColor:
      return kButtonHighlightColor;
    case kColorId_ButtonHoverColor:
      return kButtonHoverColor;

    // Label
    case kColorId_LabelEnabledColor:
      return kLabelEnabledColor;
    case kColorId_LabelDisabledColor:
      return GetSystemColor(kColorId_ButtonDisabledColor);
    case kColorId_LabelBackgroundColor:
      return kLabelBackgroundColor;

    // Textfield
    case kColorId_TextfieldDefaultColor:
      return kTextfieldDefaultColor;
    case kColorId_TextfieldDefaultBackground:
      return kTextfieldDefaultBackground;
    case kColorId_TextfieldReadOnlyColor:
      return kTextfieldReadOnlyColor;
    case kColorId_TextfieldReadOnlyBackground:
      return kTextfieldReadOnlyBackground;
    case kColorId_TextfieldSelectionColor:
      return kTextfieldSelectionColor;
    case kColorId_TextfieldSelectionBackgroundFocused:
      return kTextfieldSelectionBackgroundFocused;

    // Tooltip
    case kColorId_TooltipBackground:
      return kTooltipBackground;
    case kColorId_TooltipText:
      return kTooltipTextColor;

    // Tree
    case kColorId_TreeBackground:
      return kTreeBackground;
    case kColorId_TreeText:
      return kTreeTextColor;
    case kColorId_TreeSelectedText:
    case kColorId_TreeSelectedTextUnfocused:
      return kTreeSelectedTextColor;
    case kColorId_TreeSelectionBackgroundFocused:
    case kColorId_TreeSelectionBackgroundUnfocused:
      return kTreeSelectionBackgroundColor;
    case kColorId_TreeArrow:
      return kTreeArrowColor;

    // Table
    case kColorId_TableBackground:
      return kTableBackground;
    case kColorId_TableText:
      return kTableTextColor;
    case kColorId_TableSelectedText:
    case kColorId_TableSelectedTextUnfocused:
      return kTableSelectedTextColor;
    case kColorId_TableSelectionBackgroundFocused:
    case kColorId_TableSelectionBackgroundUnfocused:
      return kTableSelectionBackgroundColor;
    case kColorId_TableGroupingIndicatorColor:
      return kTableGroupingIndicatorColor;

    // Results Tables
    case kColorId_ResultsTableNormalBackground:
      return kTextfieldDefaultBackground;
    case kColorId_ResultsTableHoveredBackground:
      COLOR_FOR_MODE(kResultsTableHoveredBackground, mode);
    case kColorId_ResultsTableSelectedBackground:
      COLOR_FOR_MODE(kResultsTableSelectedBackground, mode);
    case kColorId_ResultsTableNormalText:
      COLOR_FOR_MODE(kResultsTableNormalText, mode);
    case kColorId_ResultsTableHoveredText:
      COLOR_FOR_MODE(kResultsTableHoveredText, mode);
    case kColorId_ResultsTableSelectedText:
      COLOR_FOR_MODE(kResultsTableSelectedText, mode);
    case kColorId_ResultsTableNormalDimmedText:
      COLOR_FOR_MODE(kResultsTableNormalDimmedText, mode);
    case kColorId_ResultsTableHoveredDimmedText:
      COLOR_FOR_MODE(kResultsTableHoveredDimmedText, mode);
    case kColorId_ResultsTableSelectedDimmedText:
      COLOR_FOR_MODE(kResultsTableSelectedDimmedText, mode);
    case kColorId_ResultsTableNormalUrl:
      COLOR_FOR_MODE(kResultsTableNormalUrl, mode);
    case kColorId_ResultsTableHoveredUrl:
    case kColorId_ResultsTableSelectedUrl:
      COLOR_FOR_MODE(kResultsTableSelectedOrHoveredUrl, mode);
    case kColorId_ResultsTableNormalDivider:
      return kResultsTableNormalDivider;
    case kColorId_ResultsTableHoveredDivider:
      return kResultsTableHoveredDivider;
    case kColorId_ResultsTableSelectedDivider:
      return kResultsTabSelectedDivider;
    case kColorId_ResultsTablePositiveText:
      COLOR_FOR_MODE(kResultsTablePositiveText, mode);
    case kColorId_ResultsTablePositiveHoveredText:
      COLOR_FOR_MODE(kResultsTablePositiveHoveredText, mode);
    case kColorId_ResultsTablePositiveSelectedText:
      COLOR_FOR_MODE(kResultsTablePositiveSelectedText, mode);
    case kColorId_ResultsTableNegativeText:
      COLOR_FOR_MODE(kResultsTableNegativeText, mode);
    case kColorId_ResultsTableNegativeHoveredText:
      COLOR_FOR_MODE(kResultsTableNegativeHoveredText, mode);
    case kColorId_ResultsTableNegativeSelectedText:
      COLOR_FOR_MODE(kResultsTableNegativeSelectedText, mode);

    default:
      NOTREACHED();
      break;
  }

  return kInvalidColorIdColor;
}

}  // namespace ui
