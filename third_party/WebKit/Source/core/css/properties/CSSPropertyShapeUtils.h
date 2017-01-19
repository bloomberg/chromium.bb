// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyShapeUtils_h
#define CSSPropertyShapeUtils_h

#include "core/css/parser/CSSParserMode.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSValue;
class CSSParserContext;
class CSSParserTokenRange;

class CSSPropertyShapeUtils {
  STATIC_ONLY(CSSPropertyShapeUtils);

  static CSSValue* consumeBasicShape(CSSParserTokenRange&,
                                     const CSSParserContext*);
  static bool consumeRadii(CSSValue* horizontalRadii[4],
                           CSSValue* verticalRadii[4],
                           CSSParserTokenRange&,
                           CSSParserMode,
                           bool useLegacyParsing);
};

}  // namespace blink

#endif  // CSSPropertyShapeUtils_h
