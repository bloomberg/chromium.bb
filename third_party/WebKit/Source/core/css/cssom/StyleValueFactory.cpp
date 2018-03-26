// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValueFactory.h"

#include "core/css/CSSCustomPropertyDeclaration.h"
#include "core/css/CSSIdentifierValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSValue.h"
#include "core/css/CSSVariableReferenceValue.h"
#include "core/css/cssom/CSSKeywordValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSPositionValue.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "core/css/cssom/CSSTransformValue.h"
#include "core/css/cssom/CSSURLImageValue.h"
#include "core/css/cssom/CSSUnparsedValue.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"
#include "core/css/parser/CSSPropertyParser.h"
#include "core/css/parser/CSSTokenizer.h"
#include "core/css/parser/CSSVariableParser.h"
#include "core/css/properties/css_property.h"
#include "core/style_property_shorthand.h"

namespace blink {

namespace {

CSSStyleValue* CreateStyleValue(const CSSValue& value) {
  if (value.IsIdentifierValue() || value.IsCustomIdentValue())
    return CSSKeywordValue::FromCSSValue(value);
  if (value.IsPrimitiveValue())
    return CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value));
  if (value.IsImageValue()) {
    return CSSURLImageValue::FromCSSValue(*ToCSSImageValue(value).Clone());
  }
  return nullptr;
}

CSSStyleValue* CreateStyleValueWithPropertyInternal(CSSPropertyID property_id,
                                                    const CSSValue& value) {
  // FIXME: We should enforce/document what the possible CSSValue structures
  // are for each property.
  switch (property_id) {
    case CSSPropertyCaretColor:
      // caret-color also supports 'auto'
      if (value.IsIdentifierValue() &&
          ToCSSIdentifierValue(value).GetValueID() == CSSValueAuto) {
        return CSSKeywordValue::Create("auto");
      }
      FALLTHROUGH;
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyColor:
    case CSSPropertyColumnRuleColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyTextDecorationColor:
    case CSSPropertyWebkitTextEmphasisColor:
      // Only 'currentcolor' is supported.
      if (value.IsIdentifierValue() &&
          ToCSSIdentifierValue(value).GetValueID() == CSSValueCurrentcolor) {
        return CSSKeywordValue::Create("currentcolor");
      }
      return CSSUnsupportedStyleValue::Create(property_id, value);
    case CSSPropertyTransform:
      return CSSTransformValue::FromCSSValue(value);
    case CSSPropertyOffsetAnchor:
    case CSSPropertyOffsetPosition:
      // offset-anchor and offset-position can be 'auto'
      if (value.IsIdentifierValue())
        return CreateStyleValue(value);
      FALLTHROUGH;
    case CSSPropertyObjectPosition:
      return CSSPositionValue::FromCSSValue(value);
    case CSSPropertyAlignItems: {
      // Computed align-items is a ValueList of either length 1 or 2.
      // Typed OM level 1 can't support "pairs", so we only return
      // a Typed OM object for length 1 lists.
      if (value.IsValueList()) {
        const auto& value_list = ToCSSValueList(value);
        if (value_list.length() != 1U)
          return nullptr;
        return CreateStyleValue(value_list.Item(0));
      }
      return CreateStyleValue(value);
    }
    default:
      // TODO(meade): Implement other properties.
      break;
  }
  return nullptr;
}

CSSStyleValue* CreateStyleValueWithProperty(CSSPropertyID property_id,
                                            const CSSValue& value) {
  // These cannot be overridden by individual properties.
  if (value.IsCSSWideKeyword())
    return CSSKeywordValue::FromCSSValue(value);
  if (value.IsVariableReferenceValue())
    return CSSUnparsedValue::FromCSSValue(ToCSSVariableReferenceValue(value));
  if (value.IsCustomPropertyDeclaration()) {
    return CSSUnparsedValue::FromCSSValue(
        ToCSSCustomPropertyDeclaration(value));
  }

  if (!CSSOMTypes::IsPropertySupported(property_id))
    return CSSUnsupportedStyleValue::Create(property_id, value);

  CSSStyleValue* style_value =
      CreateStyleValueWithPropertyInternal(property_id, value);
  if (style_value)
    return style_value;
  return CreateStyleValue(value);
}

CSSStyleValueVector UnsupportedCSSValue(CSSPropertyID property_id,
                                        const CSSValue& value) {
  CSSStyleValueVector style_value_vector;
  style_value_vector.push_back(
      CSSUnsupportedStyleValue::Create(property_id, value));
  return style_value_vector;
}

}  // namespace

CSSStyleValueVector StyleValueFactory::FromString(
    CSSPropertyID property_id,
    const String& css_text,
    const CSSParserContext* parser_context) {
  DCHECK_NE(property_id, CSSPropertyInvalid);
  CSSTokenizer tokenizer(css_text);
  const auto tokens = tokenizer.TokenizeToEOF();
  const CSSParserTokenRange range(tokens);

  HeapVector<CSSPropertyValue, 256> parsed_properties;
  if (property_id != CSSPropertyVariable &&
      CSSPropertyParser::ParseValue(property_id, false, range, parser_context,
                                    parsed_properties,
                                    StyleRule::RuleType::kStyle)) {
    if (parsed_properties.size() == 1) {
      const auto result = StyleValueFactory::CssValueToStyleValueVector(
          parsed_properties[0].Id(), *parsed_properties[0].Value());
      // TODO(801935): Handle list-valued properties.
      if (result.size() == 1U)
        result[0]->SetCSSText(css_text);
      return result;
    }

    // Shorthands are not yet supported.
    CSSStyleValueVector result;
    result.push_back(CSSUnsupportedStyleValue::Create(property_id, css_text));
    return result;
  }

  if ((property_id == CSSPropertyVariable && !tokens.IsEmpty()) ||
      CSSVariableParser::ContainsValidVariableReferences(range)) {
    const auto variable_data =
        CSSVariableData::Create(range, false /* is_animation_tainted */,
                                false /* needs variable resolution */);
    CSSStyleValueVector values;
    values.push_back(CSSUnparsedValue::FromCSSVariableData(*variable_data));
    return values;
  }

  return CSSStyleValueVector();
}

CSSStyleValue* StyleValueFactory::CssValueToStyleValue(
    CSSPropertyID property_id,
    const CSSValue& css_value) {
  DCHECK(!CSSProperty::Get(property_id).IsRepeated());
  CSSStyleValue* style_value =
      CreateStyleValueWithProperty(property_id, css_value);
  if (!style_value)
    return CSSUnsupportedStyleValue::Create(property_id, css_value);
  return style_value;
}

CSSStyleValueVector StyleValueFactory::CssValueToStyleValueVector(
    CSSPropertyID property_id,
    const CSSValue& css_value) {
  CSSStyleValueVector style_value_vector;

  CSSStyleValue* style_value =
      CreateStyleValueWithProperty(property_id, css_value);
  if (style_value) {
    style_value_vector.push_back(style_value);
    return style_value_vector;
  }

  if (!css_value.IsValueList()) {
    return UnsupportedCSSValue(property_id, css_value);
  }

  // If it's a list, we can try it as a list valued property.
  const CSSValueList& css_value_list = ToCSSValueList(css_value);
  for (const CSSValue* inner_value : css_value_list) {
    style_value = CreateStyleValueWithProperty(property_id, *inner_value);
    if (!style_value)
      return UnsupportedCSSValue(property_id, css_value);
    style_value_vector.push_back(style_value);
  }
  return style_value_vector;
}

CSSStyleValueVector StyleValueFactory::CssValueToStyleValueVector(
    const CSSValue& css_value) {
  return CssValueToStyleValueVector(CSSPropertyInvalid, css_value);
}

}  // namespace blink
