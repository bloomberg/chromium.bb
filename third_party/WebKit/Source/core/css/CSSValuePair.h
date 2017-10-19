/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef CSSValuePair_h
#define CSSValuePair_h

#include "core/CoreExport.h"
#include "core/css/CSSValue.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class CORE_EXPORT CSSValuePair : public CSSValue {
 public:
  enum IdenticalValuesPolicy { kDropIdenticalValues, kKeepIdenticalValues };

  static CSSValuePair* Create(const CSSValue* first,
                              const CSSValue* second,
                              IdenticalValuesPolicy identical_values_policy) {
    return new CSSValuePair(first, second, identical_values_policy);
  }

  const CSSValue& First() const { return *first_; }
  const CSSValue& Second() const { return *second_; }

  String CustomCSSText() const {
    String first = first_->CssText();
    String second = second_->CssText();
    if (identical_values_policy_ == kDropIdenticalValues && first == second)
      return first;
    return first + ' ' + second;
  }

  bool Equals(const CSSValuePair& other) const {
    DCHECK_EQ(identical_values_policy_, other.identical_values_policy_);
    return DataEquivalent(first_, other.first_) &&
           DataEquivalent(second_, other.second_);
  }

  void TraceAfterDispatch(blink::Visitor*);

 private:
  CSSValuePair(const CSSValue* first,
               const CSSValue* second,
               IdenticalValuesPolicy identical_values_policy)
      : CSSValue(kValuePairClass),
        first_(first),
        second_(second),
        identical_values_policy_(identical_values_policy) {
    DCHECK(first_);
    DCHECK(second_);
  }

  Member<const CSSValue> first_;
  Member<const CSSValue> second_;
  IdenticalValuesPolicy identical_values_policy_;
};

DEFINE_CSS_VALUE_TYPE_CASTS(CSSValuePair, IsValuePair());

}  // namespace blink

#endif  // CSSValuePair_h
