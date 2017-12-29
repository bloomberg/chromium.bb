// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/BorderSpacing.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"

namespace blink {
namespace CSSShorthand {

bool BorderSpacing::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValue* horizontal_spacing =
      ConsumeLength(range, context.Mode(), kValueRangeNonNegative,
                    CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
  if (!horizontal_spacing)
    return false;
  CSSValue* vertical_spacing = horizontal_spacing;
  if (!range.AtEnd()) {
    vertical_spacing =
        ConsumeLength(range, context.Mode(), kValueRangeNonNegative,
                      CSSPropertyParserHelpers::UnitlessQuirk::kAllow);
  }
  if (!vertical_spacing || !range.AtEnd())
    return false;
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitBorderHorizontalSpacing, CSSPropertyBorderSpacing,
      *horizontal_spacing, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyWebkitBorderVerticalSpacing, CSSPropertyBorderSpacing,
      *vertical_spacing, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
