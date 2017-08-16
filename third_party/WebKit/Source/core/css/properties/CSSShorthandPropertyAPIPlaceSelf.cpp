// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSShorthandPropertyAPIPlaceSelf.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSPropertyAlignmentUtils.h"
#include "core/css/properties/CSSPropertyPlaceUtils.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

bool CSSShorthandPropertyAPIPlaceSelf::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSProperty, 256>& properties) {
  DCHECK(RuntimeEnabledFeatures::CSSGridLayoutEnabled());
  DCHECK_EQ(shorthandForProperty(CSSPropertyPlaceSelf).length(), 2u);

  CSSValue* align_self_value = nullptr;
  CSSValue* justify_self_value = nullptr;

  if (!CSSPropertyPlaceUtils::ConsumePlaceAlignment(
          range, CSSPropertyAlignmentUtils::ConsumeSimplifiedItemPosition,
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
}  // namespace blink
