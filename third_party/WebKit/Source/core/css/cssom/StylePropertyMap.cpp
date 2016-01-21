// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/cssom/SimpleLength.h"
#include "core/css/cssom/StyleValue.h"

namespace blink {

StyleValue* StylePropertyMap::get(const String& propertyName)
{
    return get(cssPropertyID(propertyName));
}

HeapVector<Member<StyleValue>> StylePropertyMap::getAll(const String& propertyName)
{
    return getAll(cssPropertyID(propertyName));
}

bool StylePropertyMap::has(const String& propertyName)
{
    return has(cssPropertyID(propertyName));
}

void StylePropertyMap::set(const String& propertyName, StyleValueOrStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    set(cssPropertyID(propertyName), item, exceptionState);
}

void StylePropertyMap::append(const String& propertyName, StyleValueOrStyleValueSequenceOrString& item, ExceptionState& exceptionState)
{
    append(cssPropertyID(propertyName), item, exceptionState);
}

void StylePropertyMap::remove(const String& propertyName, ExceptionState& exceptionState)
{
    remove(cssPropertyID(propertyName), exceptionState);
}

} // namespace blink
