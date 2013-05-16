// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/fallback_theme.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "ui/native_theme/common_theme.h"

namespace {

const SkColor kMenuBackgroundColor = SK_ColorWHITE;

// Theme colors returned by GetSystemColor().
const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
// Windows:
const SkColor kWindowBackgroundColor = SK_ColorWHITE;
// Dialogs:
const SkColor kDialogBackgroundColor = SkColorSetRGB(251, 251, 251);
// FocusableBorder:
const SkColor kFocusedBorderColor = SkColorSetRGB(0x4D, 0x90, 0xFE);
const SkColor kUnfocusedBorderColor = SkColorSetRGB(0xD9, 0xD9, 0xD9);
// TextButton:
const SkColor kTextButtonBackgroundColor = SkColorSetRGB(0xDE, 0xDE, 0xDE);
const SkColor kTextButtonEnabledColor = SkColorSetRGB(0x22, 0x22, 0x22);
const SkColor kTextButtonDisabledColor = SkColorSetRGB(0x99, 0x99, 0x99);
const SkColor kTextButtonHighlightColor = SkColorSetRGB(0, 0, 0);
const SkColor kTextButtonHoverColor = kTextButtonEnabledColor;
// MenuItem:
const SkColor kEnabledMenuItemForegroundColor = kTextButtonEnabledColor;
const SkColor kDisabledMenuItemForegroundColor = kTextButtonDisabledColor;
const SkColor kFocusedMenuItemBackgroundColor = SkColorSetRGB(0xF1, 0xF1, 0xF1);
const SkColor kHoverMenuItemBackgroundColor =
    SkColorSetARGB(204, 255, 255, 255);
const SkColor kMenuSeparatorColor = SkColorSetRGB(0xED, 0xED, 0xED);
const SkColor kEnabledMenuButtonBorderColor = SkColorSetARGB(36, 0, 0, 0);
const SkColor kFocusedMenuButtonBorderColor = SkColorSetARGB(72, 0, 0, 0);
const SkColor kHoverMenuButtonBorderColor = SkColorSetARGB(72, 0, 0, 0);
// Label:
const SkColor kLabelEnabledColor = kTextButtonEnabledColor;
const SkColor kLabelDisabledColor = kTextButtonDisabledColor;
const SkColor kLabelBackgroundColor = SK_ColorWHITE;
// Textfield:
const SkColor kTextfieldDefaultColor = SK_ColorBLACK;
const SkColor kTextfieldDefaultBackground = SK_ColorWHITE;
const SkColor kTextfieldReadOnlyColor = SK_ColorDKGRAY;
const SkColor kTextfieldReadOnlyBackground = SK_ColorWHITE;
const SkColor kTextfieldSelectionBackgroundFocused =
    SkColorSetARGB(0x54, 0x60, 0xA8, 0xEB);
const SkColor kTextfieldSelectionBackgroundUnfocused = SK_ColorLTGRAY;
const SkColor kTextfieldSelectionColor =
    color_utils::AlphaBlend(SK_ColorBLACK,
        kTextfieldSelectionBackgroundFocused, 0xdd);
// Tree
const SkColor kTreeBackground = SK_ColorWHITE;
const SkColor kTreeTextColor = SK_ColorBLACK;
const SkColor kTreeSelectedTextColor = SK_ColorBLACK;
const SkColor kTreeSelectionBackgroundColor = SkColorSetRGB(0xEE, 0xEE, 0xEE);
const SkColor kTreeArrowColor = SkColorSetRGB(0x7A, 0x7A, 0x7A);
// Table
const SkColor kTableBackground = SK_ColorWHITE;
const SkColor kTableTextColor = SK_ColorBLACK;
const SkColor kTableSelectedTextColor = SK_ColorBLACK;
const SkColor kTableSelectionBackgroundColor = SkColorSetRGB(0xEE, 0xEE, 0xEE);
const SkColor kTableGroupingIndicatorColor = SkColorSetRGB(0xCC, 0xCC, 0xCC);

}  // namespace

namespace ui {

// static
FallbackTheme* FallbackTheme::instance() {
  CR_DEFINE_STATIC_LOCAL(FallbackTheme, s_native_theme, ());
  return &s_native_theme;
}

FallbackTheme::FallbackTheme() {
}

FallbackTheme::~FallbackTheme() {
}

SkColor FallbackTheme::GetSystemColor(ColorId color_id) const {
  // This implementation returns hardcoded colors.
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

    // TextButton
    case kColorId_TextButtonBackgroundColor:
      return kTextButtonBackgroundColor;
    case kColorId_TextButtonEnabledColor:
      return kTextButtonEnabledColor;
    case kColorId_TextButtonDisabledColor:
      return kTextButtonDisabledColor;
    case kColorId_TextButtonHighlightColor:
      return kTextButtonHighlightColor;
    case kColorId_TextButtonHoverColor:
      return kTextButtonHoverColor;

    // MenuItem
    case kColorId_EnabledMenuItemForegroundColor:
      return kEnabledMenuItemForegroundColor;
    case kColorId_DisabledMenuItemForegroundColor:
      return kDisabledMenuItemForegroundColor;
    case kColorId_SelectedMenuItemForegroundColor:
      return kEnabledMenuItemForegroundColor;
    case kColorId_FocusedMenuItemBackgroundColor:
      return kFocusedMenuItemBackgroundColor;
    case kColorId_HoverMenuItemBackgroundColor:
      return kHoverMenuItemBackgroundColor;
    case kColorId_MenuSeparatorColor:
      return kMenuSeparatorColor;
    case kColorId_EnabledMenuButtonBorderColor:
      return kEnabledMenuButtonBorderColor;
    case kColorId_FocusedMenuButtonBorderColor:
      return kFocusedMenuButtonBorderColor;
    case kColorId_HoverMenuButtonBorderColor:
      return kHoverMenuButtonBorderColor;

    // Label
    case kColorId_LabelEnabledColor:
      return kLabelEnabledColor;
    case kColorId_LabelDisabledColor:
      return kLabelDisabledColor;
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
    case kColorId_TextfieldSelectionBackgroundUnfocused:
      return kTextfieldSelectionBackgroundUnfocused;

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

    case kColorId_MenuBackgroundColor:
      return kMenuBackgroundColor;
    case kColorId_MenuBorderColor:
      NOTREACHED();
      break;
  }

  return kInvalidColorIdColor;
}

}  // namespace ui
