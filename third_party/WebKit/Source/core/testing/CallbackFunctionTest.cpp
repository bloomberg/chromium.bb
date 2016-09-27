// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/CallbackFunctionTest.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8TestCallback.h"
#include "bindings/core/v8/V8TestInterfaceCallback.h"
#include "bindings/core/v8/V8TestReceiverObjectCallback.h"
#include "core/html/HTMLDivElement.h"

namespace blink {

DEFINE_TRACE(CallbackFunctionTest)
{
}

String CallbackFunctionTest::testCallback(ScriptState* scriptState, V8TestCallback* callback, const String& message1, const String& message2, ExceptionState& exceptionState)
{
    ScriptWrappable* scriptWrappable;
    String returnValue;

    if (callback->call(scriptState, scriptWrappable = nullptr, message1, message2, returnValue)) {
        return String("SUCCESS: ") + returnValue;
    }
    return String("Error!");
}

void CallbackFunctionTest::testInterfaceCallback(ScriptState* scriptState, V8TestInterfaceCallback* callback, HTMLDivElement* divElement, ExceptionState& exceptionState)
{
    ScriptWrappable* scriptWrappable;

    callback->call(scriptState, scriptWrappable = nullptr, divElement);
    return;
}

void CallbackFunctionTest::testReceiverObjectCallback(ScriptState* scriptState, V8TestReceiverObjectCallback* callback, ExceptionState& exceptionState)
{
    callback->call(scriptState, this);
    return;
}

} // namespace blink
