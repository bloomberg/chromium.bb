// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSPropertyFontUtils_h
#define CSSPropertyFontUtils_h

#include "core/css/parser/CSSParserMode.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class CSSFontFeatureValue;
class CSSParserTokenRange;
class CSSValue;
class CSSValueList;

class CSSPropertyFontUtils {
  STATIC_ONLY(CSSPropertyFontUtils);

 public:
  static CSSValue* ConsumeFontSize(
      CSSParserTokenRange&,
      CSSParserMode,
      CSSPropertyParserHelpers::UnitlessQuirk =
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid);

  static CSSValue* ConsumeLineHeight(CSSParserTokenRange&, CSSParserMode);

  static CSSValueList* ConsumeFontFamily(CSSParserTokenRange&);
  static CSSValue* ConsumeGenericFamily(CSSParserTokenRange&);
  static CSSValue* ConsumeFamilyName(CSSParserTokenRange&);
  static String ConcatenateFamilyName(CSSParserTokenRange&);

  static CSSIdentifierValue* ConsumeFontStretchKeywordOnly(
      CSSParserTokenRange&);
  static CSSValue* ConsumeFontStretch(CSSParserTokenRange&,
                                      const CSSParserMode&);
  static CSSValue* ConsumeFontStyle(CSSParserTokenRange&, const CSSParserMode&);
  static CSSValue* ConsumeFontWeight(CSSParserTokenRange&,
                                     const CSSParserMode&);

  static CSSValue* ConsumeFontFeatureSettings(CSSParserTokenRange&);
  static CSSFontFeatureValue* ConsumeFontFeatureTag(CSSParserTokenRange&);

  static CSSIdentifierValue* ConsumeFontVariantCSS21(CSSParserTokenRange&);
};

}  // namespace blink

#endif  // CSSPropertyFontUtils_h
