// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaymentRequestTestBase_h
#define PaymentRequestTestBase_h

#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "platform/heap/Persistent.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DummyPageHolder;
class ScriptState;

// A base fixture for PaymentRequest tests.
class PaymentRequestTestBase : public testing::Test {
public:
    PaymentRequestTestBase();
    ~PaymentRequestTestBase() override;

    ScriptState* getScriptState();
    ExceptionState& getExceptionState();

    void setSecurityOrigin(const String&);

    v8::Local<v8::Function> expectCall();
    v8::Local<v8::Function> expectNoCall();
    void firePromiseCallbacks();

private:
    class MockFunction : public ScriptFunction {
    public:
        explicit MockFunction(ScriptState*);
        v8::Local<v8::Function> bind();
        MOCK_METHOD1(call, ScriptValue(ScriptValue));
    };

    V8TestingScope m_scope;
    Vector<Persistent<MockFunction>> m_mockFunctions;
};

} // namespace blink

#endif // PaymentRequestTestBase_h
