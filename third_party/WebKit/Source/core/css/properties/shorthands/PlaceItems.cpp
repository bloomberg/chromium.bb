// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/PlaceItems.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSShorthand {

bool PlaceItems::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyPlaceItems).length(), 2u);

  // align-items property does not allow the 'auto' value.
  if (CSSPropertyParserHelpers::IdentMatches<CSSValueAuto>(range.Peek().Id()))
    return false;

  CSSValue* align_items_value = nullptr;
  CSSValue* justify_items_value = nullptr;
  if (!CSSParsingUtils::ConsumePlaceAlignment(
          range, CSSParsingUtils::ConsumeSimplifiedItemPosition,
          align_items_value, justify_items_value))
    return false;

  DCHECK(align_items_value);
  DCHECK(justify_items_value);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyAlignItems, CSSPropertyPlaceItems, *align_items_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyJustifyItems, CSSPropertyPlaceItems, *justify_items_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
