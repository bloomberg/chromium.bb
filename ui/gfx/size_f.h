// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SIZE_F_H_
#define UI_GFX_SIZE_F_H_
#pragma once

#include <string>

#include "build/build_config.h"
#include "ui/base/ui_export.h"

#if !defined(ENABLE_DIP)
#error "This class should be used only when DIP feature is enabled"
#endif

namespace gfx {

// A floating versino of gfx::Size. This is copied, instead of using
// template, to avoid conflict with m19 branch.
// TODO(oshima): Merge this to ui/gfx/size.h using template.
class UI_EXPORT SizeF {
 public:
  SizeF();
  SizeF(float width, float height);
  ~SizeF();

  float width() const { return width_; }
  float height() const { return height_; }

  float GetArea() const { return width_ * height_; }

  void SetSize(float width, float height) {
    set_width(width);
    set_height(height);
  }

  void Enlarge(float width, float height) {
    set_width(width_ + width);
    set_height(height_ + height);
  }

  void set_width(float width);
  void set_height(float height);

  bool operator==(const SizeF& s) const {
    return width_ == s.width_ && height_ == s.height_;
  }

  bool operator!=(const SizeF& s) const {
    return !(*this == s);
  }

  bool IsEmpty() const {
    // Size doesn't allow negative dimensions, so testing for 0 is enough.
    return (width_ == 0) || (height_ == 0);
  }

  std::string ToString() const;

 private:
  float width_;
  float height_;
};

}  // namespace gfx

#endif  // UI_GFX_SIZE_F_H_
