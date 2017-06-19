// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIBorderRadius.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyShapeUtils.h"

namespace blink {

bool CSSShorthandPropertyAPIBorderRadius::parseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSProperty, 256>& properties) {
  CSSValue* horizontal_radii[4] = {0};
  CSSValue* vertical_radii[4] = {0};

  if (!CSSPropertyShapeUtils::ConsumeRadii(horizontal_radii, vertical_radii,
                                           range, context.Mode(),
                                           local_context.UseAliasParsing()))
    return false;

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderTopLeftRadius, CSSPropertyBorderRadius,
      *CSSValuePair::Create(horizontal_radii[0], vertical_radii[0],
                            CSSValuePair::kDropIdenticalValues),
      important, false /* implicit */, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderTopRightRadius, CSSPropertyBorderRadius,
      *CSSValuePair::Create(horizontal_radii[1], vertical_radii[1],
                            CSSValuePair::kDropIdenticalValues),
      important, false /* implicit */, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderBottomRightRadius, CSSPropertyBorderRadius,
      *CSSValuePair::Create(horizontal_radii[2], vertical_radii[2],
                            CSSValuePair::kDropIdenticalValues),
      important, false /* implicit */, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBorderBottomLeftRadius, CSSPropertyBorderRadius,
      *CSSValuePair::Create(horizontal_radii[3], vertical_radii[3],
                            CSSValuePair::kDropIdenticalValues),
      important, false /* implicit */, properties);
  return true;
}

}  // namespace blink
