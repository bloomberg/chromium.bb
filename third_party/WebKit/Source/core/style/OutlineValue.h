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

#ifndef OutlineValue_h
#define OutlineValue_h

#include "core/style/BorderValue.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class OutlineValue final : public BorderValue {
  DISALLOW_NEW();
  friend class ComputedStyle;

 public:
  OutlineValue() : offset_(0) {}

  bool operator==(const OutlineValue& o) const {
    return BorderValue::operator==(o) && offset_ == o.offset_ &&
           is_auto_ == o.is_auto_;
  }

  bool operator!=(const OutlineValue& o) const { return !(*this == o); }

  bool VisuallyEqual(const OutlineValue& o) const {
    if (style_ == static_cast<unsigned>(EBorderStyle::kNone) &&
        o.style_ == static_cast<unsigned>(EBorderStyle::kNone))
      return true;
    return *this == o;
  }

  int Offset() const { return offset_; }
  void SetOffset(int offset) { offset_ = offset; }

 private:
  int offset_;
};

}  // namespace blink

#endif  // OutlineValue_h
