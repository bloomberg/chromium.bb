// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyBoxShadowUtils.h"

#include "core/CSSValueKeywords.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/Length.h"

namespace blink {

class CSSIdentifierValue;
class CSSValue;

CSSValue* CSSPropertyBoxShadowUtils::ConsumeShadow(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    AllowInsetAndSpread inset_and_spread) {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      ParseSingleShadow, range, css_parser_mode, inset_and_spread);
}

CSSShadowValue* CSSPropertyBoxShadowUtils::ParseSingleShadow(
    CSSParserTokenRange& range,
    CSSParserMode css_parser_mode,
    AllowInsetAndSpread inset_and_spread) {
  CSSIdentifierValue* style = nullptr;
  CSSValue* color = nullptr;

  if (range.AtEnd())
    return nullptr;
  if (range.Peek().Id() == CSSValueInset) {
    if (inset_and_spread != AllowInsetAndSpread::kAllow)
      return nullptr;
    style = CSSPropertyParserHelpers::ConsumeIdent(range);
  }
  color = CSSPropertyParserHelpers::ConsumeColor(range, css_parser_mode);

  CSSPrimitiveValue* horizontal_offset =
      CSSPropertyParserHelpers::ConsumeLength(range, css_parser_mode,
                                              kValueRangeAll);
  if (!horizontal_offset)
    return nullptr;

  CSSPrimitiveValue* vertical_offset = CSSPropertyParserHelpers::ConsumeLength(
      range, css_parser_mode, kValueRangeAll);
  if (!vertical_offset)
    return nullptr;

  CSSPrimitiveValue* blur_radius = CSSPropertyParserHelpers::ConsumeLength(
      range, css_parser_mode, kValueRangeAll);
  CSSPrimitiveValue* spread_distance = nullptr;
  if (blur_radius) {
    // Blur radius must be non-negative.
    if (blur_radius->GetDoubleValue() < 0)
      return nullptr;
    if (inset_and_spread == AllowInsetAndSpread::kAllow) {
      spread_distance = CSSPropertyParserHelpers::ConsumeLength(
          range, css_parser_mode, kValueRangeAll);
    }
  }

  if (!range.AtEnd()) {
    if (!color)
      color = CSSPropertyParserHelpers::ConsumeColor(range, css_parser_mode);
    if (range.Peek().Id() == CSSValueInset) {
      if (inset_and_spread != AllowInsetAndSpread::kAllow || style)
        return nullptr;
      style = CSSPropertyParserHelpers::ConsumeIdent(range);
    }
  }
  return CSSShadowValue::Create(horizontal_offset, vertical_offset, blur_radius,
                                spread_distance, style, color);
}

}  // namespace blink
