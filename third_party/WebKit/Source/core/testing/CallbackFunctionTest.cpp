// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/CallbackFunctionTest.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/v8_test_callback.h"
#include "bindings/core/v8/v8_test_enum_callback.h"
#include "bindings/core/v8/v8_test_interface_callback.h"
#include "bindings/core/v8/v8_test_receiver_object_callback.h"
#include "bindings/core/v8/v8_test_sequence_callback.h"
#include "core/html/HTMLDivElement.h"

namespace blink {

void CallbackFunctionTest::Trace(blink::Visitor* visitor) {}

String CallbackFunctionTest::testCallback(V8TestCallback* callback,
                                          const String& message1,
                                          const String& message2,
                                          ExceptionState& exception_state) {
  ScriptWrappable* script_wrappable;
  String return_value;

  if (callback->call(script_wrappable = nullptr, message1, message2,
                     return_value)) {
    return String("SUCCESS: ") + return_value;
  }
  return String("Error!");
}

String CallbackFunctionTest::testNullableCallback(
    V8TestCallback* callback,
    const String& message1,
    const String& message2,
    ExceptionState& exception_state) {
  if (!callback)
    return String("Empty callback");
  return testCallback(callback, message1, message2, exception_state);
}

void CallbackFunctionTest::testInterfaceCallback(
    V8TestInterfaceCallback* callback,
    HTMLDivElement* div_element,
    ExceptionState& exception_state) {
  ScriptWrappable* script_wrappable;

  callback->call(script_wrappable = nullptr, div_element);
  return;
}

void CallbackFunctionTest::testReceiverObjectCallback(
    V8TestReceiverObjectCallback* callback,
    ExceptionState& exception_state) {
  callback->call(this);
  return;
}

Vector<String> CallbackFunctionTest::testSequenceCallback(
    V8TestSequenceCallback* callback,
    const Vector<int>& numbers,
    ExceptionState& exception_state) {
  Vector<String> return_value;
  if (callback->call(nullptr, numbers, return_value)) {
    return return_value;
  }
  return Vector<String>();
}

void CallbackFunctionTest::testEnumCallback(V8TestEnumCallback* callback,
                                            const String& enum_value,
                                            ExceptionState& exception_state) {
  callback->call(nullptr, enum_value);
}

}  // namespace blink
