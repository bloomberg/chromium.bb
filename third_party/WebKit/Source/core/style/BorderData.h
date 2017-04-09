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

#ifndef BorderData_h
#define BorderData_h

#include "core/style/BorderValue.h"
#include "core/style/NinePieceImage.h"
#include "platform/LengthSize.h"
#include "platform/geometry/IntRect.h"
#include "wtf/Allocator.h"

namespace blink {

class BorderData {
  DISALLOW_NEW();
  friend class ComputedStyle;

 public:
  BorderData()
      : top_left_(Length(0, kFixed), Length(0, kFixed)),
        top_right_(Length(0, kFixed), Length(0, kFixed)),
        bottom_left_(Length(0, kFixed), Length(0, kFixed)),
        bottom_right_(Length(0, kFixed), Length(0, kFixed)) {}

  bool HasBorder() const {
    return left_.NonZero() || right_.NonZero() || top_.NonZero() ||
           bottom_.NonZero();
  }

  bool HasBorderFill() const { return image_.HasImage() && image_.Fill(); }

  bool HasBorderRadius() const {
    if (!top_left_.Width().IsZero())
      return true;
    if (!top_right_.Width().IsZero())
      return true;
    if (!bottom_left_.Width().IsZero())
      return true;
    if (!bottom_right_.Width().IsZero())
      return true;
    return false;
  }

  float BorderLeftWidth() const {
    if (left_.Style() == kBorderStyleNone ||
        left_.Style() == kBorderStyleHidden)
      return 0;
    return left_.Width();
  }

  float BorderRightWidth() const {
    if (right_.Style() == kBorderStyleNone ||
        right_.Style() == kBorderStyleHidden)
      return 0;
    return right_.Width();
  }

  float BorderTopWidth() const {
    if (top_.Style() == kBorderStyleNone || top_.Style() == kBorderStyleHidden)
      return 0;
    return top_.Width();
  }

  float BorderBottomWidth() const {
    if (bottom_.Style() == kBorderStyleNone ||
        bottom_.Style() == kBorderStyleHidden)
      return 0;
    return bottom_.Width();
  }

  bool operator==(const BorderData& o) const {
    return left_ == o.left_ && right_ == o.right_ && top_ == o.top_ &&
           bottom_ == o.bottom_ && image_ == o.image_ && RadiiEqual(o);
  }

  bool VisuallyEqual(const BorderData& o) const {
    return left_.VisuallyEqual(o.left_) && right_.VisuallyEqual(o.right_) &&
           top_.VisuallyEqual(o.top_) && bottom_.VisuallyEqual(o.bottom_) &&
           image_ == o.image_ && RadiiEqual(o);
  }

  bool VisualOverflowEqual(const BorderData& o) const {
    return image_.Outset() == o.image_.Outset();
  }

  bool operator!=(const BorderData& o) const { return !(*this == o); }

  bool SizeEquals(const BorderData& o) const {
    return BorderLeftWidth() == o.BorderLeftWidth() &&
           BorderTopWidth() == o.BorderTopWidth() &&
           BorderRightWidth() == o.BorderRightWidth() &&
           BorderBottomWidth() == o.BorderBottomWidth();
  }

  bool RadiiEqual(const BorderData& o) const {
    return top_left_ == o.top_left_ && top_right_ == o.top_right_ &&
           bottom_left_ == o.bottom_left_ && bottom_right_ == o.bottom_right_;
  }

  const BorderValue& Left() const { return left_; }
  const BorderValue& Right() const { return right_; }
  const BorderValue& Top() const { return top_; }
  const BorderValue& Bottom() const { return bottom_; }

  const NinePieceImage& GetImage() const { return image_; }

  const LengthSize& TopLeft() const { return top_left_; }
  const LengthSize& TopRight() const { return top_right_; }
  const LengthSize& BottomLeft() const { return bottom_left_; }
  const LengthSize& BottomRight() const { return bottom_right_; }

 private:
  BorderValue left_;
  BorderValue right_;
  BorderValue top_;
  BorderValue bottom_;

  NinePieceImage image_;

  LengthSize top_left_;
  LengthSize top_right_;
  LengthSize bottom_left_;
  LengthSize bottom_right_;
};

}  // namespace blink

#endif  // BorderData_h
