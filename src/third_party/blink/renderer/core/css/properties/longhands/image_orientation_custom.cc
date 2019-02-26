// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/image_orientation.h"

#include "third_party/blink/renderer/core/css/css_primitive_value.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {
namespace css_longhand {

const CSSValue* ImageOrientation::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  DCHECK(RuntimeEnabledFeatures::ImageOrientationEnabled());
  if (range.Peek().Id() == CSSValueFromImage)
    return css_property_parser_helpers::ConsumeIdent(range);
  if (range.Peek().GetType() != kNumberToken) {
    CSSPrimitiveValue* angle = css_property_parser_helpers::ConsumeAngle(
        range, &context, base::Optional<WebFeature>());
    if (angle && angle->GetDoubleValue() == 0)
      return angle;
  }
  return nullptr;
}

const CSSValue* ImageOrientation::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.RespectImageOrientation() == kRespectImageOrientation)
    return CSSIdentifierValue::Create(CSSValueFromImage);
  return CSSPrimitiveValue::Create(0, CSSPrimitiveValue::UnitType::kDegrees);
}

}  // namespace css_longhand
}  // namespace blink
