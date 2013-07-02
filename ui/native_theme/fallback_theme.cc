// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/fallback_theme.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "ui/native_theme/common_theme.h"

namespace ui {

FallbackTheme::FallbackTheme() {
}

FallbackTheme::~FallbackTheme() {
}

SkColor FallbackTheme::GetSystemColor(ColorId color_id) const {
  // This implementation returns hardcoded colors.

  static const SkColor kInvalidColorIdColor = SkColorSetRGB(255, 0, 128);
  // Menu:
  static const SkColor kMenuBackgroundColor = SK_ColorWHITE;
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
  static const SkColor kButtonDisabledColor = SkColorSetRGB(0x99, 0x99, 0x99);
  static const SkColor kButtonHighlightColor = SkColorSetRGB(0, 0, 0);
  static const SkColor kButtonHoverColor = kButtonEnabledColor;
  // MenuItem:
  static const SkColor kEnabledMenuItemForegroundColor = kButtonEnabledColor;
  static const SkColor kDisabledMenuItemForegroundColor = kButtonDisabledColor;
  static const SkColor kFocusedMenuItemBackgroundColor =
      SkColorSetRGB(0xF1, 0xF1, 0xF1);
  static const SkColor kHoverMenuItemBackgroundColor =
      SkColorSetARGB(204, 255, 255, 255);
  static const SkColor kMenuSeparatorColor = SkColorSetRGB(0xED, 0xED, 0xED);
  static const SkColor kEnabledMenuButtonBorderColor =
      SkColorSetARGB(36, 0, 0, 0);
  static const SkColor kFocusedMenuButtonBorderColor =
      SkColorSetARGB(72, 0, 0, 0);
  static const SkColor kHoverMenuButtonBorderColor =
      SkColorSetARGB(72, 0, 0, 0);
  // Label:
  static const SkColor kLabelEnabledColor = kButtonEnabledColor;
  static const SkColor kLabelDisabledColor = kButtonDisabledColor;
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
    case kColorId_ButtonDisabledColor:
      return kButtonDisabledColor;
    case kColorId_ButtonHighlightColor:
      return kButtonHighlightColor;
    case kColorId_ButtonHoverColor:
      return kButtonHoverColor;

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
