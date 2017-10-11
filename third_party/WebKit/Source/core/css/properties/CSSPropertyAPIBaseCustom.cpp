// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains methods of CSSPropertyAPI that are not generated.

#include "core/css/properties/CSSPropertyAPI.h"

#include "core/StylePropertyShorthand.h"

namespace blink {

namespace {

CSSPropertyID ResolveToPhysicalProperty(
    TextDirection direction,
    WritingMode writing_mode,
    LogicalBoxSide logical_side,
    const StylePropertyShorthand& shorthand) {
  if (direction == TextDirection::kLtr) {
    if (IsHorizontalWritingMode(writing_mode)) {
      // The common case. The logical and physical box sides match.
      // Left = Start, Right = End, Before = Top, After = Bottom
      return shorthand.properties()[logical_side];
    }

    if (IsFlippedLinesWritingMode(writing_mode)) {
      // Start = Top, End = Bottom, Before = Left, After = Right.
      switch (logical_side) {
        case kStartSide:
          return shorthand.properties()[kTopSide];
        case kEndSide:
          return shorthand.properties()[kBottomSide];
        case kBeforeSide:
          return shorthand.properties()[kLeftSide];
        default:
          return shorthand.properties()[kRightSide];
      }
    }

    // Start = Top, End = Bottom, Before = Right, After = Left
    switch (logical_side) {
      case kStartSide:
        return shorthand.properties()[kTopSide];
      case kEndSide:
        return shorthand.properties()[kBottomSide];
      case kBeforeSide:
        return shorthand.properties()[kRightSide];
      default:
        return shorthand.properties()[kLeftSide];
    }
  }

  if (IsHorizontalWritingMode(writing_mode)) {
    // Start = Right, End = Left, Before = Top, After = Bottom
    switch (logical_side) {
      case kStartSide:
        return shorthand.properties()[kRightSide];
      case kEndSide:
        return shorthand.properties()[kLeftSide];
      case kBeforeSide:
        return shorthand.properties()[kTopSide];
      default:
        return shorthand.properties()[kBottomSide];
    }
  }

  if (IsFlippedLinesWritingMode(writing_mode)) {
    // Start = Bottom, End = Top, Before = Left, After = Right
    switch (logical_side) {
      case kStartSide:
        return shorthand.properties()[kBottomSide];
      case kEndSide:
        return shorthand.properties()[kTopSide];
      case kBeforeSide:
        return shorthand.properties()[kLeftSide];
      default:
        return shorthand.properties()[kRightSide];
    }
  }

  // Start = Bottom, End = Top, Before = Right, After = Left
  switch (logical_side) {
    case kStartSide:
      return shorthand.properties()[kBottomSide];
    case kEndSide:
      return shorthand.properties()[kTopSide];
    case kBeforeSide:
      return shorthand.properties()[kRightSide];
    default:
      return shorthand.properties()[kLeftSide];
  }
}

CSSPropertyID ResolveToPhysicalProperty(WritingMode writing_mode,
                                        LogicalExtent logical_side,
                                        const CSSPropertyID* properties) {
  if (IsHorizontalWritingMode(writing_mode))
    return properties[logical_side];
  return logical_side == kLogicalWidth ? properties[1] : properties[0];
}

}  // namespace

const CSSPropertyAPI& CSSPropertyAPI::ResolveToPhysicalPropertyAPI(
    TextDirection direction,
    WritingMode writing_mode,
    LogicalBoxSide logical_side,
    const StylePropertyShorthand& shorthand) {
  CSSPropertyID id = ResolveToPhysicalProperty(direction, writing_mode,
                                               logical_side, shorthand);
  return Get(id);
}

const CSSPropertyAPI& CSSPropertyAPI::ResolveToPhysicalPropertyAPI(
    WritingMode writing_mode,
    LogicalExtent logical_side,
    const CSSPropertyID* properties) {
  CSSPropertyID id =
      ResolveToPhysicalProperty(writing_mode, logical_side, properties);
  return Get(id);
}

const StylePropertyShorthand& CSSPropertyAPI::BorderDirections() {
  static const CSSPropertyID kProperties[4] = {
      CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom,
      CSSPropertyBorderLeft};
  DEFINE_STATIC_LOCAL(
      StylePropertyShorthand, border_directions,
      (CSSPropertyBorder, kProperties, WTF_ARRAY_LENGTH(kProperties)));
  return border_directions;
}

void CSSPropertyAPI::FilterEnabledCSSPropertiesIntoVector(
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
