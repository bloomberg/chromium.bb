// Copyright 2016 the chromium authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/cssom/StylePropertyMap.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/css/CSSValueList.h"
#include "core/css/cssom/CSSStyleValue.h"
#include "core/css/cssom/StyleValueFactory.h"

namespace blink {

void StylePropertyMap::set(const String& property_name,
                           CSSStyleValueOrCSSStyleValueSequenceOrString& item,
                           ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id != CSSPropertyInvalid && property_id != CSSPropertyVariable) {
    set(property_id, item, exception_state);
    return;
  }
  // TODO(meade): Handle custom properties here.
  exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
}

void StylePropertyMap::append(
    const String& property_name,
    CSSStyleValueOrCSSStyleValueSequenceOrString& item,
    ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id != CSSPropertyInvalid && property_id != CSSPropertyVariable) {
    append(property_id, item, exception_state);
    return;
  }
  // TODO(meade): Handle custom properties here.
  exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
}

void StylePropertyMap::remove(const String& property_name,
                              ExceptionState& exception_state) {
  CSSPropertyID property_id = cssPropertyID(property_name);
  if (property_id != CSSPropertyInvalid && property_id != CSSPropertyVariable) {
    remove(property_id, exception_state);
    return;
  }
  // TODO(meade): Handle custom properties here.
  exception_state.ThrowTypeError("Invalid propertyName: " + property_name);
}

}  // namespace blink
