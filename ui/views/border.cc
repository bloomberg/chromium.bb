// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include "base/logging.h"
#include "ui/gfx/canvas.h"
#include "ui/views/painter.h"

namespace views {

namespace {

// A simple border with different thicknesses on each side and single color.
class SidedSolidBorder : public Border {
 public:
  SidedSolidBorder(int top, int left, int bottom, int right, SkColor color);

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE;
  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE;

 private:
  int top_, left_, bottom_, right_;
  SkColor color_;
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(SidedSolidBorder);
};

SidedSolidBorder::SidedSolidBorder(int top, int left, int bottom, int right,
    SkColor color)
    : top_(top),
      left_(left),
      bottom_(bottom),
      right_(right),
      color_(color),
      insets_(top, left, bottom, right) {
}

void SidedSolidBorder::Paint(const View& view, gfx::Canvas* canvas) const {
  // Top border.
  canvas->FillRect(gfx::Rect(0, 0, view.width(), insets_.top()), color_);
  // Left border.
  canvas->FillRect(gfx::Rect(0, 0, insets_.left(), view.height()), color_);
  // Bottom border.
  canvas->FillRect(gfx::Rect(0, view.height() - insets_.bottom(), view.width(),
                             insets_.bottom()), color_);
  // Right border.
  canvas->FillRect(gfx::Rect(view.width() - insets_.right(), 0, insets_.right(),
                             view.height()), color_);
}

void SidedSolidBorder::GetInsets(gfx::Insets* insets) const {
  DCHECK(insets);
  insets->Set(insets_.top(), insets_.left(), insets_.bottom(), insets_.right());
}

// A variation of SidedSolidBorder, where each side has the same thickness.
class SolidBorder : public SidedSolidBorder {
 public:
  SolidBorder(int thickness, SkColor color)
      : SidedSolidBorder(thickness, thickness, thickness, thickness, color) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SolidBorder);
};

class EmptyBorder : public Border {
 public:
  EmptyBorder(int top, int left, int bottom, int right)
      : top_(top), left_(left), bottom_(bottom), right_(right) {}

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE {}

  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    DCHECK(insets);
    insets->Set(top_, left_, bottom_, right_);
  }

 private:
  int top_;
  int left_;
  int bottom_;
  int right_;

  DISALLOW_COPY_AND_ASSIGN(EmptyBorder);
};

class BorderPainter : public Border {
 public:
  explicit BorderPainter(Painter* painter)
      : painter_(painter) {
    DCHECK(painter);
  }

  virtual ~BorderPainter() {
    delete painter_;
    painter_ = NULL;
  }

  // Overridden from Border:
  virtual void Paint(const View& view, gfx::Canvas* canvas) const OVERRIDE {
    Painter::PaintPainterAt(canvas, painter_, view.GetLocalBounds());
  }

  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    DCHECK(insets);
    insets->Set(0, 0, 0, 0);
  }

 private:
  Painter* painter_;

  DISALLOW_COPY_AND_ASSIGN(BorderPainter);
};

} // namespace

Border::Border() {
}

Border::~Border() {
}

// static
Border* Border::CreateSolidBorder(int thickness, SkColor color) {
  return new SolidBorder(thickness, color);
}

// static
Border* Border::CreateEmptyBorder(int top, int left, int bottom, int right) {
  return new EmptyBorder(top, left, bottom, right);
}

// static
Border* Border::CreateSolidSidedBorder(int top, int left,
    int bottom, int right,
    SkColor color) {
  return new SidedSolidBorder(top, left, bottom, right, color);
}

//static
Border* Border::CreateBorderPainter(Painter* painter) {
  return new BorderPainter(painter);
}

}  // namespace views
