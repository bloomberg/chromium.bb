// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/SimpleLength.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

StyleValue* StylePropertyMap::get(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return get(propertyID);

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return nullptr;
}

HeapVector<Member<StyleValue>> StylePropertyMap::getAll(const String& propertyName, ExceptionState& exceptionState)
{
    CSSPropertyID propertyID = cssPropertyID(propertyName);
    if (propertyID != CSSPropertyInvalid)
        return getAll(propertyID);

    // TODO(meade): Handle custom properties here.
    exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
    return HeapVector<Member<StyleValue>>();
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

} // namespace blink
