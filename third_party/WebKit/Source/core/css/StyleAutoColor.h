// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleAutoColor_h
#define StyleAutoColor_h

#include "core/css/StyleColor.h"
#include "platform/graphics/Color.h"
#include "wtf/Allocator.h"

namespace blink {

class StyleAutoColor {
  DISALLOW_NEW();

 public:
  StyleAutoColor(Color color)
      : m_type(ValueType::SpecifiedColor), m_color(color) {}
  static StyleAutoColor autoColor() { return StyleAutoColor(ValueType::Auto); }
  static StyleAutoColor currentColor() {
    return StyleAutoColor(ValueType::CurrentColor);
  }

  bool isAutoColor() const { return m_type == ValueType::Auto; }
  bool isCurrentColor() const { return m_type == ValueType::CurrentColor; }
  Color color() const {
    DCHECK(m_type == ValueType::SpecifiedColor);
    return m_color;
  }

  Color resolve(Color currentColor) const {
    return m_type == ValueType::SpecifiedColor ? m_color : currentColor;
  }

  StyleColor toStyleColor() const {
    DCHECK(m_type != ValueType::Auto);
    if (m_type == ValueType::SpecifiedColor)
      return StyleColor(m_color);
    DCHECK(m_type == ValueType::CurrentColor);
    return StyleColor::currentColor();
  }

 private:
  enum class ValueType { Auto, CurrentColor, SpecifiedColor };
  StyleAutoColor(ValueType type) : m_type(type) {}

  ValueType m_type;
  Color m_color;
};

inline bool operator==(const StyleAutoColor& a, const StyleAutoColor& b) {
  if (a.isAutoColor() || b.isAutoColor())
    return a.isAutoColor() && b.isAutoColor();
  if (a.isCurrentColor() || b.isCurrentColor())
    return a.isCurrentColor() && b.isCurrentColor();
  return a.color() == b.color();
}

inline bool operator!=(const StyleAutoColor& a, const StyleAutoColor& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // StyleAutoColor_h
