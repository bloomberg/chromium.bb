// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSSimpleLength.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

void StylePropertyMap::set(const String& propertyName,
                           CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                           ExceptionState& exceptionState) {
  CSSPropertyID propertyID = cssPropertyID(propertyName);
  if (propertyID != CSSPropertyInvalid && propertyID != CSSPropertyVariable) {
    set(propertyID, item, exceptionState);
    return;
  }
  // TODO(meade): Handle custom properties here.
  exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::append(
    const String& propertyName,
    CSSStyleValueOrCSSStyleValueSequenceOrString& item,
    ExceptionState& exceptionState) {
  CSSPropertyID propertyID = cssPropertyID(propertyName);
  if (propertyID != CSSPropertyInvalid && propertyID != CSSPropertyVariable) {
    append(propertyID, item, exceptionState);
    return;
  }
  // TODO(meade): Handle custom properties here.
  exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

void StylePropertyMap::remove(const String& propertyName,
                              ExceptionState& exceptionState) {
  CSSPropertyID propertyID = cssPropertyID(propertyName);
  if (propertyID != CSSPropertyInvalid && propertyID != CSSPropertyVariable) {
    remove(propertyID, exceptionState);
    return;
  }
  // TODO(meade): Handle custom properties here.
  exceptionState.throwTypeError("Invalid propertyName: " + propertyName);
}

}  // namespace blink
