// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppliedTextDecoration_h
#define AppliedTextDecoration_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/graphics/Color.h"
#include "wtf/Allocator.h"

namespace blink {

class AppliedTextDecoration {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  AppliedTextDecoration(TextDecoration, TextDecorationStyle, Color);

  TextDecoration lines() const { return static_cast<TextDecoration>(m_lines); }
  TextDecorationStyle style() const {
    return static_cast<TextDecorationStyle>(m_style);
  }
  Color color() const { return m_color; }
  void setColor(Color color) { m_color = color; }

  bool operator==(const AppliedTextDecoration&) const;
  bool operator!=(const AppliedTextDecoration& o) const {
    return !(*this == o);
  }

 private:
  unsigned m_lines : TextDecorationBits;
  unsigned m_style : 3;  // TextDecorationStyle
  Color m_color;
};

}  // namespace blink

#endif  // AppliedTextDecoration_h
