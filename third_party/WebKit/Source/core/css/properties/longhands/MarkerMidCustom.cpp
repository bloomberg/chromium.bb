// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/MarkerMid.h"

#include "core/css/CSSURIValue.h"
#include "core/css/parser/CSSParserTokenRange.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* MarkerMid::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueNone)
    return CSSPropertyParserHelpers::ConsumeIdent(range);
  return CSSPropertyParserHelpers::ConsumeUrl(range, &context);
}

const CSSValue* MarkerMid::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  if (!svg_style.MarkerMidResource().IsEmpty()) {
    return CSSURIValue::Create(
        ComputedStyleUtils::SerializeAsFragmentIdentifier(
            svg_style.MarkerMidResource()));
  }
  return CSSIdentifierValue::Create(CSSValueNone);
}

}  // namespace CSSLonghand
}  // namespace blink
