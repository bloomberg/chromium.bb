// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentRequest.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/JSONValuesForV8.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/EventTargetModulesNames.h"
#include "modules/payments/ShippingAddress.h"

namespace blink {

// static
PaymentRequest* PaymentRequest::create(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, ExceptionState& exceptionState)
{
    return new PaymentRequest(scriptState, supportedMethods, details, PaymentOptions(), ScriptValue(), exceptionState);
}

// static
PaymentRequest* PaymentRequest::create(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, const PaymentOptions& options, ExceptionState& exceptionState)
{
    return new PaymentRequest(scriptState, supportedMethods, details, options, ScriptValue(), exceptionState);
}

// static
PaymentRequest* PaymentRequest::create(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, const PaymentOptions& options, const ScriptValue& data, ExceptionState& exceptionState)
{
    return new PaymentRequest(scriptState, supportedMethods, details, options, data, exceptionState);
}

PaymentRequest::~PaymentRequest()
{
}

ScriptPromise PaymentRequest::show(ScriptState* scriptState)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Not implemented."));
}

void PaymentRequest::abort()
{
}

const AtomicString& PaymentRequest::interfaceName() const
{
    return EventTargetNames::PaymentRequest;
}

ExecutionContext* PaymentRequest::getExecutionContext() const
{
    return m_scriptState->getExecutionContext();
}

DEFINE_TRACE(PaymentRequest)
{
    visitor->trace(m_details);
    visitor->trace(m_options);
    visitor->trace(m_shippingAddress);
    RefCountedGarbageCollectedEventTargetWithInlineData<PaymentRequest>::trace(visitor);
}

PaymentRequest::PaymentRequest(ScriptState* scriptState, const Vector<String>& supportedMethods, const PaymentDetails& details, const PaymentOptions& options, const ScriptValue& data, ExceptionState& exceptionState)
    : m_scriptState(scriptState)
    , m_supportedMethods(supportedMethods)
    , m_details(details)
    , m_options(options)
{
    if (!data.isEmpty()) {
        RefPtr<JSONValue> value = toJSONValue(data.context(), data.v8Value());
        if (value && value->getType() == JSONValue::TypeObject)
            m_stringifiedData = JSONObject::cast(value)->toJSONString();
    }
}

} // namespace blink
