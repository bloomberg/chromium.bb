// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/GridRow.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyGridUtils.h"

namespace blink {
namespace CSSShorthand {

bool GridRow::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  const StylePropertyShorthand& shorthand =
      shorthandForProperty(CSSPropertyGridRow);
  DCHECK_EQ(shorthand.length(), 2u);

  CSSValue* start_value = nullptr;
  CSSValue* end_value = nullptr;
  if (!CSSPropertyGridUtils::ConsumeGridItemPositionShorthand(
          important, range, start_value, end_value)) {
    return false;
  }

  CSSPropertyParserHelpers::AddProperty(
      shorthand.properties()[0]->PropertyID(), CSSPropertyGridRow, *start_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      shorthand.properties()[1]->PropertyID(), CSSPropertyGridRow, *end_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
