// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/CallbackFunctionTest.h"

#include "bindings/core/v8/TestCallback.h"
#include "bindings/core/v8/TestInterfaceCallback.h"
#include "bindings/core/v8/TestReceiverObjectCallback.h"
#include "bindings/core/v8/TestSequenceCallback.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/html/HTMLDivElement.h"

namespace blink {

DEFINE_TRACE(CallbackFunctionTest) {}

String CallbackFunctionTest::testCallback(TestCallback* callback,
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
    TestCallback* callback,
    const String& message1,
    const String& message2,
    ExceptionState& exception_state) {
  if (!callback)
    return String("Empty callback");
  return testCallback(callback, message1, message2, exception_state);
}

void CallbackFunctionTest::testInterfaceCallback(
    TestInterfaceCallback* callback,
    HTMLDivElement* div_element,
    ExceptionState& exception_state) {
  ScriptWrappable* script_wrappable;

  callback->call(script_wrappable = nullptr, div_element);
  return;
}

void CallbackFunctionTest::testReceiverObjectCallback(
    TestReceiverObjectCallback* callback,
    ExceptionState& exception_state) {
  callback->call(this);
  return;
}

Vector<String> CallbackFunctionTest::testSequenceCallback(
    TestSequenceCallback* callback,
    const Vector<int>& numbers,
    ExceptionState& exception_state) {
  Vector<String> return_value;
  if (callback->call(nullptr, numbers, return_value)) {
    return return_value;
  }
  return Vector<String>();
}

}  // namespace blink
