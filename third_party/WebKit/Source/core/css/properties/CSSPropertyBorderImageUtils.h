// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyBorderImageUtils_h
#define CSSPropertyBorderImageUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyBorderImageUtils {
  STATIC_ONLY(CSSPropertyBorderImageUtils);

 public:
  static CSSValue* ConsumeWebkitBorderImage(CSSParserTokenRange&,
                                            const CSSParserContext*);
  static bool ConsumeBorderImageComponents(CSSParserTokenRange&,
                                           const CSSParserContext*,
                                           CSSValue*& source,
                                           CSSValue*& slice,
                                           CSSValue*& width,
                                           CSSValue*& outset,
                                           CSSValue*& repeat,
                                           bool default_fill);
  static CSSValue* ConsumeBorderImageRepeat(CSSParserTokenRange&);
  static CSSValue* ConsumeBorderImageSlice(CSSParserTokenRange&,
                                           bool default_fill);
  static CSSValue* ConsumeBorderImageWidth(CSSParserTokenRange&);
  static CSSValue* ConsumeBorderImageOutset(CSSParserTokenRange&);
};

}  // namespace blink

#endif  // CSSPropertyBorderImageUtils_h
