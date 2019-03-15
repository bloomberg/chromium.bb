// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/properties/css_property.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_style_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/cross_thread_unsupported_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_keyword_value.h"
#include "third_party/blink/renderer/core/css/cssom/css_unit_value.h"
#include "third_party/blink/renderer/core/css/cssom/style_value_factory.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/svg_computed_style.h"
#include "third_party/blink/renderer/core/style_property_shorthand.h"

namespace blink {

const CSSProperty& GetCSSPropertyVariable() {
  return To<CSSProperty>(GetCSSPropertyVariableInternal());
}

const CSSProperty& CSSProperty::Get(CSSPropertyID id) {
  DCHECK_NE(id, CSSPropertyInvalid);
  DCHECK_LE(id, lastCSSProperty);  // last property id
  return To<CSSProperty>(CSSUnresolvedProperty::GetNonAliasProperty(id));
}

std::unique_ptr<CrossThreadStyleValue>
CSSProperty::CrossThreadStyleValueFromComputedStyle(
    const ComputedStyle& computed_style,
    const LayoutObject* layout_object,
    Node* node,
    bool allow_visited_style) const {
  const CSSValue* css_value = CSSValueFromComputedStyle(
      computed_style, layout_object, node, allow_visited_style);
  if (!css_value)
    return std::make_unique<CrossThreadUnsupportedValue>("");
  CSSStyleValue* style_value =
      StyleValueFactory::CssValueToStyleValue(GetCSSPropertyName(), *css_value);
  if (!style_value)
    return std::make_unique<CrossThreadUnsupportedValue>("");
  switch (style_value->GetType()) {
    case CSSStyleValue::StyleValueType::kKeywordType:
      return std::make_unique<CrossThreadKeywordValue>(
          To<CSSKeywordValue>(style_value)->value().IsolatedCopy());
    case CSSStyleValue::StyleValueType::kUnitType:
      return std::make_unique<CrossThreadUnitValue>(
          To<CSSUnitValue>(style_value)->value(),
          To<CSSUnitValue>(style_value)->GetInternalUnit());
    default:
      // Make an isolated copy to ensure that it is safe to pass cross thread.
      return std::make_unique<CrossThreadUnsupportedValue>(
          css_value->CssText().IsolatedCopy());
  }
}

const StylePropertyShorthand& CSSProperty::BorderDirections() {
  static const CSSProperty* kProperties[4] = {
      &GetCSSPropertyBorderTop(), &GetCSSPropertyBorderRight(),
      &GetCSSPropertyBorderBottom(), &GetCSSPropertyBorderLeft()};
  DEFINE_STATIC_LOCAL(
      StylePropertyShorthand, border_directions,
      (CSSPropertyBorder, kProperties, base::size(kProperties)));
  return border_directions;
}

const CSSProperty& CSSProperty::ResolveAfterToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSProperty** shorthand_properties = shorthand.properties();
  if (IsHorizontalWritingMode(writing_mode))
    return *shorthand_properties[kBottomSide];
  if (IsFlippedLinesWritingMode(writing_mode))
    return *shorthand_properties[kRightSide];
  return *shorthand_properties[kLeftSide];
}

const CSSProperty& CSSProperty::ResolveBeforeToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSProperty** shorthand_properties = shorthand.properties();
  if (IsHorizontalWritingMode(writing_mode))
    return *shorthand_properties[kTopSide];
  if (IsFlippedLinesWritingMode(writing_mode))
    return *shorthand_properties[kLeftSide];
  return *shorthand_properties[kRightSide];
}

const CSSProperty& CSSProperty::ResolveEndToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSProperty** shorthand_properties = shorthand.properties();
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode))
      return *shorthand_properties[kRightSide];
    return *shorthand_properties[kBottomSide];
  }
  if (IsHorizontalWritingMode(writing_mode))
    return *shorthand_properties[kLeftSide];
  return *shorthand_properties[kTopSide];
}

const CSSProperty& CSSProperty::ResolveStartToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSProperty** shorthand_properties = shorthand.properties();
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode))
      return *shorthand_properties[kLeftSide];
    return *shorthand_properties[kTopSide];
  }
  if (IsHorizontalWritingMode(writing_mode))
    return *shorthand_properties[kRightSide];
  return *shorthand_properties[kBottomSide];
}

const CSSValue* CSSProperty::CSSValueFromComputedStyle(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) const {
  const SVGComputedStyle& svg_style = style.SvgStyle();
  const CSSProperty& resolved_property =
      ResolveDirectionAwareProperty(style.Direction(), style.GetWritingMode());
  return resolved_property.CSSValueFromComputedStyleInternal(
      style, svg_style, layout_object, styled_node, allow_visited_style);
}

void CSSProperty::FilterEnabledCSSPropertiesIntoVector(
    const CSSPropertyID* properties,
    size_t propertyCount,
    Vector<const CSSProperty*>& outVector) {
  for (unsigned i = 0; i < propertyCount; i++) {
    const CSSProperty& property = Get(properties[i]);
    if (property.IsEnabled())
      outVector.push_back(&property);
  }
}

}  // namespace blink
