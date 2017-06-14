// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyShapeUtils_h
#define CSSPropertyShapeUtils_h

#include "core/css/parser/CSSParserMode.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSValue;
class CSSParserContext;
class CSSParserTokenRange;

class CSSPropertyShapeUtils {
  STATIC_ONLY(CSSPropertyShapeUtils);

 public:
  static CSSValue* ConsumeBasicShape(CSSParserTokenRange&,
                                     const CSSParserContext&);
  static bool ConsumeRadii(CSSValue* horizontal_radii[4],
                           CSSValue* vertical_radii[4],
                           CSSParserTokenRange&,
                           CSSParserMode,
                           bool use_legacy_parsing);
};

}  // namespace blink

#endif  // CSSPropertyShapeUtils_h
