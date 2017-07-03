// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography_provider.h"

#include "base/logging.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/style/typography.h"

using gfx::Font;

namespace views {
namespace {

Font::Weight GetValueBolderThan(Font::Weight weight) {
  switch (weight) {
    case Font::Weight::BOLD:
      return Font::Weight::EXTRA_BOLD;
    case Font::Weight::EXTRA_BOLD:
    case Font::Weight::BLACK:
      return Font::Weight::BLACK;
    default:
      return Font::Weight::BOLD;
  }
}

}  // namespace

// static
Font::Weight TypographyProvider::WeightNotLighterThanNormal(
    Font::Weight weight) {
  if (ResourceBundle::GetSharedInstance()
          .GetFontListWithDelta(0, Font::NORMAL, Font::Weight::NORMAL)
          .GetFontWeight() < weight)
    return weight;
  return Font::Weight::NORMAL;
}

const gfx::FontList& DefaultTypographyProvider::GetFont(int context,
                                                        int style) const {
  int size_delta;
  Font::Weight font_weight;
  GetDefaultFont(context, style, &size_delta, &font_weight);
  return ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      size_delta, Font::NORMAL, font_weight);
}

SkColor DefaultTypographyProvider::GetColor(
    int context,
    int style,
    const ui::NativeTheme& theme) const {
  ui::NativeTheme::ColorId color_id =
      ui::NativeTheme::kColorId_LabelEnabledColor;
  if (context == style::CONTEXT_BUTTON_MD) {
    switch (style) {
      case views::style::STYLE_DIALOG_BUTTON_DEFAULT:
        color_id = ui::NativeTheme::kColorId_TextOnProminentButtonColor;
        break;
      case views::style::STYLE_DISABLED:
        color_id = ui::NativeTheme::kColorId_ButtonDisabledColor;
        break;
      default:
        color_id = ui::NativeTheme::kColorId_ButtonEnabledColor;
        break;
    }
  } else if (context == style::CONTEXT_TEXTFIELD) {
    color_id = style == style::STYLE_DISABLED
                   ? ui::NativeTheme::kColorId_TextfieldReadOnlyColor
                   : ui::NativeTheme::kColorId_TextfieldDefaultColor;
  } else if (style == style::STYLE_DISABLED) {
    color_id = ui::NativeTheme::kColorId_LabelDisabledColor;
  }
  return theme.GetSystemColor(color_id);
}

int DefaultTypographyProvider::GetLineHeight(int context, int style) const {
  return 0;
}

// static
void DefaultTypographyProvider::GetDefaultFont(int context,
                                               int style,
                                               int* size_delta,
                                               Font::Weight* font_weight) {
  *font_weight = Font::Weight::NORMAL;

  switch (context) {
    case style::CONTEXT_BUTTON_MD:
      *size_delta = ui::kLabelFontSizeDelta;
      *font_weight = WeightNotLighterThanNormal(Font::Weight::MEDIUM);
      break;
    case style::CONTEXT_DIALOG_TITLE:
      *size_delta = ui::kTitleFontSizeDelta;
      break;
    case style::CONTEXT_TOUCH_MENU:
      *size_delta = -1;
      break;
    default:
      *size_delta = ui::kLabelFontSizeDelta;
      break;
  }

  switch (style) {
    case style::STYLE_TAB_ACTIVE:
      *font_weight = Font::Weight::BOLD;
      break;
    case style::STYLE_DIALOG_BUTTON_DEFAULT:
      // Only non-MD default buttons should "increase" in boldness.
      if (context == style::CONTEXT_BUTTON) {
        *font_weight = GetValueBolderThan(
            ResourceBundle::GetSharedInstance()
                .GetFontListWithDelta(*size_delta, Font::NORMAL, *font_weight)
                .GetFontWeight());
      }
      break;
  }
}

}  // namespace views
