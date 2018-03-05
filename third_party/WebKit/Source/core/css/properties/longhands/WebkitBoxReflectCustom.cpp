// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/WebkitBoxReflect.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace {

CSSValue* ConsumeReflect(CSSParserTokenRange& range,
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
    mask = CSSParsingUtils::ConsumeWebkitBorderImage(range, context);
    if (!mask)
      return nullptr;
  }
  return cssvalue::CSSReflectValue::Create(direction, offset, mask);
}

}  // namespace
namespace CSSLonghand {

const CSSValue* WebkitBoxReflect::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumeReflect(range, context);
}

const CSSValue* WebkitBoxReflect::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForReflection(style.BoxReflect(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
