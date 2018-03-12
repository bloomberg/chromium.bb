// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8ForCore.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/parser/CSSParserContext.h"
#include "core/css/properties/css_property.h"
#include "core/style_property_shorthand.h"

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
  return style_value_vector.IsEmpty() ? nullptr : style_value_vector[0];
}

CSSStyleValueVector CSSStyleValue::parseAll(
    const ExecutionContext* execution_context,
    const String& property_name,
    const String& value,
    ExceptionState& exception_state) {
  return ParseCSSStyleValue(execution_context, property_name, value,
                            exception_state);
}

String CSSStyleValue::toString() const {
  const CSSValue* result = ToCSSValue();
  DCHECK(result);
  return result->CssText();
}

}  // namespace blink
