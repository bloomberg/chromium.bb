// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/column_gap.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ColumnGap::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeGapLength(range, context);
}

const CSSValue* ColumnGap::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool) const {
  return ComputedStyleUtils::ValueForGapLength(style.ColumnGap(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
