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
#include "platform/wtf/Allocator.h"

namespace blink {

class BorderData {
  DISALLOW_NEW();
  friend class ComputedStyle;

 public:
  BorderData() {}

  bool HasBorder() const {
    return left_.NonZero() || right_.NonZero() || top_.NonZero() ||
           bottom_.NonZero();
  }

  bool HasBorderFill() const { return image_.HasImage() && image_.Fill(); }

  bool HasBorderColorReferencingCurrentColor() const {
    return (left_.NonZero() && left_.GetColor().IsCurrentColor()) ||
           (right_.NonZero() && right_.GetColor().IsCurrentColor()) ||
           (top_.NonZero() && top_.GetColor().IsCurrentColor()) ||
           (bottom_.NonZero() && bottom_.GetColor().IsCurrentColor());
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
           bottom_ == o.bottom_ && image_ == o.image_;
  }

  bool VisuallyEqual(const BorderData& o) const {
    return left_.VisuallyEqual(o.left_) && right_.VisuallyEqual(o.right_) &&
           top_.VisuallyEqual(o.top_) && bottom_.VisuallyEqual(o.bottom_) &&
           image_ == o.image_;
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

  const BorderValue& Left() const { return left_; }
  const BorderValue& Right() const { return right_; }
  const BorderValue& Top() const { return top_; }
  const BorderValue& Bottom() const { return bottom_; }

  const NinePieceImage& GetImage() const { return image_; }

 private:
  BorderValue left_;
  BorderValue right_;
  BorderValue top_;
  BorderValue bottom_;

  NinePieceImage image_;
};

}  // namespace blink

#endif  // BorderData_h
