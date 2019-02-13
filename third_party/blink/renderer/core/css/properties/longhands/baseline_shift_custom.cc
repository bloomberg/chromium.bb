// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/baseline_shift.h"

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* BaselineShift::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueBaseline || id == CSSValueSub || id == CSSValueSuper)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeLengthOrPercent(
      range, kSVGAttributeMode, kValueRangeAll);
}

const CSSValue* BaselineShift::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle& svg_style,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  switch (svg_style.BaselineShift()) {
    case BS_SUPER:
      return CSSIdentifierValue::Create(CSSValueSuper);
    case BS_SUB:
      return CSSIdentifierValue::Create(CSSValueSub);
    case BS_LENGTH:
      return ComputedStyleUtils::ZoomAdjustedPixelValueForLength(
          svg_style.BaselineShiftValue(), style);
  }
  NOTREACHED();
  return nullptr;
}

void BaselineShift::ApplyInherit(StyleResolverState& state) const {
  const SVGComputedStyle& parent_svg_style = state.ParentStyle()->SvgStyle();
  EBaselineShift baseline_shift = parent_svg_style.BaselineShift();
  SVGComputedStyle& svg_style = state.Style()->AccessSVGStyle();
  svg_style.SetBaselineShift(baseline_shift);
  if (baseline_shift == BS_LENGTH)
    svg_style.SetBaselineShiftValue(parent_svg_style.BaselineShiftValue());
}

void BaselineShift::ApplyValue(StyleResolverState& state,
                               const CSSValue& value) const {
  SVGComputedStyle& svg_style = state.Style()->AccessSVGStyle();
  if (!value.IsIdentifierValue()) {
    svg_style.SetBaselineShift(BS_LENGTH);
    svg_style.SetBaselineShiftValue(StyleBuilderConverter::ConvertLength(
        state, ToCSSPrimitiveValue(value)));
    return;
  }
  switch (ToCSSIdentifierValue(value).GetValueID()) {
    case CSSValueBaseline:
      svg_style.SetBaselineShift(BS_LENGTH);
      svg_style.SetBaselineShiftValue(Length::Fixed());
      return;
    case CSSValueSub:
      svg_style.SetBaselineShift(BS_SUB);
      return;
    case CSSValueSuper:
      svg_style.SetBaselineShift(BS_SUPER);
      return;
    default:
      NOTREACHED();
  }
}

}  // namespace css_longhand
}  // namespace blink
