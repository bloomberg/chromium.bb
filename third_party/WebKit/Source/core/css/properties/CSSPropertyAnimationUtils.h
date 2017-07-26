// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyAnimationUtils_h
#define CSSPropertyAnimationUtils_h

#include "core/CSSPropertyNames.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;
class CSSValueList;
class StylePropertyShorthand;

constexpr size_t kMaxNumAnimationLonghands = 8;
using ConsumeAnimationItemValue = CSSValue* (*)(CSSPropertyID,
                                                CSSParserTokenRange&,
                                                const CSSParserContext&,
                                                bool use_legacy_parsing);

class CSSPropertyAnimationUtils {
  STATIC_ONLY(CSSPropertyAnimationUtils);

 public:
  static bool ConsumeAnimationShorthand(
      const StylePropertyShorthand&,
      HeapVector<Member<CSSValueList>, kMaxNumAnimationLonghands>&,
      ConsumeAnimationItemValue,
      CSSParserTokenRange&,
      const CSSParserContext&,
      bool use_legacy_parsing);
};

}  // namespace blink

#endif  // CSSPropertyAnimationUtils_h
