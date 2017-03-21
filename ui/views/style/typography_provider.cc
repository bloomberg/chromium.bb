// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography_provider.h"

#include "base/logging.h"
#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/style/typography.h"

namespace views {

const gfx::FontList& DefaultTypographyProvider::GetFont(int context,
                                                        int style) const {
  int size_delta;
  gfx::Font::Weight font_weight;
  GetDefaultFont(context, style, &size_delta, &font_weight);
  return ResourceBundle::GetSharedInstance().GetFontListWithDelta(
      size_delta, gfx::Font::NORMAL, font_weight);
}

void DefaultTypographyProvider::GetDefaultFont(
    int context,
    int style,
    int* size_delta,
    gfx::Font::Weight* font_weight) const {
  switch (context) {
    case style::CONTEXT_DIALOG_TITLE:
      *size_delta = ui::kTitleFontSizeDelta;
      break;
    default:
      *size_delta = ui::kLabelFontSizeDelta;
      break;
  }

  switch (style) {
    case style::STYLE_TAB_ACTIVE:
      *font_weight = gfx::Font::Weight::BOLD;
      break;
    default:
      *font_weight = gfx::Font::Weight::NORMAL;
      break;
  }
}

SkColor DefaultTypographyProvider::GetColor(int context, int style) const {
  return SK_ColorBLACK;
}

int DefaultTypographyProvider::GetLineHeight(int context, int style) const {
  return 0;
}

}  // namespace views
