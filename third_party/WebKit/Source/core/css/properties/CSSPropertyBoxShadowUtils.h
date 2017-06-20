// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyBoxShadowUtils_h
#define CSSPropertyBoxShadowUtils_h

#include "core/css/parser/CSSParserMode.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserTokenRange;
class CSSShadowValue;
class CSSValue;

enum class AllowInsetAndSpread { kAllow, kForbid };

class CSSPropertyBoxShadowUtils {
  STATIC_ONLY(CSSPropertyBoxShadowUtils);

 public:
  static CSSValue* ConsumeShadow(CSSParserTokenRange&,
                                 CSSParserMode,
                                 AllowInsetAndSpread);
  static CSSShadowValue* ParseSingleShadow(CSSParserTokenRange&,
                                           CSSParserMode,
                                           AllowInsetAndSpread);
};

}  // namespace blink

#endif  // CSSPropertyBoxShadowUtils_h
