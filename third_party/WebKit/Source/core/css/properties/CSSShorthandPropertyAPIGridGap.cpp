// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIGridGap.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

bool CSSShorthandPropertyAPIGridGap::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  DCHECK_EQ(shorthandForProperty(CSSPropertyGridGap).length(), 2u);
  CSSValue* row_gap = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  CSSValue* column_gap = CSSPropertyParserHelpers::ConsumeLengthOrPercent(
      range, context.Mode(), kValueRangeNonNegative);
  if (!row_gap || !range.AtEnd())
    return false;
  if (!column_gap)
    column_gap = row_gap;
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridRowGap, CSSPropertyGridGap, *row_gap, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyGridColumnGap, CSSPropertyGridGap, *column_gap, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

}  // namespace blink
