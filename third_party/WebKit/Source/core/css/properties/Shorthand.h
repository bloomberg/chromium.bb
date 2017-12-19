// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Shorthand_h
#define Shorthand_h

#include "core/css/properties/CSSProperty.h"

namespace blink {

class CSSPropertyValue;

class Shorthand : public CSSProperty {
 public:
  constexpr Shorthand() : CSSProperty() {}

  // Parses and consumes entire shorthand value from the token range and adds
  // all constituent parsed longhand properties to the 'properties' set.
  // Returns false if the input is invalid.
  virtual bool ParseShorthand(
      bool important,
      CSSParserTokenRange&,
      const CSSParserContext&,
      const CSSParserLocalContext&,
      HeapVector<CSSPropertyValue, 256>& properties) const {
    NOTREACHED();
    return false;
  }
  bool IsShorthand() const override { return true; }
};

DEFINE_TYPE_CASTS(Shorthand,
                  CSSProperty,
                  shorthand,
                  shorthand->IsShorthand(),
                  shorthand.IsShorthand());

}  // namespace blink

#endif  // Shorthand_h
