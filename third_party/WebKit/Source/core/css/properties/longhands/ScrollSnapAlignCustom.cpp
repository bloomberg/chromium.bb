// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ScrollSnapAlign.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ScrollSnapAlign::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID x_id = range.Peek().Id();
  if (x_id != CSSValueNone && x_id != CSSValueStart && x_id != CSSValueEnd &&
      x_id != CSSValueCenter)
    return nullptr;
  CSSValue* x_value = CSSPropertyParserHelpers::ConsumeIdent(range);
  if (range.AtEnd())
    return x_value;

  CSSValueID y_id = range.Peek().Id();
  if (y_id != CSSValueNone && y_id != CSSValueStart && y_id != CSSValueEnd &&
      y_id != CSSValueCenter)
    return x_value;
  CSSValue* y_value = CSSPropertyParserHelpers::ConsumeIdent(range);
  CSSValuePair* pair = CSSValuePair::Create(x_value, y_value,
                                            CSSValuePair::kDropIdenticalValues);
  return pair;
}

const CSSValue* ScrollSnapAlign::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForScrollSnapAlign(style.GetScrollSnapAlign(),
                                                     style);
}

}  // namespace CSSLonghand
}  // namespace blink
