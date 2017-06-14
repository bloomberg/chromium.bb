// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyMarginUtils_h
#define CSSPropertyMarginUtils_h

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSPropertyMarginUtils {
  STATIC_ONLY(CSSPropertyMarginUtils);

 public:
  static CSSValue* ConsumeMarginOrOffset(
      CSSParserTokenRange&,
      CSSParserMode,
      CSSPropertyParserHelpers::UnitlessQuirk);
};

}  // namespace blink

#endif  // CSSPropertyMarginUtils_h
