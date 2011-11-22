// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_INSETS_H_
#define UI_GFX_INSETS_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/base/ui_export.h"

#if defined(TOOLKIT_USES_GTK)
#include <gtk/gtk.h>
#endif

namespace gfx {

//
// An insets represents the borders of a container (the space the container must
// leave at each of its edges).
//

class UI_EXPORT Insets {
 public:
  Insets() : top_(0), left_(0), bottom_(0), right_(0) {}
  Insets(int top, int left, int bottom, int right)
      : top_(top),
        left_(left),
        bottom_(bottom),
        right_(right) {}
#if defined(TOOLKIT_USES_GTK)
  explicit Insets(const GtkBorder& border)
      : top_(border.top),
        left_(border.left),
        bottom_(border.bottom),
        right_(border.right) {}
#endif

  ~Insets() {}

  int top() const { return top_; }
  int left() const { return left_; }
  int bottom() const { return bottom_; }
  int right() const { return right_; }

  // Returns the total width taken up by the insets, which is the sum of the
  // left and right insets.
  int width() const { return left_ + right_; }

  // Returns the total height taken up by the insets, which is the sum of the
  // top and bottom insets.
  int height() const { return top_ + bottom_; }

  // Returns true if the insets are empty.
  bool empty() const { return width() == 0 && height() == 0; }

  void Set(int top, int left, int bottom, int right) {
    top_ = top;
    left_ = left;
    bottom_ = bottom;
    right_ = right;
  }

  bool operator==(const Insets& insets) const {
    return top_ == insets.top_ && left_ == insets.left_ &&
           bottom_ == insets.bottom_ && right_ == insets.right_;
  }

  bool operator!=(const Insets& insets) const {
    return !(*this == insets);
  }

  Insets& operator+=(const Insets& insets) {
    top_ += insets.top_;
    left_ += insets.left_;
    bottom_ += insets.bottom_;
    right_ += insets.right_;
    return *this;
  }

  // Returns a string representation of the insets.
  std::string ToString() const;

 private:
  int top_;
  int left_;
  int bottom_;
  int right_;
};

}  // namespace gfx

#endif  // UI_GFX_INSETS_H_
