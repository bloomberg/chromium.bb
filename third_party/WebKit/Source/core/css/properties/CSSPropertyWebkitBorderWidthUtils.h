// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyWebkitBorderWidthUtils_h
#define CSSPropertyWebkitBorderWidthUtils_h

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSPropertyWebkitBorderWidthUtils {
  STATIC_ONLY(CSSPropertyWebkitBorderWidthUtils);

 public:
  static CSSValue* ConsumeBorderWidth(CSSParserTokenRange&,
                                      CSSParserMode,
                                      CSSPropertyParserHelpers::UnitlessQuirk);
};

}  // namespace blink

#endif  // CSSPropertyWebkitBorderWidthUtils_h
