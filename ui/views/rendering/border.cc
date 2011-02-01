// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/rendering/border.h"

#include "gfx/canvas.h"
#include "ui/views/view.h"

namespace ui {

namespace internal {

class SolidColorBorder : public Border {
 public:
  SolidColorBorder(int thickness, SkColor color) : color_(color) {
    set_insets(gfx::Insets(thickness, thickness, thickness, thickness));
  }
  virtual ~SolidColorBorder() {
  }

  // Overridden from Border:
  virtual void Paint(const View* view, gfx::Canvas* canvas) const {
    canvas->FillRectInt(color_, 0, 0, view->width(), insets().top());
    canvas->FillRectInt(color_, 0, 0, insets().left(), view->height());
    canvas->FillRectInt(color_, 0, view->height() - insets().bottom(),
                        view->width(), insets().bottom());
    canvas->FillRectInt(color_, view->width() - insets().right(), 0,
                        insets().right(), view->height());
  }

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(SolidColorBorder);
};

}

////////////////////////////////////////////////////////////////////////////////
// Border, public:

Border::~Border() {
}

// static
Border* Border::CreateSolidBorder(int thickness, SkColor color) {
  return new internal::SolidColorBorder(thickness, color);
}

// static
Border* Border::CreateTransparentBorder(const gfx::Insets& insets) {
  Border* b = new Border;
  b->set_insets(insets);
  return b;
}

void Border::Paint(const View* view, gfx::Canvas* canvas) const {
  // Nothing to do.
}

////////////////////////////////////////////////////////////////////////////////
// Border, private:

Border::Border() {
}

}  // namespace ui

