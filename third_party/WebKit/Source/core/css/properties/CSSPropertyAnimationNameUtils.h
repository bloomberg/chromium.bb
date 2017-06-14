// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAnimationNameUtils_h
#define CSSPropertyAnimationNameUtils_h

#include "core/css/parser/CSSParserContext.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyAnimationNameUtils {
  STATIC_ONLY(CSSPropertyAnimationNameUtils);

 public:
  static CSSValue* ConsumeAnimationName(CSSParserTokenRange&,
                                        const CSSParserContext*,
                                        bool allow_quoted_name);
};

}  // namespace blink

#endif  // CSSPropertyAnimationNameUtils_h
