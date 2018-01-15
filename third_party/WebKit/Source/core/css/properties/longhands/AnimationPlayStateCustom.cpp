// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/AnimationPlayState.h"

#include "core/CSSValueKeywords.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationPlayState::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSPropertyParserHelpers::ConsumeIdent<CSSValueRunning, CSSValuePaused>,
      range);
}

const CSSValue* AnimationPlayState::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (size_t i = 0; i < animation_data->PlayStateList().size(); ++i) {
      list->Append(*ComputedStyleUtils::ValueForAnimationPlayState(
          animation_data->PlayStateList()[i]));
    }
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueRunning));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
