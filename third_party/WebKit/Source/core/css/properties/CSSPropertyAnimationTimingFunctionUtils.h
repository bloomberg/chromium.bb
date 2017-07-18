// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAnimationTimingFunctionUtils_h
#define CSSPropertyAnimationTimingFunctionUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;

class CSSPropertyAnimationTimingFunctionUtils {
  STATIC_ONLY(CSSPropertyAnimationTimingFunctionUtils);

 public:
  static CSSValue* ConsumeAnimationTimingFunction(CSSParserTokenRange&);
};

}  // namespace blink

#endif  // CSSPropertyAnimationTimingFunctionUtils_h
