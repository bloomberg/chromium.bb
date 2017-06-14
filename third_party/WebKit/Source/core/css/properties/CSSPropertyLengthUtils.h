// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyLengthUtils_h
#define CSSPropertyLengthUtils_h

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyLengthUtils {
  STATIC_ONLY(CSSPropertyLengthUtils);

 public:
  static CSSValue* ConsumeMaxWidthOrHeight(
      CSSParserTokenRange&,
      const CSSParserContext&,
      CSSPropertyParserHelpers::UnitlessQuirk =
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
  static CSSValue* ConsumeWidthOrHeight(
      CSSParserTokenRange&,
      const CSSParserContext&,
      CSSPropertyParserHelpers::UnitlessQuirk =
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
};

}  // namespace blink

#endif  // CSSPropertyLengthUtils_h
