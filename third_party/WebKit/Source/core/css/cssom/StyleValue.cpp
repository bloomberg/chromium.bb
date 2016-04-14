// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StyleValue.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/ToV8.h"
#include "core/StylePropertyShorthand.h"
#include "core/css/cssom/StyleValueFactory.h"
#include "core/css/parser/CSSParser.h"

namespace blink {

ScriptValue StyleValue::parse(ScriptState* scriptState, const String& propertyName, const String& value, ExceptionState& exceptionState)
{
    if (propertyName.isEmpty()) {
        exceptionState.throwTypeError("Property name cannot be empty");
        return ScriptValue::createNull(scriptState);
    }

    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID == CSSPropertyInvalid) {
        exceptionState.throwTypeError("Invalid property name");
        return ScriptValue::createNull(scriptState);
    }
    if (isShorthandProperty(propertyID)) {
        exceptionState.throwTypeError("Parsing shorthand properties is not supported");
        return ScriptValue::createNull(scriptState);
    }

    CSSValue* cssValue = CSSParser::parseSingleValue(propertyID, value, strictCSSParserContext());
    if (!cssValue)
        return ScriptValue::createNull(scriptState);

    StyleValue* styleValue = StyleValueFactory::create(propertyID, *cssValue);
    if (!styleValue)
        return ScriptValue::createNull(scriptState);

    v8::Local<v8::Value> wrappedValue = toV8(styleValue, scriptState->context()->Global(), scriptState->isolate());
    return ScriptValue(scriptState, wrappedValue);
}

} // namespace blink
