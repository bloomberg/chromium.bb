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

#include "base/macros.h"
#include "core/StylePropertyShorthand.h"
#include "core/animation/css/CSSAnimationData.h"
#include "core/animation/css/CSSTransitionData.h"
#include "core/css/BasicShapeFunctions.h"
#include "core/css/CSSBasicShapeValues.h"
#include "core/css/CSSColorValue.h"
#include "core/css/CSSCounterValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSCustomIdentValue.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSFontFamilyValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFontStyleRangeValue.h"
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
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStringValue.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSURIValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePair.h"
#include "core/css/PropertyRegistry.h"
#include "core/css/ZoomAdjustedPixelValue.h"
#include "core/css/properties/ComputedStyleUtils.h"
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

static CSSValueList* ValuesForGridShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSlashSeparated();
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        *shorthand.properties()[i], style, layout_object, styled_node,
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
        *shorthand.properties()[i], style, layout_object, styled_node,
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
  StylePropertyShorthand shorthand = fontVariantShorthand();
  VariantShorthandCases shorthand_case = kAllNormal;
  for (size_t i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value = ComputedStyleCSSValueMapping::Get(
        *shorthand.properties()[i], style, layout_object, styled_node,
        allow_visited_style);

    if (shorthand_case == kAllNormal && value->IsIdentifierValue() &&
        ToCSSIdentifierValue(value)->GetValueID() == CSSValueNone &&
        shorthand.properties()[i]->IDEquals(CSSPropertyFontVariantLigatures)) {
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
      for (size_t i = 0; i < shorthand.length(); ++i) {
        const CSSValue* value = ComputedStyleCSSValueMapping::Get(
            *shorthand.properties()[i], style, layout_object, styled_node,
            allow_visited_style);
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

static CSSValueList* ValuesForSidesShorthand(
    const StylePropertyShorthand& shorthand,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  CSSValueList* list = CSSValueList::CreateSpaceSeparated();
  // Assume the properties are in the usual order top, right, bottom, left.
  const CSSValue* top_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[0], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* right_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[1], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* bottom_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[2], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* left_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[3], style, layout_object, styled_node,
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
      *shorthand.properties()[0], style, layout_object, styled_node,
      allow_visited_style);
  const CSSValue* end_value = ComputedStyleCSSValueMapping::Get(
      *shorthand.properties()[1], style, layout_object, styled_node,
      allow_visited_style);
  // Both properties must be specified.
  if (!start_value || !end_value)
    return nullptr;

  CSSValuePair* pair = CSSValuePair::Create(start_value, end_value,
                                            CSSValuePair::kDropIdenticalValues);
  return pair;
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

std::unique_ptr<HashMap<AtomicString, scoped_refptr<CSSVariableData>>>
ComputedStyleCSSValueMapping::GetVariables(const ComputedStyle& style) {
  // TODO(timloh): Also return non-inherited variables
  StyleInheritedVariables* variables = style.InheritedVariables();
  if (variables)
    return variables->GetVariables();
  return nullptr;
}

const CSSValue* ComputedStyleCSSValueMapping::Get(
    const CSSProperty& property,
    const ComputedStyle& style,
    const LayoutObject* layout_object,
    Node* styled_node,
    bool allow_visited_style) {
  const CSSProperty& resolved_property = property.ResolveDirectionAwareProperty(
      style.Direction(), style.GetWritingMode());

  switch (resolved_property.PropertyID()) {
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
    case CSSPropertyFlex:
      return ValuesForShorthandProperty(flexShorthand(), style, layout_object,
                                        styled_node, allow_visited_style);
    case CSSPropertyFlexFlow:
      return ValuesForShorthandProperty(flexFlowShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyGridGap:
      return ValuesForShorthandProperty(gridGapShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyTextDecoration:
      return ValuesForShorthandProperty(textDecorationShorthand(), style,
                                        layout_object, styled_node,
                                        allow_visited_style);
    case CSSPropertyBorder: {
      const CSSValue* value =
          Get(GetCSSPropertyBorderTop(), style, layout_object, styled_node,
              allow_visited_style);
      static const CSSProperty* kProperties[3] = {&GetCSSPropertyBorderRight(),
                                                  &GetCSSPropertyBorderBottom(),
                                                  &GetCSSPropertyBorderLeft()};
      for (size_t i = 0; i < WTF_ARRAY_LENGTH(kProperties); ++i) {
        if (!DataEquivalent(value, Get(*kProperties[i], style, layout_object,
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
    case CSSPropertyFontVariant:
      return ValuesForFontVariantProperty(style, layout_object, styled_node,
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
    case CSSPropertyScrollMargin:
      return ValuesForSidesShorthand(scrollMarginShorthand(), style,
                                     layout_object, styled_node,
                                     allow_visited_style);
    case CSSPropertyScrollMarginBlock:
      return ValuesForInlineBlockShorthand(scrollMarginBlockShorthand(), style,
                                           layout_object, styled_node,
                                           allow_visited_style);
    case CSSPropertyScrollMarginInline:
      return ValuesForInlineBlockShorthand(scrollMarginInlineShorthand(), style,
                                           layout_object, styled_node,
                                           allow_visited_style);
    default:
      return resolved_property.CSSValueFromComputedStyle(
          style, layout_object, styled_node, allow_visited_style);
  }
}

}  // namespace blink
