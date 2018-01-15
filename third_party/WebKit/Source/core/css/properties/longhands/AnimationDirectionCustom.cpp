// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/AnimationDirection.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationDirection::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueNormal, CSSValueAlternate,
                                             CSSValueReverse,
                                             CSSValueAlternateReverse>,
      range);
}

const CSSValue* AnimationDirection::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (size_t i = 0; i < animation_data->DirectionList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationDirection(
          animation_data->DirectionList()[i]));
    }
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueNormal));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
