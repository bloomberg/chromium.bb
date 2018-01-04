// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/CSSProperty.h"

namespace blink {

namespace {

CSSStyleValueVector ParseCSSStyleValue(
    const ExecutionContext* execution_context,
    const String& property_name,
    const String& value,
    ExceptionState& exception_state) {
  const CSSPropertyID property_id = cssPropertyID(property_name);

  if (property_id == CSSPropertyInvalid) {
    exception_state.ThrowTypeError("Invalid property name");
    return CSSStyleValueVector();
  }

  if (CSSProperty::Get(property_id).IsShorthand()) {
    exception_state.ThrowTypeError(
        "Parsing shorthand properties is not supported");
    return CSSStyleValueVector();
  }

  const auto style_values = StyleValueFactory::FromString(
      property_id, value, CSSParserContext::Create(*execution_context));
  if (style_values.IsEmpty()) {
    exception_state.ThrowTypeError("The value provided ('" + value +
                                   "') could not be parsed as a '" +
                                   property_name + "'.");
    return CSSStyleValueVector();
  }

  return style_values;
}

}  // namespace

CSSStyleValue* CSSStyleValue::parse(const ExecutionContext* execution_context,
                                    const String& property_name,
                                    const String& value,
                                    ExceptionState& exception_state) {
  CSSStyleValueVector style_value_vector = ParseCSSStyleValue(
      execution_context, property_name, value, exception_state);
  if (style_value_vector.IsEmpty())
    return nullptr;

  return style_value_vector[0];
}

Nullable<CSSStyleValueVector> CSSStyleValue::parseAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    const String& value,
    ExceptionState& exception_state) {
  CSSStyleValueVector style_value_vector = ParseCSSStyleValue(
      execution_context, property_name, value, exception_state);
  if (style_value_vector.IsEmpty())
    return nullptr;

  return style_value_vector;
}

String CSSStyleValue::toString() const {
  const CSSValue* result = ToCSSValue();
  // TODO(crbug.com/782103): Remove this once all CSSStyleValues
  // support toCSSValue().
  return result ? result->CssText() : "";
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
