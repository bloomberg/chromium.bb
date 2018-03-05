// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "core/css/properties/longhands/Orphans.h"

#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* Orphans::ParseSingleValue(CSSParserTokenRange& range,
                                          const CSSParserContext& context,
                                          const CSSParserLocalContext&) const {
  return CSSPropertyParserHelpers::ConsumePositiveInteger(range);
}

const CSSValue* Orphans::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return CSSPrimitiveValue::Create(style.Orphans(),
                                   CSSPrimitiveValue::UnitType::kNumber);
}

}  // namespace CSSLonghand
}  // namespace blink
