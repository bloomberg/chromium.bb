// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus_border.h"

#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/views/view.h"

namespace views {
namespace {

class DashedFocusBorder : public FocusBorder {
 public:
  DashedFocusBorder(
      int left_inset, int top_inset, int right_inset, int bottom_inset)
      : left_inset_(left_inset),
        top_inset_(top_inset),
        right_inset_(right_inset),
        bottom_inset_(bottom_inset) {
  }

  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE {
    gfx::Rect rect(view.GetLocalBounds());
    rect.Inset(left_inset_, top_inset_, right_inset_, bottom_inset_);
    canvas->DrawFocusRect(rect);
  }

 private:
  int left_inset_;
  int top_inset_;
  int right_inset_;
  int bottom_inset_;

  DISALLOW_COPY_AND_ASSIGN(DashedFocusBorder);
};

class SolidFocusBorder : public FocusBorder {
 public:
  SolidFocusBorder(SkColor focus_color, const gfx::Insets& insets)
      : focus_color_(focus_color), insets_(insets) {
  }

  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE {
    gfx::Rect rect(view.GetLocalBounds());
    rect.Inset(insets_);
    canvas->DrawSolidFocusRect(rect, focus_color_);
  }

 private:
  // The focus color to use.
  SkColor focus_color_;

  // The insets to use.
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(SolidFocusBorder);
};

}  // namespace

FocusBorder::~FocusBorder() {
}

// static
FocusBorder* FocusBorder::CreateDashedFocusBorder() {
  return new DashedFocusBorder(0, 0, 0, 0);
}

// static
FocusBorder* FocusBorder::CreateDashedFocusBorder(
    int left, int top, int right, int bottom) {
  return new DashedFocusBorder(left, top, right, bottom);
}

// static
FocusBorder* FocusBorder::CreateSolidFocusBorder(
    SkColor focus_color, const gfx::Insets& insets) {
  return new SolidFocusBorder(focus_color, insets);
}

FocusBorder::FocusBorder() {
}

}  // namespace views
