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

#ifndef BorderStyle_h
#define BorderStyle_h

#include "core/css/StyleColor.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/graphics/Color.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class BorderStyle {
  DISALLOW_NEW();
  friend class ComputedStyle;

 public:
  BorderStyle() : style_(kBorderStyleNone), is_auto_(kOutlineIsAutoOff) {}

  bool NonZero() const { return (style_ != kBorderStyleNone); }

  bool operator==(const BorderStyle& o) const { return style_ == o.style_; }

  // The default width is 3px, but if the style is none we compute a value of 0
  // (in ComputedStyle itself)
  bool VisuallyEqual(const BorderStyle& o) const {
    if (style_ == kBorderStyleNone && o.style_ == kBorderStyleNone)
      return true;
    if (style_ == kBorderStyleHidden && o.style_ == kBorderStyleHidden)
      return true;
    return *this == o;
  }

  bool operator!=(const BorderStyle& o) const { return !(*this == o); }

  EBorderStyle Style() const { return static_cast<EBorderStyle>(style_); }
  void SetStyle(EBorderStyle style) { style_ = style; }

  OutlineIsAuto IsAuto() const { return static_cast<OutlineIsAuto>(is_auto_); }
  void SetIsAuto(OutlineIsAuto is_auto) { is_auto_ = is_auto; }

 protected:
  unsigned style_ : 4;  // EBorderStyle

  // This is only used by OutlineValue but moved here to keep the bits packed.
  unsigned is_auto_ : 1;  // OutlineIsAuto
};

}  // namespace blink

#endif  // BorderStyle_h
