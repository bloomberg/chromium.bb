// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focusable_border.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/insets.h"

namespace {

// Define the size of the insets
const int kTopInsetSize = 4;
const int kLeftInsetSize = 4;
const int kBottomInsetSize = 4;
const int kRightInsetSize = 4;

// Color settings for border.
// These are tentative, and should be derived from theme, system
// settings and current settings.
const SkColor kFocusedBorderColor = SK_ColorCYAN;
const SkColor kDefaultBorderColor = SK_ColorGRAY;

}  // namespace

namespace views {

FocusableBorder::FocusableBorder()
    : has_focus_(false),
      insets_(kTopInsetSize, kLeftInsetSize,
              kBottomInsetSize, kRightInsetSize) {
}

void FocusableBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  SkRect rect;
  rect.set(SkIntToScalar(0), SkIntToScalar(0),
           SkIntToScalar(view.width()), SkIntToScalar(view.height()));
  SkScalar corners[8] = {
    // top-left
    SkIntToScalar(insets_.left()),
    SkIntToScalar(insets_.top()),
    // top-right
    SkIntToScalar(insets_.right()),
    SkIntToScalar(insets_.top()),
    // bottom-right
    SkIntToScalar(insets_.right()),
    SkIntToScalar(insets_.bottom()),
    // bottom-left
    SkIntToScalar(insets_.left()),
    SkIntToScalar(insets_.bottom()),
  };
  SkPath path;
  path.addRoundRect(rect, corners);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  // TODO(oshima): Copy what WebKit does for focused border.
  paint.setColor(has_focus_ ? kFocusedBorderColor : kDefaultBorderColor);
  paint.setStrokeWidth(SkIntToScalar(has_focus_ ? 2 : 1));

  canvas->GetSkCanvas()->drawPath(path, paint);
}

void FocusableBorder::GetInsets(gfx::Insets* insets) const {
  *insets = insets_;
}

void FocusableBorder::SetInsets(int top, int left, int bottom, int right) {
  insets_.Set(top, left, bottom, right);
}

}  // namespace views
