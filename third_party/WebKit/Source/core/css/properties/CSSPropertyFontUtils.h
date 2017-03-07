// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyFontUtils_h
#define CSSPropertyFontUtils_h

#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "wtf/Allocator.h"

namespace blink {

class CSSFontFeatureValue;
class CSSParserTokenRange;
class CSSValue;
class CSSValueList;

class CSSPropertyFontUtils {
  STATIC_ONLY(CSSPropertyFontUtils);

  static CSSValue* consumeFontSize(
      CSSParserTokenRange&,
      CSSParserMode,
      CSSPropertyParserHelpers::UnitlessQuirk =
          CSSPropertyParserHelpers::UnitlessQuirk::Forbid);

  static CSSValue* consumeLineHeight(CSSParserTokenRange&, CSSParserMode);

  static CSSValueList* consumeFontFamily(CSSParserTokenRange&);
  static CSSValue* consumeGenericFamily(CSSParserTokenRange&);
  static CSSValue* consumeFamilyName(CSSParserTokenRange&);
  static String concatenateFamilyName(CSSParserTokenRange&);

  static CSSIdentifierValue* consumeFontWeight(CSSParserTokenRange&);

  static CSSValue* consumeFontFeatureSettings(CSSParserTokenRange&);
  static CSSFontFeatureValue* consumeFontFeatureTag(CSSParserTokenRange&);
};

}  // namespace blink

#endif  // CSSPropertyFontUtils_h
