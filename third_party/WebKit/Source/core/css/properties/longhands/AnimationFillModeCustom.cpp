// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/AnimationFillMode.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationFillMode::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueNone, CSSValueForwards,
                                             CSSValueBackwards, CSSValueBoth>,
      range);
}

const CSSValue* AnimationFillMode::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (size_t i = 0; i < animation_data->FillModeList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationFillMode(
          animation_data->FillModeList()[i]));
    }
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueNone));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
