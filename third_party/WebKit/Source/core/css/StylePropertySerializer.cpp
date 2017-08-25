/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#include "core/css/StylePropertySerializer.h"

#include <bitset>
#include "core/CSSValueKeywords.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSPendingSubstitutionValue.h"
#include "core/css/CSSPropertyMetadata.h"
#include "core/css/CSSValuePool.h"
#include "core/css/properties/CSSPropertyAPI.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

StylePropertySerializer::StylePropertySetForSerializer::
    StylePropertySetForSerializer(const StylePropertySet& properties)
    : property_set_(&properties),
      all_index_(property_set_->FindPropertyIndex(CSSPropertyAll)),
      need_to_expand_all_(false) {
  if (!HasAllProperty())
    return;

  StylePropertySet::PropertyReference all_property =
      property_set_->PropertyAt(all_index_);
  for (unsigned i = 0; i < property_set_->PropertyCount(); ++i) {
    StylePropertySet::PropertyReference property = property_set_->PropertyAt(i);
    if (CSSProperty::IsAffectedByAllProperty(property.Id())) {
      if (all_property.IsImportant() && !property.IsImportant())
        continue;
      if (static_cast<unsigned>(all_index_) >= i)
        continue;
      if (property.Value() == all_property.Value() &&
          property.IsImportant() == all_property.IsImportant())
        continue;
      need_to_expand_all_ = true;
    }
    if (!isCSSPropertyIDWithName(property.Id()))
      continue;
    longhand_property_used_.set(property.Id() - firstCSSProperty);
  }
}

DEFINE_TRACE(StylePropertySerializer::StylePropertySetForSerializer) {
  visitor->Trace(property_set_);
}

unsigned StylePropertySerializer::StylePropertySetForSerializer::PropertyCount()
    const {
  if (!HasExpandedAllProperty())
    return property_set_->PropertyCount();
  return lastCSSProperty - firstCSSProperty + 1;
}

StylePropertySerializer::PropertyValueForSerializer
StylePropertySerializer::StylePropertySetForSerializer::PropertyAt(
    unsigned index) const {
  if (!HasExpandedAllProperty())
    return StylePropertySerializer::PropertyValueForSerializer(
        property_set_->PropertyAt(index));

  CSSPropertyID property_id =
      static_cast<CSSPropertyID>(index + firstCSSProperty);
  DCHECK(isCSSPropertyIDWithName(property_id));
  if (longhand_property_used_.test(index)) {
    int index = property_set_->FindPropertyIndex(property_id);
    DCHECK_NE(index, -1);
    return StylePropertySerializer::PropertyValueForSerializer(
        property_set_->PropertyAt(index));
  }

  StylePropertySet::PropertyReference property =
      property_set_->PropertyAt(all_index_);
  return StylePropertySerializer::PropertyValueForSerializer(
      property_id, &property.Value(), property.IsImportant());
}

bool StylePropertySerializer::StylePropertySetForSerializer::
    ShouldProcessPropertyAt(unsigned index) const {
  // StylePropertySet has all valid longhands. We should process.
  if (!HasAllProperty())
    return true;

  // If all is not expanded, we need to process "all" and properties which
  // are not overwritten by "all".
  if (!need_to_expand_all_) {
    StylePropertySet::PropertyReference property =
        property_set_->PropertyAt(index);
    if (property.Id() == CSSPropertyAll ||
        !CSSProperty::IsAffectedByAllProperty(property.Id()))
      return true;
    if (!isCSSPropertyIDWithName(property.Id()))
      return false;
    return longhand_property_used_.test(property.Id() - firstCSSProperty);
  }

  CSSPropertyID property_id =
      static_cast<CSSPropertyID>(index + firstCSSProperty);
  DCHECK(isCSSPropertyIDWithName(property_id));

  // Since "all" is expanded, we don't need to process "all".
  // We should not process expanded shorthands (e.g. font, background,
  // and so on) either.
  if (isShorthandProperty(property_id) || property_id == CSSPropertyAll)
    return false;

  // The all property is a shorthand that resets all CSS properties except
  // direction and unicode-bidi. It only accepts the CSS-wide keywords.
  // c.f. http://dev.w3.org/csswg/css-cascade/#all-shorthand
  if (!CSSProperty::IsAffectedByAllProperty(property_id))
    return longhand_property_used_.test(index);

  return true;
}

int StylePropertySerializer::StylePropertySetForSerializer::FindPropertyIndex(
    CSSPropertyID property_id) const {
  if (!HasExpandedAllProperty())
    return property_set_->FindPropertyIndex(property_id);
  return property_id - firstCSSProperty;
}

const CSSValue*
StylePropertySerializer::StylePropertySetForSerializer::GetPropertyCSSValue(
    CSSPropertyID property_id) const {
  int index = FindPropertyIndex(property_id);
  if (index == -1)
    return nullptr;
  StylePropertySerializer::PropertyValueForSerializer value = PropertyAt(index);
  return value.Value();
}

bool StylePropertySerializer::StylePropertySetForSerializer::
    IsDescriptorContext() const {
  return property_set_->CssParserMode() == kCSSViewportRuleMode ||
         property_set_->CssParserMode() == kCSSFontFaceRuleMode;
}

StylePropertySerializer::StylePropertySerializer(
    const StylePropertySet& properties)
    : property_set_(properties) {}

String StylePropertySerializer::GetCustomPropertyText(
    const PropertyValueForSerializer& property,
    bool is_not_first_decl) const {
  DCHECK_EQ(property.Id(), CSSPropertyVariable);
  StringBuilder result;
  if (is_not_first_decl)
    result.Append(' ');
  const CSSCustomPropertyDeclaration* value =
      ToCSSCustomPropertyDeclaration(property.Value());
  result.Append(value->GetName());
  result.Append(':');
  if (!value->Value())
    result.Append(' ');
  result.Append(value->CustomCSSText());
  if (property.IsImportant())
    result.Append(" !important");
  result.Append(';');
  return result.ToString();
}

static String GetApplyAtRuleText(const CSSValue* value,
                                 bool is_not_first_decl) {
  StringBuilder result;
  if (is_not_first_decl)
    result.Append(' ');
  result.Append("@apply ");
  result.Append(ToCSSCustomIdentValue(value)->Value());
  result.Append(';');
  return result.ToString();
}

String StylePropertySerializer::GetPropertyText(CSSPropertyID property_id,
                                                const String& value,
                                                bool is_important,
                                                bool is_not_first_decl) const {
  StringBuilder result;
  if (is_not_first_decl)
    result.Append(' ');
  result.Append(getPropertyName(property_id));
  result.Append(": ");
  result.Append(value);
  if (is_important)
    result.Append(" !important");
  result.Append(';');
  return result.ToString();
}

String StylePropertySerializer::AsText() const {
  StringBuilder result;

  std::bitset<numCSSProperties> longhand_serialized;
  std::bitset<numCSSProperties> shorthand_appeared;

  unsigned size = property_set_.PropertyCount();
  unsigned num_decls = 0;
  for (unsigned n = 0; n < size; ++n) {
    if (!property_set_.ShouldProcessPropertyAt(n))
      continue;

    StylePropertySerializer::PropertyValueForSerializer property =
        property_set_.PropertyAt(n);
    CSSPropertyID property_id = property.Id();
    // Only enabled properties should be part of the style.
    DCHECK(CSSPropertyMetadata::IsEnabledProperty(property_id));
    // All shorthand properties should have been expanded at parse time.
    DCHECK(property_set_.IsDescriptorContext() ||
           (CSSPropertyMetadata::IsProperty(property_id) &&
            !isShorthandProperty(property_id)));
    DCHECK(!property_set_.IsDescriptorContext() ||
           CSSPropertyAPI::Get(property_id).IsDescriptor());

    switch (property_id) {
      case CSSPropertyVariable:
        result.Append(GetCustomPropertyText(property, num_decls++));
        continue;
      case CSSPropertyAll:
        result.Append(GetPropertyText(property_id, property.Value()->CssText(),
                                      property.IsImportant(), num_decls++));
        continue;
      case CSSPropertyApplyAtRule:
        result.Append(GetApplyAtRuleText(property.Value(), num_decls++));
        continue;
      default:
        break;
    }
    if (longhand_serialized.test(property_id - firstCSSProperty))
      continue;

    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(property_id, &shorthands);
    bool serialized_as_shorthand = false;
    for (const StylePropertyShorthand& shorthand : shorthands) {
      // Some aliases are implemented as a shorthand, in which case
      // we prefer to not use the shorthand.
      if (shorthand.length() == 1)
        continue;

      CSSPropertyID shorthand_property = shorthand.id();
      int shorthand_property_index = shorthand_property - firstCSSProperty;
      // TODO(timloh): Do we actually need this check? A previous comment
      // said "old UAs can't recognize them but are important for editing"
      // but Firefox doesn't do this.
      if (shorthand_property == CSSPropertyFont)
        continue;
      // We already tried serializing as this shorthand
      if (shorthand_appeared.test(shorthand_property_index))
        continue;

      shorthand_appeared.set(shorthand_property_index);
      bool serialized_other_longhand = false;
      for (unsigned i = 0; i < shorthand.length(); i++) {
        if (longhand_serialized.test(shorthand.properties()[i] -
                                     firstCSSProperty)) {
          serialized_other_longhand = true;
          break;
        }
      }
      if (serialized_other_longhand)
        continue;

      String shorthand_result =
          StylePropertySerializer::GetPropertyValue(shorthand_property);
      if (shorthand_result.IsEmpty())
        continue;

      result.Append(GetPropertyText(shorthand_property, shorthand_result,
                                    property.IsImportant(), num_decls++));
      serialized_as_shorthand = true;
      for (unsigned i = 0; i < shorthand.length(); i++)
        longhand_serialized.set(shorthand.properties()[i] - firstCSSProperty);
      break;
    }

    if (serialized_as_shorthand)
      continue;

    result.Append(GetPropertyText(property_id, property.Value()->CssText(),
                                  property.IsImportant(), num_decls++));
  }

  DCHECK(!num_decls ^ !result.IsEmpty());
  return result.ToString();
}

// As per css-cascade, shorthands do not expand longhands to the value
// "initial", except when the shorthand is set to "initial", instead
// setting "missing" sub-properties to their initial values. This means
// that a shorthand can never represent a list of subproperties where
// some are "initial" and some are not, and so serialization should
// always fail in these cases (as per cssom). However we currently use
// "initial" instead of the initial values for certain shorthands, so
// these are special-cased here.
// TODO(timloh): Don't use "initial" in shorthands and remove this
// special-casing
static bool AllowInitialInShorthand(CSSPropertyID property_id) {
  switch (property_id) {
    case CSSPropertyBackground:
    case CSSPropertyBorder:
    case CSSPropertyBorderTop:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderLeft:
    case CSSPropertyOutline:
    case CSSPropertyColumnRule:
    case CSSPropertyColumns:
    case CSSPropertyFlex:
    case CSSPropertyFlexFlow:
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
    case CSSPropertyGridArea:
    case CSSPropertyGridGap:
    case CSSPropertyListStyle:
    case CSSPropertyOffset:
    case CSSPropertyTextDecoration:
    case CSSPropertyWebkitMarginCollapse:
    case CSSPropertyWebkitMask:
    case CSSPropertyWebkitTextEmphasis:
    case CSSPropertyWebkitTextStroke:
      return true;
    default:
      return false;
  }
}

String StylePropertySerializer::CommonShorthandChecks(
    const StylePropertyShorthand& shorthand) const {
  int longhand_count = shorthand.length();
  DCHECK_LE(longhand_count, 17);
  const CSSValue* longhands[17] = {};

  bool has_important = false;
  bool has_non_important = false;

  for (int i = 0; i < longhand_count; i++) {
    int index = property_set_.FindPropertyIndex(shorthand.properties()[i]);
    if (index == -1)
      return g_empty_string;
    PropertyValueForSerializer value = property_set_.PropertyAt(index);

    has_important |= value.IsImportant();
    has_non_important |= !value.IsImportant();
    longhands[i] = value.Value();
  }

  if (has_important && has_non_important)
    return g_empty_string;

  if (longhands[0]->IsCSSWideKeyword() ||
      longhands[0]->IsPendingSubstitutionValue()) {
    bool success = true;
    for (int i = 1; i < longhand_count; i++) {
      if (!DataEquivalent(longhands[i], longhands[0])) {
        // This should just return emptyString but some shorthands currently
        // allow 'initial' for their longhands.
        success = false;
        break;
      }
    }
    if (success) {
      if (longhands[0]->IsPendingSubstitutionValue())
        return ToCSSPendingSubstitutionValue(longhands[0])
            ->ShorthandValue()
            ->CssText();
      return longhands[0]->CssText();
    }
  }

  bool allow_initial = AllowInitialInShorthand(shorthand.id());
  for (int i = 0; i < longhand_count; i++) {
    const CSSValue& value = *longhands[i];
    if (!allow_initial && value.IsInitialValue())
      return g_empty_string;
    if (value.IsInheritedValue() || value.IsUnsetValue() ||
        value.IsPendingSubstitutionValue())
      return g_empty_string;
    if (value.IsVariableReferenceValue())
      return g_empty_string;
  }

  return String();
}

String StylePropertySerializer::GetPropertyValue(
    CSSPropertyID property_id) const {
  const StylePropertyShorthand& shorthand = shorthandForProperty(property_id);
  // TODO(timloh): This is weird, why do we call this with non-shorthands at
  // all?
  if (!shorthand.length())
    return String();

  String result = CommonShorthandChecks(shorthand);
  if (!result.IsNull())
    return result;

  switch (property_id) {
    case CSSPropertyAnimation:
      return GetLayeredShorthandValue(animationShorthand());
    case CSSPropertyBorderSpacing:
      return BorderSpacingValue(borderSpacingShorthand());
    case CSSPropertyBackgroundPosition:
      return GetLayeredShorthandValue(backgroundPositionShorthand());
    case CSSPropertyBackgroundRepeat:
      return BackgroundRepeatPropertyValue();
    case CSSPropertyBackground:
      return GetLayeredShorthandValue(backgroundShorthand());
    case CSSPropertyBorder:
      return BorderPropertyValue();
    case CSSPropertyBorderTop:
      return GetShorthandValue(borderTopShorthand());
    case CSSPropertyBorderRight:
      return GetShorthandValue(borderRightShorthand());
    case CSSPropertyBorderBottom:
      return GetShorthandValue(borderBottomShorthand());
    case CSSPropertyBorderLeft:
      return GetShorthandValue(borderLeftShorthand());
    case CSSPropertyOutline:
      return GetShorthandValue(outlineShorthand());
    case CSSPropertyBorderColor:
      return Get4Values(borderColorShorthand());
    case CSSPropertyBorderWidth:
      return Get4Values(borderWidthShorthand());
    case CSSPropertyBorderStyle:
      return Get4Values(borderStyleShorthand());
    case CSSPropertyColumnRule:
      return GetShorthandValue(columnRuleShorthand());
    case CSSPropertyColumns:
      return GetShorthandValue(columnsShorthand());
    case CSSPropertyFlex:
      return GetShorthandValue(flexShorthand());
    case CSSPropertyFlexFlow:
      return GetShorthandValue(flexFlowShorthand());
    case CSSPropertyGridColumn:
      return GetShorthandValue(gridColumnShorthand(), " / ");
    case CSSPropertyGridRow:
      return GetShorthandValue(gridRowShorthand(), " / ");
    case CSSPropertyGridArea:
      return GetShorthandValue(gridAreaShorthand(), " / ");
    case CSSPropertyGridGap:
      return GetShorthandValue(gridGapShorthand());
    case CSSPropertyPlaceContent:
      return GetAlignmentShorthandValue(placeContentShorthand());
    case CSSPropertyPlaceItems:
      return GetAlignmentShorthandValue(placeItemsShorthand());
    case CSSPropertyPlaceSelf:
      return GetAlignmentShorthandValue(placeSelfShorthand());
    case CSSPropertyFont:
      return FontValue();
    case CSSPropertyFontVariant:
      return FontVariantValue();
    case CSSPropertyMargin:
      return Get4Values(marginShorthand());
    case CSSPropertyOffset:
      return OffsetValue();
    case CSSPropertyWebkitMarginCollapse:
      return GetShorthandValue(webkitMarginCollapseShorthand());
    case CSSPropertyOverflow:
      return GetCommonValue(overflowShorthand());
    case CSSPropertyScrollBoundaryBehavior:
      return GetCommonValue(scrollBoundaryBehaviorShorthand());
    case CSSPropertyPadding:
      return Get4Values(paddingShorthand());
    case CSSPropertyTextDecoration:
      return GetShorthandValue(textDecorationShorthand());
    case CSSPropertyTransition:
      return GetLayeredShorthandValue(transitionShorthand());
    case CSSPropertyListStyle:
      return GetShorthandValue(listStyleShorthand());
    case CSSPropertyWebkitMaskPosition:
      return GetLayeredShorthandValue(webkitMaskPositionShorthand());
    case CSSPropertyWebkitMaskRepeat:
      return GetLayeredShorthandValue(webkitMaskRepeatShorthand());
    case CSSPropertyWebkitMask:
      return GetLayeredShorthandValue(webkitMaskShorthand());
    case CSSPropertyWebkitTextEmphasis:
      return GetShorthandValue(webkitTextEmphasisShorthand());
    case CSSPropertyWebkitTextStroke:
      return GetShorthandValue(webkitTextStrokeShorthand());
    case CSSPropertyMarker: {
      if (const CSSValue* value =
              property_set_.GetPropertyCSSValue(CSSPropertyMarkerStart))
        return value->CssText();
      return String();
    }
    case CSSPropertyBorderRadius:
      return Get4Values(borderRadiusShorthand());
    case CSSPropertyScrollPadding:
      return Get4Values(scrollPaddingShorthand());
    case CSSPropertyScrollPaddingBlock:
      return Get2Values(scrollPaddingBlockShorthand());
    case CSSPropertyScrollPaddingInline:
      return Get2Values(scrollPaddingInlineShorthand());
    case CSSPropertyScrollSnapMargin:
      return Get4Values(scrollSnapMarginShorthand());
    case CSSPropertyScrollSnapMarginBlock:
      return Get2Values(scrollSnapMarginBlockShorthand());
    case CSSPropertyScrollSnapMarginInline:
      return Get2Values(scrollSnapMarginInlineShorthand());
    default:
      return String();
  }
}

String StylePropertySerializer::BorderSpacingValue(
    const StylePropertyShorthand& shorthand) const {
  const CSSValue* horizontal_value =
      property_set_.GetPropertyCSSValue(shorthand.properties()[0]);
  const CSSValue* vertical_value =
      property_set_.GetPropertyCSSValue(shorthand.properties()[1]);

  String horizontal_value_css_text = horizontal_value->CssText();
  String vertical_value_css_text = vertical_value->CssText();
  if (horizontal_value_css_text == vertical_value_css_text)
    return horizontal_value_css_text;
  return horizontal_value_css_text + ' ' + vertical_value_css_text;
}

void StylePropertySerializer::AppendFontLonghandValueIfNotNormal(
    CSSPropertyID property_id,
    StringBuilder& result) const {
  int found_property_index = property_set_.FindPropertyIndex(property_id);
  DCHECK_NE(found_property_index, -1);

  const CSSValue* val = property_set_.PropertyAt(found_property_index).Value();
  if (val->IsIdentifierValue() &&
      ToCSSIdentifierValue(val)->GetValueID() == CSSValueNormal)
    return;

  char prefix = '\0';
  switch (property_id) {
    case CSSPropertyFontStyle:
      break;  // No prefix.
    case CSSPropertyFontFamily:
    case CSSPropertyFontStretch:
    case CSSPropertyFontVariantCaps:
    case CSSPropertyFontVariantLigatures:
    case CSSPropertyFontVariantNumeric:
    case CSSPropertyFontWeight:
      prefix = ' ';
      break;
    case CSSPropertyLineHeight:
      prefix = '/';
      break;
    default:
      NOTREACHED();
  }

  if (prefix && !result.IsEmpty())
    result.Append(prefix);

  String value;
  // In the font-variant shorthand a "none" ligatures value needs to be
  // expanded.
  if (property_id == CSSPropertyFontVariantLigatures &&
      val->IsIdentifierValue() &&
      ToCSSIdentifierValue(val)->GetValueID() == CSSValueNone) {
    value =
        "no-common-ligatures no-discretionary-ligatures "
        "no-historical-ligatures no-contextual";
  } else {
    value = property_set_.PropertyAt(found_property_index).Value()->CssText();
  }

  result.Append(value);
}

String StylePropertySerializer::FontValue() const {
  int font_size_property_index =
      property_set_.FindPropertyIndex(CSSPropertyFontSize);
  int font_family_property_index =
      property_set_.FindPropertyIndex(CSSPropertyFontFamily);
  int font_variant_caps_property_index =
      property_set_.FindPropertyIndex(CSSPropertyFontVariantCaps);
  int font_variant_ligatures_property_index =
      property_set_.FindPropertyIndex(CSSPropertyFontVariantLigatures);
  int font_variant_numeric_property_index =
      property_set_.FindPropertyIndex(CSSPropertyFontVariantNumeric);
  DCHECK_NE(font_size_property_index, -1);
  DCHECK_NE(font_family_property_index, -1);
  DCHECK_NE(font_variant_caps_property_index, -1);
  DCHECK_NE(font_variant_ligatures_property_index, -1);
  DCHECK_NE(font_variant_numeric_property_index, -1);

  PropertyValueForSerializer font_size_property =
      property_set_.PropertyAt(font_size_property_index);
  PropertyValueForSerializer font_family_property =
      property_set_.PropertyAt(font_family_property_index);
  PropertyValueForSerializer font_variant_caps_property =
      property_set_.PropertyAt(font_variant_caps_property_index);
  PropertyValueForSerializer font_variant_ligatures_property =
      property_set_.PropertyAt(font_variant_ligatures_property_index);
  PropertyValueForSerializer font_variant_numeric_property =
      property_set_.PropertyAt(font_variant_numeric_property_index);

  // Check that non-initial font-variant subproperties are not conflicting with
  // this serialization.
  const CSSValue* ligatures_value = font_variant_ligatures_property.Value();
  const CSSValue* numeric_value = font_variant_numeric_property.Value();
  if ((ligatures_value->IsIdentifierValue() &&
       ToCSSIdentifierValue(ligatures_value)->GetValueID() != CSSValueNormal) ||
      ligatures_value->IsValueList() ||
      (numeric_value->IsIdentifierValue() &&
       ToCSSIdentifierValue(numeric_value)->GetValueID() != CSSValueNormal) ||
      numeric_value->IsValueList())
    return g_empty_string;

  StringBuilder result;
  AppendFontLonghandValueIfNotNormal(CSSPropertyFontStyle, result);

  const CSSValue* val = font_variant_caps_property.Value();
  if (val->IsIdentifierValue() &&
      (ToCSSIdentifierValue(val)->GetValueID() != CSSValueSmallCaps &&
       ToCSSIdentifierValue(val)->GetValueID() != CSSValueNormal))
    return g_empty_string;
  AppendFontLonghandValueIfNotNormal(CSSPropertyFontVariantCaps, result);

  AppendFontLonghandValueIfNotNormal(CSSPropertyFontWeight, result);
  AppendFontLonghandValueIfNotNormal(CSSPropertyFontStretch, result);
  if (!result.IsEmpty())
    result.Append(' ');
  result.Append(font_size_property.Value()->CssText());
  AppendFontLonghandValueIfNotNormal(CSSPropertyLineHeight, result);
  if (!result.IsEmpty())
    result.Append(' ');
  result.Append(font_family_property.Value()->CssText());
  return result.ToString();
}

String StylePropertySerializer::FontVariantValue() const {
  StringBuilder result;

  // TODO(drott): Decide how we want to return ligature values in shorthands,
  // reduced to "none" or spelled out, filed as W3C bug:
  // https://www.w3.org/Bugs/Public/show_bug.cgi?id=29594
  AppendFontLonghandValueIfNotNormal(CSSPropertyFontVariantLigatures, result);
  AppendFontLonghandValueIfNotNormal(CSSPropertyFontVariantCaps, result);
  AppendFontLonghandValueIfNotNormal(CSSPropertyFontVariantNumeric, result);

  if (result.IsEmpty()) {
    return "normal";
  }

  return result.ToString();
}

String StylePropertySerializer::OffsetValue() const {
  StringBuilder result;
  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    const CSSValue* position =
        property_set_.GetPropertyCSSValue(CSSPropertyOffsetPosition);
    if (!position->IsInitialValue()) {
      result.Append(position->CssText());
    }
  }
  const CSSValue* path =
      property_set_.GetPropertyCSSValue(CSSPropertyOffsetPath);
  const CSSValue* distance =
      property_set_.GetPropertyCSSValue(CSSPropertyOffsetDistance);
  const CSSValue* rotate =
      property_set_.GetPropertyCSSValue(CSSPropertyOffsetRotate);
  if (!path->IsInitialValue()) {
    if (!result.IsEmpty())
      result.Append(" ");
    result.Append(path->CssText());
    if (!distance->IsInitialValue()) {
      result.Append(" ");
      result.Append(distance->CssText());
    }
    if (!rotate->IsInitialValue()) {
      result.Append(" ");
      result.Append(rotate->CssText());
    }
  } else {
    DCHECK(distance->IsInitialValue());
    DCHECK(rotate->IsInitialValue());
  }
  if (RuntimeEnabledFeatures::CSSOffsetPositionAnchorEnabled()) {
    const CSSValue* anchor =
        property_set_.GetPropertyCSSValue(CSSPropertyOffsetAnchor);
    if (!anchor->IsInitialValue()) {
      result.Append(" / ");
      result.Append(anchor->CssText());
    }
  }
  return result.ToString();
}

String StylePropertySerializer::Get2Values(
    const StylePropertyShorthand& shorthand) const {
  // Assume the properties are in the usual order start, end.
  int start_value_index =
      property_set_.FindPropertyIndex(shorthand.properties()[0]);
  int end_value_index =
      property_set_.FindPropertyIndex(shorthand.properties()[1]);

  if (start_value_index == -1 || end_value_index == -1)
    return String();

  PropertyValueForSerializer start =
      property_set_.PropertyAt(start_value_index);
  PropertyValueForSerializer end = property_set_.PropertyAt(end_value_index);

  bool show_end = !DataEquivalent(start.Value(), end.Value());

  StringBuilder result;
  result.Append(start.Value()->CssText());
  if (show_end) {
    result.Append(' ');
    result.Append(end.Value()->CssText());
  }
  return result.ToString();
}

String StylePropertySerializer::Get4Values(
    const StylePropertyShorthand& shorthand) const {
  // Assume the properties are in the usual order top, right, bottom, left.
  int top_value_index =
      property_set_.FindPropertyIndex(shorthand.properties()[0]);
  int right_value_index =
      property_set_.FindPropertyIndex(shorthand.properties()[1]);
  int bottom_value_index =
      property_set_.FindPropertyIndex(shorthand.properties()[2]);
  int left_value_index =
      property_set_.FindPropertyIndex(shorthand.properties()[3]);

  if (top_value_index == -1 || right_value_index == -1 ||
      bottom_value_index == -1 || left_value_index == -1)
    return String();

  PropertyValueForSerializer top = property_set_.PropertyAt(top_value_index);
  PropertyValueForSerializer right =
      property_set_.PropertyAt(right_value_index);
  PropertyValueForSerializer bottom =
      property_set_.PropertyAt(bottom_value_index);
  PropertyValueForSerializer left = property_set_.PropertyAt(left_value_index);

  bool show_left = !DataEquivalent(right.Value(), left.Value());
  bool show_bottom = !DataEquivalent(top.Value(), bottom.Value()) || show_left;
  bool show_right = !DataEquivalent(top.Value(), right.Value()) || show_bottom;

  StringBuilder result;
  result.Append(top.Value()->CssText());
  if (show_right) {
    result.Append(' ');
    result.Append(right.Value()->CssText());
  }
  if (show_bottom) {
    result.Append(' ');
    result.Append(bottom.Value()->CssText());
  }
  if (show_left) {
    result.Append(' ');
    result.Append(left.Value()->CssText());
  }
  return result.ToString();
}

String StylePropertySerializer::GetLayeredShorthandValue(
    const StylePropertyShorthand& shorthand) const {
  const unsigned size = shorthand.length();

  // Begin by collecting the properties into a vector.
  HeapVector<Member<const CSSValue>> values(size);
  // If the below loop succeeds, there should always be at minimum 1 layer.
  size_t num_layers = 1U;

  // TODO(timloh): Shouldn't we fail if the lists are differently sized, with
  // the exception of background-color?
  for (size_t i = 0; i < size; i++) {
    values[i] = property_set_.GetPropertyCSSValue(shorthand.properties()[i]);
    if (values[i]->IsBaseValueList()) {
      const CSSValueList* value_list = ToCSSValueList(values[i]);
      num_layers = std::max(num_layers, value_list->length());
    }
  }

  StringBuilder result;

  // Now stitch the properties together.
  for (size_t layer = 0; layer < num_layers; layer++) {
    StringBuilder layer_result;
    bool use_repeat_x_shorthand = false;
    bool use_repeat_y_shorthand = false;
    bool use_single_word_shorthand = false;
    bool found_position_xcss_property = false;
    bool found_position_ycss_property = false;

    for (unsigned property_index = 0; property_index < size; property_index++) {
      const CSSValue* value = nullptr;
      CSSPropertyID property = shorthand.properties()[property_index];

      // Get a CSSValue for this property and layer.
      if (values[property_index]->IsBaseValueList()) {
        const CSSValueList* property_values =
            ToCSSValueList(values[property_index]);
        // There might not be an item for this layer for this property.
        if (layer < property_values->length())
          value = &property_values->Item(layer);
      } else if (layer == 0 || (layer != num_layers - 1 &&
                                property == CSSPropertyBackgroundColor)) {
        // Singletons except background color belong in the 0th layer.
        // Background color belongs in the last layer.
        value = values[property_index];
      }
      // No point proceeding if there's not a value to look at.
      if (!value)
        continue;

      // Special case for background-repeat.
      if (property == CSSPropertyBackgroundRepeatX ||
          property == CSSPropertyWebkitMaskRepeatX) {
        DCHECK(shorthand.properties()[property_index + 1] ==
                   CSSPropertyBackgroundRepeatY ||
               shorthand.properties()[property_index + 1] ==
                   CSSPropertyWebkitMaskRepeatY);
        const CSSValue& y_value =
            values[property_index + 1]->IsValueList()
                ? ToCSSValueList(values[property_index + 1])->Item(layer)
                : *values[property_index + 1];

        // FIXME: At some point we need to fix this code to avoid returning an
        // invalid shorthand, since some longhand combinations are not
        // serializable into a single shorthand.
        if (!value->IsIdentifierValue() || !y_value.IsIdentifierValue())
          continue;

        CSSValueID x_id = ToCSSIdentifierValue(value)->GetValueID();
        CSSValueID y_id = ToCSSIdentifierValue(y_value).GetValueID();
        // Maybe advance propertyIndex to look at the next CSSValue in the list
        // for the checks below.
        if (x_id == y_id) {
          use_single_word_shorthand = true;
          property = shorthand.properties()[++property_index];
        } else if (x_id == CSSValueRepeat && y_id == CSSValueNoRepeat) {
          use_repeat_x_shorthand = true;
          property = shorthand.properties()[++property_index];
        } else if (x_id == CSSValueNoRepeat && y_id == CSSValueRepeat) {
          use_repeat_y_shorthand = true;
          property = shorthand.properties()[++property_index];
        }
      }

      if (!value->IsInitialValue()) {
        if (property == CSSPropertyBackgroundSize ||
            property == CSSPropertyWebkitMaskSize) {
          if (found_position_ycss_property || found_position_xcss_property)
            layer_result.Append(" / ");
          else
            layer_result.Append(" 0% 0% / ");
        } else if (!layer_result.IsEmpty()) {
          // Do this second to avoid ending up with an extra space in the output
          // if we hit the continue above.
          layer_result.Append(' ');
        }

        if (use_repeat_x_shorthand) {
          use_repeat_x_shorthand = false;
          layer_result.Append(getValueName(CSSValueRepeatX));
        } else if (use_repeat_y_shorthand) {
          use_repeat_y_shorthand = false;
          layer_result.Append(getValueName(CSSValueRepeatY));
        } else {
          if (use_single_word_shorthand)
            use_single_word_shorthand = false;
          layer_result.Append(value->CssText());
        }
        if (property == CSSPropertyBackgroundPositionX ||
            property == CSSPropertyWebkitMaskPositionX)
          found_position_xcss_property = true;
        if (property == CSSPropertyBackgroundPositionY ||
            property == CSSPropertyWebkitMaskPositionY) {
          found_position_ycss_property = true;
          // background-position is a special case. If only the first offset is
          // specified, the second one defaults to "center", not the same value.
        }
      }
    }
    if (!layer_result.IsEmpty()) {
      if (!result.IsEmpty())
        result.Append(", ");
      result.Append(layer_result);
    }
  }

  return result.ToString();
}

String StylePropertySerializer::GetShorthandValue(
    const StylePropertyShorthand& shorthand,
    String separator) const {
  StringBuilder result;
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        property_set_.GetPropertyCSSValue(shorthand.properties()[i]);
    String value_text = value->CssText();
    if (value->IsInitialValue())
      continue;
    if (!result.IsEmpty())
      result.Append(separator);
    result.Append(value_text);
  }
  return result.ToString();
}

// only returns a non-null value if all properties have the same, non-null value
String StylePropertySerializer::GetCommonValue(
    const StylePropertyShorthand& shorthand) const {
  String res;
  for (unsigned i = 0; i < shorthand.length(); ++i) {
    const CSSValue* value =
        property_set_.GetPropertyCSSValue(shorthand.properties()[i]);
    // FIXME: CSSInitialValue::CssText should generate the right value.
    String text = value->CssText();
    if (res.IsNull())
      res = text;
    else if (res != text)
      return String();
  }
  return res;
}

String StylePropertySerializer::GetAlignmentShorthandValue(
    const StylePropertyShorthand& shorthand) const {
  String value = GetCommonValue(shorthand);
  if (value.IsNull() || value.IsEmpty())
    return GetShorthandValue(shorthand);
  return value;
}

String StylePropertySerializer::BorderPropertyValue() const {
  const StylePropertyShorthand properties[3] = {
      borderWidthShorthand(), borderStyleShorthand(), borderColorShorthand()};
  StringBuilder result;
  for (size_t i = 0; i < WTF_ARRAY_LENGTH(properties); ++i) {
    String value = GetCommonValue(properties[i]);
    if (value.IsNull())
      return String();
    if (value == "initial")
      continue;
    if (!result.IsEmpty())
      result.Append(' ');
    result.Append(value);
  }
  return result.IsEmpty() ? String() : result.ToString();
}

static void AppendBackgroundRepeatValue(StringBuilder& builder,
                                        const CSSValue& repeat_xcss_value,
                                        const CSSValue& repeat_ycss_value) {
  // FIXME: Ensure initial values do not appear in CSS_VALUE_LISTS.
  DEFINE_STATIC_LOCAL(CSSIdentifierValue, initial_repeat_value,
                      (CSSIdentifierValue::Create(CSSValueRepeat)));
  const CSSIdentifierValue& repeat_x =
      repeat_xcss_value.IsInitialValue()
          ? initial_repeat_value
          : ToCSSIdentifierValue(repeat_xcss_value);
  const CSSIdentifierValue& repeat_y =
      repeat_ycss_value.IsInitialValue()
          ? initial_repeat_value
          : ToCSSIdentifierValue(repeat_ycss_value);
  CSSValueID repeat_x_value_id = repeat_x.GetValueID();
  CSSValueID repeat_y_value_id = repeat_y.GetValueID();
  if (repeat_x_value_id == repeat_y_value_id) {
    builder.Append(repeat_x.CssText());
  } else if (repeat_x_value_id == CSSValueNoRepeat &&
             repeat_y_value_id == CSSValueRepeat) {
    builder.Append("repeat-y");
  } else if (repeat_x_value_id == CSSValueRepeat &&
             repeat_y_value_id == CSSValueNoRepeat) {
    builder.Append("repeat-x");
  } else {
    builder.Append(repeat_x.CssText());
    builder.Append(' ');
    builder.Append(repeat_y.CssText());
  }
}

String StylePropertySerializer::BackgroundRepeatPropertyValue() const {
  const CSSValue& repeat_x =
      *property_set_.GetPropertyCSSValue(CSSPropertyBackgroundRepeatX);
  const CSSValue& repeat_y =
      *property_set_.GetPropertyCSSValue(CSSPropertyBackgroundRepeatY);

  const CSSValueList* repeat_x_list = 0;
  int repeat_x_length = 1;
  if (repeat_x.IsValueList()) {
    repeat_x_list = &ToCSSValueList(repeat_x);
    repeat_x_length = repeat_x_list->length();
  } else if (!repeat_x.IsIdentifierValue()) {
    return String();
  }

  const CSSValueList* repeat_y_list = 0;
  int repeat_y_length = 1;
  if (repeat_y.IsValueList()) {
    repeat_y_list = &ToCSSValueList(repeat_y);
    repeat_y_length = repeat_y_list->length();
  } else if (!repeat_y.IsIdentifierValue()) {
    return String();
  }

  size_t shorthand_length =
      lowestCommonMultiple(repeat_x_length, repeat_y_length);
  StringBuilder builder;
  for (size_t i = 0; i < shorthand_length; ++i) {
    if (i)
      builder.Append(", ");

    const CSSValue& x_value =
        repeat_x_list ? repeat_x_list->Item(i % repeat_x_list->length())
                      : repeat_x;
    const CSSValue& y_value =
        repeat_y_list ? repeat_y_list->Item(i % repeat_y_list->length())
                      : repeat_y;
    AppendBackgroundRepeatValue(builder, x_value, y_value);
  }
  return builder.ToString();
}

}  // namespace blink
