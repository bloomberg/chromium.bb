/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/ScriptPromise.h"

#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/V8ThrowException.h"
#include "bindings/v8/custom/V8PromiseCustom.h"

#include <v8.h>

namespace WebCore {

ScriptPromise::ScriptPromise(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    if (value.IsEmpty())
        return;

    if (!V8PromiseCustom::isPromise(value, isolate) && !value->IsPromise()) {
        m_promise = ScriptValue(v8::Handle<v8::Value>(), isolate);
        V8ThrowException::throwTypeError("the given value is not a Promise", isolate);
        return;
    }
    m_promise = ScriptValue(value, isolate);
}

ScriptPromise ScriptPromise::then(PassOwnPtr<ScriptFunction> onFulfilled, PassOwnPtr<ScriptFunction> onRejected)
{
    if (m_promise.hasNoValue())
        return ScriptPromise();

    v8::Local<v8::Object> promise = m_promise.v8Value().As<v8::Object>();
    v8::Local<v8::Function> v8OnFulfilled = ScriptFunction::adoptByGarbageCollector(onFulfilled);
    v8::Local<v8::Function> v8OnRejected = ScriptFunction::adoptByGarbageCollector(onRejected);

    if (V8PromiseCustom::isPromise(promise, isolate()))
        return ScriptPromise(V8PromiseCustom::then(promise, v8OnFulfilled, v8OnRejected, isolate()), isolate());

    ASSERT(promise->IsPromise());
    // Return this Promise if no handlers are given.
    // In fact it is not the exact bahavior of Promise.prototype.then
    // but that is not a problem in this case.
    v8::Local<v8::Promise> resultPromise = promise.As<v8::Promise>();
    // FIXME: Use Then once it is introduced.
    if (!v8OnFulfilled.IsEmpty()) {
        resultPromise = resultPromise->Chain(v8OnFulfilled);
        if (resultPromise.IsEmpty()) {
            // v8::Promise::Chain may return an empty value, for example when
            // the stack is exhausted.
            return ScriptPromise();
        }
    }
    if (!v8OnRejected.IsEmpty())
        resultPromise = resultPromise->Catch(v8OnRejected);

    return ScriptPromise(resultPromise, isolate());
}

ScriptPromise ScriptPromise::cast(const ScriptValue& value)
{
    if (value.hasNoValue())
        return ScriptPromise();
    return cast(value.v8Value(), value.isolate());
}

ScriptPromise ScriptPromise::cast(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    if (value.IsEmpty())
        return ScriptPromise();
    if (V8PromiseCustom::isPromise(value, isolate) || value->IsPromise())
        return ScriptPromise(value, isolate);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(isolate);
    ScriptPromise promise = resolver->promise();
    resolver->resolve(value);
    return promise;
}

ScriptPromise ScriptPromise::reject(const ScriptValue& value)
{
    if (value.hasNoValue())
        return ScriptPromise();
    return reject(value.v8Value(), value.isolate());
}

ScriptPromise ScriptPromise::reject(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    if (value.IsEmpty())
        return ScriptPromise();

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(isolate);
    ScriptPromise promise = resolver->promise();
    resolver->reject(value);
    return promise;
}

ScriptPromise ScriptPromise::rejectWithError(V8ErrorType type, const String& message, v8::Isolate* isolate)
{
    return reject(V8ThrowException::createError(type, message, isolate), isolate);
}

ScriptPromise ScriptPromise::rejectWithError(v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    return reject(value, isolate);
}

ScriptPromise ScriptPromise::rejectWithTypeError(const String& message, v8::Isolate* isolate)
{
    return reject(V8ThrowException::createTypeError(message, isolate), isolate);
}

ScriptPromise ScriptPromise::rejectWithArityTypeErrorForMethod(
    const char* method, const char* type, unsigned expected, unsigned providedLeastNumMandatoryParams, v8::Isolate* isolate)
{
    String message = ExceptionMessages::failedToExecute(method, type, ExceptionMessages::notEnoughArguments(expected, providedLeastNumMandatoryParams));
    return rejectWithTypeError(message, isolate);
}

ScriptPromise ScriptPromise::rejectWithArityTypeErrorForConstructor(
    const char* type, unsigned expected, unsigned providedLeastNumMandatoryParams, v8::Isolate* isolate)
{
    String message = ExceptionMessages::failedToConstruct(type, ExceptionMessages::notEnoughArguments(expected, providedLeastNumMandatoryParams));
    return rejectWithTypeError(message, isolate);
}

ScriptPromise ScriptPromise::rejectWithArityTypeError(ExceptionState& exceptionState, unsigned expected, unsigned providedLeastNumMandatoryParams)
{
    exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(expected, providedLeastNumMandatoryParams));
    return exceptionState.reject();
}

} // namespace WebCore
