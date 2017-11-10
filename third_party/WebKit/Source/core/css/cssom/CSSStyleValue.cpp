// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/parser/CSSParser.h"

namespace blink {

namespace {

CSSStyleValueVector ParseCSSStyleValue(const String& property_name,
                                       const String& value,
                                       ExceptionState& exception_state) {
  const CSSPropertyID property_id = cssPropertyID(property_name);

  // TODO(775804): Handle custom properties
  if (property_id == CSSPropertyInvalid || property_id == CSSPropertyVariable) {
    exception_state.ThrowTypeError("Invalid property name");
    return CSSStyleValueVector();
  }

  if (isShorthandProperty(property_id)) {
    exception_state.ThrowTypeError(
        "Parsing shorthand properties is not supported");
    return CSSStyleValueVector();
  }

  // TODO(crbug.com/783031): This should probably use an existing parser context
  // (e.g. from execution context) to parse relative URLs correctly.
  const CSSValue* css_value =
      CSSParser::ParseSingleValue(property_id, value, StrictCSSParserContext());
  if (!css_value) {
    exception_state.ThrowDOMException(
        kSyntaxError, "The value provided ('" + value +
                          "') could not be parsed as a '" + property_name +
                          "'.");
    return CSSStyleValueVector();
  }

  CSSStyleValueVector style_value_vector =
      StyleValueFactory::CssValueToStyleValueVector(property_id, *css_value);
  DCHECK(!style_value_vector.IsEmpty());
  return style_value_vector;
}

}  // namespace

CSSStyleValue* CSSStyleValue::parse(const String& property_name,
                                    const String& value,
                                    ExceptionState& exception_state) {
  CSSStyleValueVector style_value_vector =
      ParseCSSStyleValue(property_name, value, exception_state);
  if (style_value_vector.IsEmpty())
    return nullptr;

  return style_value_vector[0];
}

Nullable<CSSStyleValueVector> CSSStyleValue::parseAll(
    const String& property_name,
    const String& value,
    ExceptionState& exception_state) {
  CSSStyleValueVector style_value_vector =
      ParseCSSStyleValue(property_name, value, exception_state);
  if (style_value_vector.IsEmpty())
    return nullptr;

  return style_value_vector;
}

String CSSStyleValue::StyleValueTypeToString(StyleValueType type) {
  switch (type) {
    case StyleValueType::kNumberType:
      return "number";
    case StyleValueType::kPercentType:
      return "percent";
    case StyleValueType::kLengthType:
      return "length";
    case StyleValueType::kAngleType:
      return "angle";
    case StyleValueType::kTimeType:
      return "time";
    case StyleValueType::kFrequencyType:
      return "frequency";
    case StyleValueType::kResolutionType:
      return "resolution";
    case StyleValueType::kFlexType:
      return "flex";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace blink
