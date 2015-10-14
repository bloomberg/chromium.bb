// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/border.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/canvas.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace views {

namespace {

// A simple border with different thicknesses on each side and single color.
class SolidSidedBorder : public Border {
 public:
  SolidSidedBorder(const gfx::Insets& insets, SkColor color);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;
  const SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(SolidSidedBorder);
};

SolidSidedBorder::SolidSidedBorder(const gfx::Insets& insets, SkColor color)
    : insets_(insets),
      color_(color) {
}

void SolidSidedBorder::Paint(const View& view, gfx::Canvas* canvas) {
  // Top border.
  canvas->FillRect(gfx::Rect(0, 0, view.width(), insets_.top()), color_);
  // Left border.
  canvas->FillRect(gfx::Rect(0, insets_.top(), insets_.left(),
                             view.height() - insets_.height()), color_);
  // Bottom border.
  canvas->FillRect(gfx::Rect(0, view.height() - insets_.bottom(), view.width(),
                             insets_.bottom()), color_);
  // Right border.
  canvas->FillRect(gfx::Rect(view.width() - insets_.right(), insets_.top(),
                             insets_.right(), view.height() - insets_.height()),
                   color_);
}

gfx::Insets SolidSidedBorder::GetInsets() const {
  return insets_;
}

gfx::Size SolidSidedBorder::GetMinimumSize() const {
  return gfx::Size(insets_.width(), insets_.height());
}

class EmptyBorder : public Border {
 public:
  explicit EmptyBorder(const gfx::Insets& insets);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override ;
  gfx::Size GetMinimumSize() const override;

 private:
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(EmptyBorder);
};

EmptyBorder::EmptyBorder(const gfx::Insets& insets) : insets_(insets) {
}

void EmptyBorder::Paint(const View& view, gfx::Canvas* canvas) {
}

gfx::Insets EmptyBorder::GetInsets() const {
  return insets_;
}

gfx::Size EmptyBorder::GetMinimumSize() const {
  return gfx::Size();
}

class BorderPainter : public Border {
 public:
  BorderPainter(Painter* painter, const gfx::Insets& insets);

  // Overridden from Border:
  void Paint(const View& view, gfx::Canvas* canvas) override;
  gfx::Insets GetInsets() const override;
  gfx::Size GetMinimumSize() const override;

 private:
  scoped_ptr<Painter> painter_;
  const gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(BorderPainter);
};

BorderPainter::BorderPainter(Painter* painter, const gfx::Insets& insets)
    : painter_(painter),
      insets_(insets) {
  DCHECK(painter);
}

void BorderPainter::Paint(const View& view, gfx::Canvas* canvas) {
  Painter::PaintPainterAt(canvas, painter_.get(), view.GetLocalBounds());
}

gfx::Insets BorderPainter::GetInsets() const {
  return insets_;
}

gfx::Size BorderPainter::GetMinimumSize() const {
  return painter_->GetMinimumSize();
}

}  // namespace

Border::Border() {
}

Border::~Border() {
}

// static
scoped_ptr<Border> Border::NullBorder() {
  return nullptr;
}

// static
scoped_ptr<Border> Border::CreateSolidBorder(int thickness, SkColor color) {
  return make_scoped_ptr(new SolidSidedBorder(
      gfx::Insets(thickness, thickness, thickness, thickness), color));
}

// static
scoped_ptr<Border> Border::CreateEmptyBorder(const gfx::Insets& insets) {
  return make_scoped_ptr(new EmptyBorder(insets));
}

// static
scoped_ptr<Border> Border::CreateEmptyBorder(int top,
                                             int left,
                                             int bottom,
                                             int right) {
  return CreateEmptyBorder(gfx::Insets(top, left, bottom, right));
}

// static
scoped_ptr<Border> Border::CreateSolidSidedBorder(int top,
                                                  int left,
                                                  int bottom,
                                                  int right,
                                                  SkColor color) {
  return make_scoped_ptr(new SolidSidedBorder(
      gfx::Insets(top, left, bottom, right), color));
}

// static
scoped_ptr<Border> Border::CreateBorderPainter(Painter* painter,
                                               const gfx::Insets& insets) {
  return make_scoped_ptr(new BorderPainter(painter, insets));
}

}  // namespace views
