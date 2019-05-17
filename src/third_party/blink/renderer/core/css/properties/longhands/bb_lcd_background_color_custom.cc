// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/bb_lcd_background_color.h"

#include "third_party/blink/renderer/core/css/css_color_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* BbLcdBackgroundColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Allow the special focus color even in HTML Standard parsing mode.
  if (range.Peek().Id() == CSSValueAuto || range.Peek().Id() == CSSValueNone)
    return css_property_parser_helpers::ConsumeIdent(range);
  return css_property_parser_helpers::ConsumeColor(range, context.Mode());
}

const CSSValue* BbLcdBackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  blink::Color color = style.BbLcdBackgroundColor();
  return cssvalue::CSSColorValue::Create(color.Rgb());
}

void BbLcdBackgroundColor::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetLcdBackgroundColorSource(
    ComputedStyleInitialValues::InitialLcdBackgroundColorSource());
  state.Style()->SetBbLcdBackgroundColor(
    ComputedStyleInitialValues::InitialBbLcdBackgroundColor());
}

void BbLcdBackgroundColor::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetLcdBackgroundColorSource(
    state.ParentStyle()->LcdBackgroundColorSource());
  state.Style()->SetBbLcdBackgroundColor(
    state.ParentStyle()->BbLcdBackgroundColor());
}

void BbLcdBackgroundColor::ApplyValue(StyleResolverState& state,
                                      const CSSValue& value) const {
  if (value.IsIdentifierValue()) {
      CSSValueID id = ToCSSIdentifierValue(value).GetValueID();

      switch (id) {
      case CSSValueNone:
          state.Style()->SetLcdBackgroundColorSource(
              ELcdBackgroundColorSource::kNone);
          state.Style()->SetBbLcdBackgroundColor(
              blink::Color::kTransparent);
          return;
      case CSSValueAuto:
          state.Style()->SetLcdBackgroundColorSource(
              ELcdBackgroundColorSource::kAuto);
          state.Style()->SetBbLcdBackgroundColor(
              blink::Color::kTransparent);
          return;
      default:
          if (StyleColor::IsColorKeyword(id)) {
              state.Style()->SetLcdBackgroundColorSource(
                  ELcdBackgroundColorSource::kColor);
              state.Style()->SetBbLcdBackgroundColor(
                  StyleBuilderConverter::ConvertColor(state, value));
          }
          return;
      }
  }
  else if (value.IsColorValue()) {
      state.Style()->SetLcdBackgroundColorSource(
          ELcdBackgroundColorSource::kColor);
      state.Style()->SetBbLcdBackgroundColor(
          StyleBuilderConverter::ConvertColor(state, value));
  }
}

}  // namespace css_longhand
}  // namespace blink
