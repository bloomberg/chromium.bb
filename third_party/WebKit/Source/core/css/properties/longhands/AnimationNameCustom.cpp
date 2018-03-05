// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/AnimationName.h"

#include "core/css/parser/CSSParserLocalContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* AnimationName::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context) const {
  // Allow quoted name if this is an alias property.
  return CSSPropertyParserHelpers::ConsumeCommaSeparatedList(
      CSSParsingUtils::ConsumeAnimationName, range, context,
      local_context.UseAliasParsing());
}

const CSSValue* AnimationName::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  const CSSAnimationData* animation_data = style.Animations();
  if (animation_data) {
    for (size_t i = 0; i < animation_data->NameList().size(); ++i)
      list->Append(*CSSCustomIdentValue::Create(animation_data->NameList()[i]));
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueNone));
  }
  return list;
}

}  // namespace CSSLonghand
}  // namespace blink
