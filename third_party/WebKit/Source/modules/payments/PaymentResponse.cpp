// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/PaymentResponse.h"

#include "bindings/core/v8/JSONValuesForV8.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

PaymentResponse::PaymentResponse()
{
}

PaymentResponse::~PaymentResponse()
{
}

ScriptValue PaymentResponse::details(ScriptState* scriptState, ExceptionState& exceptionState) const
{
    return ScriptValue(scriptState, fromJSONString(scriptState, m_stringifiedDetails, exceptionState));
}

ScriptPromise PaymentResponse::complete(ScriptState* scriptState, bool success)
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Not implemented."));
}

} // namespace blink
