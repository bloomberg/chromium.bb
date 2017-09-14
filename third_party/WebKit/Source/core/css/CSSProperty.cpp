/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/CSSProperty.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/properties/CSSPropertyAPI.h"
#include "core/style/ComputedStyleConstants.h"

namespace blink {

struct SameSizeAsCSSProperty {
  uint32_t bitfields;
  Member<void*> value;
};

static_assert(sizeof(CSSProperty) == sizeof(SameSizeAsCSSProperty),
              "CSSProperty should stay small");

CSSPropertyID StylePropertyMetadata::ShorthandID() const {
  if (!is_set_from_shorthand_)
    return CSSPropertyInvalid;

  Vector<StylePropertyShorthand, 4> shorthands;
  getMatchingShorthandsForLonghand(static_cast<CSSPropertyID>(property_id_),
                                   &shorthands);
  DCHECK(shorthands.size());
  DCHECK_GE(index_in_shorthands_vector_, 0u);
  DCHECK_LT(index_in_shorthands_vector_, shorthands.size());
  return shorthands.at(index_in_shorthands_vector_).id();
}

enum LogicalBoxSide { kBeforeSide, kEndSide, kAfterSide, kStartSide };
enum PhysicalBoxSide { kTopSide, kRightSide, kBottomSide, kLeftSide };

static CSSPropertyID ResolveToPhysicalProperty(
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

enum LogicalExtent { kLogicalWidth, kLogicalHeight };

static CSSPropertyID ResolveToPhysicalProperty(
    WritingMode writing_mode,
    LogicalExtent logical_side,
    const CSSPropertyID* properties) {
  if (IsHorizontalWritingMode(writing_mode))
    return properties[logical_side];
  return logical_side == kLogicalWidth ? properties[1] : properties[0];
}

static const StylePropertyShorthand& BorderDirections() {
  static const CSSPropertyID kProperties[4] = {
      CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom,
      CSSPropertyBorderLeft};
  DEFINE_STATIC_LOCAL(
      StylePropertyShorthand, border_directions,
      (CSSPropertyBorder, kProperties, WTF_ARRAY_LENGTH(kProperties)));
  return border_directions;
}

CSSPropertyID CSSProperty::ResolveDirectionAwareProperty(
    CSSPropertyID property_id,
    TextDirection direction,
    WritingMode writing_mode) {
  switch (property_id) {
    case CSSPropertyWebkitMarginEnd:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       marginShorthand());
    case CSSPropertyWebkitMarginStart:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       marginShorthand());
    case CSSPropertyWebkitMarginBefore:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       marginShorthand());
    case CSSPropertyWebkitMarginAfter:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       marginShorthand());
    case CSSPropertyWebkitPaddingEnd:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       paddingShorthand());
    case CSSPropertyWebkitPaddingStart:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       paddingShorthand());
    case CSSPropertyWebkitPaddingBefore:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       paddingShorthand());
    case CSSPropertyWebkitPaddingAfter:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       paddingShorthand());
    case CSSPropertyWebkitBorderEnd:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       BorderDirections());
    case CSSPropertyWebkitBorderStart:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       BorderDirections());
    case CSSPropertyWebkitBorderBefore:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       BorderDirections());
    case CSSPropertyWebkitBorderAfter:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       BorderDirections());
    case CSSPropertyWebkitBorderEndColor:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       borderColorShorthand());
    case CSSPropertyWebkitBorderStartColor:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       borderColorShorthand());
    case CSSPropertyWebkitBorderBeforeColor:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       borderColorShorthand());
    case CSSPropertyWebkitBorderAfterColor:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       borderColorShorthand());
    case CSSPropertyWebkitBorderEndStyle:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       borderStyleShorthand());
    case CSSPropertyWebkitBorderStartStyle:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       borderStyleShorthand());
    case CSSPropertyWebkitBorderBeforeStyle:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       borderStyleShorthand());
    case CSSPropertyWebkitBorderAfterStyle:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       borderStyleShorthand());
    case CSSPropertyWebkitBorderEndWidth:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       borderWidthShorthand());
    case CSSPropertyWebkitBorderStartWidth:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       borderWidthShorthand());
    case CSSPropertyWebkitBorderBeforeWidth:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       borderWidthShorthand());
    case CSSPropertyWebkitBorderAfterWidth:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       borderWidthShorthand());
    case CSSPropertyScrollPaddingInlineStart:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       scrollPaddingShorthand());
    case CSSPropertyScrollPaddingInlineEnd:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       scrollPaddingShorthand());
    case CSSPropertyScrollPaddingBlockStart:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       scrollPaddingShorthand());
    case CSSPropertyScrollPaddingBlockEnd:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       scrollPaddingShorthand());
    case CSSPropertyScrollSnapMarginInlineStart:
      return ResolveToPhysicalProperty(direction, writing_mode, kStartSide,
                                       scrollSnapMarginShorthand());
    case CSSPropertyScrollSnapMarginInlineEnd:
      return ResolveToPhysicalProperty(direction, writing_mode, kEndSide,
                                       scrollSnapMarginShorthand());
    case CSSPropertyScrollSnapMarginBlockStart:
      return ResolveToPhysicalProperty(direction, writing_mode, kBeforeSide,
                                       scrollSnapMarginShorthand());
    case CSSPropertyScrollSnapMarginBlockEnd:
      return ResolveToPhysicalProperty(direction, writing_mode, kAfterSide,
                                       scrollSnapMarginShorthand());
    case CSSPropertyInlineSize:
    case CSSPropertyWebkitLogicalWidth: {
      const CSSPropertyID kProperties[2] = {CSSPropertyWidth,
                                            CSSPropertyHeight};
      return ResolveToPhysicalProperty(writing_mode, kLogicalWidth,
                                       kProperties);
    }
    case CSSPropertyBlockSize:
    case CSSPropertyWebkitLogicalHeight: {
      const CSSPropertyID kProperties[2] = {CSSPropertyWidth,
                                            CSSPropertyHeight};
      return ResolveToPhysicalProperty(writing_mode, kLogicalHeight,
                                       kProperties);
    }
    case CSSPropertyMinInlineSize:
    case CSSPropertyWebkitMinLogicalWidth: {
      const CSSPropertyID kProperties[2] = {CSSPropertyMinWidth,
                                            CSSPropertyMinHeight};
      return ResolveToPhysicalProperty(writing_mode, kLogicalWidth,
                                       kProperties);
    }
    case CSSPropertyMinBlockSize:
    case CSSPropertyWebkitMinLogicalHeight: {
      const CSSPropertyID kProperties[2] = {CSSPropertyMinWidth,
                                            CSSPropertyMinHeight};
      return ResolveToPhysicalProperty(writing_mode, kLogicalHeight,
                                       kProperties);
    }
    case CSSPropertyMaxInlineSize:
    case CSSPropertyWebkitMaxLogicalWidth: {
      const CSSPropertyID kProperties[2] = {CSSPropertyMaxWidth,
                                            CSSPropertyMaxHeight};
      return ResolveToPhysicalProperty(writing_mode, kLogicalWidth,
                                       kProperties);
    }
    case CSSPropertyMaxBlockSize:
    case CSSPropertyWebkitMaxLogicalHeight: {
      const CSSPropertyID kProperties[2] = {CSSPropertyMaxWidth,
                                            CSSPropertyMaxHeight};
      return ResolveToPhysicalProperty(writing_mode, kLogicalHeight,
                                       kProperties);
    }
    default:
      return property_id;
  }
}

void CSSProperty::FilterEnabledCSSPropertiesIntoVector(
    const CSSPropertyID* properties,
    size_t propertyCount,
    Vector<CSSPropertyID>& outVector) {
  for (unsigned i = 0; i < propertyCount; i++) {
    CSSPropertyID property = properties[i];
    if (CSSPropertyAPI::Get(property).IsEnabled())
      outVector.push_back(property);
  }
}

bool CSSProperty::operator==(const CSSProperty& other) const {
  return DataEquivalent(value_, other.value_) &&
         IsImportant() == other.IsImportant();
}

}  // namespace blink
