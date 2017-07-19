/*
 * Copyright (C) 2004 Zack Rusin <zack@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc.
 * All rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2011 Sencha, Inc. All rights reserved.
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include "core/css/ComputedStyleCSSValueMapping.h"

#include "core/StylePropertyShorthand.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSBorderImageSliceValue.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFontVariationValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSPathValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPrimitiveValueMappings.h"
#include "core/css/CSSQuadValue.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/zoomAdjustedPixelValue.h"
#include "core/layout/LayoutBlock.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutGrid.h"
#include "core/layout/LayoutObject.h"
#include "core/style/ComputedStyle.h"
#include "core/style/ContentData.h"
#include "core/style/CursorData.h"
#include "core/style/QuotesData.h"
#include "core/style/ShadowList.h"
#include "core/style/StyleInheritedVariables.h"
#include "core/style/StyleNonInheritedVariables.h"
#include "platform/LengthFunctions.h"

namespace blink {

using namespace cssvalue;

inline static bool IsFlexOrGrid(const ComputedStyle* style) {
  return style && style->IsDisplayFlexibleOrGridBox();
}

inline static CSSValue* ZoomAdjustedPixelValueOrAuto(
    const Length& length,
    const ComputedStyle& style) {
  if (length.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return ZoomAdjustedPixelValue(length.Value(), style);
}

static CSSValue* ZoomAdjustedPixelValueForLength(const Length& length,
                                                 const ComputedStyle& style) {
  if (length.IsFixed())
    return ZoomAdjustedPixelValue(length.Value(), style);
  return CSSValue::Create(length, style.EffectiveZoom());
}

static CSSValue* PixelValueForUnzoomedLength(
    const UnzoomedLength& unzoomed_length,
    const ComputedStyle& style) {
  const Length& length = unzoomed_length.length();
  if (length.IsFixed())
    return CSSPrimitiveValue::Create(length.Value(),
                                     CSSPrimitiveValue::UnitType::kPixels);
  return CSSValue::Create(length, style.EffectiveZoom());
}

static CSSValueList* CreatePositionListForLayer(CSSPropertyID property_id,
                                                const FillLayer& layer,
                                                const ComputedStyle& style) {
  CSSValueList* position_list = CSSValueList::CreateSpaceSeparated();
  if (layer.IsBackgroundXOriginSet()) {
    DCHECK(property_id == CSSPropertyBackgroundPosition ||
           property_id == CSSPropertyWebkitMaskPosition);
    position_list->Append(
        *CSSIdentifierValue::Create(layer.BackgroundXOrigin()));
  }
  position_list->Append(
      *ZoomAdjustedPixelValueForLength(layer.XPosition(), style));
  if (layer.IsBackgroundYOriginSet()) {
    DCHECK(property_id == CSSPropertyBackgroundPosition ||
           property_id == CSSPropertyWebkitMaskPosition);
    position_list->Append(
        *CSSIdentifierValue::Create(layer.BackgroundYOrigin()));
  }
  position_list->Append(
      *ZoomAdjustedPixelValueForLength(layer.YPosition(), style));
  return position_list;
}

CSSValue* ComputedStyleCSSValueMapping::CurrentColorOrValidColor(
    const ComputedStyle& style,
    const StyleColor& color) {
  // This function does NOT look at visited information, so that computed style
  // doesn't expose that.
  return CSSColorValue::Create(color.Resolve(style.GetColor()).Rgb());
}

static CSSValue* ValueForFillSize(const FillSize& fill_size,
                                  const ComputedStyle& style) {
  if (fill_size.type == kContain)
    return CSSIdentifierValue::Create(CSSValueContain);

  if (fill_size.type == kCover)
    return CSSIdentifierValue::Create(CSSValueCover);

  if (fill_size.size.Height().IsAuto())
    return ZoomAdjustedPixelValueForLength(fill_size.size.Width(), style);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ZoomAdjustedPixelValueForLength(fill_size.size.Width(), style));
  list->Append(
      *ZoomAdjustedPixelValueForLength(fill_size.size.Height(), style));
  return list;
}

static CSSValue* ValueForFillRepeat(EFillRepeat x_repeat,
                                    EFillRepeat y_repeat) {
  // For backwards compatibility, if both values are equal, just return one of
  // them. And if the two values are equivalent to repeat-x or repeat-y, just
  // return the shorthand.
  if (x_repeat == y_repeat)
    return CSSIdentifierValue::Create(x_repeat);
  if (x_repeat == kRepeatFill && y_repeat == kNoRepeatFill)
    return CSSIdentifierValue::Create(CSSValueRepeatX);
  if (x_repeat == kNoRepeatFill && y_repeat == kRepeatFill)
    return CSSIdentifierValue::Create(CSSValueRepeatY);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(x_repeat));
  list->Append(*CSSIdentifierValue::Create(y_repeat));
  return list;
}

static CSSValue* ValueForFillSourceType(EMaskSourceType type) {
  switch (type) {
    case kMaskAlpha:
      return CSSIdentifierValue::Create(CSSValueAlpha);
    case kMaskLuminance:
      return CSSIdentifierValue::Create(CSSValueLuminance);
  }

  NOTREACHED();

  return nullptr;
}

static CSSValue* ValueForPositionOffset(const ComputedStyle& style,
                                        CSSPropertyID property_id,
                                        const LayoutObject* layout_object) {
  Length offset, opposite;
  switch (property_id) {
    case CSSPropertyLeft:
      offset = style.Left();
      opposite = style.Right();
      break;
    case CSSPropertyRight:
      offset = style.Right();
      opposite = style.Left();
      break;
    case CSSPropertyTop:
      offset = style.Top();
      opposite = style.Bottom();
      break;
    case CSSPropertyBottom:
      offset = style.Bottom();
      opposite = style.Top();
      break;
    default:
      return nullptr;
  }

  if (offset.IsPercentOrCalc() && layout_object && layout_object->IsBox() &&
      layout_object->IsPositioned()) {
    LayoutUnit containing_block_size =
        (property_id == CSSPropertyLeft || property_id == CSSPropertyRight)
            ? ToLayoutBox(layout_object)
                  ->ContainingBlockLogicalWidthForContent()
            : ToLayoutBox(layout_object)
                  ->ContainingBlockLogicalHeightForGetComputedStyle();
    return ZoomAdjustedPixelValue(ValueForLength(offset, containing_block_size),
                                  style);
  }

  if (offset.IsAuto() && layout_object) {
    // If the property applies to a positioned element and the resolved value of
    // the display property is not none, the resolved value is the used value.
    // Position offsets have special meaning for position sticky so we return
    // auto when offset.isAuto() on a sticky position object:
    // https://crbug.com/703816.
    if (layout_object->IsRelPositioned()) {
      // If e.g. left is auto and right is not auto, then left's computed value
      // is negative right. So we get the opposite length unit and see if it is
      // auto.
      if (opposite.IsAuto())
        return CSSPrimitiveValue::Create(0,
                                         CSSPrimitiveValue::UnitType::kPixels);

      if (opposite.IsPercentOrCalc()) {
        if (layout_object->IsBox()) {
          LayoutUnit containing_block_size =
              (property_id == CSSPropertyLeft ||
               property_id == CSSPropertyRight)
                  ? ToLayoutBox(layout_object)
                        ->ContainingBlockLogicalWidthForContent()
                  : ToLayoutBox(layout_object)
                        ->ContainingBlockLogicalHeightForGetComputedStyle();
          return ZoomAdjustedPixelValue(
              -FloatValueForLength(opposite, containing_block_size), style);
        }
        // FIXME:  fall back to auto for position:relative, display:inline
        return CSSIdentifierValue::Create(CSSValueAuto);
      }

      // Length doesn't provide operator -, so multiply by -1.
      opposite *= -1.f;
      return ZoomAdjustedPixelValueForLength(opposite, style);
    }

    if (layout_object->IsOutOfFlowPositioned() && layout_object->IsBox()) {
      // For fixed and absolute positioned elements, the top, left, bottom, and
      // right are defined relative to the corresponding sides of the containing
      // block.
      LayoutBlock* container = layout_object->ContainingBlock();
      const LayoutBox* layout_box = ToLayoutBox(layout_object);

      // clientOffset is the distance from this object's border edge to the
      // container's padding edge. Thus it includes margins which we subtract
      // below.
      const LayoutSize client_offset =
          layout_box->LocationOffset() -
          LayoutSize(container->ClientLeft(), container->ClientTop());
      LayoutUnit position;

      switch (property_id) {
        case CSSPropertyLeft:
          position = client_offset.Width() - layout_box->MarginLeft();
          break;
        case CSSPropertyTop:
          position = client_offset.Height() - layout_box->MarginTop();
          break;
        case CSSPropertyRight:
          position = container->ClientWidth() - layout_box->MarginRight() -
                     (layout_box->OffsetWidth() + client_offset.Width());
          break;
        case CSSPropertyBottom:
          position = container->ClientHeight() - layout_box->MarginBottom() -
                     (layout_box->OffsetHeight() + client_offset.Height());
          break;
        default:
          NOTREACHED();
      }
      return ZoomAdjustedPixelValue(position, style);
    }
  }

  if (offset.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);

  return ZoomAdjustedPixelValueForLength(offset, style);
}

static CSSBorderImageSliceValue* ValueForNinePieceImageSlice(
    const NinePieceImage& image) {
  // Create the slices.
  CSSPrimitiveValue* top = nullptr;
  CSSPrimitiveValue* right = nullptr;
  CSSPrimitiveValue* bottom = nullptr;
  CSSPrimitiveValue* left = nullptr;

  // TODO(alancutter): Make this code aware of calc lengths.
  if (image.ImageSlices().Top().IsPercentOrCalc())
    top = CSSPrimitiveValue::Create(image.ImageSlices().Top().Value(),
                                    CSSPrimitiveValue::UnitType::kPercentage);
  else
    top = CSSPrimitiveValue::Create(image.ImageSlices().Top().Value(),
                                    CSSPrimitiveValue::UnitType::kNumber);

  if (image.ImageSlices().Right() == image.ImageSlices().Top() &&
      image.ImageSlices().Bottom() == image.ImageSlices().Top() &&
      image.ImageSlices().Left() == image.ImageSlices().Top()) {
    right = top;
    bottom = top;
    left = top;
  } else {
    if (image.ImageSlices().Right().IsPercentOrCalc())
      right =
          CSSPrimitiveValue::Create(image.ImageSlices().Right().Value(),
                                    CSSPrimitiveValue::UnitType::kPercentage);
    else
      right = CSSPrimitiveValue::Create(image.ImageSlices().Right().Value(),
                                        CSSPrimitiveValue::UnitType::kNumber);

    if (image.ImageSlices().Bottom() == image.ImageSlices().Top() &&
        image.ImageSlices().Right() == image.ImageSlices().Left()) {
      bottom = top;
      left = right;
    } else {
      if (image.ImageSlices().Bottom().IsPercentOrCalc())
        bottom =
            CSSPrimitiveValue::Create(image.ImageSlices().Bottom().Value(),
                                      CSSPrimitiveValue::UnitType::kPercentage);
      else
        bottom =
            CSSPrimitiveValue::Create(image.ImageSlices().Bottom().Value(),
                                      CSSPrimitiveValue::UnitType::kNumber);

      if (image.ImageSlices().Left() == image.ImageSlices().Right()) {
        left = right;
      } else {
        if (image.ImageSlices().Left().IsPercentOrCalc())
          left = CSSPrimitiveValue::Create(
              image.ImageSlices().Left().Value(),
              CSSPrimitiveValue::UnitType::kPercentage);
        else
          left =
              CSSPrimitiveValue::Create(image.ImageSlices().Left().Value(),
                                        CSSPrimitiveValue::UnitType::kNumber);
      }
    }
  }

  return CSSBorderImageSliceValue::Create(
      CSSQuadValue::Create(top, right, bottom, left,
                           CSSQuadValue::kSerializeAsQuad),
      image.Fill());
}

static CSSValue* ValueForBorderImageLength(
    const BorderImageLength& border_image_length,
    const ComputedStyle& style) {
  if (border_image_length.IsNumber())
    return CSSPrimitiveValue::Create(border_image_length.Number(),
                                     CSSPrimitiveValue::UnitType::kNumber);
  return CSSValue::Create(border_image_length.length(), style.EffectiveZoom());
}

static CSSQuadValue* ValueForNinePieceImageQuad(const BorderImageLengthBox& box,
                                                const ComputedStyle& style) {
  // Create the slices.
  CSSValue* top = nullptr;
  CSSValue* right = nullptr;
  CSSValue* bottom = nullptr;
  CSSValue* left = nullptr;

  top = ValueForBorderImageLength(box.Top(), style);

  if (box.Right() == box.Top() && box.Bottom() == box.Top() &&
      box.Left() == box.Top()) {
    right = top;
    bottom = top;
    left = top;
  } else {
    right = ValueForBorderImageLength(box.Right(), style);

    if (box.Bottom() == box.Top() && box.Right() == box.Left()) {
      bottom = top;
      left = right;
    } else {
      bottom = ValueForBorderImageLength(box.Bottom(), style);

      if (box.Left() == box.Right())
        left = right;
      else
        left = ValueForBorderImageLength(box.Left(), style);
    }
  }
  return CSSQuadValue::Create(top, right, bottom, left,
                              CSSQuadValue::kSerializeAsQuad);
}

static CSSValueID ValueForRepeatRule(int rule) {
  switch (rule) {
    case kRepeatImageRule:
      return CSSValueRepeat;
    case kRoundImageRule:
      return CSSValueRound;
    case kSpaceImageRule:
      return CSSValueSpace;
    default:
      return CSSValueStretch;
  }
}

static CSSValue* ValueForNinePieceImageRepeat(const NinePieceImage& image) {
  CSSIdentifierValue* horizontal_repeat = nullptr;
  CSSIdentifierValue* vertical_repeat = nullptr;

  horizontal_repeat =
      CSSIdentifierValue::Create(ValueForRepeatRule(image.HorizontalRule()));
  if (image.HorizontalRule() == image.VerticalRule()) {
    vertical_repeat = horizontal_repeat;
  } else {
    vertical_repeat =
        CSSIdentifierValue::Create(ValueForRepeatRule(image.VerticalRule()));
  }
  return CSSValuePair::Create(horizontal_repeat, vertical_repeat,
                              CSSValuePair::kDropIdenticalValues);
}

static CSSValue* ValueForNinePieceImage(const NinePieceImage& image,
                                        const ComputedStyle& style) {
  if (!image.HasImage())
    return CSSIdentifierValue::Create(CSSValueNone);

  // Image first.
  CSSValue* image_value = nullptr;
  if (image.GetImage())
    image_value = image.GetImage()->ComputedCSSValue();

  // Create the image slice.
  CSSBorderImageSliceValue* image_slices = ValueForNinePieceImageSlice(image);

  // Create the border area slices.
  CSSValue* border_slices =
      ValueForNinePieceImageQuad(image.BorderSlices(), style);

  // Create the border outset.
  CSSValue* outset = ValueForNinePieceImageQuad(image.Outset(), style);

  // Create the repeat rules.
  CSSValue* repeat = ValueForNinePieceImageRepeat(image);

  return CreateBorderImageValue(image_value, image_slices, border_slices,
                                outset, repeat);
}

static CSSValue* ValueForReflection(const StyleReflection* reflection,
                                    const ComputedStyle& style) {
  if (!reflection)
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSPrimitiveValue* offset = nullptr;
  // TODO(alancutter): Make this work correctly for calc lengths.
  if (reflection->Offset().IsPercentOrCalc())
    offset =
        CSSPrimitiveValue::Create(reflection->Offset().Percent(),
                                  CSSPrimitiveValue::UnitType::kPercentage);
  else
    offset = ZoomAdjustedPixelValue(reflection->Offset().Value(), style);

  CSSIdentifierValue* direction = nullptr;
  switch (reflection->Direction()) {
    case kReflectionBelow:
      direction = CSSIdentifierValue::Create(CSSValueBelow);
      break;
    case kReflectionAbove:
      direction = CSSIdentifierValue::Create(CSSValueAbove);
      break;
    case kReflectionLeft:
      direction = CSSIdentifierValue::Create(CSSValueLeft);
      break;
    case kReflectionRight:
      direction = CSSIdentifierValue::Create(CSSValueRight);
      break;
  }

  return CSSReflectValue::Create(
      direction, offset, ValueForNinePieceImage(reflection->Mask(), style));
}

static CSSValueList* ValueForItemPositionWithOverflowAlignment(
    const StyleSelfAlignmentData& data) {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();
  if (data.PositionType() == kLegacyPosition)
    result->Append(*CSSIdentifierValue::Create(CSSValueLegacy));
  if (data.GetPosition() == kItemPositionBaseline) {
    result->Append(
        *CSSValuePair::Create(CSSIdentifierValue::Create(CSSValueBaseline),
                              CSSIdentifierValue::Create(CSSValueBaseline),
                              CSSValuePair::kDropIdenticalValues));
  } else if (data.GetPosition() == kItemPositionLastBaseline) {
    result->Append(
        *CSSValuePair::Create(CSSIdentifierValue::Create(CSSValueLast),
                              CSSIdentifierValue::Create(CSSValueBaseline),
                              CSSValuePair::kDropIdenticalValues));
  } else {
    result->Append(*CSSIdentifierValue::Create(data.GetPosition()));
  }
  if (data.GetPosition() >= kItemPositionCenter &&
      data.Overflow() != kOverflowAlignmentDefault)
    result->Append(*CSSIdentifierValue::Create(data.Overflow()));
  DCHECK_LE(result->length(), 2u);
  return result;
}

static CSSValueList* ValuesForGridShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSlashSeparated();
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        shorthand.properties()[i], style, layout_object, styled_node,
        allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

static CSSValueList* ValuesForShorthandProperty(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        shorthand.properties()[i], style, layout_object, styled_node,
        allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }
  return list;
}

static CSSValue* ExpandNoneLigaturesValue() {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*CSSIdentifierValue::Create(CSSValueNoCommonLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueNoDiscretionaryLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueNoHistoricalLigatures));
  list->Append(*CSSIdentifierValue::Create(CSSValueNoContextual));
  return list;
}

static CSSValue* ValuesForFontVariantProperty(const ComputedStyle& style,
                                              const LayoutObject* layout_object,
                                              Node* styled_node,
                                              bool allow_visited_style) {
  enum VariantShorthandCases {
    kAllNormal,
    kNoneLigatures,
    kConcatenateNonNormal
  };
  VariantShorthandCases shorthand_case = kAllNormal;
  for (size_t i = 0; i < fontVariantShorthand().length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        fontVariantShorthand().properties()[i], style, layout_object,
        styled_node, allow_visited_style);

    if (shorthand_case == kAllNormal && value->IsIdentifierValue() &&
        ToCSSIdentifierValue(value)->GetValueID() == CSSValueNone &&
        fontVariantShorthand().properties()[i] ==
            CSSPropertyFontVariantLigatures) {
      shorthand_case = kNoneLigatures;
    } else if (!(value->IsIdentifierValue() &&
                 ToCSSIdentifierValue(value)->GetValueID() == CSSValueNormal)) {
      shorthand_case = kConcatenateNonNormal;
      break;
    }
  }

  switch (shorthand_case) {
    case kAllNormal:
      return CSSIdentifierValue::Create(CSSValueNormal);
    case kNoneLigatures:
      return CSSIdentifierValue::Create(CSSValueNone);
    case kConcatenateNonNormal: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      for (size_t i = 0; i < fontVariantShorthand().length(); ++i) {
        const CSSValue* value = ComputedStyleCSSValueMapping::Get(
            fontVariantShorthand().properties()[i], style, layout_object,
            styled_node, allow_visited_style);
        DCHECK(value);
        if (value->IsIdentifierValue() &&
            ToCSSIdentifierValue(value)->GetValueID() == CSSValueNone) {
          list->Append(*ExpandNoneLigaturesValue());
        } else if (!(value->IsIdentifierValue() &&
                     ToCSSIdentifierValue(value)->GetValueID() ==
                         CSSValueNormal)) {
          list->Append(*value);
        }
      }
      return list;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValueList* ValuesForBackgroundShorthand(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* ret = CSSValueList::CreateCommaSeparated();
  const FillLayer* curr_layer = &style.BackgroundLayers();
  for (; curr_layer; curr_layer = curr_layer->Next()) {
    CSSValueList* list = CSSValueList::CreateSlashSeparated();
    CSSValueList* before_slash = CSSValueList::CreateSpaceSeparated();
    if (!curr_layer->Next()) {  // color only for final layer
      const CSSValue* value = ComputedStyleCSSValueMapping::Get(
          CSSPropertyBackgroundColor, style, layout_object, styled_node,
          allow_visited_style);
      DCHECK(value);
      before_slash->Append(*value);
    }
    before_slash->Append(curr_layer->GetImage()
                             ? *curr_layer->GetImage()->ComputedCSSValue()
                             : *CSSIdentifierValue::Create(CSSValueNone));
    before_slash->Append(
        *ValueForFillRepeat(curr_layer->RepeatX(), curr_layer->RepeatY()));
    before_slash->Append(*CSSIdentifierValue::Create(curr_layer->Attachment()));
    before_slash->Append(*CreatePositionListForLayer(
        CSSPropertyBackgroundPosition, *curr_layer, style));
    list->Append(*before_slash);
    CSSValueList* after_slash = CSSValueList::CreateSpaceSeparated();
    after_slash->Append(*ValueForFillSize(curr_layer->Size(), style));
    after_slash->Append(*CSSIdentifierValue::Create(curr_layer->Origin()));
    after_slash->Append(*CSSIdentifierValue::Create(curr_layer->Clip()));
    list->Append(*after_slash);
    ret->Append(*list);
  }
  return ret;
}

static CSSValueList*
ValueForContentPositionAndDistributionWithOverflowAlignment(
    const StyleContentAlignmentData& data,
    CSSValueID normal_behavior_value_id) {
  CSSValueList* result = CSSValueList::CreateSpaceSeparated();
  // Handle content-distribution values
  if (data.Distribution() != kContentDistributionDefault)
    result->Append(*CSSIdentifierValue::Create(data.Distribution()));

  // Handle content-position values (either as fallback or actual value)
  switch (data.GetPosition()) {
    case kContentPositionNormal:
      // Handle 'normal' value, not valid as content-distribution fallback.
      if (data.Distribution() == kContentDistributionDefault) {
        result->Append(*CSSIdentifierValue::Create(
            RuntimeEnabledFeatures::CSSGridLayoutEnabled()
                ? CSSValueNormal
                : normal_behavior_value_id));
      }
      break;
    case kContentPositionLastBaseline:
      result->Append(
          *CSSValuePair::Create(CSSIdentifierValue::Create(CSSValueLast),
                                CSSIdentifierValue::Create(CSSValueBaseline),
                                CSSValuePair::kDropIdenticalValues));
      break;
    default:
      result->Append(*CSSIdentifierValue::Create(data.GetPosition()));
  }

  // Handle overflow-alignment (only allowed for content-position values)
  if ((data.GetPosition() >= kContentPositionCenter ||
       data.Distribution() != kContentDistributionDefault) &&
      data.Overflow() != kOverflowAlignmentDefault)
    result->Append(*CSSIdentifierValue::Create(data.Overflow()));
  DCHECK_GT(result->length(), 0u);
  DCHECK_LE(result->length(), 3u);
  return result;
}

static CSSValue* ValueForLineHeight(const ComputedStyle& style) {
  Length length = style.LineHeight();
  if (length.IsNegative())
    return CSSIdentifierValue::Create(CSSValueNormal);

  return ZoomAdjustedPixelValue(
      FloatValueForLength(length, style.GetFontDescription().ComputedSize()),
      style);
}

static CSSValue* ValueForPosition(const LengthPoint& position,
                                  const ComputedStyle& style) {
  DCHECK((position.X() == kAuto) == (position.Y() == kAuto));
  if (position.X() == kAuto) {
    return CSSIdentifierValue::Create(CSSValueAuto);
  }
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ZoomAdjustedPixelValueForLength(position.X(), style));
  list->Append(*ZoomAdjustedPixelValueForLength(position.Y(), style));
  return list;
}

static CSSValueID IdentifierForFamily(const AtomicString& family) {
  if (family == FontFamilyNames::webkit_cursive)
    return CSSValueCursive;
  if (family == FontFamilyNames::webkit_fantasy)
    return CSSValueFantasy;
  if (family == FontFamilyNames::webkit_monospace)
    return CSSValueMonospace;
  if (family == FontFamilyNames::webkit_pictograph)
    return CSSValueWebkitPictograph;
  if (family == FontFamilyNames::webkit_sans_serif)
    return CSSValueSansSerif;
  if (family == FontFamilyNames::webkit_serif)
    return CSSValueSerif;
  return CSSValueInvalid;
}

static CSSValue* ValueForFamily(const AtomicString& family) {
  if (CSSValueID family_identifier = IdentifierForFamily(family))
    return CSSIdentifierValue::Create(family_identifier);
  return CSSFontFamilyValue::Create(family.GetString());
}

static CSSValueList* ValueForFontFamily(const ComputedStyle& style) {
  const FontFamily& first_family = style.GetFontDescription().Family();
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const FontFamily* family = &first_family; family;
       family = family->Next())
    list->Append(*ValueForFamily(family->Family()));
  return list;
}

static CSSPrimitiveValue* ValueForFontSize(const ComputedStyle& style) {
  return ZoomAdjustedPixelValue(style.GetFontDescription().ComputedSize(),
                                style);
}

static CSSIdentifierValue* ValueForFontStretch(const ComputedStyle& style) {
  return CSSIdentifierValue::Create(style.GetFontDescription().Stretch());
}

static CSSIdentifierValue* ValueForFontStyle(const ComputedStyle& style) {
  return CSSIdentifierValue::Create(style.GetFontDescription().Style());
}

static CSSIdentifierValue* ValueForFontWeight(const ComputedStyle& style) {
  return CSSIdentifierValue::Create(style.GetFontDescription().Weight());
}

static CSSIdentifierValue* ValueForFontVariantCaps(const ComputedStyle& style) {
  FontDescription::FontVariantCaps variant_caps =
      style.GetFontDescription().VariantCaps();
  switch (variant_caps) {
    case FontDescription::kCapsNormal:
      return CSSIdentifierValue::Create(CSSValueNormal);
    case FontDescription::kSmallCaps:
      return CSSIdentifierValue::Create(CSSValueSmallCaps);
    case FontDescription::kAllSmallCaps:
      return CSSIdentifierValue::Create(CSSValueAllSmallCaps);
    case FontDescription::kPetiteCaps:
      return CSSIdentifierValue::Create(CSSValuePetiteCaps);
    case FontDescription::kAllPetiteCaps:
      return CSSIdentifierValue::Create(CSSValueAllPetiteCaps);
    case FontDescription::kUnicase:
      return CSSIdentifierValue::Create(CSSValueUnicase);
    case FontDescription::kTitlingCaps:
      return CSSIdentifierValue::Create(CSSValueTitlingCaps);
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValue* ValueForFontVariantLigatures(const ComputedStyle& style) {
  FontDescription::LigaturesState common_ligatures_state =
      style.GetFontDescription().CommonLigaturesState();
  FontDescription::LigaturesState discretionary_ligatures_state =
      style.GetFontDescription().DiscretionaryLigaturesState();
  FontDescription::LigaturesState historical_ligatures_state =
      style.GetFontDescription().HistoricalLigaturesState();
  FontDescription::LigaturesState contextual_ligatures_state =
      style.GetFontDescription().ContextualLigaturesState();
  if (common_ligatures_state == FontDescription::kNormalLigaturesState &&
      discretionary_ligatures_state == FontDescription::kNormalLigaturesState &&
      historical_ligatures_state == FontDescription::kNormalLigaturesState &&
      contextual_ligatures_state == FontDescription::kNormalLigaturesState)
    return CSSIdentifierValue::Create(CSSValueNormal);

  if (common_ligatures_state == FontDescription::kDisabledLigaturesState &&
      discretionary_ligatures_state ==
          FontDescription::kDisabledLigaturesState &&
      historical_ligatures_state == FontDescription::kDisabledLigaturesState &&
      contextual_ligatures_state == FontDescription::kDisabledLigaturesState)
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (common_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        common_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueNoCommonLigatures
            : CSSValueCommonLigatures));
  if (discretionary_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        discretionary_ligatures_state ==
                FontDescription::kDisabledLigaturesState
            ? CSSValueNoDiscretionaryLigatures
            : CSSValueDiscretionaryLigatures));
  if (historical_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        historical_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueNoHistoricalLigatures
            : CSSValueHistoricalLigatures));
  if (contextual_ligatures_state != FontDescription::kNormalLigaturesState)
    value_list->Append(*CSSIdentifierValue::Create(
        contextual_ligatures_state == FontDescription::kDisabledLigaturesState
            ? CSSValueNoContextual
            : CSSValueContextual));
  return value_list;
}

static CSSValue* ValueForFontVariantNumeric(const ComputedStyle& style) {
  FontVariantNumeric variant_numeric =
      style.GetFontDescription().VariantNumeric();
  if (variant_numeric.IsAllNormal())
    return CSSIdentifierValue::Create(CSSValueNormal);

  CSSValueList* value_list = CSSValueList::CreateSpaceSeparated();
  if (variant_numeric.NumericFigureValue() != FontVariantNumeric::kNormalFigure)
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericFigureValue() == FontVariantNumeric::kLiningNums
            ? CSSValueLiningNums
            : CSSValueOldstyleNums));
  if (variant_numeric.NumericSpacingValue() !=
      FontVariantNumeric::kNormalSpacing) {
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericSpacingValue() ==
                FontVariantNumeric::kProportionalNums
            ? CSSValueProportionalNums
            : CSSValueTabularNums));
  }
  if (variant_numeric.NumericFractionValue() !=
      FontVariantNumeric::kNormalFraction)
    value_list->Append(*CSSIdentifierValue::Create(
        variant_numeric.NumericFractionValue() ==
                FontVariantNumeric::kDiagonalFractions
            ? CSSValueDiagonalFractions
            : CSSValueStackedFractions));
  if (variant_numeric.OrdinalValue() == FontVariantNumeric::kOrdinalOn)
    value_list->Append(*CSSIdentifierValue::Create(CSSValueOrdinal));
  if (variant_numeric.SlashedZeroValue() == FontVariantNumeric::kSlashedZeroOn)
    value_list->Append(*CSSIdentifierValue::Create(CSSValueSlashedZero));

  return value_list;
}

static CSSValue* SpecifiedValueForGridTrackBreadth(
    const GridLength& track_breadth,
    const ComputedStyle& style) {
  if (!track_breadth.IsLength())
    return CSSPrimitiveValue::Create(track_breadth.Flex(),
                                     CSSPrimitiveValue::UnitType::kFraction);

  const Length& track_breadth_length = track_breadth.length();
  if (track_breadth_length.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);
  return ZoomAdjustedPixelValueForLength(track_breadth_length, style);
}

static CSSValue* SpecifiedValueForGridTrackSize(const GridTrackSize& track_size,
                                                const ComputedStyle& style) {
  switch (track_size.GetType()) {
    case kLengthTrackSizing:
      return SpecifiedValueForGridTrackBreadth(track_size.MinTrackBreadth(),
                                               style);
    case kMinMaxTrackSizing: {
      if (track_size.MinTrackBreadth().IsAuto() &&
          track_size.MaxTrackBreadth().IsFlex()) {
        return CSSPrimitiveValue::Create(
            track_size.MaxTrackBreadth().Flex(),
            CSSPrimitiveValue::UnitType::kFraction);
      }

      auto* min_max_track_breadths = CSSFunctionValue::Create(CSSValueMinmax);
      min_max_track_breadths->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.MinTrackBreadth(), style));
      min_max_track_breadths->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.MaxTrackBreadth(), style));
      return min_max_track_breadths;
    }
    case kFitContentTrackSizing: {
      auto* fit_content_track_breadth =
          CSSFunctionValue::Create(CSSValueFitContent);
      fit_content_track_breadth->Append(*SpecifiedValueForGridTrackBreadth(
          track_size.FitContentTrackBreadth(), style));
      return fit_content_track_breadth;
    }
  }
  NOTREACHED();
  return nullptr;
}

class OrderedNamedLinesCollector {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(OrderedNamedLinesCollector);

 public:
  OrderedNamedLinesCollector(const ComputedStyle& style,
                             bool is_row_axis,
                             size_t auto_repeat_tracks_count)
      : ordered_named_grid_lines_(is_row_axis
                                      ? style.OrderedNamedGridColumnLines()
                                      : style.OrderedNamedGridRowLines()),
        ordered_named_auto_repeat_grid_lines_(
            is_row_axis ? style.AutoRepeatOrderedNamedGridColumnLines()
                        : style.AutoRepeatOrderedNamedGridRowLines()),
        insertion_point_(is_row_axis
                             ? style.GridAutoRepeatColumnsInsertionPoint()
                             : style.GridAutoRepeatRowsInsertionPoint()),
        auto_repeat_total_tracks_(auto_repeat_tracks_count),
        auto_repeat_track_list_length_(
            is_row_axis ? style.GridAutoRepeatColumns().size()
                        : style.GridAutoRepeatRows().size()) {}

  bool IsEmpty() const {
    return ordered_named_grid_lines_.IsEmpty() &&
           ordered_named_auto_repeat_grid_lines_.IsEmpty();
  }
  void CollectLineNamesForIndex(CSSGridLineNamesValue&, size_t index) const;

 private:
  enum NamedLinesType { kNamedLines, kAutoRepeatNamedLines };
  void AppendLines(CSSGridLineNamesValue&, size_t index, NamedLinesType) const;

  const OrderedNamedGridLines& ordered_named_grid_lines_;
  const OrderedNamedGridLines& ordered_named_auto_repeat_grid_lines_;
  size_t insertion_point_;
  size_t auto_repeat_total_tracks_;
  size_t auto_repeat_track_list_length_;
};

void OrderedNamedLinesCollector::AppendLines(
    CSSGridLineNamesValue& line_names_value,
    size_t index,
    NamedLinesType type) const {
  auto iter = type == kNamedLines
                  ? ordered_named_grid_lines_.find(index)
                  : ordered_named_auto_repeat_grid_lines_.find(index);
  auto end_iter = type == kNamedLines
                      ? ordered_named_grid_lines_.end()
                      : ordered_named_auto_repeat_grid_lines_.end();
  if (iter == end_iter)
    return;

  for (auto line_name : iter->value)
    line_names_value.Append(
        *CSSCustomIdentValue::Create(AtomicString(line_name)));
}

void OrderedNamedLinesCollector::CollectLineNamesForIndex(
    CSSGridLineNamesValue& line_names_value,
    size_t i) const {
  DCHECK(!IsEmpty());
  if (ordered_named_auto_repeat_grid_lines_.IsEmpty() || i < insertion_point_) {
    AppendLines(line_names_value, i, kNamedLines);
    return;
  }

  DCHECK(auto_repeat_total_tracks_);

  if (i > insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, i - (auto_repeat_total_tracks_ - 1),
                kNamedLines);
    return;
  }

  if (i == insertion_point_) {
    AppendLines(line_names_value, i, kNamedLines);
    AppendLines(line_names_value, 0, kAutoRepeatNamedLines);
    return;
  }

  if (i == insertion_point_ + auto_repeat_total_tracks_) {
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                kAutoRepeatNamedLines);
    AppendLines(line_names_value, insertion_point_ + 1, kNamedLines);
    return;
  }

  size_t auto_repeat_index_in_first_repetition =
      (i - insertion_point_) % auto_repeat_track_list_length_;
  if (!auto_repeat_index_in_first_repetition && i > insertion_point_)
    AppendLines(line_names_value, auto_repeat_track_list_length_,
                kAutoRepeatNamedLines);
  AppendLines(line_names_value, auto_repeat_index_in_first_repetition,
              kAutoRepeatNamedLines);
}

static void AddValuesForNamedGridLinesAtIndex(
    OrderedNamedLinesCollector& collector,
    size_t i,
    CSSValueList& list) {
  if (collector.IsEmpty())
    return;

  CSSGridLineNamesValue* line_names = CSSGridLineNamesValue::Create();
  collector.CollectLineNamesForIndex(*line_names, i);
  if (line_names->length())
    list.Append(*line_names);
}

static CSSValue* ValueForGridTrackSizeList(GridTrackSizingDirection direction,
                                           const ComputedStyle& style) {
  const Vector<GridTrackSize>& auto_track_sizes =
      direction == kForColumns ? style.GridAutoColumns() : style.GridAutoRows();

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (auto& track_size : auto_track_sizes)
    list->Append(*SpecifiedValueForGridTrackSize(track_size, style));
  return list;
}

static CSSValue* ValueForGridTrackList(GridTrackSizingDirection direction,
                                       const LayoutObject* layout_object,
                                       const ComputedStyle& style) {
  bool is_row_axis = direction == kForColumns;
  const Vector<GridTrackSize>& track_sizes =
      is_row_axis ? style.GridTemplateColumns() : style.GridTemplateRows();
  const Vector<GridTrackSize>& auto_repeat_track_sizes =
      is_row_axis ? style.GridAutoRepeatColumns() : style.GridAutoRepeatRows();
  bool is_layout_grid = layout_object && layout_object->IsLayoutGrid();

  // Handle the 'none' case.
  bool track_list_is_empty =
      track_sizes.IsEmpty() && auto_repeat_track_sizes.IsEmpty();
  if (is_layout_grid && track_list_is_empty) {
    // For grids we should consider every listed track, whether implicitly or
    // explicitly created. Empty grids have a sole grid line per axis.
    auto& positions = is_row_axis
                          ? ToLayoutGrid(layout_object)->ColumnPositions()
                          : ToLayoutGrid(layout_object)->RowPositions();
    track_list_is_empty = positions.size() == 1;
  }

  if (track_list_is_empty)
    return CSSIdentifierValue::Create(CSSValueNone);

  size_t auto_repeat_total_tracks =
      is_layout_grid
          ? ToLayoutGrid(layout_object)->AutoRepeatCountForDirection(direction)
          : 0;
  OrderedNamedLinesCollector collector(style, is_row_axis,
                                       auto_repeat_total_tracks);
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  size_t insertion_index;
  if (is_layout_grid) {
    const auto* grid = ToLayoutGrid(layout_object);
    Vector<LayoutUnit> computed_track_sizes =
        grid->TrackSizesForComputedStyle(direction);
    size_t num_tracks = computed_track_sizes.size();

    for (size_t i = 0; i < num_tracks; ++i) {
      AddValuesForNamedGridLinesAtIndex(collector, i, *list);
      list->Append(*ZoomAdjustedPixelValue(computed_track_sizes[i], style));
    }
    AddValuesForNamedGridLinesAtIndex(collector, num_tracks + 1, *list);

    insertion_index = num_tracks;
  } else {
    for (size_t i = 0; i < track_sizes.size(); ++i) {
      AddValuesForNamedGridLinesAtIndex(collector, i, *list);
      list->Append(*SpecifiedValueForGridTrackSize(track_sizes[i], style));
    }
    insertion_index = track_sizes.size();
  }
  // Those are the trailing <string>* allowed in the syntax.
  AddValuesForNamedGridLinesAtIndex(collector, insertion_index, *list);
  return list;
}

static CSSValue* ValueForGridPosition(const GridPosition& position) {
  if (position.IsAuto())
    return CSSIdentifierValue::Create(CSSValueAuto);

  if (position.IsNamedGridArea())
    return CSSCustomIdentValue::Create(position.NamedGridLine());

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (position.IsSpan()) {
    list->Append(*CSSIdentifierValue::Create(CSSValueSpan));
    list->Append(*CSSPrimitiveValue::Create(
        position.SpanPosition(), CSSPrimitiveValue::UnitType::kNumber));
  } else {
    list->Append(*CSSPrimitiveValue::Create(
        position.IntegerPosition(), CSSPrimitiveValue::UnitType::kNumber));
  }

  if (!position.NamedGridLine().IsNull())
    list->Append(*CSSCustomIdentValue::Create(position.NamedGridLine()));
  return list;
}

static LayoutRect SizingBox(const LayoutObject* layout_object) {
  if (!layout_object->IsBox())
    return LayoutRect();

  const LayoutBox* box = ToLayoutBox(layout_object);
  return box->Style()->BoxSizing() == EBoxSizing::kBorderBox
             ? box->BorderBoxRect()
             : box->ComputedCSSContentBoxRect();
}

static CSSValue* RenderTextDecorationFlagsToCSSValue(
    TextDecoration text_decoration) {
  // Blink value is ignored.
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (EnumHasFlags(text_decoration, TextDecoration::kUnderline))
    list->Append(*CSSIdentifierValue::Create(CSSValueUnderline));
  if (EnumHasFlags(text_decoration, TextDecoration::kOverline))
    list->Append(*CSSIdentifierValue::Create(CSSValueOverline));
  if (EnumHasFlags(text_decoration, TextDecoration::kLineThrough))
    list->Append(*CSSIdentifierValue::Create(CSSValueLineThrough));

  if (!list->length())
    return CSSIdentifierValue::Create(CSSValueNone);
  return list;
}

static CSSValue* ValueForTextDecorationStyle(
    ETextDecorationStyle text_decoration_style) {
  switch (text_decoration_style) {
    case ETextDecorationStyle::kSolid:
      return CSSIdentifierValue::Create(CSSValueSolid);
    case ETextDecorationStyle::kDouble:
      return CSSIdentifierValue::Create(CSSValueDouble);
    case ETextDecorationStyle::kDotted:
      return CSSIdentifierValue::Create(CSSValueDotted);
    case ETextDecorationStyle::kDashed:
      return CSSIdentifierValue::Create(CSSValueDashed);
    case ETextDecorationStyle::kWavy:
      return CSSIdentifierValue::Create(CSSValueWavy);
  }

  NOTREACHED();
  return CSSInitialValue::Create();
}

static CSSValue* ValueForTextDecorationSkip(
    TextDecorationSkip text_decoration_skip) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (EnumHasFlags(text_decoration_skip, TextDecorationSkip::kObjects))
    list->Append(*CSSIdentifierValue::Create(CSSValueObjects));
  if (EnumHasFlags(text_decoration_skip, TextDecorationSkip::kInk))
    list->Append(*CSSIdentifierValue::Create(CSSValueInk));

  DCHECK(list->length());
  return list;
}

static CSSValue* TouchActionFlagsToCSSValue(TouchAction touch_action) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (touch_action == TouchAction::kTouchActionAuto) {
    list->Append(*CSSIdentifierValue::Create(CSSValueAuto));
  } else if (touch_action == TouchAction::kTouchActionNone) {
    list->Append(*CSSIdentifierValue::Create(CSSValueNone));
  } else if (touch_action == TouchAction::kTouchActionManipulation) {
    list->Append(*CSSIdentifierValue::Create(CSSValueManipulation));
  } else {
    if ((touch_action & TouchAction::kTouchActionPanX) ==
        TouchAction::kTouchActionPanX)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanX));
    else if (touch_action & TouchAction::kTouchActionPanLeft)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanLeft));
    else if (touch_action & TouchAction::kTouchActionPanRight)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanRight));
    if ((touch_action & TouchAction::kTouchActionPanY) ==
        TouchAction::kTouchActionPanY)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanY));
    else if (touch_action & TouchAction::kTouchActionPanUp)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanUp));
    else if (touch_action & TouchAction::kTouchActionPanDown)
      list->Append(*CSSIdentifierValue::Create(CSSValuePanDown));

    if ((touch_action & TouchAction::kTouchActionPinchZoom) ==
        TouchAction::kTouchActionPinchZoom)
      list->Append(*CSSIdentifierValue::Create(CSSValuePinchZoom));
  }

  DCHECK(list->length());
  return list;
}

static CSSValue* ValueForWillChange(
    const Vector<CSSPropertyID>& will_change_properties,
    bool will_change_contents,
    bool will_change_scroll_position) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (will_change_contents)
    list->Append(*CSSIdentifierValue::Create(CSSValueContents));
  if (will_change_scroll_position)
    list->Append(*CSSIdentifierValue::Create(CSSValueScrollPosition));
  for (size_t i = 0; i < will_change_properties.size(); ++i)
    list->Append(*CSSCustomIdentValue::Create(will_change_properties[i]));
  if (!list->length())
    list->Append(*CSSIdentifierValue::Create(CSSValueAuto));
  return list;
}

static CSSValue* ValueForAnimationDelay(const CSSTimingData* timing_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (timing_data) {
    for (size_t i = 0; i < timing_data->DelayList().size(); ++i)
      list->Append(*CSSPrimitiveValue::Create(
          timing_data->DelayList()[i], CSSPrimitiveValue::UnitType::kSeconds));
  } else {
    list->Append(*CSSPrimitiveValue::Create(
        CSSTimingData::InitialDelay(), CSSPrimitiveValue::UnitType::kSeconds));
  }
  return list;
}

static CSSValue* ValueForAnimationDirection(
    Timing::PlaybackDirection direction) {
  switch (direction) {
    case Timing::PlaybackDirection::NORMAL:
      return CSSIdentifierValue::Create(CSSValueNormal);
    case Timing::PlaybackDirection::ALTERNATE_NORMAL:
      return CSSIdentifierValue::Create(CSSValueAlternate);
    case Timing::PlaybackDirection::REVERSE:
      return CSSIdentifierValue::Create(CSSValueReverse);
    case Timing::PlaybackDirection::ALTERNATE_REVERSE:
      return CSSIdentifierValue::Create(CSSValueAlternateReverse);
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValue* ValueForAnimationDuration(const CSSTimingData* timing_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (timing_data) {
    for (size_t i = 0; i < timing_data->DurationList().size(); ++i)
      list->Append(
          *CSSPrimitiveValue::Create(timing_data->DurationList()[i],
                                     CSSPrimitiveValue::UnitType::kSeconds));
  } else {
    list->Append(
        *CSSPrimitiveValue::Create(CSSTimingData::InitialDuration(),
                                   CSSPrimitiveValue::UnitType::kSeconds));
  }
  return list;
}

static CSSValue* ValueForAnimationFillMode(Timing::FillMode fill_mode) {
  switch (fill_mode) {
    case Timing::FillMode::NONE:
      return CSSIdentifierValue::Create(CSSValueNone);
    case Timing::FillMode::FORWARDS:
      return CSSIdentifierValue::Create(CSSValueForwards);
    case Timing::FillMode::BACKWARDS:
      return CSSIdentifierValue::Create(CSSValueBackwards);
    case Timing::FillMode::BOTH:
      return CSSIdentifierValue::Create(CSSValueBoth);
    default:
      NOTREACHED();
      return nullptr;
  }
}

static CSSValue* ValueForAnimationIterationCount(double iteration_count) {
  if (iteration_count == std::numeric_limits<double>::infinity())
    return CSSIdentifierValue::Create(CSSValueInfinite);
  return CSSPrimitiveValue::Create(iteration_count,
                                   CSSPrimitiveValue::UnitType::kNumber);
}

static CSSValue* ValueForAnimationPlayState(EAnimPlayState play_state) {
  if (play_state == kAnimPlayStatePlaying)
    return CSSIdentifierValue::Create(CSSValueRunning);
  DCHECK_EQ(play_state, kAnimPlayStatePaused);
  return CSSIdentifierValue::Create(CSSValuePaused);
}

static CSSValue* CreateTimingFunctionValue(
    const TimingFunction* timing_function) {
  switch (timing_function->GetType()) {
    case TimingFunction::Type::CUBIC_BEZIER: {
      const CubicBezierTimingFunction* bezier_timing_function =
          ToCubicBezierTimingFunction(timing_function);
      if (bezier_timing_function->GetEaseType() !=
          CubicBezierTimingFunction::EaseType::CUSTOM) {
        CSSValueID value_id = CSSValueInvalid;
        switch (bezier_timing_function->GetEaseType()) {
          case CubicBezierTimingFunction::EaseType::EASE:
            value_id = CSSValueEase;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN:
            value_id = CSSValueEaseIn;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_OUT:
            value_id = CSSValueEaseOut;
            break;
          case CubicBezierTimingFunction::EaseType::EASE_IN_OUT:
            value_id = CSSValueEaseInOut;
            break;
          default:
            NOTREACHED();
            return nullptr;
        }
        return CSSIdentifierValue::Create(value_id);
      }
      return CSSCubicBezierTimingFunctionValue::Create(
          bezier_timing_function->X1(), bezier_timing_function->Y1(),
          bezier_timing_function->X2(), bezier_timing_function->Y2());
    }

    case TimingFunction::Type::STEPS: {
      const StepsTimingFunction* steps_timing_function =
          ToStepsTimingFunction(timing_function);
      StepsTimingFunction::StepPosition position =
          steps_timing_function->GetStepPosition();
      int steps = steps_timing_function->NumberOfSteps();
      DCHECK(position == StepsTimingFunction::StepPosition::START ||
             position == StepsTimingFunction::StepPosition::END);

      if (steps > 1)
        return CSSStepsTimingFunctionValue::Create(steps, position);
      CSSValueID value_id = position == StepsTimingFunction::StepPosition::START
                                ? CSSValueStepStart
                                : CSSValueStepEnd;
      return CSSIdentifierValue::Create(value_id);
    }

    case TimingFunction::Type::FRAMES: {
      const FramesTimingFunction* frames_timing_function =
          ToFramesTimingFunction(timing_function);
      int frames = frames_timing_function->NumberOfFrames();
      return CSSFramesTimingFunctionValue::Create(frames);
    }

    default:
      return CSSIdentifierValue::Create(CSSValueLinear);
  }
}

static CSSValue* ValueForAnimationTimingFunction(
    const CSSTimingData* timing_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (timing_data) {
    for (size_t i = 0; i < timing_data->TimingFunctionList().size(); ++i)
      list->Append(*CreateTimingFunctionValue(
          timing_data->TimingFunctionList()[i].Get()));
  } else {
    list->Append(*CreateTimingFunctionValue(
        CSSTimingData::InitialTimingFunction().Get()));
  }
  return list;
}

static CSSValueList* ValuesForBorderRadiusCorner(LengthSize radius,
                                                 const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (radius.Width().GetType() == kPercent)
    list->Append(*CSSPrimitiveValue::Create(
        radius.Width().Percent(), CSSPrimitiveValue::UnitType::kPercentage));
  else
    list->Append(*ZoomAdjustedPixelValueForLength(radius.Width(), style));
  if (radius.Height().GetType() == kPercent)
    list->Append(*CSSPrimitiveValue::Create(
        radius.Height().Percent(), CSSPrimitiveValue::UnitType::kPercentage));
  else
    list->Append(*ZoomAdjustedPixelValueForLength(radius.Height(), style));
  return list;
}

static const CSSValue& ValueForBorderRadiusCorner(LengthSize radius,
                                                  const ComputedStyle& style) {
  CSSValueList& list = *ValuesForBorderRadiusCorner(radius, style);
  if (list.Item(0) == list.Item(1))
    return list.Item(0);
  return list;
}

static CSSFunctionValue* ValueForMatrixTransform(TransformationMatrix transform,
                                                 const ComputedStyle& style) {
  CSSFunctionValue* transform_value = nullptr;
  transform.Zoom(1 / style.EffectiveZoom());
  if (transform.IsAffine()) {
    transform_value = CSSFunctionValue::Create(CSSValueMatrix);

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.A(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.B(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.C(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.D(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.E(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.F(), CSSPrimitiveValue::UnitType::kNumber));
  } else {
    transform_value = CSSFunctionValue::Create(CSSValueMatrix3d);

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M11(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M12(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M13(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M14(), CSSPrimitiveValue::UnitType::kNumber));

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M21(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M22(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M23(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M24(), CSSPrimitiveValue::UnitType::kNumber));

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M31(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M32(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M33(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M34(), CSSPrimitiveValue::UnitType::kNumber));

    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M41(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M42(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M43(), CSSPrimitiveValue::UnitType::kNumber));
    transform_value->Append(*CSSPrimitiveValue::Create(
        transform.M44(), CSSPrimitiveValue::UnitType::kNumber));
  }

  return transform_value;
}

static CSSValue* ComputedTransform(const LayoutObject* layout_object,
                                   const ComputedStyle& style) {
  if (!layout_object || !style.HasTransform())
    return CSSIdentifierValue::Create(CSSValueNone);

  IntRect box;
  if (layout_object->IsBox())
    box = PixelSnappedIntRect(ToLayoutBox(layout_object)->BorderBoxRect());

  TransformationMatrix transform;
  style.ApplyTransform(transform, LayoutSize(box.Size()),
                       ComputedStyle::kExcludeTransformOrigin,
                       ComputedStyle::kExcludeMotionPath,
                       ComputedStyle::kExcludeIndependentTransformProperties);

  // FIXME: Need to print out individual functions
  // (https://bugs.webkit.org/show_bug.cgi?id=23924)
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForMatrixTransform(transform, style));

  return list;
}

static CSSValue* CreateTransitionPropertyValue(
    const CSSTransitionData::TransitionProperty& property) {
  if (property.property_type == CSSTransitionData::kTransitionNone)
    return CSSIdentifierValue::Create(CSSValueNone);
  if (property.property_type == CSSTransitionData::kTransitionUnknownProperty)
    return CSSCustomIdentValue::Create(property.property_string);
  DCHECK_EQ(property.property_type,
            CSSTransitionData::kTransitionKnownProperty);
  return CSSCustomIdentValue::Create(
      getPropertyNameAtomicString(property.unresolved_property));
}

static CSSValue* ValueForTransitionProperty(
    const CSSTransitionData* transition_data) {
  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  if (transition_data) {
    for (size_t i = 0; i < transition_data->PropertyList().size(); ++i)
      list->Append(
          *CreateTransitionPropertyValue(transition_data->PropertyList()[i]));
  } else {
    list->Append(*CSSIdentifierValue::Create(CSSValueAll));
  }
  return list;
}

CSSValueID ValueForQuoteType(const QuoteType quote_type) {
  switch (quote_type) {
    case NO_OPEN_QUOTE:
      return CSSValueNoOpenQuote;
    case NO_CLOSE_QUOTE:
      return CSSValueNoCloseQuote;
    case CLOSE_QUOTE:
      return CSSValueCloseQuote;
    case OPEN_QUOTE:
      return CSSValueOpenQuote;
  }
  NOTREACHED();
  return CSSValueInvalid;
}

static CSSValue* ValueForContentData(const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const ContentData* content_data = style.GetContentData(); content_data;
       content_data = content_data->Next()) {
    if (content_data->IsCounter()) {
      const CounterContent* counter =
          ToCounterContentData(content_data)->Counter();
      DCHECK(counter);
      CSSCustomIdentValue* identifier =
          CSSCustomIdentValue::Create(counter->Identifier());
      CSSStringValue* separator = CSSStringValue::Create(counter->Separator());
      CSSValueID list_style_ident = CSSValueNone;
      if (counter->ListStyle() != EListStyleType::kNone) {
        // TODO(sashab): Change this to use a converter instead of
        // CSSPrimitiveValueMappings.
        list_style_ident =
            CSSIdentifierValue::Create(counter->ListStyle())->GetValueID();
      }
      CSSIdentifierValue* list_style =
          CSSIdentifierValue::Create(list_style_ident);
      list->Append(*CSSCounterValue::Create(identifier, list_style, separator));
    } else if (content_data->IsImage()) {
      const StyleImage* image = ToImageContentData(content_data)->GetImage();
      DCHECK(image);
      list->Append(*image->ComputedCSSValue());
    } else if (content_data->IsText()) {
      list->Append(
          *CSSStringValue::Create(ToTextContentData(content_data)->GetText()));
    } else if (content_data->IsQuote()) {
      const QuoteType quote_type = ToQuoteContentData(content_data)->Quote();
      list->Append(*CSSIdentifierValue::Create(ValueForQuoteType(quote_type)));
    } else {
      NOTREACHED();
    }
  }
  return list;
}

static CSSValue* ValueForCounterDirectives(const ComputedStyle& style,
                                           CSSPropertyID property_id) {
  const CounterDirectiveMap* map = style.GetCounterDirectives();
  if (!map)
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (const auto& item : *map) {
    bool is_valid_counter_value = property_id == CSSPropertyCounterIncrement
                                      ? item.value.IsIncrement()
                                      : item.value.IsReset();
    if (!is_valid_counter_value)
      continue;

    list->Append(*CSSCustomIdentValue::Create(item.key));
    short number = property_id == CSSPropertyCounterIncrement
                       ? item.value.IncrementValue()
                       : item.value.ResetValue();
    list->Append(*CSSPrimitiveValue::Create(
        (double)number, CSSPrimitiveValue::UnitType::kInteger));
  }

  if (!list->length())
    return CSSIdentifierValue::Create(CSSValueNone);

  return list;
}

static CSSValue* ValueForShape(const ComputedStyle& style,
                               ShapeValue* shape_value) {
  if (!shape_value)
    return CSSIdentifierValue::Create(CSSValueNone);
  if (shape_value->GetType() == ShapeValue::kBox)
    return CSSIdentifierValue::Create(shape_value->CssBox());
  if (shape_value->GetType() == ShapeValue::kImage) {
    if (shape_value->GetImage())
      return shape_value->GetImage()->ComputedCSSValue();
    return CSSIdentifierValue::Create(CSSValueNone);
  }

  DCHECK_EQ(shape_value->GetType(), ShapeValue::kShape);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForBasicShape(style, shape_value->Shape()));
  if (shape_value->CssBox() != kBoxMissing)
    list->Append(*CSSIdentifierValue::Create(shape_value->CssBox()));
  return list;
}

static CSSValueList* ValuesForSidesShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // Assume the properties are in the usual order top, right, bottom, left.
  const CSSValue* top_value = ComputedStyleCSSValueMapping::Get(
      shorthand.properties()[0], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* right_value = ComputedStyleCSSValueMapping::Get(
      shorthand.properties()[1], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* bottom_value = ComputedStyleCSSValueMapping::Get(
      shorthand.properties()[2], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* left_value = ComputedStyleCSSValueMapping::Get(
      shorthand.properties()[3], style, layout_object, styled_node,
      allow_visited_style);

  // All 4 properties must be specified.
  if (!top_value || !right_value || !bottom_value || !left_value)
    return nullptr;

  bool show_left = !DataEquivalent(right_value, left_value);
  bool show_bottom = !DataEquivalent(top_value, bottom_value) || show_left;
  bool show_right = !DataEquivalent(top_value, right_value) || show_bottom;

  list->Append(*top_value);
  if (show_right)
    list->Append(*right_value);
  if (show_bottom)
    list->Append(*bottom_value);
  if (show_left)
    list->Append(*left_value);

  return list;
}

static CSSValuePair* ValuesForInlineBlockShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  const CSSValue* start_value = ComputedStyleCSSValueMapping::Get(
      shorthand.properties()[0], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* end_value = ComputedStyleCSSValueMapping::Get(
      shorthand.properties()[1], style, layout_object, styled_node,
      allow_visited_style);
  // Both properties must be specified.
  if (!start_value || !end_value)
    return nullptr;

  CSSValuePair* pair = CSSValuePair::Create(start_value, end_value,
                                            CSSValuePair::kDropIdenticalValues);
  return pair;
}

static CSSValueList* ValueForBorderRadiusShorthand(const ComputedStyle& style) {
  CSSValueList* list = CSSValueList::CreateSlashSeparated();

  bool show_horizontal_bottom_left = style.BorderTopRightRadius().Width() !=
                                     style.BorderBottomLeftRadius().Width();
  bool show_horizontal_bottom_right =
      show_horizontal_bottom_left || (style.BorderBottomRightRadius().Width() !=
                                      style.BorderTopLeftRadius().Width());
  bool show_horizontal_top_right =
      show_horizontal_bottom_right || (style.BorderTopRightRadius().Width() !=
                                       style.BorderTopLeftRadius().Width());

  bool show_vertical_bottom_left = style.BorderTopRightRadius().Height() !=
                                   style.BorderBottomLeftRadius().Height();
  bool show_vertical_bottom_right =
      show_vertical_bottom_left || (style.BorderBottomRightRadius().Height() !=
                                    style.BorderTopLeftRadius().Height());
  bool show_vertical_top_right =
      show_vertical_bottom_right || (style.BorderTopRightRadius().Height() !=
                                     style.BorderTopLeftRadius().Height());

  CSSValueList* top_left_radius =
      ValuesForBorderRadiusCorner(style.BorderTopLeftRadius(), style);
  CSSValueList* top_right_radius =
      ValuesForBorderRadiusCorner(style.BorderTopRightRadius(), style);
  CSSValueList* bottom_right_radius =
      ValuesForBorderRadiusCorner(style.BorderBottomRightRadius(), style);
  CSSValueList* bottom_left_radius =
      ValuesForBorderRadiusCorner(style.BorderBottomLeftRadius(), style);

  CSSValueList* horizontal_radii = CSSValueList::CreateSpaceSeparated();
  horizontal_radii->Append(top_left_radius->Item(0));
  if (show_horizontal_top_right)
    horizontal_radii->Append(top_right_radius->Item(0));
  if (show_horizontal_bottom_right)
    horizontal_radii->Append(bottom_right_radius->Item(0));
  if (show_horizontal_bottom_left)
    horizontal_radii->Append(bottom_left_radius->Item(0));

  list->Append(*horizontal_radii);

  CSSValueList* vertical_radii = CSSValueList::CreateSpaceSeparated();
  vertical_radii->Append(top_left_radius->Item(1));
  if (show_vertical_top_right)
    vertical_radii->Append(top_right_radius->Item(1));
  if (show_vertical_bottom_right)
    vertical_radii->Append(bottom_right_radius->Item(1));
  if (show_vertical_bottom_left)
    vertical_radii->Append(bottom_left_radius->Item(1));

  if (!vertical_radii->Equals(ToCSSValueList(list->Item(0))))
    list->Append(*vertical_radii);

  return list;
}

static CSSValue* StrokeDashArrayToCSSValueList(const SVGDashArray& dashes,
                                               const ComputedStyle& style) {
  if (dashes.IsEmpty())
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  for (const Length& dash_length : dashes.GetVector())
    list->Append(*ZoomAdjustedPixelValueForLength(dash_length, style));

  return list;
}

static CSSValue* PaintOrderToCSSValueList(const SVGComputedStyle& svg_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  for (int i = 0; i < 3; i++) {
    EPaintOrderType paint_order_type = svg_style.PaintOrderType(i);
    switch (paint_order_type) {
      case PT_FILL:
      case PT_STROKE:
      case PT_MARKERS:
        list->Append(*CSSIdentifierValue::Create(paint_order_type));
        break;
      case PT_NONE:
      default:
        NOTREACHED();
        break;
    }
  }

  return list;
}

static CSSValue* AdjustSVGPaintForCurrentColor(SVGPaintType paint_type,
                                               const String& url,
                                               const Color& color,
                                               const Color& current_color) {
  if (paint_type >= SVG_PAINTTYPE_URI_NONE) {
    CSSValueList* values = CSSValueList::CreateSpaceSeparated();
    values->Append(*CSSURIValue::Create(AtomicString(url)));
    if (paint_type == SVG_PAINTTYPE_URI_NONE)
      values->Append(*CSSIdentifierValue::Create(CSSValueNone));
    else if (paint_type == SVG_PAINTTYPE_URI_CURRENTCOLOR)
      values->Append(*CSSColorValue::Create(current_color.Rgb()));
    else if (paint_type == SVG_PAINTTYPE_URI_RGBCOLOR)
      values->Append(*CSSColorValue::Create(color.Rgb()));
    return values;
  }
  if (paint_type == SVG_PAINTTYPE_NONE)
    return CSSIdentifierValue::Create(CSSValueNone);
  if (paint_type == SVG_PAINTTYPE_CURRENTCOLOR)
    return CSSColorValue::Create(current_color.Rgb());

  return CSSColorValue::Create(color.Rgb());
}

static inline AtomicString SerializeAsFragmentIdentifier(
    const AtomicString& resource) {
  return "#" + resource;
}

CSSValue* ComputedStyleCSSValueMapping::ValueForShadowData(
    const ShadowData& shadow,
    const ComputedStyle& style,
    bool use_spread) {
  CSSPrimitiveValue* x = ZoomAdjustedPixelValue(shadow.X(), style);
  CSSPrimitiveValue* y = ZoomAdjustedPixelValue(shadow.Y(), style);
  CSSPrimitiveValue* blur = ZoomAdjustedPixelValue(shadow.Blur(), style);
  CSSPrimitiveValue* spread =
      use_spread ? ZoomAdjustedPixelValue(shadow.Spread(), style) : nullptr;
  CSSIdentifierValue* shadow_style =
      shadow.Style() == kNormal ? nullptr
                                : CSSIdentifierValue::Create(CSSValueInset);
  CSSValue* color = CurrentColorOrValidColor(style, shadow.GetColor());
  return CSSShadowValue::Create(x, y, blur, spread, shadow_style, color);
}

CSSValue* ComputedStyleCSSValueMapping::ValueForShadowList(
    const ShadowList* shadow_list,
    const ComputedStyle& style,
    bool use_spread) {
  if (!shadow_list)
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateCommaSeparated();
  size_t shadow_count = shadow_list->Shadows().size();
  for (size_t i = 0; i < shadow_count; ++i)
    list->Append(
        *ValueForShadowData(shadow_list->Shadows()[i], style, use_spread));
  return list;
}

CSSValue* ComputedStyleCSSValueMapping::ValueForFilter(
    const ComputedStyle& style,
    const FilterOperations& filter_operations) {
  if (filter_operations.Operations().IsEmpty())
    return CSSIdentifierValue::Create(CSSValueNone);

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();

  CSSFunctionValue* filter_value = nullptr;

  for (const auto& operation : filter_operations.Operations()) {
    FilterOperation* filter_operation = operation.Get();
    switch (filter_operation->GetType()) {
      case FilterOperation::REFERENCE:
        filter_value = CSSFunctionValue::Create(CSSValueUrl);
        filter_value->Append(*CSSStringValue::Create(
            ToReferenceFilterOperation(filter_operation)->Url()));
        break;
      case FilterOperation::GRAYSCALE:
        filter_value = CSSFunctionValue::Create(CSSValueGrayscale);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::SEPIA:
        filter_value = CSSFunctionValue::Create(CSSValueSepia);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::SATURATE:
        filter_value = CSSFunctionValue::Create(CSSValueSaturate);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::HUE_ROTATE:
        filter_value = CSSFunctionValue::Create(CSSValueHueRotate);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicColorMatrixFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kDegrees));
        break;
      case FilterOperation::INVERT:
        filter_value = CSSFunctionValue::Create(CSSValueInvert);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::OPACITY:
        filter_value = CSSFunctionValue::Create(CSSValueOpacity);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::BRIGHTNESS:
        filter_value = CSSFunctionValue::Create(CSSValueBrightness);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::CONTRAST:
        filter_value = CSSFunctionValue::Create(CSSValueContrast);
        filter_value->Append(*CSSPrimitiveValue::Create(
            ToBasicComponentTransferFilterOperation(filter_operation)->Amount(),
            CSSPrimitiveValue::UnitType::kNumber));
        break;
      case FilterOperation::BLUR:
        filter_value = CSSFunctionValue::Create(CSSValueBlur);
        filter_value->Append(*ZoomAdjustedPixelValue(
            ToBlurFilterOperation(filter_operation)->StdDeviation().Value(),
            style));
        break;
      case FilterOperation::DROP_SHADOW: {
        const auto& drop_shadow_operation =
            ToDropShadowFilterOperation(*filter_operation);
        filter_value = CSSFunctionValue::Create(CSSValueDropShadow);
        // We want our computed style to look like that of a text shadow (has
        // neither spread nor inset style).
        filter_value->Append(
            *ValueForShadowData(drop_shadow_operation.Shadow(), style, false));
        break;
      }
      default:
        NOTREACHED();
        break;
    }
    list->Append(*filter_value);
  }

  return list;
}

CSSValue* ComputedStyleCSSValueMapping::ValueForOffset(
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    CSSValue* position = ValueForPosition(style.OffsetPosition(), style);
    if (!position->IsIdentifierValue())
      list->Append(*position);
    else
      DCHECK(ToCSSIdentifierValue(position)->GetValueID() == CSSValueAuto);
  }

  CSSPropertyID longhands[] = {CSSPropertyOffsetPath, CSSPropertyOffsetDistance,
                               CSSPropertyOffsetRotate};
  for (CSSPropertyID longhand : longhands) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        longhand, style, layout_object, styled_node, allow_visited_style);
    DCHECK(value);
    list->Append(*value);
  }

  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    CSSValue* anchor = ValueForPosition(style.OffsetAnchor(), style);
    if (!anchor->IsIdentifierValue()) {
      // Add a slash before anchor.
      CSSValueList* result = CSSValueList::CreateSlashSeparated();
      result->Append(*list);
      result->Append(*anchor);
      return result;
    } else {
      DCHECK(ToCSSIdentifierValue(anchor)->GetValueID() == CSSValueAuto);
    }
  }
  return list;
}

CSSValue* ComputedStyleCSSValueMapping::ValueForFont(
    const ComputedStyle& style) {
  // Add a slash between size and line-height.
  CSSValueList* size_and_line_height = CSSValueList::CreateSlashSeparated();
  size_and_line_height->Append(*ValueForFontSize(style));
  size_and_line_height->Append(*ValueForLineHeight(style));

  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  list->Append(*ValueForFontStyle(style));

  // Check that non-initial font-variant subproperties are not conflicting with
  // this serialization.
  CSSValue* ligatures_value = ValueForFontVariantLigatures(style);
  CSSValue* numeric_value = ValueForFontVariantNumeric(style);
  // FIXME: Use DataEquivalent<CSSValue>(...) once http://crbug.com/729447 is
  // resolved.
  if (!DataEquivalent(
          ligatures_value,
          static_cast<CSSValue*>(CSSIdentifierValue::Create(CSSValueNormal))) ||
      !DataEquivalent(
          numeric_value,
          static_cast<CSSValue*>(CSSIdentifierValue::Create(CSSValueNormal))))
    return nullptr;

  CSSIdentifierValue* caps_value = ValueForFontVariantCaps(style);
  if (caps_value->GetValueID() != CSSValueNormal &&
      caps_value->GetValueID() != CSSValueSmallCaps)
    return nullptr;
  list->Append(*caps_value);

  list->Append(*ValueForFontWeight(style));
  list->Append(*ValueForFontStretch(style));
  list->Append(*size_and_line_height);
  list->Append(*ValueForFontFamily(style));

  return list;
}

static CSSValue* ValueForScrollSnapType(const ScrollSnapType& type,
                                        const ComputedStyle& style) {
  if (!type.is_none) {
    return CSSValuePair::Create(CSSIdentifierValue::Create(type.axis),
                                CSSIdentifierValue::Create(type.strictness),
                                CSSValuePair::kDropIdenticalValues);
  }
  return CSSIdentifierValue::Create(CSSValueNone);
}

static CSSValue* ValueForScrollSnapAlign(const ScrollSnapAlign& align,
                                         const ComputedStyle& style) {
  return CSSValuePair::Create(CSSIdentifierValue::Create(align.alignmentX),
                              CSSIdentifierValue::Create(align.alignmentY),
                              CSSValuePair::kDropIdenticalValues);
}

// Returns a suitable value for the page-break-(before|after) property, given
// the computed value of the more general break-(before|after) property.
static CSSValue* ValueForPageBreakBetween(EBreakBetween break_value) {
  switch (break_value) {
    case EBreakBetween::kAvoidColumn:
    case EBreakBetween::kColumn:
    case EBreakBetween::kRecto:
    case EBreakBetween::kVerso:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakBetween::kPage:
      return CSSIdentifierValue::Create(CSSValueAlways);
    case EBreakBetween::kAvoidPage:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the -webkit-column-break-(before|after)
// property, given the computed value of the more general break-(before|after)
// property.
static CSSValue* ValueForWebkitColumnBreakBetween(EBreakBetween break_value) {
  switch (break_value) {
    case EBreakBetween::kAvoidPage:
    case EBreakBetween::kLeft:
    case EBreakBetween::kPage:
    case EBreakBetween::kRecto:
    case EBreakBetween::kRight:
    case EBreakBetween::kVerso:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakBetween::kColumn:
      return CSSIdentifierValue::Create(CSSValueAlways);
    case EBreakBetween::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the page-break-inside property, given the
// computed value of the more general break-inside property.
static CSSValue* ValueForPageBreakInside(EBreakInside break_value) {
  switch (break_value) {
    case EBreakInside::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakInside::kAvoidPage:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

// Returns a suitable value for the -webkit-column-break-inside property, given
// the computed value of the more general break-inside property.
static CSSValue* ValueForWebkitColumnBreakInside(EBreakInside break_value) {
  switch (break_value) {
    case EBreakInside::kAvoidPage:
      return CSSIdentifierValue::Create(CSSValueAuto);
    case EBreakInside::kAvoidColumn:
      return CSSIdentifierValue::Create(CSSValueAvoid);
    default:
      return CSSIdentifierValue::Create(break_value);
  }
}

const CSSValue* ComputedStyleCSSValueMapping::Get(
    const AtomicString custom_property_name,
    const ComputedStyle& style,
    const PropertyRegistry* registry) {
  if (registry) {
    const PropertyRegistration* registration =
        registry->Registration(custom_property_name);
    if (registration) {
      const CSSValue* result = style.GetRegisteredVariable(
          custom_property_name, registration->Inherits());
      if (result)
        return result;
      return registration->Initial();
    }
  }

  bool is_inherited_property = true;
  CSSVariableData* data =
      style.GetVariable(custom_property_name, is_inherited_property);
  if (!data)
    return nullptr;

  return CSSCustomPropertyDeclaration::Create(custom_property_name, data);
}

std::unique_ptr<HashMap<AtomicString, RefPtr<CSSVariableData>>>
ComputedStyleCSSValueMapping::GetVariables(const ComputedStyle& style) {
  // TODO(timloh): Also return non-inherited variables
  StyleInheritedVariables* variables = style.InheritedVariables();
  if (variables)
    return variables->GetVariables();
  return nullptr;
}

static bool WidthOrHeightPropertyAppliesToObject(const LayoutObject& object) {
  // According to
  // http://www.w3.org/TR/CSS2/visudet.html#the-width-property and
  // http://www.w3.org/TR/CSS2/visudet.html#the-height-property, the "width" or
  // "height" property does not apply to non-atomic inline elements.
  if (!object.IsAtomicInlineLevel() && object.IsInline())
    return false;

  // Non-root SVG should be treated as non-atomic inline no matter how we
  // implement it internally (e.g. LayoutSVGBlock is based on LayoutBlockFlow).
  return !object.IsSVGChild();
}

CSSPrimitiveValue* ZoomAdjustedPixelValueWithBorderWidthRounding(
    double value,
    const ComputedStyle& style) {
  const double px_value = AdjustFloatForAbsoluteZoom(value, style);

  return CSSPrimitiveValue::Create(
      px_value > 0.0 && px_value < 1 ? 1.0 : px_value,
      CSSPrimitiveValue::UnitType::kPixels);
}

const CSSValue* ComputedStyleCSSValueMapping::Get(
    CSSPropertyID property_id,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  const SVGComputedStyle& svg_style = style.SvgStyle();
  property_id = CSSProperty::ResolveDirectionAwareProperty(
      property_id, style.Direction(), style.GetWritingMode());
  switch (property_id) {
    case CSSPropertyInvalid:
      return nullptr;

    case CSSPropertyBackgroundColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyBackgroundColor)
                           .Rgb())
                 : CurrentColorOrValidColor(style, style.BackgroundColor());
    case CSSPropertyBackgroundImage:
    case CSSPropertyWebkitMaskImage: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer = property_id == CSSPropertyWebkitMaskImage
                                        ? &style.MaskLayers()
                                        : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next()) {
        if (curr_layer->GetImage())
          list->Append(*curr_layer->GetImage()->ComputedCSSValue());
        else
          list->Append(*CSSIdentifierValue::Create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitMaskSize: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer = property_id == CSSPropertyWebkitMaskSize
                                        ? &style.MaskLayers()
                                        : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next())
        list->Append(*ValueForFillSize(curr_layer->Size(), style));
      return list;
    }
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyWebkitMaskRepeat: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer = property_id == CSSPropertyWebkitMaskRepeat
                                        ? &style.MaskLayers()
                                        : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next())
        list->Append(
            *ValueForFillRepeat(curr_layer->RepeatX(), curr_layer->RepeatY()));
      return list;
    }
    case CSSPropertyMaskSourceType: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      for (const FillLayer* curr_layer = &style.MaskLayers(); curr_layer;
           curr_layer = curr_layer->Next())
        list->Append(*ValueForFillSourceType(curr_layer->MaskSourceType()));
      return list;
    }
    case CSSPropertyWebkitMaskComposite: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer =
          property_id == CSSPropertyWebkitMaskComposite
              ? &style.MaskLayers()
              : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next())
        list->Append(*CSSIdentifierValue::Create(curr_layer->Composite()));
      return list;
    }
    case CSSPropertyBackgroundAttachment: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
           curr_layer = curr_layer->Next())
        list->Append(*CSSIdentifierValue::Create(curr_layer->Attachment()));
      return list;
    }
    case CSSPropertyBackgroundClip:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskOrigin: {
      bool is_clip = property_id == CSSPropertyBackgroundClip ||
                     property_id == CSSPropertyWebkitBackgroundClip ||
                     property_id == CSSPropertyWebkitMaskClip;
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer = (property_id == CSSPropertyWebkitMaskClip ||
                                     property_id == CSSPropertyWebkitMaskOrigin)
                                        ? &style.MaskLayers()
                                        : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next()) {
        EFillBox box = is_clip ? curr_layer->Clip() : curr_layer->Origin();
        list->Append(*CSSIdentifierValue::Create(box));
      }
      return list;
    }
    case CSSPropertyBackgroundPosition:
    case CSSPropertyWebkitMaskPosition: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer = property_id == CSSPropertyWebkitMaskPosition
                                        ? &style.MaskLayers()
                                        : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next())
        list->Append(
            *CreatePositionListForLayer(property_id, *curr_layer, style));
      return list;
    }
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyWebkitMaskPositionX: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer =
          property_id == CSSPropertyWebkitMaskPositionX
              ? &style.MaskLayers()
              : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next())
        list->Append(
            *ZoomAdjustedPixelValueForLength(curr_layer->XPosition(), style));
      return list;
    }
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyWebkitMaskPositionY: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const FillLayer* curr_layer =
          property_id == CSSPropertyWebkitMaskPositionY
              ? &style.MaskLayers()
              : &style.BackgroundLayers();
      for (; curr_layer; curr_layer = curr_layer->Next())
        list->Append(
            *ZoomAdjustedPixelValueForLength(curr_layer->YPosition(), style));
      return list;
    }
    case CSSPropertyBorderCollapse:
      if (style.BorderCollapse() == EBorderCollapse::kCollapse)
        return CSSIdentifierValue::Create(CSSValueCollapse);
      return CSSIdentifierValue::Create(CSSValueSeparate);
    case CSSPropertyBorderSpacing: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(
          *ZoomAdjustedPixelValue(style.HorizontalBorderSpacing(), style));
      list->Append(
          *ZoomAdjustedPixelValue(style.VerticalBorderSpacing(), style));
      return list;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing:
      return ZoomAdjustedPixelValue(style.HorizontalBorderSpacing(), style);
    case CSSPropertyWebkitBorderVerticalSpacing:
      return ZoomAdjustedPixelValue(style.VerticalBorderSpacing(), style);
    case CSSPropertyBorderImageSource:
      if (style.BorderImageSource())
        return style.BorderImageSource()->ComputedCSSValue();
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyBorderTopColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyBorderTopColor)
                           .Rgb())
                 : CurrentColorOrValidColor(style, style.BorderTopColor());
    case CSSPropertyBorderRightColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyBorderRightColor)
                           .Rgb())
                 : CurrentColorOrValidColor(style, style.BorderRightColor());
    case CSSPropertyBorderBottomColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyBorderBottomColor)
                           .Rgb())
                 : CurrentColorOrValidColor(style, style.BorderBottomColor());
    case CSSPropertyBorderLeftColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyBorderLeftColor)
                           .Rgb())
                 : CurrentColorOrValidColor(style, style.BorderLeftColor());
    case CSSPropertyBorderTopStyle:
      return CSSIdentifierValue::Create(style.BorderTopStyle());
    case CSSPropertyBorderRightStyle:
      return CSSIdentifierValue::Create(style.BorderRightStyle());
    case CSSPropertyBorderBottomStyle:
      return CSSIdentifierValue::Create(style.BorderBottomStyle());
    case CSSPropertyBorderLeftStyle:
      return CSSIdentifierValue::Create(style.BorderLeftStyle());
    case CSSPropertyBorderTopWidth:
      return ZoomAdjustedPixelValueWithBorderWidthRounding(
          style.BorderTopWidth(), style);
    case CSSPropertyBorderRightWidth:
      return ZoomAdjustedPixelValueWithBorderWidthRounding(
          style.BorderRightWidth(), style);
    case CSSPropertyBorderBottomWidth:
      return ZoomAdjustedPixelValueWithBorderWidthRounding(
          style.BorderBottomWidth(), style);
    case CSSPropertyBorderLeftWidth:
      return ZoomAdjustedPixelValueWithBorderWidthRounding(
          style.BorderLeftWidth(), style);
    case CSSPropertyBottom:
      return ValueForPositionOffset(style, CSSPropertyBottom, layout_object);
    case CSSPropertyWebkitBoxAlign:
      return CSSIdentifierValue::Create(style.BoxAlign());
    case CSSPropertyWebkitBoxDecorationBreak:
      if (style.BoxDecorationBreak() == EBoxDecorationBreak::kSlice)
        return CSSIdentifierValue::Create(CSSValueSlice);
      return CSSIdentifierValue::Create(CSSValueClone);
    case CSSPropertyWebkitBoxDirection:
      return CSSIdentifierValue::Create(style.BoxDirection());
    case CSSPropertyWebkitBoxFlex:
      return CSSPrimitiveValue::Create(style.BoxFlex(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyWebkitBoxFlexGroup:
      return CSSPrimitiveValue::Create(style.BoxFlexGroup(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyWebkitBoxLines:
      return CSSIdentifierValue::Create(style.BoxLines());
    case CSSPropertyWebkitBoxOrdinalGroup:
      return CSSPrimitiveValue::Create(style.BoxOrdinalGroup(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyWebkitBoxOrient:
      return CSSIdentifierValue::Create(style.BoxOrient());
    case CSSPropertyWebkitBoxPack:
      return CSSIdentifierValue::Create(style.BoxPack());
    case CSSPropertyWebkitBoxReflect:
      return ValueForReflection(style.BoxReflect(), style);
    case CSSPropertyBoxShadow:
      return ValueForShadowList(style.BoxShadow(), style, true);
    case CSSPropertyCaptionSide:
      return CSSIdentifierValue::Create(style.CaptionSide());
    case CSSPropertyCaretColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyCaretColor).Rgb())
                 : CurrentColorOrValidColor(
                       style, style.CaretColor().IsAutoColor()
                                  ? StyleColor::CurrentColor()
                                  : style.CaretColor().ToStyleColor());
    case CSSPropertyClear:
      return CSSIdentifierValue::Create(style.Clear());
    case CSSPropertyColor:
      return CSSColorValue::Create(
          allow_visited_style
              ? style.VisitedDependentColor(CSSPropertyColor).Rgb()
              : style.GetColor().Rgb());
    case CSSPropertyWebkitPrintColorAdjust:
      return CSSIdentifierValue::Create(style.PrintColorAdjust());
    case CSSPropertyColumnCount:
      if (style.HasAutoColumnCount())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSPrimitiveValue::Create(style.ColumnCount(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyColumnFill:
      return CSSIdentifierValue::Create(style.GetColumnFill());
    case CSSPropertyColumnGap:
      if (style.HasNormalColumnGap())
        return CSSIdentifierValue::Create(CSSValueNormal);
      return ZoomAdjustedPixelValue(style.ColumnGap(), style);
    case CSSPropertyColumnRuleColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyOutlineColor)
                           .Rgb())
                 : CurrentColorOrValidColor(style, style.ColumnRuleColor());
    case CSSPropertyColumnRuleStyle:
      return CSSIdentifierValue::Create(style.ColumnRuleStyle());
    case CSSPropertyColumnRuleWidth:
      return ZoomAdjustedPixelValue(style.ColumnRuleWidth(), style);
    case CSSPropertyColumnSpan:
      return CSSIdentifierValue::Create(
          static_cast<unsigned>(style.GetColumnSpan()) ? CSSValueAll
                                                       : CSSValueNone);
    case CSSPropertyWebkitColumnBreakAfter:
      return ValueForWebkitColumnBreakBetween(style.BreakAfter());
    case CSSPropertyWebkitColumnBreakBefore:
      return ValueForWebkitColumnBreakBetween(style.BreakBefore());
    case CSSPropertyWebkitColumnBreakInside:
      return ValueForWebkitColumnBreakInside(style.BreakInside());
    case CSSPropertyColumnWidth:
      if (style.HasAutoColumnWidth())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return ZoomAdjustedPixelValue(style.ColumnWidth(), style);
    case CSSPropertyTabSize:
      return CSSPrimitiveValue::Create(
          style.GetTabSize().GetPixelSize(1.0),
          style.GetTabSize().IsSpaces() ? CSSPrimitiveValue::UnitType::kNumber
                                        : CSSPrimitiveValue::UnitType::kPixels);
    case CSSPropertyTextSizeAdjust:
      if (style.GetTextSizeAdjust().IsAuto())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSPrimitiveValue::Create(
          style.GetTextSizeAdjust().Multiplier() * 100,
          CSSPrimitiveValue::UnitType::kPercentage);
    case CSSPropertyCursor: {
      CSSValueList* list = nullptr;
      CursorList* cursors = style.Cursors();
      if (cursors && cursors->size() > 0) {
        list = CSSValueList::CreateCommaSeparated();
        for (const CursorData& cursor : *cursors) {
          if (StyleImage* image = cursor.GetImage()) {
            list->Append(*CSSCursorImageValue::Create(
                *image->ComputedCSSValue(), cursor.HotSpotSpecified(),
                cursor.HotSpot()));
          }
        }
      }
      CSSValue* value = CSSIdentifierValue::Create(style.Cursor());
      if (list) {
        list->Append(*value);
        return list;
      }
      return value;
    }
    case CSSPropertyDirection:
      return CSSIdentifierValue::Create(style.Direction());
    case CSSPropertyDisplay:
      return CSSIdentifierValue::Create(style.Display());
    case CSSPropertyEmptyCells:
      return CSSIdentifierValue::Create(style.EmptyCells());
    case CSSPropertyPlaceContent: {
      // TODO (jfernandez): The spec states that we should return the specified
      // value.
      return ValuesForShorthandProperty(placeContentShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    }
    case CSSPropertyPlaceItems: {
      // TODO (jfernandez): The spec states that we should return the specified
      // value.
      return ValuesForShorthandProperty(placeItemsShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    }
    case CSSPropertyPlaceSelf: {
      // TODO (jfernandez): The spec states that we should return the specified
      // value.
      return ValuesForShorthandProperty(placeSelfShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    }
    case CSSPropertyAlignContent:
      return ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.AlignContent(), CSSValueStretch);
    case CSSPropertyAlignItems:
      return ValueForItemPositionWithOverflowAlignment(style.AlignItems());
    case CSSPropertyAlignSelf:
      return ValueForItemPositionWithOverflowAlignment(style.AlignSelf());
    case CSSPropertyFlex:
      return ValuesForShorthandProperty(flexShorthand(), style, layout_object,
                                        styled_node, allow_visited_style);
    case CSSPropertyFlexBasis:
      return ZoomAdjustedPixelValueForLength(style.FlexBasis(), style);
    case CSSPropertyFlexDirection:
      return CSSIdentifierValue::Create(style.FlexDirection());
    case CSSPropertyFlexFlow:
      return ValuesForShorthandProperty(flexFlowShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyFlexGrow:
      return CSSPrimitiveValue::Create(style.FlexGrow(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyFlexShrink:
      return CSSPrimitiveValue::Create(style.FlexShrink(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyFlexWrap:
      return CSSIdentifierValue::Create(style.FlexWrap());
    case CSSPropertyJustifyContent:
      return ValueForContentPositionAndDistributionWithOverflowAlignment(
          style.JustifyContent(), CSSValueFlexStart);
    case CSSPropertyOrder:
      return CSSPrimitiveValue::Create(style.Order(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyFloat:
      if (style.Display() != EDisplay::kNone && style.HasOutOfFlowPosition())
        return CSSIdentifierValue::Create(CSSValueNone);
      return CSSIdentifierValue::Create(style.Floating());
    case CSSPropertyFont:
      return ValueForFont(style);
    case CSSPropertyFontFamily:
      return ValueForFontFamily(style);
    case CSSPropertyFontSize:
      return ValueForFontSize(style);
    case CSSPropertyFontSizeAdjust:
      if (style.HasFontSizeAdjust())
        return CSSPrimitiveValue::Create(style.FontSizeAdjust(),
                                         CSSPrimitiveValue::UnitType::kNumber);
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyFontStretch:
      return ValueForFontStretch(style);
    case CSSPropertyFontStyle:
      return ValueForFontStyle(style);
    case CSSPropertyFontVariant:
      return ValuesForFontVariantProperty(style, layout_object, styled_node,
                                          allow_visited_style);
    case CSSPropertyFontWeight:
      return ValueForFontWeight(style);
    case CSSPropertyFontFeatureSettings: {
      const FontFeatureSettings* feature_settings =
          style.GetFontDescription().FeatureSettings();
      if (!feature_settings || !feature_settings->size())
        return CSSIdentifierValue::Create(CSSValueNormal);
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      for (unsigned i = 0; i < feature_settings->size(); ++i) {
        const FontFeature& feature = feature_settings->at(i);
        CSSFontFeatureValue* feature_value =
            CSSFontFeatureValue::Create(feature.Tag(), feature.Value());
        list->Append(*feature_value);
      }
      return list;
    }
    case CSSPropertyFontVariationSettings: {
      DCHECK(RuntimeEnabledFeatures::CSSVariableFontsEnabled());
      const FontVariationSettings* variation_settings =
          style.GetFontDescription().VariationSettings();
      if (!variation_settings || !variation_settings->size())
        return CSSIdentifierValue::Create(CSSValueNormal);
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      for (unsigned i = 0; i < variation_settings->size(); ++i) {
        const FontVariationAxis& variation_axis = variation_settings->at(i);
        CSSFontVariationValue* variation_value = CSSFontVariationValue::Create(
            variation_axis.Tag(), variation_axis.Value());
        list->Append(*variation_value);
      }
      return list;
    }
    case CSSPropertyGridAutoFlow: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      switch (style.GetGridAutoFlow()) {
        case kAutoFlowRow:
        case kAutoFlowRowDense:
          list->Append(*CSSIdentifierValue::Create(CSSValueRow));
          break;
        case kAutoFlowColumn:
        case kAutoFlowColumnDense:
          list->Append(*CSSIdentifierValue::Create(CSSValueColumn));
          break;
        default:
          NOTREACHED();
      }

      switch (style.GetGridAutoFlow()) {
        case kAutoFlowRowDense:
        case kAutoFlowColumnDense:
          list->Append(*CSSIdentifierValue::Create(CSSValueDense));
          break;
        default:
          // Do nothing.
          break;
      }

      return list;
    }
    // Specs mention that getComputedStyle() should return the used value of the
    // property instead of the computed one for grid-template-{rows|columns} but
    // not for the grid-auto-{rows|columns} as things like grid-auto-columns:
    // 2fr; cannot be resolved to a value in pixels as the '2fr' means very
    // different things depending on the size of the explicit grid or the number
    // of implicit tracks added to the grid. See
    // http://lists.w3.org/Archives/Public/www-style/2013Nov/0014.html
    case CSSPropertyGridAutoColumns:
      return ValueForGridTrackSizeList(kForColumns, style);
    case CSSPropertyGridAutoRows:
      return ValueForGridTrackSizeList(kForRows, style);

    case CSSPropertyGridTemplateColumns:
      return ValueForGridTrackList(kForColumns, layout_object, style);
    case CSSPropertyGridTemplateRows:
      return ValueForGridTrackList(kForRows, layout_object, style);

    case CSSPropertyGridColumnStart:
      return ValueForGridPosition(style.GridColumnStart());
    case CSSPropertyGridColumnEnd:
      return ValueForGridPosition(style.GridColumnEnd());
    case CSSPropertyGridRowStart:
      return ValueForGridPosition(style.GridRowStart());
    case CSSPropertyGridRowEnd:
      return ValueForGridPosition(style.GridRowEnd());
    case CSSPropertyGridColumn:
      return ValuesForGridShorthand(gridColumnShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridRow:
      return ValuesForGridShorthand(gridRowShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridArea:
      return ValuesForGridShorthand(gridAreaShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridTemplate:
      return ValuesForGridShorthand(gridTemplateShorthand(), style,
                                    layout_object, styled_node,
                                    allow_visited_style);
    case CSSPropertyGrid:
      return ValuesForGridShorthand(gridShorthand(), style, layout_object,
                                    styled_node, allow_visited_style);
    case CSSPropertyGridTemplateAreas:
      if (!style.NamedGridAreaRowCount()) {
        DCHECK(!style.NamedGridAreaColumnCount());
        return CSSIdentifierValue::Create(CSSValueNone);
      }

      return CSSGridTemplateAreasValue::Create(
          style.NamedGridArea(), style.NamedGridAreaRowCount(),
          style.NamedGridAreaColumnCount());
    case CSSPropertyGridColumnGap:
      return ZoomAdjustedPixelValueForLength(style.GridColumnGap(), style);
    case CSSPropertyGridRowGap:
      return ZoomAdjustedPixelValueForLength(style.GridRowGap(), style);
    case CSSPropertyGridGap:
      return ValuesForShorthandProperty(gridGapShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);

    case CSSPropertyHeight:
      if (layout_object) {
        if (!WidthOrHeightPropertyAppliesToObject(*layout_object))
          return CSSIdentifierValue::Create(CSSValueAuto);
        return ZoomAdjustedPixelValue(SizingBox(layout_object).Height(), style);
      }
      return ZoomAdjustedPixelValueForLength(style.Height(), style);
    case CSSPropertyWebkitHighlight:
      if (style.Highlight() == g_null_atom)
        return CSSIdentifierValue::Create(CSSValueNone);
      return CSSStringValue::Create(style.Highlight());
    case CSSPropertyHyphens:
      return CSSIdentifierValue::Create(style.GetHyphens());
    case CSSPropertyWebkitHyphenateCharacter:
      if (style.HyphenationString().IsNull())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSStringValue::Create(style.HyphenationString());
    case CSSPropertyImageRendering:
      return CSSIdentifierValue::Create(style.ImageRendering());
    case CSSPropertyImageOrientation:
      if (style.RespectImageOrientation() == kRespectImageOrientation)
        return CSSIdentifierValue::Create(CSSValueFromImage);
      return CSSPrimitiveValue::Create(0,
                                       CSSPrimitiveValue::UnitType::kDegrees);
    case CSSPropertyIsolation:
      return CSSIdentifierValue::Create(style.Isolation());
    case CSSPropertyJustifyItems:
      return ValueForItemPositionWithOverflowAlignment(
          style.JustifyItems().GetPosition() == kItemPositionAuto
              ? ComputedStyle::InitialDefaultAlignment()
              : style.JustifyItems());
    case CSSPropertyJustifySelf:
      return ValueForItemPositionWithOverflowAlignment(style.JustifySelf());
    case CSSPropertyLeft:
      return ValueForPositionOffset(style, CSSPropertyLeft, layout_object);
    case CSSPropertyLetterSpacing:
      if (!style.LetterSpacing())
        return CSSIdentifierValue::Create(CSSValueNormal);
      return ZoomAdjustedPixelValue(style.LetterSpacing(), style);
    case CSSPropertyWebkitLineClamp:
      if (style.LineClamp().IsNone())
        return CSSIdentifierValue::Create(CSSValueNone);
      return CSSPrimitiveValue::Create(
          style.LineClamp().Value(),
          style.LineClamp().IsPercentage()
              ? CSSPrimitiveValue::UnitType::kPercentage
              : CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyLineHeight:
      return ValueForLineHeight(style);
    case CSSPropertyLineHeightStep:
      return ZoomAdjustedPixelValue(style.LineHeightStep(), style);
    case CSSPropertyListStyleImage:
      if (style.ListStyleImage())
        return style.ListStyleImage()->ComputedCSSValue();
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyListStylePosition:
      return CSSIdentifierValue::Create(style.ListStylePosition());
    case CSSPropertyListStyleType:
      return CSSIdentifierValue::Create(style.ListStyleType());
    case CSSPropertyWebkitLocale:
      if (style.Locale().IsNull())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSStringValue::Create(style.Locale());
    case CSSPropertyMarginTop: {
      Length margin_top = style.MarginTop();
      if (margin_top.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(margin_top, style);
      return ZoomAdjustedPixelValue(ToLayoutBox(layout_object)->MarginTop(),
                                    style);
    }
    case CSSPropertyMarginRight: {
      Length margin_right = style.MarginRight();
      if (margin_right.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(margin_right, style);
      float value;
      if (margin_right.IsPercentOrCalc()) {
        // LayoutBox gives a marginRight() that is the distance between the
        // right-edge of the child box and the right-edge of the containing box,
        // when display == EDisplay::kBlock. Let's calculate the absolute value
        // of the specified margin-right % instead of relying on LayoutBox's
        // marginRight() value.
        value = MinimumValueForLength(
                    margin_right, ToLayoutBox(layout_object)
                                      ->ContainingBlockLogicalWidthForContent())
                    .ToFloat();
      } else {
        value = ToLayoutBox(layout_object)->MarginRight().ToFloat();
      }
      return ZoomAdjustedPixelValue(value, style);
    }
    case CSSPropertyMarginBottom: {
      Length margin_bottom = style.MarginBottom();
      if (margin_bottom.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(margin_bottom, style);
      return ZoomAdjustedPixelValue(ToLayoutBox(layout_object)->MarginBottom(),
                                    style);
    }
    case CSSPropertyMarginLeft: {
      Length margin_left = style.MarginLeft();
      if (margin_left.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(margin_left, style);
      return ZoomAdjustedPixelValue(ToLayoutBox(layout_object)->MarginLeft(),
                                    style);
    }
    case CSSPropertyWebkitUserModify:
      return CSSIdentifierValue::Create(style.UserModify());
    case CSSPropertyMaxHeight: {
      const Length& max_height = style.MaxHeight();
      if (max_height.IsMaxSizeNone())
        return CSSIdentifierValue::Create(CSSValueNone);
      return ZoomAdjustedPixelValueForLength(max_height, style);
    }
    case CSSPropertyMaxWidth: {
      const Length& max_width = style.MaxWidth();
      if (max_width.IsMaxSizeNone())
        return CSSIdentifierValue::Create(CSSValueNone);
      return ZoomAdjustedPixelValueForLength(max_width, style);
    }
    case CSSPropertyMinHeight:
      if (style.MinHeight().IsAuto()) {
        Node* parent = styled_node->parentNode();
        if (IsFlexOrGrid(parent ? parent->EnsureComputedStyle() : nullptr))
          return CSSIdentifierValue::Create(CSSValueAuto);
        return ZoomAdjustedPixelValue(0, style);
      }
      return ZoomAdjustedPixelValueForLength(style.MinHeight(), style);
    case CSSPropertyMinWidth:
      if (style.MinWidth().IsAuto()) {
        Node* parent = styled_node->parentNode();
        if (IsFlexOrGrid(parent ? parent->EnsureComputedStyle() : nullptr))
          return CSSIdentifierValue::Create(CSSValueAuto);
        return ZoomAdjustedPixelValue(0, style);
      }
      return ZoomAdjustedPixelValueForLength(style.MinWidth(), style);
    case CSSPropertyObjectFit:
      return CSSIdentifierValue::Create(style.GetObjectFit());
    case CSSPropertyObjectPosition:
      return CSSValuePair::Create(
          ZoomAdjustedPixelValueForLength(style.ObjectPosition().X(), style),
          ZoomAdjustedPixelValueForLength(style.ObjectPosition().Y(), style),
          CSSValuePair::kKeepIdenticalValues);
    case CSSPropertyOpacity:
      return CSSPrimitiveValue::Create(style.Opacity(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyOrphans:
      return CSSPrimitiveValue::Create(style.Orphans(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyOutlineColor:
      return allow_visited_style
                 ? CSSColorValue::Create(
                       style.VisitedDependentColor(CSSPropertyOutlineColor)
                           .Rgb())
                 : CurrentColorOrValidColor(style, style.OutlineColor());
    case CSSPropertyOutlineOffset:
      return ZoomAdjustedPixelValue(style.OutlineOffset(), style);
    case CSSPropertyOutlineStyle:
      if (style.OutlineStyleIsAuto())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSIdentifierValue::Create(style.OutlineStyle());
    case CSSPropertyOutlineWidth:
      return ZoomAdjustedPixelValue(style.OutlineWidth(), style);
    case CSSPropertyOverflow:
      if (style.OverflowX() == style.OverflowY())
        return CSSIdentifierValue::Create(style.OverflowX());
      return nullptr;
    case CSSPropertyOverflowAnchor:
      return CSSIdentifierValue::Create(style.OverflowAnchor());
    case CSSPropertyOverflowWrap:
      return CSSIdentifierValue::Create(style.OverflowWrap());
    case CSSPropertyOverflowX:
      return CSSIdentifierValue::Create(style.OverflowX());
    case CSSPropertyOverflowY:
      return CSSIdentifierValue::Create(style.OverflowY());
    case CSSPropertyPaddingTop: {
      Length padding_top = style.PaddingTop();
      if (padding_top.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(padding_top, style);
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingTop(), style);
    }
    case CSSPropertyPaddingRight: {
      Length padding_right = style.PaddingRight();
      if (padding_right.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(padding_right, style);
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingRight(), style);
    }
    case CSSPropertyPaddingBottom: {
      Length padding_bottom = style.PaddingBottom();
      if (padding_bottom.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(padding_bottom, style);
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingBottom(), style);
    }
    case CSSPropertyPaddingLeft: {
      Length padding_left = style.PaddingLeft();
      if (padding_left.IsFixed() || !layout_object || !layout_object->IsBox())
        return ZoomAdjustedPixelValueForLength(padding_left, style);
      return ZoomAdjustedPixelValue(
          ToLayoutBox(layout_object)->ComputedCSSPaddingLeft(), style);
    }
    case CSSPropertyBreakAfter:
      return CSSIdentifierValue::Create(style.BreakAfter());
    case CSSPropertyBreakBefore:
      return CSSIdentifierValue::Create(style.BreakBefore());
    case CSSPropertyBreakInside:
      return CSSIdentifierValue::Create(style.BreakInside());
    case CSSPropertyPageBreakAfter:
      return ValueForPageBreakBetween(style.BreakAfter());
    case CSSPropertyPageBreakBefore:
      return ValueForPageBreakBetween(style.BreakBefore());
    case CSSPropertyPageBreakInside:
      return ValueForPageBreakInside(style.BreakInside());
    case CSSPropertyPosition:
      return CSSIdentifierValue::Create(style.GetPosition());
    case CSSPropertyQuotes:
      if (!style.Quotes()) {
        // TODO(ramya.v): We should return the quote values that we're actually
        // using.
        return nullptr;
      }
      if (style.Quotes()->size()) {
        CSSValueList* list = CSSValueList::CreateSpaceSeparated();
        for (int i = 0; i < style.Quotes()->size(); i++) {
          list->Append(
              *CSSStringValue::Create(style.Quotes()->GetOpenQuote(i)));
          list->Append(
              *CSSStringValue::Create(style.Quotes()->GetCloseQuote(i)));
        }
        return list;
      }
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyRight:
      return ValueForPositionOffset(style, CSSPropertyRight, layout_object);
    case CSSPropertyWebkitRubyPosition:
      return CSSIdentifierValue::Create(style.GetRubyPosition());
    case CSSPropertyScrollBehavior:
      return CSSIdentifierValue::Create(style.GetScrollBehavior());
    case CSSPropertyTableLayout:
      return CSSIdentifierValue::Create(style.TableLayout());
    case CSSPropertyTextAlign:
      return CSSIdentifierValue::Create(style.GetTextAlign());
    case CSSPropertyTextAlignLast:
      return CSSIdentifierValue::Create(style.TextAlignLast());
    case CSSPropertyTextDecoration:
      if (RuntimeEnabledFeatures::CSS3TextDecorationsEnabled())
        return ValuesForShorthandProperty(textDecorationShorthand(), style,
                                          layout_object, styled_node,
                                          allow_visited_style);
    // Fall through.
    case CSSPropertyTextDecorationLine:
      return RenderTextDecorationFlagsToCSSValue(style.GetTextDecoration());
    case CSSPropertyTextDecorationSkip:
      return ValueForTextDecorationSkip(style.GetTextDecorationSkip());
    case CSSPropertyTextDecorationStyle:
      return ValueForTextDecorationStyle(style.TextDecorationStyle());
    case CSSPropertyTextDecorationColor:
      return CurrentColorOrValidColor(style, style.TextDecorationColor());
    case CSSPropertyTextJustify:
      return CSSIdentifierValue::Create(style.GetTextJustify());
    case CSSPropertyTextUnderlinePosition:
      return CSSIdentifierValue::Create(style.GetTextUnderlinePosition());
    case CSSPropertyWebkitTextDecorationsInEffect:
      return RenderTextDecorationFlagsToCSSValue(
          style.TextDecorationsInEffect());
    case CSSPropertyWebkitTextFillColor:
      return CurrentColorOrValidColor(style, style.TextFillColor());
    case CSSPropertyWebkitTextEmphasisColor:
      return CurrentColorOrValidColor(style, style.TextEmphasisColor());
    case CSSPropertyWebkitTextEmphasisPosition:
      return CSSIdentifierValue::Create(style.GetTextEmphasisPosition());
    case CSSPropertyWebkitTextEmphasisStyle:
      switch (style.GetTextEmphasisMark()) {
        case TextEmphasisMark::kNone:
          return CSSIdentifierValue::Create(CSSValueNone);
        case TextEmphasisMark::kCustom:
          return CSSStringValue::Create(style.TextEmphasisCustomMark());
        case TextEmphasisMark::kAuto:
          NOTREACHED();
        // Fall through
        case TextEmphasisMark::kDot:
        case TextEmphasisMark::kCircle:
        case TextEmphasisMark::kDoubleCircle:
        case TextEmphasisMark::kTriangle:
        case TextEmphasisMark::kSesame: {
          CSSValueList* list = CSSValueList::CreateSpaceSeparated();
          list->Append(
              *CSSIdentifierValue::Create(style.GetTextEmphasisFill()));
          list->Append(
              *CSSIdentifierValue::Create(style.GetTextEmphasisMark()));
          return list;
        }
      }
    case CSSPropertyTextIndent: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*ZoomAdjustedPixelValueForLength(style.TextIndent(), style));
      if (RuntimeEnabledFeatures::CSS3TextEnabled() &&
          (style.GetTextIndentLine() == TextIndentLine::kEachLine ||
           style.GetTextIndentType() == TextIndentType::kHanging)) {
        if (style.GetTextIndentLine() == TextIndentLine::kEachLine)
          list->Append(*CSSIdentifierValue::Create(CSSValueEachLine));
        if (style.GetTextIndentType() == TextIndentType::kHanging)
          list->Append(*CSSIdentifierValue::Create(CSSValueHanging));
      }
      return list;
    }
    case CSSPropertyTextShadow:
      return ValueForShadowList(style.TextShadow(), style, false);
    case CSSPropertyTextRendering:
      return CSSIdentifierValue::Create(
          style.GetFontDescription().TextRendering());
    case CSSPropertyTextOverflow:
      if (style.TextOverflow() != ETextOverflow::kClip)
        return CSSIdentifierValue::Create(CSSValueEllipsis);
      return CSSIdentifierValue::Create(CSSValueClip);
    case CSSPropertyWebkitTextSecurity:
      return CSSIdentifierValue::Create(style.TextSecurity());
    case CSSPropertyWebkitTextStrokeColor:
      return CurrentColorOrValidColor(style, style.TextStrokeColor());
    case CSSPropertyWebkitTextStrokeWidth:
      return ZoomAdjustedPixelValue(style.TextStrokeWidth(), style);
    case CSSPropertyTextTransform:
      return CSSIdentifierValue::Create(style.TextTransform());
    case CSSPropertyTop:
      return ValueForPositionOffset(style, CSSPropertyTop, layout_object);
    case CSSPropertyTouchAction:
      return TouchActionFlagsToCSSValue(style.GetTouchAction());
    case CSSPropertyUnicodeBidi:
      return CSSIdentifierValue::Create(style.GetUnicodeBidi());
    case CSSPropertyVerticalAlign:
      switch (style.VerticalAlign()) {
        case EVerticalAlign::kBaseline:
          return CSSIdentifierValue::Create(CSSValueBaseline);
        case EVerticalAlign::kMiddle:
          return CSSIdentifierValue::Create(CSSValueMiddle);
        case EVerticalAlign::kSub:
          return CSSIdentifierValue::Create(CSSValueSub);
        case EVerticalAlign::kSuper:
          return CSSIdentifierValue::Create(CSSValueSuper);
        case EVerticalAlign::kTextTop:
          return CSSIdentifierValue::Create(CSSValueTextTop);
        case EVerticalAlign::kTextBottom:
          return CSSIdentifierValue::Create(CSSValueTextBottom);
        case EVerticalAlign::kTop:
          return CSSIdentifierValue::Create(CSSValueTop);
        case EVerticalAlign::kBottom:
          return CSSIdentifierValue::Create(CSSValueBottom);
        case EVerticalAlign::kBaselineMiddle:
          return CSSIdentifierValue::Create(CSSValueWebkitBaselineMiddle);
        case EVerticalAlign::kLength:
          return ZoomAdjustedPixelValueForLength(style.GetVerticalAlignLength(),
                                                 style);
      }
      NOTREACHED();
      return nullptr;
    case CSSPropertyVisibility:
      return CSSIdentifierValue::Create(style.Visibility());
    case CSSPropertyWhiteSpace:
      return CSSIdentifierValue::Create(style.WhiteSpace());
    case CSSPropertyWidows:
      return CSSPrimitiveValue::Create(style.Widows(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyWidth:
      if (layout_object) {
        if (!WidthOrHeightPropertyAppliesToObject(*layout_object))
          return CSSIdentifierValue::Create(CSSValueAuto);
        return ZoomAdjustedPixelValue(SizingBox(layout_object).Width(), style);
      }
      return ZoomAdjustedPixelValueForLength(style.Width(), style);
    case CSSPropertyWillChange:
      return ValueForWillChange(style.WillChangeProperties(),
                                style.WillChangeContents(),
                                style.WillChangeScrollPosition());
    case CSSPropertyWordBreak:
      return CSSIdentifierValue::Create(style.WordBreak());
    case CSSPropertyWordSpacing:
      return ZoomAdjustedPixelValue(style.WordSpacing(), style);
    case CSSPropertyWordWrap:
      return CSSIdentifierValue::Create(style.OverflowWrap());
    case CSSPropertyLineBreak:
    case CSSPropertyWebkitLineBreak:
      return CSSIdentifierValue::Create(style.GetLineBreak());
    case CSSPropertyResize:
      return CSSIdentifierValue::Create(style.Resize());
    case CSSPropertyFontKerning:
      return CSSIdentifierValue::Create(
          style.GetFontDescription().GetKerning());
    case CSSPropertyWebkitFontSmoothing:
      return CSSIdentifierValue::Create(
          style.GetFontDescription().FontSmoothing());
    case CSSPropertyFontVariantLigatures:
      return ValueForFontVariantLigatures(style);
    case CSSPropertyFontVariantCaps:
      return ValueForFontVariantCaps(style);
    case CSSPropertyFontVariantNumeric:
      return ValueForFontVariantNumeric(style);
    case CSSPropertyZIndex:
      if (style.HasAutoZIndex() || !style.IsStackingContext())
        return CSSIdentifierValue::Create(CSSValueAuto);
      return CSSPrimitiveValue::Create(style.ZIndex(),
                                       CSSPrimitiveValue::UnitType::kInteger);
    case CSSPropertyZoom:
      return CSSPrimitiveValue::Create(style.Zoom(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyBoxSizing:
      if (style.BoxSizing() == EBoxSizing::kContentBox)
        return CSSIdentifierValue::Create(CSSValueContentBox);
      return CSSIdentifierValue::Create(CSSValueBorderBox);
    case CSSPropertyWebkitAppRegion:
      return CSSIdentifierValue::Create(style.DraggableRegionMode() ==
                                                EDraggableRegionMode::kDrag
                                            ? CSSValueDrag
                                            : CSSValueNoDrag);
    case CSSPropertyAnimationDelay:
      return ValueForAnimationDelay(style.Animations());
    case CSSPropertyAnimationDirection: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->DirectionList().size(); ++i)
          list->Append(
              *ValueForAnimationDirection(animation_data->DirectionList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueNormal));
      }
      return list;
    }
    case CSSPropertyAnimationDuration:
      return ValueForAnimationDuration(style.Animations());
    case CSSPropertyAnimationFillMode: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->FillModeList().size(); ++i)
          list->Append(
              *ValueForAnimationFillMode(animation_data->FillModeList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyAnimationIterationCount: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->IterationCountList().size(); ++i)
          list->Append(*ValueForAnimationIterationCount(
              animation_data->IterationCountList()[i]));
      } else {
        list->Append(*CSSPrimitiveValue::Create(
            CSSAnimationData::InitialIterationCount(),
            CSSPrimitiveValue::UnitType::kNumber));
      }
      return list;
    }
    case CSSPropertyAnimationName: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->NameList().size(); ++i)
          list->Append(
              *CSSCustomIdentValue::Create(animation_data->NameList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueNone));
      }
      return list;
    }
    case CSSPropertyAnimationPlayState: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        for (size_t i = 0; i < animation_data->PlayStateList().size(); ++i)
          list->Append(
              *ValueForAnimationPlayState(animation_data->PlayStateList()[i]));
      } else {
        list->Append(*CSSIdentifierValue::Create(CSSValueRunning));
      }
      return list;
    }
    case CSSPropertyAnimationTimingFunction:
      return ValueForAnimationTimingFunction(style.Animations());
    case CSSPropertyAnimation: {
      const CSSAnimationData* animation_data = style.Animations();
      if (animation_data) {
        CSSValueList* animations_list = CSSValueList::CreateCommaSeparated();
        for (size_t i = 0; i < animation_data->NameList().size(); ++i) {
          CSSValueList* list = CSSValueList::CreateSpaceSeparated();
          list->Append(
              *CSSCustomIdentValue::Create(animation_data->NameList()[i]));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(animation_data->DurationList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          list->Append(*CreateTimingFunctionValue(
              CSSTimingData::GetRepeated(animation_data->TimingFunctionList(),
                                         i)
                  .Get()));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(animation_data->DelayList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          list->Append(
              *ValueForAnimationIterationCount(CSSTimingData::GetRepeated(
                  animation_data->IterationCountList(), i)));
          list->Append(*ValueForAnimationDirection(
              CSSTimingData::GetRepeated(animation_data->DirectionList(), i)));
          list->Append(*ValueForAnimationFillMode(
              CSSTimingData::GetRepeated(animation_data->FillModeList(), i)));
          list->Append(*ValueForAnimationPlayState(
              CSSTimingData::GetRepeated(animation_data->PlayStateList(), i)));
          animations_list->Append(*list);
        }
        return animations_list;
      }

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      // animation-name default value.
      list->Append(*CSSIdentifierValue::Create(CSSValueNone));
      list->Append(
          *CSSPrimitiveValue::Create(CSSAnimationData::InitialDuration(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(*CreateTimingFunctionValue(
          CSSAnimationData::InitialTimingFunction().Get()));
      list->Append(
          *CSSPrimitiveValue::Create(CSSAnimationData::InitialDelay(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(
          *CSSPrimitiveValue::Create(CSSAnimationData::InitialIterationCount(),
                                     CSSPrimitiveValue::UnitType::kNumber));
      list->Append(
          *ValueForAnimationDirection(CSSAnimationData::InitialDirection()));
      list->Append(
          *ValueForAnimationFillMode(CSSAnimationData::InitialFillMode()));
      // Initial animation-play-state.
      list->Append(*CSSIdentifierValue::Create(CSSValueRunning));
      return list;
    }
    case CSSPropertyWebkitAppearance:
      return CSSIdentifierValue::Create(style.Appearance());
    case CSSPropertyBackfaceVisibility:
      return CSSIdentifierValue::Create(
          (style.BackfaceVisibility() == EBackfaceVisibility::kHidden)
              ? CSSValueHidden
              : CSSValueVisible);
    case CSSPropertyWebkitBorderImage:
      return ValueForNinePieceImage(style.BorderImage(), style);
    case CSSPropertyBorderImageOutset:
      return ValueForNinePieceImageQuad(style.BorderImage().Outset(), style);
    case CSSPropertyBorderImageRepeat:
      return ValueForNinePieceImageRepeat(style.BorderImage());
    case CSSPropertyBorderImageSlice:
      return ValueForNinePieceImageSlice(style.BorderImage());
    case CSSPropertyBorderImageWidth:
      return ValueForNinePieceImageQuad(style.BorderImage().BorderSlices(),
                                        style);
    case CSSPropertyWebkitMaskBoxImage:
      return ValueForNinePieceImage(style.MaskBoxImage(), style);
    case CSSPropertyWebkitMaskBoxImageOutset:
      return ValueForNinePieceImageQuad(style.MaskBoxImage().Outset(), style);
    case CSSPropertyWebkitMaskBoxImageRepeat:
      return ValueForNinePieceImageRepeat(style.MaskBoxImage());
    case CSSPropertyWebkitMaskBoxImageSlice:
      return ValueForNinePieceImageSlice(style.MaskBoxImage());
    case CSSPropertyWebkitMaskBoxImageWidth:
      return ValueForNinePieceImageQuad(style.MaskBoxImage().BorderSlices(),
                                        style);
    case CSSPropertyWebkitMaskBoxImageSource:
      if (style.MaskBoxImageSource())
        return style.MaskBoxImageSource()->ComputedCSSValue();
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyWebkitFontSizeDelta:
      // Not a real style property -- used by the editing engine -- so has no
      // computed value.
      return nullptr;
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginAfterCollapse:
      return CSSIdentifierValue::Create(style.MarginAfterCollapse());
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
      return CSSIdentifierValue::Create(style.MarginBeforeCollapse());
    case CSSPropertyPerspective:
      if (!style.HasPerspective())
        return CSSIdentifierValue::Create(CSSValueNone);
      return ZoomAdjustedPixelValue(style.Perspective(), style);
    case CSSPropertyPerspectiveOrigin: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (layout_object) {
        LayoutRect box;
        if (layout_object->IsBox())
          box = ToLayoutBox(layout_object)->BorderBoxRect();

        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOriginX(), box.Width()),
            style));
        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.PerspectiveOriginY(), box.Height()),
            style));
      } else {
        list->Append(*ZoomAdjustedPixelValueForLength(
            style.PerspectiveOriginX(), style));
        list->Append(*ZoomAdjustedPixelValueForLength(
            style.PerspectiveOriginY(), style));
      }
      return list;
    }
    case CSSPropertyWebkitRtlOrdering:
      return CSSIdentifierValue::Create(style.RtlOrdering() == EOrder::kVisual
                                            ? CSSValueVisual
                                            : CSSValueLogical);
    case CSSPropertyWebkitTapHighlightColor:
      return CurrentColorOrValidColor(style, style.TapHighlightColor());
    case CSSPropertyWebkitUserDrag:
      return CSSIdentifierValue::Create(style.UserDrag());
    case CSSPropertyUserSelect:
      return CSSIdentifierValue::Create(style.UserSelect());
    case CSSPropertyBorderBottomLeftRadius:
      return &ValueForBorderRadiusCorner(style.BorderBottomLeftRadius(), style);
    case CSSPropertyBorderBottomRightRadius:
      return &ValueForBorderRadiusCorner(style.BorderBottomRightRadius(),
                                         style);
    case CSSPropertyBorderTopLeftRadius:
      return &ValueForBorderRadiusCorner(style.BorderTopLeftRadius(), style);
    case CSSPropertyBorderTopRightRadius:
      return &ValueForBorderRadiusCorner(style.BorderTopRightRadius(), style);
    case CSSPropertyClip: {
      if (style.HasAutoClip())
        return CSSIdentifierValue::Create(CSSValueAuto);
      CSSValue* top = ZoomAdjustedPixelValueOrAuto(style.Clip().Top(), style);
      CSSValue* right =
          ZoomAdjustedPixelValueOrAuto(style.Clip().Right(), style);
      CSSValue* bottom =
          ZoomAdjustedPixelValueOrAuto(style.Clip().Bottom(), style);
      CSSValue* left = ZoomAdjustedPixelValueOrAuto(style.Clip().Left(), style);
      return CSSQuadValue::Create(top, right, bottom, left,
                                  CSSQuadValue::kSerializeAsRect);
    }
    case CSSPropertySpeak:
      return CSSIdentifierValue::Create(style.Speak());
    case CSSPropertyTransform:
      return ComputedTransform(layout_object, style);
    case CSSPropertyTransformBox:
      return CSSIdentifierValue::Create(style.TransformBox());
    case CSSPropertyTransformOrigin: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (layout_object) {
        LayoutRect box;
        if (layout_object->IsBox())
          box = ToLayoutBox(layout_object)->BorderBoxRect();

        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.TransformOriginX(), box.Width()),
            style));
        list->Append(*ZoomAdjustedPixelValue(
            MinimumValueForLength(style.TransformOriginY(), box.Height()),
            style));
        if (style.TransformOriginZ() != 0)
          list->Append(
              *ZoomAdjustedPixelValue(style.TransformOriginZ(), style));
      } else {
        list->Append(
            *ZoomAdjustedPixelValueForLength(style.TransformOriginX(), style));
        list->Append(
            *ZoomAdjustedPixelValueForLength(style.TransformOriginY(), style));
        if (style.TransformOriginZ() != 0)
          list->Append(
              *ZoomAdjustedPixelValue(style.TransformOriginZ(), style));
      }
      return list;
    }
    case CSSPropertyTransformStyle:
      return CSSIdentifierValue::Create(
          (style.TransformStyle3D() == ETransformStyle3D::kPreserve3d)
              ? CSSValuePreserve3d
              : CSSValueFlat);
    case CSSPropertyTransitionDelay:
      return ValueForAnimationDelay(style.Transitions());
    case CSSPropertyTransitionDuration:
      return ValueForAnimationDuration(style.Transitions());
    case CSSPropertyTransitionProperty:
      return ValueForTransitionProperty(style.Transitions());
    case CSSPropertyTransitionTimingFunction:
      return ValueForAnimationTimingFunction(style.Transitions());
    case CSSPropertyTransition: {
      const CSSTransitionData* transition_data = style.Transitions();
      if (transition_data) {
        CSSValueList* transitions_list = CSSValueList::CreateCommaSeparated();
        for (size_t i = 0; i < transition_data->PropertyList().size(); ++i) {
          CSSValueList* list = CSSValueList::CreateSpaceSeparated();
          list->Append(*CreateTransitionPropertyValue(
              transition_data->PropertyList()[i]));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(transition_data->DurationList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          list->Append(*CreateTimingFunctionValue(
              CSSTimingData::GetRepeated(transition_data->TimingFunctionList(),
                                         i)
                  .Get()));
          list->Append(*CSSPrimitiveValue::Create(
              CSSTimingData::GetRepeated(transition_data->DelayList(), i),
              CSSPrimitiveValue::UnitType::kSeconds));
          transitions_list->Append(*list);
        }
        return transitions_list;
      }

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      // transition-property default value.
      list->Append(*CSSIdentifierValue::Create(CSSValueAll));
      list->Append(
          *CSSPrimitiveValue::Create(CSSTransitionData::InitialDuration(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      list->Append(*CreateTimingFunctionValue(
          CSSTransitionData::InitialTimingFunction().Get()));
      list->Append(
          *CSSPrimitiveValue::Create(CSSTransitionData::InitialDelay(),
                                     CSSPrimitiveValue::UnitType::kSeconds));
      return list;
    }
    case CSSPropertyPointerEvents:
      return CSSIdentifierValue::Create(style.PointerEvents());
    case CSSPropertyWritingMode:
    case CSSPropertyWebkitWritingMode:
      return CSSIdentifierValue::Create(style.GetWritingMode());
    case CSSPropertyWebkitTextCombine:
      if (style.TextCombine() == ETextCombine::kAll)
        return CSSIdentifierValue::Create(CSSValueHorizontal);
    case CSSPropertyTextCombineUpright:
      return CSSIdentifierValue::Create(style.TextCombine());
    case CSSPropertyWebkitTextOrientation:
      if (style.GetTextOrientation() == ETextOrientation::kMixed)
        return CSSIdentifierValue::Create(CSSValueVerticalRight);
    case CSSPropertyTextOrientation:
      return CSSIdentifierValue::Create(style.GetTextOrientation());
    case CSSPropertyContent:
      return ValueForContentData(style);
    case CSSPropertyCounterIncrement:
      return ValueForCounterDirectives(style, property_id);
    case CSSPropertyCounterReset:
      return ValueForCounterDirectives(style, property_id);
    case CSSPropertyClipPath:
      if (ClipPathOperation* operation = style.ClipPath()) {
        if (operation->GetType() == ClipPathOperation::SHAPE)
          return ValueForBasicShape(
              style, ToShapeClipPathOperation(operation)->GetBasicShape());
        if (operation->GetType() == ClipPathOperation::REFERENCE) {
          return CSSURIValue::Create(
              AtomicString(ToReferenceClipPathOperation(operation)->Url()));
        }
      }
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyShapeMargin:
      return CSSValue::Create(style.ShapeMargin(), style.EffectiveZoom());
    case CSSPropertyShapeImageThreshold:
      return CSSPrimitiveValue::Create(style.ShapeImageThreshold(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyShapeOutside:
      return ValueForShape(style, style.ShapeOutside());
    case CSSPropertyFilter:
      return ValueForFilter(style, style.Filter());
    case CSSPropertyBackdropFilter:
      return ValueForFilter(style, style.BackdropFilter());
    case CSSPropertyMixBlendMode:
      return CSSIdentifierValue::Create(style.BlendMode());

    case CSSPropertyBackgroundBlendMode: {
      CSSValueList* list = CSSValueList::CreateCommaSeparated();
      for (const FillLayer* curr_layer = &style.BackgroundLayers(); curr_layer;
           curr_layer = curr_layer->Next())
        list->Append(*CSSIdentifierValue::Create(curr_layer->BlendMode()));
      return list;
    }
    case CSSPropertyBackground:
      return ValuesForBackgroundShorthand(style, layout_object, styled_node,
                                          allow_visited_style);
    case CSSPropertyBorder: {
      const CSSValue* value = Get(CSSPropertyBorderTop, style, layout_object,
                                  styled_node, allow_visited_style);
      const CSSPropertyID kProperties[] = {CSSPropertyBorderRight,
                                           CSSPropertyBorderBottom,
                                           CSSPropertyBorderLeft};
      for (size_t i = 0; i < WTF_ARRAY_LENGTH(kProperties); ++i) {
        if (!DataEquivalent(value, Get(kProperties[i], style, layout_object,
                                       styled_node, allow_visited_style)))
          return nullptr;
      }
      return value;
    }
    case CSSPropertyBorderBottom:
      return ValuesForShorthandProperty(borderBottomShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderColor:
      return ValuesForSidesShorthand(borderColorShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyBorderLeft:
      return ValuesForShorthandProperty(borderLeftShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderImage:
      return ValueForNinePieceImage(style.BorderImage(), style);
    case CSSPropertyBorderRadius:
      return ValueForBorderRadiusShorthand(style);
    case CSSPropertyBorderRight:
      return ValuesForShorthandProperty(borderRightShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderStyle:
      return ValuesForSidesShorthand(borderStyleShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyBorderTop:
      return ValuesForShorthandProperty(borderTopShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorderWidth:
      return ValuesForSidesShorthand(borderWidthShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyColumnRule:
      return ValuesForShorthandProperty(columnRuleShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyColumns:
      return ValuesForShorthandProperty(columnsShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyListStyle:
      return ValuesForShorthandProperty(listStyleShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyMargin:
      return ValuesForSidesShorthand(marginShorthand(), style, layout_object,
                                     styled_node, allow_visited_style);
    case CSSPropertyOutline:
      return ValuesForShorthandProperty(outlineShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyPadding:
      return ValuesForSidesShorthand(paddingShorthand(), style, layout_object,
                                     styled_node, allow_visited_style);
    case CSSPropertyScrollPadding:
      return ValuesForSidesShorthand(scrollPaddingShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyScrollPaddingBlock:
      return ValuesForInlineBlockShorthand(scrollPaddingBlockShorthand(), style,
                                           layout_object, styled_node,
                                           allow_visited_style);
    case CSSPropertyScrollPaddingInline:
      return ValuesForInlineBlockShorthand(scrollPaddingInlineShorthand(),
                                           style, layout_object, styled_node,
                                           allow_visited_style);
    case CSSPropertyScrollSnapMargin:
      return ValuesForSidesShorthand(scrollSnapMarginShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyScrollSnapMarginBlock:
      return ValuesForInlineBlockShorthand(scrollSnapMarginBlockShorthand(),
                                           style, layout_object, styled_node,
                                           allow_visited_style);
    case CSSPropertyScrollSnapMarginInline:
      return ValuesForInlineBlockShorthand(scrollSnapMarginInlineShorthand(),
                                           style, layout_object, styled_node,
                                           allow_visited_style);
    // Individual properties not part of the spec.
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
      return nullptr;

    case CSSPropertyOffset:
      return ValueForOffset(style, layout_object, styled_node,
                            allow_visited_style);

    case CSSPropertyOffsetAnchor:
      return ValueForPosition(style.OffsetAnchor(), style);

    case CSSPropertyOffsetPosition:
      return ValueForPosition(style.OffsetPosition(), style);

    case CSSPropertyOffsetPath:
      if (const BasicShape* style_motion_path = style.OffsetPath())
        return ValueForBasicShape(style, style_motion_path);
      return CSSIdentifierValue::Create(CSSValueNone);

    case CSSPropertyOffsetDistance:
      return ZoomAdjustedPixelValueForLength(style.OffsetDistance(), style);

    case CSSPropertyOffsetRotate: {
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (style.OffsetRotate().type == kOffsetRotationAuto)
        list->Append(*CSSIdentifierValue::Create(CSSValueAuto));
      list->Append(*CSSPrimitiveValue::Create(
          style.OffsetRotate().angle, CSSPrimitiveValue::UnitType::kDegrees));
      return list;
    }

    // Unimplemented CSS 3 properties (including CSS3 shorthand properties).
    case CSSPropertyWebkitTextEmphasis:
      return nullptr;

    // Directional properties are resolved by resolveDirectionAwareProperty()
    // before the switch.
    case CSSPropertyBlockSize:
    case CSSPropertyInlineSize:
    case CSSPropertyMaxBlockSize:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyWebkitBorderEnd:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderStart:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderAfter:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitBorderBefore:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyWebkitMaxLogicalHeight:
      NOTREACHED();
      return nullptr;

    // Unimplemented @font-face properties.
    case CSSPropertyFontDisplay:
    case CSSPropertySrc:
    case CSSPropertyUnicodeRange:
      return nullptr;

    // Other unimplemented properties.
    case CSSPropertyPage:  // for @page
    case CSSPropertySize:  // for @page
      return nullptr;

    // Unimplemented -webkit- properties.
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY:
    case CSSPropertyWebkitTextStroke:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ:
      return nullptr;

    // @viewport rule properties.
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
      return nullptr;

    // SVG properties.
    case CSSPropertyClipRule:
      return CSSIdentifierValue::Create(svg_style.ClipRule());
    case CSSPropertyFloodOpacity:
      return CSSPrimitiveValue::Create(svg_style.FloodOpacity(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyStopOpacity:
      return CSSPrimitiveValue::Create(svg_style.StopOpacity(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyColorInterpolation:
      return CSSIdentifierValue::Create(svg_style.ColorInterpolation());
    case CSSPropertyColorInterpolationFilters:
      return CSSIdentifierValue::Create(svg_style.ColorInterpolationFilters());
    case CSSPropertyFillOpacity:
      return CSSPrimitiveValue::Create(svg_style.FillOpacity(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyFillRule:
      return CSSIdentifierValue::Create(svg_style.FillRule());
    case CSSPropertyColorRendering:
      return CSSIdentifierValue::Create(svg_style.ColorRendering());
    case CSSPropertyShapeRendering:
      return CSSIdentifierValue::Create(svg_style.ShapeRendering());
    case CSSPropertyStrokeLinecap:
      return CSSIdentifierValue::Create(svg_style.CapStyle());
    case CSSPropertyStrokeLinejoin:
      return CSSIdentifierValue::Create(svg_style.JoinStyle());
    case CSSPropertyStrokeMiterlimit:
      return CSSPrimitiveValue::Create(svg_style.StrokeMiterLimit(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyStrokeOpacity:
      return CSSPrimitiveValue::Create(svg_style.StrokeOpacity(),
                                       CSSPrimitiveValue::UnitType::kNumber);
    case CSSPropertyAlignmentBaseline:
      return CSSIdentifierValue::Create(svg_style.AlignmentBaseline());
    case CSSPropertyDominantBaseline:
      return CSSIdentifierValue::Create(svg_style.DominantBaseline());
    case CSSPropertyTextAnchor:
      return CSSIdentifierValue::Create(svg_style.TextAnchor());
    case CSSPropertyMask:
      if (!svg_style.MaskerResource().IsEmpty())
        return CSSURIValue::Create(
            SerializeAsFragmentIdentifier(svg_style.MaskerResource()));
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyFloodColor:
      return CurrentColorOrValidColor(style, svg_style.FloodColor());
    case CSSPropertyLightingColor:
      return CurrentColorOrValidColor(style, svg_style.LightingColor());
    case CSSPropertyStopColor:
      return CurrentColorOrValidColor(style, svg_style.StopColor());
    case CSSPropertyFill:
      return AdjustSVGPaintForCurrentColor(
          svg_style.FillPaintType(), svg_style.FillPaintUri(),
          svg_style.FillPaintColor(), style.GetColor());
    case CSSPropertyMarkerEnd:
      if (!svg_style.MarkerEndResource().IsEmpty())
        return CSSURIValue::Create(
            SerializeAsFragmentIdentifier(svg_style.MarkerEndResource()));
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyMarkerMid:
      if (!svg_style.MarkerMidResource().IsEmpty())
        return CSSURIValue::Create(
            SerializeAsFragmentIdentifier(svg_style.MarkerMidResource()));
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyMarkerStart:
      if (!svg_style.MarkerStartResource().IsEmpty())
        return CSSURIValue::Create(
            SerializeAsFragmentIdentifier(svg_style.MarkerStartResource()));
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyStroke:
      return AdjustSVGPaintForCurrentColor(
          svg_style.StrokePaintType(), svg_style.StrokePaintUri(),
          svg_style.StrokePaintColor(), style.GetColor());
    case CSSPropertyStrokeDasharray:
      return StrokeDashArrayToCSSValueList(*svg_style.StrokeDashArray(), style);
    case CSSPropertyStrokeDashoffset:
      return ZoomAdjustedPixelValueForLength(svg_style.StrokeDashOffset(),
                                             style);
    case CSSPropertyStrokeWidth:
      return PixelValueForUnzoomedLength(svg_style.StrokeWidth(), style);
    case CSSPropertyBaselineShift: {
      switch (svg_style.BaselineShift()) {
        case BS_SUPER:
          return CSSIdentifierValue::Create(CSSValueSuper);
        case BS_SUB:
          return CSSIdentifierValue::Create(CSSValueSub);
        case BS_LENGTH:
          return ZoomAdjustedPixelValueForLength(svg_style.BaselineShiftValue(),
                                                 style);
      }
      NOTREACHED();
      return nullptr;
    }
    case CSSPropertyBufferedRendering:
      return CSSIdentifierValue::Create(svg_style.BufferedRendering());
    case CSSPropertyPaintOrder:
      return PaintOrderToCSSValueList(svg_style);
    case CSSPropertyVectorEffect:
      return CSSIdentifierValue::Create(svg_style.VectorEffect());
    case CSSPropertyMaskType:
      return CSSIdentifierValue::Create(svg_style.MaskType());
    case CSSPropertyMarker:
      // the above properties are not yet implemented in the engine
      return nullptr;
    case CSSPropertyD:
      if (const StylePath* style_path = svg_style.D())
        return style_path->ComputedCSSValue();
      return CSSIdentifierValue::Create(CSSValueNone);
    case CSSPropertyCx:
      return ZoomAdjustedPixelValueForLength(svg_style.Cx(), style);
    case CSSPropertyCy:
      return ZoomAdjustedPixelValueForLength(svg_style.Cy(), style);
    case CSSPropertyX:
      return ZoomAdjustedPixelValueForLength(svg_style.X(), style);
    case CSSPropertyY:
      return ZoomAdjustedPixelValueForLength(svg_style.Y(), style);
    case CSSPropertyR:
      return ZoomAdjustedPixelValueForLength(svg_style.R(), style);
    case CSSPropertyRx:
      return ZoomAdjustedPixelValueForLength(svg_style.Rx(), style);
    case CSSPropertyRy:
      return ZoomAdjustedPixelValueForLength(svg_style.Ry(), style);
    case CSSPropertyScrollSnapType:
      return ValueForScrollSnapType(style.GetScrollSnapType(), style);
    case CSSPropertyScrollSnapAlign:
      return ValueForScrollSnapAlign(style.GetScrollSnapAlign(), style);
    case CSSPropertyScrollSnapStop:
      return CSSIdentifierValue::Create(style.ScrollSnapStop());
    case CSSPropertyScrollPaddingTop:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingTop(), style);
    case CSSPropertyScrollPaddingRight:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingRight(), style);
    case CSSPropertyScrollPaddingBottom:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingBottom(),
                                             style);
    case CSSPropertyScrollPaddingLeft:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingLeft(), style);
    case CSSPropertyScrollPaddingBlockStart:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingBlockStart(),
                                             style);
    case CSSPropertyScrollPaddingBlockEnd:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingBlockEnd(),
                                             style);
    case CSSPropertyScrollPaddingInlineStart:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingInlineStart(),
                                             style);
    case CSSPropertyScrollPaddingInlineEnd:
      return ZoomAdjustedPixelValueForLength(style.ScrollPaddingInlineEnd(),
                                             style);
    case CSSPropertyScrollSnapMarginTop:
      return ZoomAdjustedPixelValueForLength(style.ScrollSnapMarginTop(),
                                             style);
    case CSSPropertyScrollSnapMarginRight:
      return ZoomAdjustedPixelValueForLength(style.ScrollSnapMarginRight(),
                                             style);
    case CSSPropertyScrollSnapMarginBottom:
      return ZoomAdjustedPixelValueForLength(style.ScrollSnapMarginBottom(),
                                             style);
    case CSSPropertyScrollSnapMarginLeft:
      return ZoomAdjustedPixelValueForLength(style.ScrollSnapMarginLeft(),
                                             style);
    case CSSPropertyScrollSnapMarginBlockStart:
      return ZoomAdjustedPixelValueForLength(style.ScrollSnapMarginBlockStart(),
                                             style);
    case CSSPropertyScrollSnapMarginBlockEnd:
      return ZoomAdjustedPixelValueForLength(style.ScrollSnapMarginBlockEnd(),
                                             style);
    case CSSPropertyScrollSnapMarginInlineStart:
      return ZoomAdjustedPixelValueForLength(
          style.ScrollSnapMarginInlineStart(), style);
    case CSSPropertyScrollSnapMarginInlineEnd:
      return ZoomAdjustedPixelValueForLength(style.ScrollSnapMarginInlineEnd(),
                                             style);
    case CSSPropertyScrollBoundaryBehavior:
      if (style.ScrollBoundaryBehaviorX() == style.ScrollBoundaryBehaviorY())
        return CSSIdentifierValue::Create(style.ScrollBoundaryBehaviorX());
      return nullptr;
    case CSSPropertyScrollBoundaryBehaviorX:
      return CSSIdentifierValue::Create(style.ScrollBoundaryBehaviorX());
    case CSSPropertyScrollBoundaryBehaviorY:
      return CSSIdentifierValue::Create(style.ScrollBoundaryBehaviorY());
    case CSSPropertyTranslate: {
      if (!style.Translate())
        return CSSIdentifierValue::Create(CSSValueNone);

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (layout_object && layout_object->IsBox()) {
        LayoutRect box = ToLayoutBox(layout_object)->BorderBoxRect();
        list->Append(*ZoomAdjustedPixelValue(
            FloatValueForLength(style.Translate()->X(), box.Width().ToFloat()),
            style));

        if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0)
          list->Append(*ZoomAdjustedPixelValue(
              FloatValueForLength(style.Translate()->Y(),
                                  box.Height().ToFloat()),
              style));

      } else {
        // No box to resolve the percentage values
        list->Append(
            *ZoomAdjustedPixelValueForLength(style.Translate()->X(), style));

        if (!style.Translate()->Y().IsZero() || style.Translate()->Z() != 0)
          list->Append(
              *ZoomAdjustedPixelValueForLength(style.Translate()->Y(), style));
      }

      if (style.Translate()->Z() != 0)
        list->Append(*ZoomAdjustedPixelValue(style.Translate()->Z(), style));

      return list;
    }
    case CSSPropertyRotate: {
      if (!style.Rotate())
        return CSSIdentifierValue::Create(CSSValueNone);

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (style.Rotate()->X() != 0 || style.Rotate()->Y() != 0 ||
          style.Rotate()->Z() != 1) {
        list->Append(*CSSPrimitiveValue::Create(
            style.Rotate()->X(), CSSPrimitiveValue::UnitType::kNumber));
        list->Append(*CSSPrimitiveValue::Create(
            style.Rotate()->Y(), CSSPrimitiveValue::UnitType::kNumber));
        list->Append(*CSSPrimitiveValue::Create(
            style.Rotate()->Z(), CSSPrimitiveValue::UnitType::kNumber));
      }
      list->Append(*CSSPrimitiveValue::Create(
          style.Rotate()->Angle(), CSSPrimitiveValue::UnitType::kDegrees));
      return list;
    }
    case CSSPropertyScale: {
      if (!style.Scale())
        return CSSIdentifierValue::Create(CSSValueNone);
      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      list->Append(*CSSPrimitiveValue::Create(
          style.Scale()->X(), CSSPrimitiveValue::UnitType::kNumber));
      if (style.Scale()->Y() == 1 && style.Scale()->Z() == 1)
        return list;
      list->Append(*CSSPrimitiveValue::Create(
          style.Scale()->Y(), CSSPrimitiveValue::UnitType::kNumber));
      if (style.Scale()->Z() != 1)
        list->Append(*CSSPrimitiveValue::Create(
            style.Scale()->Z(), CSSPrimitiveValue::UnitType::kNumber));
      return list;
    }
    case CSSPropertyContain: {
      if (!style.Contain())
        return CSSIdentifierValue::Create(CSSValueNone);
      if (style.Contain() == kContainsStrict)
        return CSSIdentifierValue::Create(CSSValueStrict);
      if (style.Contain() == kContainsContent)
        return CSSIdentifierValue::Create(CSSValueContent);

      CSSValueList* list = CSSValueList::CreateSpaceSeparated();
      if (style.ContainsStyle())
        list->Append(*CSSIdentifierValue::Create(CSSValueStyle));
      if (style.Contain() & kContainsLayout)
        list->Append(*CSSIdentifierValue::Create(CSSValueLayout));
      if (style.ContainsPaint())
        list->Append(*CSSIdentifierValue::Create(CSSValuePaint));
      if (style.ContainsSize())
        list->Append(*CSSIdentifierValue::Create(CSSValueSize));
      DCHECK(list->length());
      return list;
    }
    case CSSPropertyVariable:
      // Variables are retrieved via get(AtomicString).
      NOTREACHED();
      return nullptr;
    case CSSPropertyAll:
      return nullptr;
    default:
      break;
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
