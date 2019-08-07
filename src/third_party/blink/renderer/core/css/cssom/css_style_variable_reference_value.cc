// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/css_style_variable_reference_value.h"

namespace blink {

CSSStyleVariableReferenceValue* CSSStyleVariableReferenceValue::Create(
    const String& variable,
    ExceptionState& exception_state) {
  return Create(variable, nullptr, exception_state);
}

CSSStyleVariableReferenceValue* CSSStyleVariableReferenceValue::Create(
    const String& variable,
    CSSUnparsedValue* fallback,
    ExceptionState& exception_state) {
  CSSStyleVariableReferenceValue* result = Create(variable, fallback);
  if (!result) {
    exception_state.ThrowTypeError("Invalid custom property name");
    return nullptr;
  }

  return result;
}

CSSStyleVariableReferenceValue* CSSStyleVariableReferenceValue::Create(
    const String& variable,
    CSSUnparsedValue* fallback) {
  if (!variable.StartsWith("--"))
    return nullptr;
  return MakeGarbageCollected<CSSStyleVariableReferenceValue>(variable,
                                                              fallback);
}

void CSSStyleVariableReferenceValue::setVariable(
    const String& value,
    ExceptionState& exception_state) {
  if (!value.StartsWith("--")) {
    exception_state.ThrowTypeError("Invalid custom property name");
    return;
  }
  variable_ = value;
}

}  // namespace blink
