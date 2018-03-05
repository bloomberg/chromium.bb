// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/longhands/ObjectPosition.h"

#include "core/css/CSSValuePair.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "core/css/properties/ComputedStyleUtils.h"
#include "core/frame/WebFeature.h"
#include "core/style/ComputedStyle.h"

namespace blink {
namespace CSSLonghand {

const CSSValue* ObjectPosition::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  return ConsumePosition(range, context,
                         CSSPropertyParserHelpers::UnitlessQuirk::kForbid,
                         WebFeature::kThreeValuedPositionObjectPosition);
}

const CSSValue* ObjectPosition::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return CSSValuePair::Create(
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().X(), style),
      ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          style.ObjectPosition().Y(), style),
      CSSValuePair::kKeepIdenticalValues);
}

}  // namespace CSSLonghand
}  // namespace blink
