// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/shorthands/background_repeat.h"

#include "third_party/blink/renderer/core/css/css_initial_value.h"
#include "third_party/blink/renderer/core/css/parser/css_parser_context.h"
#include "third_party/blink/renderer/core/css/parser/css_property_parser_helpers.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/css_parsing_utils.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {
namespace css_shorthand {

bool BackgroundRepeat::ParseShorthand(
    bool important,
    CSSParserTokenRange& range,
    const CSSParserContext& context,
    const CSSParserLocalContext& local_context,
    HeapVector<CSSPropertyValue, 256>& properties) const {
  CSSValue* result_x = nullptr;
  CSSValue* result_y = nullptr;
  bool implicit = false;
  if (!css_parsing_utils::ConsumeRepeatStyle(range, result_x, result_y,
                                             implicit) ||
      !range.AtEnd())
    return false;

  css_property_parser_helpers::AddProperty(
      CSSPropertyBackgroundRepeatX, CSSPropertyBackgroundRepeat, *result_x,
      important,
      implicit ? css_property_parser_helpers::IsImplicitProperty::kImplicit
               : css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);
  css_property_parser_helpers::AddProperty(
      CSSPropertyBackgroundRepeatY, CSSPropertyBackgroundRepeat, *result_y,
      important,
      implicit ? css_property_parser_helpers::IsImplicitProperty::kImplicit
               : css_property_parser_helpers::IsImplicitProperty::kNotImplicit,
      properties);

  return true;
}

const CSSValue* BackgroundRepeat::CSSValueFromComputedStyleInternal(
    const ComputedStyle& style,
    const SVGComputedStyle&,
    const LayoutObject*,
    Node*,
    bool allow_visited_style) const {
  return ComputedStyleUtils::BackgroundRepeatOrWebkitMaskRepeat(
      &style.BackgroundLayers());
}

}  // namespace css_shorthand
}  // namespace blink
