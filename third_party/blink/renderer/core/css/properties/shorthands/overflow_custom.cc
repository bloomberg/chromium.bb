// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/overflow.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_fast_paths.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {
namespace CSSShorthand {

bool Overflow::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext&,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValueID id = range.ConsumeIncludingWhitespace().Id();
  if (!CSSParserFastPaths::IsValidKeywordPropertyAndValue(CSSPropertyOverflowY,
                                                          id, context.Mode()))
    return false;
  if (!range.AtEnd())
    return false;
  CSSValue* overflow_y_value = CSSIdentifierValue::Create(id);

  CSSValue* overflow_x_value = nullptr;

  // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y.
  // If
  // this value has been set using the shorthand, then for now overflow-x
  // will default to auto, but once we implement pagination controls, it
  // should default to hidden. If the overflow-y value is anything but
  // paged-x or paged-y, then overflow-x and overflow-y should have the
  // same
  // value.
  if (id == CSSValueWebkitPagedX || id == CSSValueWebkitPagedY)
    overflow_x_value = CSSIdentifierValue::Create(CSSValueAuto);
  else
    overflow_x_value = overflow_y_value;
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyOverflowX, CSSPropertyOverflow, *overflow_x_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  CSSPropertyParserHelpers::AddProperty(
      CSSPropertyOverflowY, CSSPropertyOverflow, *overflow_y_value, important,
      CSSPropertyParserHelpers::IsImplicitProperty::kNotImplicit, properties);
  return true;
}

const CSSValue* Overflow::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node* styled_node,
    bool allow_visited_style) const {
  if (style.OverflowX() == style.OverflowY())
    return CSSIdentifierValue::Create(style.OverflowX());
  return nullptr;
}

}  // namespace CSSShorthand
}  // namespace blink
