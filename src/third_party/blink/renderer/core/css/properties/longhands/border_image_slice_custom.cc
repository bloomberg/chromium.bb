// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/border_image_slice.h"

#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* BorderImageSlice::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext&,
    const CSSParserLocalContext&) const {
  return css_parsing_utils::ConsumeBorderImageSlice(
      range, css_parsing_utils::DefaultFill::kNoFill);
}

const CSSValue* BorderImageSlice::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForNinePieceImageSlice(style.BorderImage());
}

const CSSValue* BorderImageSlice::InitialValue() const {
  DEFINE_STATIC_LOCAL(
      Persistent<CSSBorderImageSliceValue>, value,
      (CSSBorderImageSliceValue::Create(
          CSSQuadValue::Create(
              CSSPrimitiveValue::Create(
                  100, CSSPrimitiveValue::UnitType::kPercentage),
              CSSQuadValue::kSerializeAsQuad),
          /* fill */ false)));
  return value;
}

}  // namespace css_longhand
}  // namespace blink
