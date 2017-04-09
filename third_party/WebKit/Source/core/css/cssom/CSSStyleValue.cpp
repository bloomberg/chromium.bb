// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/CSSStyleValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/parser/CSSParser.h"

namespace blink {

ScriptValue CSSStyleValue::parse(ScriptState* script_state,
                                 const String& property_name,
                                 const String& value,
                                 ExceptionState& exception_state) {
  if (property_name.IsEmpty()) {
    exception_state.ThrowTypeError("Property name cannot be empty");
    return ScriptValue::CreateNull(script_state);
  }

  CSSPropertyID property_id = cssPropertyID(property_name);
  // TODO(timloh): Handle custom properties
  if (property_id == CSSPropertyInvalid || property_id == CSSPropertyVariable) {
    exception_state.ThrowTypeError("Invalid property name");
    return ScriptValue::CreateNull(script_state);
  }
  if (isShorthandProperty(property_id)) {
    exception_state.ThrowTypeError(
        "Parsing shorthand properties is not supported");
    return ScriptValue::CreateNull(script_state);
  }

  const CSSValue* css_value =
      CSSParser::ParseSingleValue(property_id, value, StrictCSSParserContext());
  if (!css_value)
    return ScriptValue::CreateNull(script_state);

  CSSStyleValueVector style_value_vector =
      StyleValueFactory::CssValueToStyleValueVector(property_id, *css_value);
  if (style_value_vector.size() != 1) {
    // TODO(meade): Support returning a CSSStyleValueOrCSSStyleValueSequence
    // from this function.
    return ScriptValue::CreateNull(script_state);
  }

  v8::Local<v8::Value> wrapped_value =
      ToV8(style_value_vector[0], script_state->GetContext()->Global(),
           script_state->GetIsolate());
  return ScriptValue(script_state, wrapped_value);
}

}  // namespace blink
