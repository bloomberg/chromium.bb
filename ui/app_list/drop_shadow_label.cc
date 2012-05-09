// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/drop_shadow_label.h"

#include "base/utf_string_conversions.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/skbitmap_operations.h"

using views::Label;

namespace app_list {

DropShadowLabel::DropShadowLabel() {
}

DropShadowLabel::~DropShadowLabel() {
}

void DropShadowLabel::SetTextShadows(int shadow_count,
                                     const gfx::ShadowValue* shadows) {
  text_shadows_.clear();

  if (shadow_count && shadows) {
    for (int i = 0; i < shadow_count; ++i)
      text_shadows_.push_back(shadows[i]);
  }
}

gfx::Insets DropShadowLabel::GetInsets() const {
  gfx::Insets insets = views::Label::GetInsets();
  gfx::Insets shadow_margin = gfx::ShadowValue::GetMargin(text_shadows_);
  // Negate |shadow_margin| to convert it to a padding insets needed inside
  // the bounds and combine with label's insets.
  insets += -shadow_margin;
  return insets;
}

void DropShadowLabel::PaintText(gfx::Canvas* canvas,
                                const string16& text,
                                const gfx::Rect& text_bounds,
                                int flags) {
  SkColor text_color = enabled() ? enabled_color() : disabled_color();
  canvas->DrawStringWithShadows(text,
                                font(),
                                text_color,
                                text_bounds,
                                flags,
                                text_shadows_);

  if (HasFocus() || paint_as_focused()) {
    gfx::Rect focus_bounds = text_bounds;
    focus_bounds.Inset(-Label::kFocusBorderPadding,
                       -Label::kFocusBorderPadding);
    canvas->DrawFocusRect(focus_bounds);
  }
}

}  // namespace app_list
