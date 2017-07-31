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

#ifndef StyleReflection_h
#define StyleReflection_h

#include "core/css/CSSReflectionDirection.h"
#include "core/style/NinePieceImage.h"
#include "platform/Length.h"
#include "platform/wtf/RefCounted.h"

namespace blink {

class StyleReflection : public RefCounted<StyleReflection> {
 public:
  static RefPtr<StyleReflection> Create() {
    return AdoptRef(new StyleReflection);
  }

  bool operator==(const StyleReflection& o) const {
    return direction_ == o.direction_ && offset_ == o.offset_ &&
           mask_ == o.mask_;
  }
  bool operator!=(const StyleReflection& o) const { return !(*this == o); }

  CSSReflectionDirection Direction() const { return direction_; }
  Length Offset() const { return offset_; }
  const NinePieceImage& Mask() const { return mask_; }

  void SetDirection(CSSReflectionDirection dir) { direction_ = dir; }
  void SetOffset(const Length& length) { offset_ = length; }
  void SetMask(const NinePieceImage& image) { mask_ = image; }

 private:
  StyleReflection()
      : direction_(kReflectionBelow),
        offset_(0, kFixed),
        mask_(NinePieceImage::MaskDefaults()) {}

  CSSReflectionDirection direction_;
  Length offset_;
  NinePieceImage mask_;
};

}  // namespace blink

#endif  // StyleReflection_h
