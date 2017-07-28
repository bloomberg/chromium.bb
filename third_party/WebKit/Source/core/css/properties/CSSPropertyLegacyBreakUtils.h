// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyLegacyBreakUtils_h
#define CSSPropertyLegacyBreakUtils_h

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;

class CSSPropertyLegacyBreakUtils {
  STATIC_ONLY(CSSPropertyLegacyBreakUtils);

 public:
  // The fragmentation spec says that page-break-(after|before|inside) are to be
  // treated as shorthands for their break-(after|before|inside) counterparts.
  // We'll do the same for the non-standard properties
  // -webkit-column-break-(after|before|inside).
  static bool ConsumeFromPageBreakBetween(CSSParserTokenRange&, CSSValueID&);
  static bool ConsumeFromColumnBreakBetween(CSSParserTokenRange&, CSSValueID&);
  static bool ConsumeFromColumnOrPageBreakInside(CSSParserTokenRange&,
                                                 CSSValueID&);
};

}  // namespace blink

#endif  // CSSPropertyLegacyBreakUtils_h
