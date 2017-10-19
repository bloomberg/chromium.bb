/*
 * Copyright (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 */

#ifndef CSSQuadValue_h
#define CSSQuadValue_h

#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

class CORE_EXPORT CSSQuadValue : public CSSValue {
 public:
  enum TypeForSerialization { kSerializeAsRect, kSerializeAsQuad };

  static CSSQuadValue* Create(CSSValue* top,
                              CSSValue* right,
                              CSSValue* bottom,
                              CSSValue* left,
                              TypeForSerialization serialization_type) {
    return new CSSQuadValue(top, right, bottom, left, serialization_type);
  }

  CSSValue* Top() const { return top_.Get(); }
  CSSValue* Right() const { return right_.Get(); }
  CSSValue* Bottom() const { return bottom_.Get(); }
  CSSValue* Left() const { return left_.Get(); }

  TypeForSerialization SerializationType() { return serialization_type_; }

  String CustomCSSText() const;

  bool Equals(const CSSQuadValue& other) const {
    return DataEquivalent(top_, other.top_) &&
           DataEquivalent(right_, other.right_) &&
           DataEquivalent(left_, other.left_) &&
           DataEquivalent(bottom_, other.bottom_);
  }

  void TraceAfterDispatch(blink::Visitor*);

 protected:
  CSSQuadValue(CSSValue* top,
               CSSValue* right,
               CSSValue* bottom,
               CSSValue* left,
               TypeForSerialization serialization_type)
      : CSSValue(kQuadClass),
        serialization_type_(serialization_type),
        top_(top),
        right_(right),
        bottom_(bottom),
        left_(left) {}

 private:
  TypeForSerialization serialization_type_;
  Member<CSSValue> top_;
  Member<CSSValue> right_;
  Member<CSSValue> bottom_;
  Member<CSSValue> left_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSQuadValue, IsQuadValue());

}  // namespace blink

#endif  // CSSQuadValue_h
