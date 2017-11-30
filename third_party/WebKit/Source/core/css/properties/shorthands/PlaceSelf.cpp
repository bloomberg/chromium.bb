// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/PlaceSelf.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSShorthand {

bool PlaceSelf::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyPlaceSelf).length(), 2u);

  CSSValue* align_self_value = nullptr;
  CSSValue* justify_self_value = nullptr;

  if (!CSSParsingUtils::ConsumePlaceAlignment(
          range, CSSParsingUtils::ConsumeSimplifiedItemPosition,
          align_self_value, justify_self_value))
    return false;

  DCHECK(align_self_value);
  DCHECK(justify_self_value);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyAlignSelf, CSSPropertyPlaceSelf, *align_self_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyJustifySelf, CSSPropertyPlaceSelf, *justify_self_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
