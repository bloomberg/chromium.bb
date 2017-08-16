// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIBorder.h"

#include "core/css/CSSInitialValue.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {

bool CSSShorthandPropertyAPIBorder::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) {
  CSSValue* width = nullptr;
  const CSSValue* style = nullptr;
  CSSValue* color = nullptr;

  while (!width || !style || !color) {
    if (!width) {
      width = CSSPropertyParserHelpers::ConsumeLineWidth(
          range, context.Mode(),
          CSSPropertyParserHelpers::UnitlessQuirk::kForbid);
      if (width)
        continue;
    }
    if (!style) {
      bool needs_legacy_parsing = false;
      style = CSSPropertyParserHelpers::ParseLonghandViaAPI(
          CSSPropertyBorderLeftStyle, CSSPropertyBorder, context, range,
          needs_legacy_parsing);
      DCHECK(!needs_legacy_parsing);
      if (style)
        continue;
    }
    if (!color) {
      color = CSSPropertyParserHelpers::ConsumeColor(range, context.Mode());
      if (color)
        continue;
    }
    break;
  }

  if (!width && !style && !color)
    return false;

  if (!width)
    width = CSSInitialValue::Create();
  if (!style)
    style = CSSInitialValue::Create();
  if (!color)
    color = CSSInitialValue::Create();

  CSSPropertyParserHelpers::AddExpandedPropertyForValue(
      CSSPropertyBorderWidth, *width, important, properties);
  CSSPropertyParserHelpers::AddExpandedPropertyForValue(
      CSSPropertyBorderStyle, *style, important, properties);
  CSSPropertyParserHelpers::AddExpandedPropertyForValue(
      CSSPropertyBorderColor, *color, important, properties);
  CSSPropertyParserHelpers::AddExpandedPropertyForValue(
      CSSPropertyBorderImage, *CSSInitialValue::Create(), important,
      properties);

  return range.AtEnd();
}

}  // namespace blink
