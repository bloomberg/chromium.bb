// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/webkit_text_emphasis_style.h"

#include "third_party/blink/renderer/core/css/css_primitive_value_mappings.h"
#include "third_party/blink/renderer/core/css/css_string_value.h"
#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* WebkitTextEmphasisStyle::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  CSSValueID id = range.Peek().Id();
  if (id == CSSValueID::kNone)
    return css_property_parser_helpers::ConsumeIdent(range);

  if (CSSValue* text_emphasis_style =
          css_property_parser_helpers::ConsumeString(range))
    return text_emphasis_style;

  CSSIdentifierValue* fill =
      css_property_parser_helpers::ConsumeIdent<CSSValueID::kFilled,
                                                CSSValueID::kOpen>(range);
  CSSIdentifierValue* shape = css_property_parser_helpers::ConsumeIdent<
      CSSValueID::kDot, CSSValueID::kCircle, CSSValueID::kDoubleCircle,
      CSSValueID::kTriangle, CSSValueID::kSesame>(range);
  if (!fill) {
    fill = css_property_parser_helpers::ConsumeIdent<CSSValueID::kFilled,
                                                     CSSValueID::kOpen>(range);
  }
  if (fill && shape) {
    CSSValueList* parsed_values = CSSValueList::CreateSpaceSeparated();
    parsed_values->Append(*fill);
    parsed_values->Append(*shape);
    return parsed_values;
  }
  if (fill)
    return fill;
  if (shape)
    return shape;
  return nullptr;
}

const CSSValue* WebkitTextEmphasisStyle::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  switch (style.GetTextEmphasisMark()) {
    case TextEmphasisMark::kNone:
      return CSSIdentifierValue::Create(CSSValueID::kNone);
    case TextEmphasisMark::kCustom:
      return CSSStringValue::Create(style.TextEmphasisCustomMark());
    case TextEmphasisMark::kAuto:
      NOTREACHED();
      FALLTHROUGH;
    case TextEmphasisMark::kDot:
    case TextEmphasisMark::kCircle:
    case TextEmphasisMark::kDoubleCircle:
    case TextEmphasisMark::kTriangle:
    case TextEmphasisMark::kSesame: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisFill()));
      list->Append(*CSSIdentifierValue::Create(style.GetTextEmphasisMark()));
      return list;
    }
  }
  NOTREACHED();
  return nullptr;
}

void WebkitTextEmphasisStyle::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetTextEmphasisFill(
      ComputedStyleInitialValues::InitialTextEmphasisFill());
  state.Style()->SetTextEmphasisMark(
      ComputedStyleInitialValues::InitialTextEmphasisMark());
  state.Style()->SetTextEmphasisCustomMark(
      ComputedStyleInitialValues::InitialTextEmphasisCustomMark());
}

void WebkitTextEmphasisStyle::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetTextEmphasisFill(
      state.ParentStyle()->GetTextEmphasisFill());
  state.Style()->SetTextEmphasisMark(
      state.ParentStyle()->GetTextEmphasisMark());
  state.Style()->SetTextEmphasisCustomMark(
      state.ParentStyle()->TextEmphasisCustomMark());
}

void WebkitTextEmphasisStyle::ApplyValue(StyleResolverState& state,
                                         const CSSValue& value) const {
  if (const auto* list = DynamicTo<CSSValueList>(value)) {
    DCHECK_EQ(list->length(), 2U);
    for (unsigned i = 0; i < 2; ++i) {
      const auto& ident_value = To<CSSIdentifierValue>(list->Item(i));
      if (ident_value.GetValueID() == CSSValueID::kFilled ||
          ident_value.GetValueID() == CSSValueID::kOpen) {
        state.Style()->SetTextEmphasisFill(
            ident_value.ConvertTo<TextEmphasisFill>());
      } else {
        state.Style()->SetTextEmphasisMark(
            ident_value.ConvertTo<TextEmphasisMark>());
      }
    }
    state.Style()->SetTextEmphasisCustomMark(g_null_atom);
    return;
  }

  if (auto* string_value = DynamicTo<CSSStringValue>(value)) {
    state.Style()->SetTextEmphasisFill(TextEmphasisFill::kFilled);
    state.Style()->SetTextEmphasisMark(TextEmphasisMark::kCustom);
    state.Style()->SetTextEmphasisCustomMark(
        AtomicString(string_value->Value()));
    return;
  }

  const auto& identifier_value = To<CSSIdentifierValue>(value);

  state.Style()->SetTextEmphasisCustomMark(g_null_atom);

  if (identifier_value.GetValueID() == CSSValueID::kFilled ||
      identifier_value.GetValueID() == CSSValueID::kOpen) {
    state.Style()->SetTextEmphasisFill(
        identifier_value.ConvertTo<TextEmphasisFill>());
    state.Style()->SetTextEmphasisMark(TextEmphasisMark::kAuto);
  } else {
    state.Style()->SetTextEmphasisFill(TextEmphasisFill::kFilled);
    state.Style()->SetTextEmphasisMark(
        identifier_value.ConvertTo<TextEmphasisMark>());
  }
}

}  // namespace css_longhand
}  // namespace blink
