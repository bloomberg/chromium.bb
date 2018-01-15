// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ColumnWidth.h"

#include "core/css/ZoomAdjustedPixelValue.h"
#include "core/css/properties/CSSParsingUtils.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ColumnWidth::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return CSSParsingUtils::ConsumeColumnWidth(range);
}

const CSSValue* ColumnWidth::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.HasAutoColumnWidth())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return ZoomAdjustedPixelValue(style.ColumnWidth(), style);
}

}  // namespace CSSLonghand
}  // namespace blink
