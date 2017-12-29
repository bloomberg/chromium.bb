// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/PlaceContent.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"

namespace blink {
namespace CSSShorthand {

bool PlaceContent::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  DCHECK_EQ(shorthandForProperty(CSSPropertyPlaceContent).length(), 2u);

  CSSValue* align_content_value = nullptr;
  CSSValue* justify_content_value = nullptr;

  if (!CSSParsingUtils::ConsumePlaceAlignment(
          range, CSSParsingUtils::ConsumeSimplifiedContentPosition,
          align_content_value, justify_content_value))
    return false;

  DCHECK(align_content_value);
  DCHECK(justify_content_value);

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyAlignContent, CSSPropertyPlaceContent, *align_content_value,
      important, CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit,
      properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyJustifyContent, CSSPropertyPlaceContent,
      *justify_content_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);

  return true;
}

}  // namespace CSSShorthand
}  // namespace blink
