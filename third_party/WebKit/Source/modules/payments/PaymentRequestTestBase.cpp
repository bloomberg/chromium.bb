// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequestTestBase.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/SecurityOrigin.h"

namespace blink {

PaymentRequestTestBase::PaymentRequestTestBase()
{
    setSecurityOrigin("https://www.example.com/");
}

PaymentRequestTestBase::~PaymentRequestTestBase()
{
    firePromiseCallbacks();
    for (MockFunction* mockFunction : m_mockFunctions) {
        testing::Mock::VerifyAndClearExpectations(mockFunction);
    }
}

ScriptState* PaymentRequestTestBase::getScriptState()
{
    return m_scope.getScriptState();
}

ExceptionState& PaymentRequestTestBase::getExceptionState()
{
    return m_scope.getExceptionState();
}

void PaymentRequestTestBase::setSecurityOrigin(const String& securityOrigin)
{
    m_scope.document().updateSecurityOrigin(SecurityOrigin::create(KURL(KURL(), securityOrigin)));
}

v8::Local<v8::Function> PaymentRequestTestBase::expectCall()
{
    m_mockFunctions.append(new MockFunction(getScriptState()));
    EXPECT_CALL(*m_mockFunctions.last(), call(testing::_));
    return m_mockFunctions.last()->bind();
}

v8::Local<v8::Function> PaymentRequestTestBase::expectNoCall()
{
    m_mockFunctions.append(new MockFunction(getScriptState()));
    EXPECT_CALL(*m_mockFunctions.last(), call(testing::_)).Times(0);
    return m_mockFunctions.last()->bind();
}

void PaymentRequestTestBase::firePromiseCallbacks()
{
    v8::MicrotasksScope::PerformCheckpoint(getScriptState()->isolate());
}

PaymentRequestTestBase::MockFunction::MockFunction(ScriptState* scriptState)
    : ScriptFunction(scriptState)
{
    ON_CALL(*this, call(testing::_)).WillByDefault(testing::ReturnArg<0>());
}

v8::Local<v8::Function> PaymentRequestTestBase::MockFunction::bind()
{
    return bindToV8Function();
}

} // namespace blink
