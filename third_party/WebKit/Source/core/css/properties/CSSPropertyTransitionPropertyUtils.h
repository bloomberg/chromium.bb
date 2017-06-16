// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyTransitionPropertyUtils_h
#define CSSPropertyTransitionPropertyUtils_h

#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSValue;
class CSSValueList;

class CSSPropertyTransitionPropertyUtils {
  STATIC_ONLY(CSSPropertyTransitionPropertyUtils);

 public:
  static CSSValue* ConsumeTransitionProperty(CSSParserTokenRange&);
  static bool IsValidPropertyList(const CSSValueList&);
};

}  // namespace blink

#endif  // CSSPropertyTransitionPropertyUtils_h
