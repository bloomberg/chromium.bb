// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands.h"

#include "third_party/blink/renderer/core/css/css_color.h"
#include "third_party/blink/renderer/core/css/cssom/css_color_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/css/resolver/style_builder_converter.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver_state.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* BbLcdBackgroundColor::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  // Allow the special focus color even in HTML Standard parsing mode.
  if (range.Peek().Id() == CSSValueID::kAuto || range.Peek().Id() == CSSValueID::kNone)
    return css_parsing_utils::ConsumeIdent(range);
  return css_parsing_utils::ConsumeColor(range, context);
}

const CSSValue* BbLcdBackgroundColor::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const LayoutObject*,
    bool allow_visited_style) const {
  blink::Color color = style.BbLcdBackgroundColor();
  return cssvalue::CSSColor::Create(color.Rgb());
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
  if (state.ParentStyle()->LcdBackgroundColorSource() == ELcdBackgroundColorSource::kAuto) {
    state.Style()->SetBbLcdBackgroundColor(
      state.Style()->EffectiveBackgroundColorForAutoLcd());
  }
  else {
    state.Style()->SetBbLcdBackgroundColor(
      state.ParentStyle()->BbLcdBackgroundColor());
  }
}

void BbLcdBackgroundColor::ApplyValue(StyleResolverState& state,
                                      const CSSValue& value) const {
  if (value.IsIdentifierValue()) {
      auto* identifier_value = DynamicTo<CSSIdentifierValue>(value);
      CSSValueID id = identifier_value->GetValueID();

      switch (id) {
      case CSSValueID::kNone:
          state.Style()->SetLcdBackgroundColorSource(
              ELcdBackgroundColorSource::kNone);
          state.Style()->SetBbLcdBackgroundColor(
              blink::Color::kTransparent);
          return;
      case CSSValueID::kAuto:
          state.Style()->SetLcdBackgroundColorSource(
              ELcdBackgroundColorSource::kAuto);
          state.Style()->SetBbLcdBackgroundColor(
              state.Style()->EffectiveBackgroundColorForAutoLcd());
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
