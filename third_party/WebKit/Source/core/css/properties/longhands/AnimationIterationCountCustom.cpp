// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/AnimationIterationCount.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationIterationCount::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSParsingUtils::ConsumeAnimationIterationCount, range);
}

const CSSValue* AnimationIterationCount::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (size_t i = 0; i < animation_data->IterationCountList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationIterationCount(
          animation_data->IterationCountList()[i]));
    }
  } else {
    list->Append(
        *CSSPrimitiveValue::Create(CSSAnimationData::InitialIterationCount(),
                                   CSSPrimitiveValue::UnitType::kNumber));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
