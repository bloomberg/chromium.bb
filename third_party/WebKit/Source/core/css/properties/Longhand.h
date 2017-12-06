// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Longhand_h
#define Longhand_h

#include "core/css/properties/CSSProperty.h"

namespace blink {

class CSSValue;
class StyleResolverState;

class Longhand : public CSSProperty {
 public:
  constexpr Longhand(CSSPropertyID id) : CSSProperty(id) {}

  // Parses and consumes a longhand property value from the token range.
  // Returns nullptr if the input is invalid.
  virtual const CSSValue* ParseSingleValue(CSSParserTokenRange&,
                                           const CSSParserContext&,
                                           const CSSParserLocalContext&) const {
    return nullptr;
  }
  virtual void ApplyInitial(StyleResolverState&) const { NOTREACHED(); }
  virtual void ApplyInherit(StyleResolverState&) const { NOTREACHED(); }
  virtual void ApplyValue(StyleResolverState&, const CSSValue&) const {
    NOTREACHED();
  }
  bool IsLonghand() const override { return true; }
};

DEFINE_TYPE_CASTS(Longhand,
                  CSSProperty,
                  longhand,
                  longhand->IsLonghand(),
                  longhand.IsLonghand());

}  // namespace blink

#endif  // Longhand_h
