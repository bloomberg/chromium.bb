// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_BORDER_H_
#define UI_VIEWS_BORDER_H_

#include "base/logging.h"
#include "gfx/insets.h"
#include "third_party/skia/include/core/SkColor.h"

namespace gfx {
class Canvas;
}

namespace ui {

class View;

////////////////////////////////////////////////////////////////////////////////
// Border class
//
//  A class that provides padding for a View. Subclass to provide custom
//  rendering of the Border. Insets determine the size of border.
//
class Border {
 public:
  virtual ~Border();

  // Create various common border types.
  static Border* CreateSolidBorder(int thickness, SkColor color);
  static Border* CreateTransparentBorder(const gfx::Insets& insets);

  gfx::Insets insets() const { return insets_; }
  void set_insets(const gfx::Insets& insets) { insets_ = insets; }

  virtual void Paint(const View* view, gfx::Canvas* canvas) const;

 protected:
  Border();

 private:
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(Border);
};

}  // namespace ui

#endif  // UI_VIEWS_BORDER_H_
