// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyTransformUtils_h
#define CSSPropertyTransformUtils_h

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserLocalContext;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyTransformUtils {
  STATIC_ONLY(CSSPropertyTransformUtils);

 public:
  static CSSValue* ConsumeTransformList(CSSParserTokenRange&,
                                        const CSSParserContext&,
                                        const CSSParserLocalContext&);
};

}  // namespace blink

#endif  // CSSPropertyTransformUtils_h
