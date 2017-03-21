// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/style/typography.h"

#include "base/logging.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/views_delegate.h"

namespace views {
namespace style {
namespace {

void ValidateContextAndStyle(int text_context, int text_style) {
  DCHECK_GE(text_context, VIEWS_TEXT_CONTEXT_START);
  DCHECK_LT(text_context, TEXT_CONTEXT_MAX);
  DCHECK_GE(text_style, VIEWS_TEXT_STYLE_START);
}

}  // namespace

const gfx::FontList& GetFont(int text_context, int text_style) {
  ValidateContextAndStyle(text_context, text_style);
  return ViewsDelegate::GetInstance()->GetTypographyProvider().GetFont(
      text_context, text_style);
}

SkColor GetColor(int text_context, int text_style) {
  ValidateContextAndStyle(text_context, text_style);
  return ViewsDelegate::GetInstance()->GetTypographyProvider().GetColor(
      text_context, text_style);
}

int GetLineHeight(int text_context, int text_style) {
  ValidateContextAndStyle(text_context, text_style);
  return ViewsDelegate::GetInstance()->GetTypographyProvider().GetLineHeight(
      text_context, text_style);
}

}  // namespace style
}  // namespace views
