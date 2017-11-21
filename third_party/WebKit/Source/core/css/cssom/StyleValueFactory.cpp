// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValueFactory.h"

#include "core/css/CSSImageValue.h"
#include "core/css/CSSValue.h"
#include "core/css/cssom/CSSKeywordValue.h"
#include "core/css/cssom/CSSNumericValue.h"
#include "core/css/cssom/CSSOMTypes.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/CSSStyleVariableReferenceValue.h"
#include "core/css/cssom/CSSTransformValue.h"
#include "core/css/cssom/CSSURLImageValue.h"
#include "core/css/cssom/CSSUnparsedValue.h"
#include "core/css/cssom/CSSUnsupportedStyleValue.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/properties/CSSProperty.h"

namespace blink {

namespace {

CSSStyleValue* CreateStyleValueWithPropertyInternal(CSSPropertyID property_id,
                                                    const CSSValue& value) {
  switch (property_id) {
    case CSSPropertyTransform:
      return CSSTransformValue::FromCSSValue(value);
    default:
      // TODO(meade): Implement other properties.
      break;
  }
  return nullptr;
}

CSSStyleValue* CreateStyleValue(const CSSValue& value) {
  if (value.IsCSSWideKeyword() || value.IsIdentifierValue() ||
      value.IsCustomIdentValue())
    return CSSKeywordValue::FromCSSValue(value);
  if (value.IsPrimitiveValue())
    return CSSNumericValue::FromCSSValue(ToCSSPrimitiveValue(value));
  if (value.IsVariableReferenceValue())
    return CSSUnparsedValue::FromCSSValue(ToCSSVariableReferenceValue(value));
  if (value.IsImageValue()) {
    return CSSURLImageValue::Create(
        ToCSSImageValue(value).ValueWithURLMadeAbsolute());
  }
  return nullptr;
}

CSSStyleValue* CreateStyleValueWithProperty(CSSPropertyID property_id,
                                            const CSSValue& value) {
  CSSStyleValue* style_value =
      CreateStyleValueWithPropertyInternal(property_id, value);
  if (style_value)
    return style_value;
  return CreateStyleValue(value);
}

CSSStyleValueVector UnsupportedCSSValue(const CSSValue& value) {
  CSSStyleValueVector style_value_vector;
  style_value_vector.push_back(
      CSSUnsupportedStyleValue::Create(value.CssText()));
  return style_value_vector;
}

}  // namespace

CSSStyleValueVector StyleValueFactory::FromString(
    CSSPropertyID property_id,
    const String& value,
    SecureContextMode secure_context_mode) {
  DCHECK_NE(property_id, CSSPropertyInvalid);
  DCHECK(!CSSProperty::Get(property_id).IsShorthand());

  // TODO(775804): Handle custom properties
  if (property_id == CSSPropertyVariable) {
    return CSSStyleValueVector();
  }

  // TODO(crbug.com/783031): This should probably use an existing parser context
  // (e.g. from execution context) to parse relative URLs correctly.
  const CSSValue* css_value = CSSParser::ParseSingleValue(
      property_id, value, StrictCSSParserContext(secure_context_mode));
  if (!css_value)
    return CSSStyleValueVector();

  CSSStyleValueVector style_value_vector =
      StyleValueFactory::CssValueToStyleValueVector(property_id, *css_value);
  DCHECK(!style_value_vector.IsEmpty());
  return style_value_vector;
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
    return UnsupportedCSSValue(css_value);
  }

  // If it's a list, we can try it as a list valued property.
  const CSSValueList& css_value_list = ToCSSValueList(css_value);
  for (const CSSValue* inner_value : css_value_list) {
    style_value = CreateStyleValueWithProperty(property_id, *inner_value);
    if (!style_value)
      return UnsupportedCSSValue(css_value);
    style_value_vector.push_back(style_value);
  }
  return style_value_vector;
}

CSSStyleValueVector StyleValueFactory::CssValueToStyleValueVector(
    const CSSValue& css_value) {
  return CssValueToStyleValueVector(CSSPropertyInvalid, css_value);
}

}  // namespace blink
