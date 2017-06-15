// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppliedTextDecoration_h
#define AppliedTextDecoration_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class AppliedTextDecoration {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  AppliedTextDecoration(TextDecoration, ETextDecorationStyle, Color);

  TextDecoration Lines() const { return static_cast<TextDecoration>(lines_); }
  ETextDecorationStyle Style() const {
    return static_cast<ETextDecorationStyle>(style_);
  }
  Color GetColor() const { return color_; }
  void SetColor(Color color) { color_ = color; }

  bool operator==(const AppliedTextDecoration&) const;
  bool operator!=(const AppliedTextDecoration& o) const {
    return !(*this == o);
  }

 private:
  unsigned lines_ : kTextDecorationBits;
  unsigned style_ : 3;  // ETextDecorationStyle
  Color color_;
};

}  // namespace blink

#endif  // AppliedTextDecoration_h
