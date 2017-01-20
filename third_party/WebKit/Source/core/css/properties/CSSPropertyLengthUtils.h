// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyLengthUtils_h
#define CSSPropertyLengthUtils_h

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSParserContext;
class CSSParserTokenRange;
class CSSValue;

class CSSPropertyLengthUtils {
  STATIC_ONLY(CSSPropertyLengthUtils);

  static CSSValue* consumeMaxWidthOrHeight(
      CSSParserTokenRange&,
      const CSSParserContext*,
      CSSPropertyParserHelpers::UnitlessQuirk =
          CSSPropertyParserHelpers::UnitlessQuirk::Forbid);
  static CSSValue* consumeWidthOrHeight(
      CSSParserTokenRange&,
      const CSSParserContext*,
      CSSPropertyParserHelpers::UnitlessQuirk =
          CSSPropertyParserHelpers::UnitlessQuirk::Forbid);
};

}  // namespace blink

#endif  // CSSPropertyLengthUtils_h
