// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/SimpleLength.h"
#include "core/css/cssom/StyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

StyleValue* StylePropertyMap::get(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID == CSSPropertyInvalid) {
        // TODO(meade): Handle custom properties here.
        exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
        return nullptr;
    }

    StyleValueVector styleVector = getAll(propertyID);
    if (styleVector.isEmpty())
        return nullptr;

    return styleVector[0];
}

StylePropertyMap::StyleValueVector StylePropertyMap::getAll(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return getAll(propertyID);

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return StyleValueVector();
}

bool StylePropertyMap::has(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return has(propertyID);

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return false;
}

void StylePropertyMap::set(const String& propertyName, StyleValueOrStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        set(propertyID, item, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::append(const String& propertyName, StyleValueOrStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        append(propertyID, item, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::remove(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid) {
        remove(propertyID, exceptionState);
        return;
    }
    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

StylePropertyMap::StyleValueVector StylePropertyMap::cssValueToStyleValueVector(CSSPropertyID propertyID, const CSSValue& cssValue)
{
    StyleValueVector styleValueVector;

    if (!cssValue.isValueList()) {
        StyleValue* styleValue = StyleValueFactory::create(propertyID, cssValue);
        if (styleValue)
            styleValueVector.append(styleValue);
        return styleValueVector;
    }

    for (CSSValue* value : *toCSSValueList(&cssValue)) {
        StyleValue* styleValue = StyleValueFactory::create(propertyID, *value);
        if (!styleValue)
            return StyleValueVector();
        styleValueVector.append(styleValue);
    }
    return styleValueVector;
}

} // namespace blink
