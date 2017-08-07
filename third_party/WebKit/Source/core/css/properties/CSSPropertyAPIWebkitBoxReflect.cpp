// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPIWebkitBoxReflect.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyBorderImageUtils.h"

namespace blink {

namespace {

static CSSValue* ConsumeReflect(CSSParserTokenRange& range,
                                const CSSParserContext& context) {
  CSSIdentifierValue* direction = CSSPropertyParserHelpers::ConsumeIdent<
      CSSValueAbove, CSSValueBelow, CSSValueLeft, CSSValueRight>(range);
  if (!direction)
    return nullptr;

  CSSPrimitiveValue* offset = nullptr;
  if (range.AtEnd()) {
    offset = CSSPrimitiveValue::Create(0, CSSPrimitiveValue::UnitType::kPixels);
  } else {
    offset = ConsumeLengthOrPercent(
        range, context.Mode(), kValueRangeAll,
        CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
    if (!offset)
      return nullptr;
  }

  CSSValue* mask = nullptr;
  if (!range.AtEnd()) {
    mask =
        CSSPropertyBorderImageUtils::ConsumeWebkitBorderImage(range, context);
    if (!mask)
      return nullptr;
  }
  return CSSReflectValue::Create(direction, offset, mask);
}

}  // namespace

const CSSValue* CSSPropertyAPIWebkitBoxReflect::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) {
  return ConsumeReflect(range, context);
}

}  // namespace blink
