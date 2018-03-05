// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/shorthands/WebkitColumnBreakInside.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSShorthand {

bool WebkitColumnBreakInside::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValueID value;
  if (!CSSParsingUtils::ConsumeFromColumnOrPageBreakInside(range, value)) {
    return false;
  }

  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyBreakInside, CSSPropertyWebkitColumnBreakInside,
      *CSSIdentifierValue::Create(value), important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* WebkitColumnBreakInside::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForWebkitColumnBreakInside(
      style.BreakInside());
}

}  // namespace CSSShorthand
}  // namespace blink
