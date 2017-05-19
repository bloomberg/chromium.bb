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

  bool HasBorderFill() const { return image_.HasImage() && image_.Fill(); }

  bool operator==(const BorderData& o) const { return image_ == o.image_; }

  bool VisuallyEqual(const BorderData& o) const { return image_ == o.image_; }

  bool VisualOverflowEqual(const BorderData& o) const {
    return image_.Outset() == o.image_.Outset();
  }

  bool operator!=(const BorderData& o) const { return !(*this == o); }


  const NinePieceImage& GetImage() const { return image_; }

 private:
  NinePieceImage image_;
};

}  // namespace blink

#endif  // BorderData_h
