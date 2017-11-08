// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains methods of CSSProperty that are not generated.

#include "core/css/properties/CSSProperty.h"

#include "core/StylePropertyShorthand.h"

namespace blink {

const StylePropertyShorthand& CSSProperty::BorderDirections() {
  static const CSSPropertyID kProperties[4] = {
      CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom,
      CSSPropertyBorderLeft};
  DEFINE_STATIC_LOCAL(
      StylePropertyShorthand, border_directions,
      (CSSPropertyBorder, kProperties, WTF_ARRAY_LENGTH(kProperties)));
  return border_directions;
}

CSSPropertyID CSSProperty::ResolveAfterToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSPropertyID* shorthand_properties = shorthand.properties();
  if (IsHorizontalWritingMode(writing_mode))
    return shorthand_properties[kBottomSide];
  if (IsFlippedLinesWritingMode(writing_mode))
    return shorthand_properties[kRightSide];
  return shorthand_properties[kLeftSide];
}

CSSPropertyID CSSProperty::ResolveBeforeToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSPropertyID* shorthand_properties = shorthand.properties();
  if (IsHorizontalWritingMode(writing_mode))
    return shorthand_properties[kTopSide];
  if (IsFlippedLinesWritingMode(writing_mode))
    return shorthand_properties[kLeftSide];
  return shorthand_properties[kRightSide];
}

CSSPropertyID CSSProperty::ResolveEndToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSPropertyID* shorthand_properties = shorthand.properties();
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode))
      return shorthand_properties[kRightSide];
    return shorthand_properties[kBottomSide];
  }
  if (IsHorizontalWritingMode(writing_mode))
    return shorthand_properties[kLeftSide];
  return shorthand_properties[kTopSide];
}

CSSPropertyID CSSProperty::ResolveStartToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    const StylePropertyShorthand& shorthand) {
  const CSSPropertyID* shorthand_properties = shorthand.properties();
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode))
      return shorthand_properties[kLeftSide];
    return shorthand_properties[kTopSide];
  }
  if (IsHorizontalWritingMode(writing_mode))
    return shorthand_properties[kRightSide];
  return shorthand_properties[kBottomSide];
}

void CSSProperty::FilterEnabledCSSPropertiesIntoVector(
    const CSSPropertyID* properties,
    size_t propertyCount,
    Vector<CSSPropertyID>& outVector) {
  for (unsigned i = 0; i < propertyCount; i++) {
    CSSPropertyID property = properties[i];
    if (Get(property).IsEnabled())
      outVector.push_back(property);
  }
}

}  // namespace blink
