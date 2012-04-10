// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/focusable_border.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/native_theme.h"
#include "ui/gfx/skia_util.h"

namespace {

// Define the size of the insets
const int kTopInsetSize = 4;
const int kLeftInsetSize = 4;
const int kBottomInsetSize = 4;
const int kRightInsetSize = 4;

}  // namespace

namespace views {

FocusableBorder::FocusableBorder()
    : has_focus_(false),
      insets_(kTopInsetSize, kLeftInsetSize,
              kBottomInsetSize, kRightInsetSize) {
}

void FocusableBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  SkPath path;
  path.addRect(gfx::RectToSkRect(view.GetLocalBounds()), SkPath::kCW_Direction);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  SkColor focus_color = gfx::NativeTheme::instance()->GetSystemColor(
      has_focus_ ? gfx::NativeTheme::kColorId_FocusedBorderColor
          : gfx::NativeTheme::kColorId_UnfocusedBorderColor);
  paint.setColor(focus_color);
  paint.setStrokeWidth(SkIntToScalar(2));

  canvas->DrawPath(path, paint);
}

void FocusableBorder::GetInsets(gfx::Insets* insets) const {
  *insets = insets_;
}

void FocusableBorder::SetInsets(int top, int left, int bottom, int right) {
  insets_.Set(top, left, bottom, right);
}

}  // namespace views
