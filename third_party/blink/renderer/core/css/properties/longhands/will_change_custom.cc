// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/longhands/will_change.h"

#include "third_party/blink/renderer/core/css/css_value_list.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_token_range.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace css_longhand {

const CSSValue* WillChange::ParseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&) const {
  if (range.Peek().Id() == CSSValueID::kAuto)
    return css_property_parser_helpers::ConsumeIdent(range);

  CSSValueList* values = CSSValueList::CreateCommaSeparated();
  // Every comma-separated list of identifiers is a valid will-change value,
  // unless the list includes an explicitly disallowed identifier.
  while (true) {
    if (range.Peek().GetType() != kIdentToken)
      return nullptr;
    CSSPropertyID unresolved_property =
        UnresolvedCSSPropertyID(range.Peek().Value());
    if (unresolved_property != CSSPropertyID::kInvalid &&
        unresolved_property != CSSPropertyID::kVariable) {
#if DCHECK_IS_ON()
      DCHECK(CSSProperty::Get(resolveCSSPropertyID(unresolved_property))
                 .IsEnabled());
#endif
      // Now "all" is used by both CSSValue and CSSPropertyValue.
      // Need to return nullptr when currentValue is CSSPropertyID::kAll.
      if (unresolved_property == CSSPropertyID::kWillChange ||
          unresolved_property == CSSPropertyID::kAll)
        return nullptr;
      values->Append(
          *MakeGarbageCollected<CSSCustomIdentValue>(unresolved_property));
      range.ConsumeIncludingWhitespace();
    } else {
      switch (range.Peek().Id()) {
        case CSSValueID::kNone:
        case CSSValueID::kAll:
        case CSSValueID::kAuto:
        case CSSValueID::kDefault:
        case CSSValueID::kInitial:
        case CSSValueID::kInherit:
          return nullptr;
        case CSSValueID::kContents:
        case CSSValueID::kScrollPosition:
          values->Append(*css_property_parser_helpers::ConsumeIdent(range));
          break;
        default:
          range.ConsumeIncludingWhitespace();
          break;
      }
    }

    if (range.AtEnd())
      break;
    if (!css_property_parser_helpers::ConsumeCommaIncludingWhitespace(range))
      return nullptr;
  }

  return values;
}

const CSSValue* WillChange::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  return ComputedStyleUtils::ValueForWillChange(
      style.WillChangeProperties(), style.WillChangeContents(),
      style.WillChangeScrollPosition());
}

void WillChange::ApplyInitial(StyleResolverState& state) const {
  state.Style()->SetWillChangeContents(false);
  state.Style()->SetWillChangeScrollPosition(false);
  state.Style()->SetWillChangeProperties(Vector<CSSPropertyID>());
  state.Style()->SetSubtreeWillChangeContents(
      state.ParentStyle()->SubtreeWillChangeContents());
}

void WillChange::ApplyInherit(StyleResolverState& state) const {
  state.Style()->SetWillChangeContents(
      state.ParentStyle()->WillChangeContents());
  state.Style()->SetWillChangeScrollPosition(
      state.ParentStyle()->WillChangeScrollPosition());
  state.Style()->SetWillChangeProperties(
      state.ParentStyle()->WillChangeProperties());
  state.Style()->SetSubtreeWillChangeContents(
      state.ParentStyle()->SubtreeWillChangeContents());
}

void WillChange::ApplyValue(StyleResolverState& state,
                            const CSSValue& value) const {
  bool will_change_contents = false;
  bool will_change_scroll_position = false;
  Vector<CSSPropertyID> will_change_properties;

  if (auto* identifier_value = DynamicTo<CSSIdentifierValue>(value)) {
    DCHECK_EQ(identifier_value->GetValueID(), CSSValueID::kAuto);
  } else {
    for (auto& will_change_value : To<CSSValueList>(value)) {
      if (auto* ident_value =
              DynamicTo<CSSCustomIdentValue>(will_change_value.Get())) {
        will_change_properties.push_back(ident_value->ValueAsPropertyID());
      } else if (To<CSSIdentifierValue>(*will_change_value).GetValueID() ==
                 CSSValueID::kContents) {
        will_change_contents = true;
      } else if (To<CSSIdentifierValue>(*will_change_value).GetValueID() ==
                 CSSValueID::kScrollPosition) {
        will_change_scroll_position = true;
      } else {
        NOTREACHED();
      }
    }
  }
  state.Style()->SetWillChangeContents(will_change_contents);
  state.Style()->SetWillChangeScrollPosition(will_change_scroll_position);
  state.Style()->SetWillChangeProperties(will_change_properties);
  state.Style()->SetSubtreeWillChangeContents(
      will_change_contents || state.ParentStyle()->SubtreeWillChangeContents());
}

}  // namespace css_longhand
}  // namespace blink
