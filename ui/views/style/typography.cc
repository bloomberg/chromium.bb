// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography.h"

#include "base/logging.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography_provider.h"

namespace views {
namespace style {
namespace {

void ValidateContextAndStyle(int context, int style) {
  DCHECK_GE(context, VIEWS_TEXT_CONTEXT_START);
  DCHECK_LT(context, TEXT_CONTEXT_MAX);
  DCHECK_GE(style, VIEWS_TEXT_STYLE_START);
}

}  // namespace

const gfx::FontList& GetFont(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetFont(context, style);
}

SkColor GetColor(int context, int style, const ui::NativeTheme* theme) {
  ValidateContextAndStyle(context, style);
  DCHECK(theme);
  return LayoutProvider::Get()->GetTypographyProvider().GetColor(context, style,
                                                                 *theme);
}

int GetLineHeight(int context, int style) {
  ValidateContextAndStyle(context, style);
  return LayoutProvider::Get()->GetTypographyProvider().GetLineHeight(context,
                                                                      style);
}

}  // namespace style
}  // namespace views
