// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAPI_h
#define CSSPropertyAPI_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSProperty.h"
#include "core/css/parser/CSSParserTokenRange.h"

namespace blink {

class CSSParserContext;
class CSSParserLocalContext;
class CSSValue;

class CSSPropertyAPI {
 public:
  static const CSSPropertyAPI& Get(CSSPropertyID);

  constexpr CSSPropertyAPI() {}

  virtual const CSSValue* ParseSingleValue(CSSPropertyID,
                                           CSSParserTokenRange&,
                                           const CSSParserContext&,
                                           const CSSParserLocalContext&) const;
  virtual bool ParseShorthand(CSSPropertyID,
                              bool important,
                              CSSParserTokenRange&,
                              const CSSParserContext&,
                              const CSSParserLocalContext&,
                              HeapVector<CSSProperty, 256>&) const;

  virtual bool IsInterpolable() const { return false; }
};

}  // namespace blink

#endif  // CSSPropertyAPI_h
