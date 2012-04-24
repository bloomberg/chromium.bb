// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SIZE_BASE_H_
#define UI_GFX_SIZE_BASE_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/base/ui_export.h"

namespace gfx {

// A size has width and height values.
template<typename Class, typename Type>
class UI_EXPORT SizeBase {
 public:
  Type width() const { return width_; }
  Type height() const { return height_; }

  Type GetArea() const { return width_ * height_; }

  void SetSize(Type width, Type height) {
    set_width(width);
    set_height(height);
  }

  void Enlarge(Type width, Type height) {
    set_width(width_ + width);
    set_height(height_ + height);
  }

  Class Scale(float scale) const {
    return Scale(scale, scale);
  }

  Class Scale(float x_scale, float y_scale) const {
    return Class(static_cast<Type>(width_ * x_scale),
                 static_cast<Type>(height_ * y_scale));
  }

  void set_width(Type width);
  void set_height(Type height);

  bool operator==(const Class& s) const {
    return width_ == s.width_ && height_ == s.height_;
  }

  bool operator!=(const Class& s) const {
    return !(*this == s);
  }

  bool IsEmpty() const {
    // Size doesn't allow negative dimensions, so testing for 0 is enough.
    return (width_ == 0) || (height_ == 0);
  }

 protected:
  SizeBase(Type width, Type height);
  // Destructor is intentionally made non virtual and protected.
  // Do not make this public.
  ~SizeBase();

 private:
  Type width_;
  Type height_;
};

}  // namespace gfx

#endif  // UI_GFX_SIZE_BASE_H_
