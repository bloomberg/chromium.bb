/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef CollapsedBorderValue_h
#define CollapsedBorderValue_h

#include "core/style/BorderValue.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CollapsedBorderValue {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  CollapsedBorderValue()
      : color_(0),
        color_is_current_color_(true),
        width_(0),
        style_(kBorderStyleNone),
        precedence_(kBorderPrecedenceOff),
        transparent_(false) {}

  CollapsedBorderValue(const BorderValue& border,
                       const StyleColor& color,
                       EBorderPrecedence precedence)
      : color_(color.Resolve(Color())),
        color_is_current_color_(color.IsCurrentColor()),
        width_(border.NonZero() ? border.Width() : 0),
        style_(border.Style()),
        precedence_(precedence),
        transparent_(border.IsTransparent()) {}

  unsigned Width() const { return style_ > kBorderStyleHidden ? width_ : 0; }
  EBorderStyle Style() const { return static_cast<EBorderStyle>(style_); }
  bool Exists() const { return precedence_ != kBorderPrecedenceOff; }
  StyleColor GetColor() const {
    return color_is_current_color_ ? StyleColor::CurrentColor()
                                   : StyleColor(color_);
  }
  bool IsTransparent() const { return transparent_; }
  EBorderPrecedence Precedence() const {
    return static_cast<EBorderPrecedence>(precedence_);
  }

  bool IsSameIgnoringColor(const CollapsedBorderValue& o) const {
    return Width() == o.Width() && Style() == o.Style() &&
           Precedence() == o.Precedence();
  }

  bool VisuallyEquals(const CollapsedBorderValue& o) const {
    if (!IsVisible() && !o.IsVisible())
      return true;
    return GetColor() == o.GetColor() && IsTransparent() == o.IsTransparent() &&
           IsSameIgnoringColor(o);
  }

  bool IsVisible() const {
    return Style() > kBorderStyleHidden && !IsTransparent() && Exists();
  }

  bool ShouldPaint(
      const CollapsedBorderValue& table_current_border_value) const {
    return IsVisible() && IsSameIgnoringColor(table_current_border_value);
  }

 private:
  Color color_;
  unsigned color_is_current_color_ : 1;
  unsigned width_ : 23;
  unsigned style_ : 4;       // EBorderStyle
  unsigned precedence_ : 3;  // EBorderPrecedence
  unsigned transparent_ : 1;
};

}  // namespace blink

#endif  // CollapsedBorderValue_h
