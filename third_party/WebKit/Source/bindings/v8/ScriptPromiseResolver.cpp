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
#include "bindings/v8/ScriptPromiseResolver.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "bindings/v8/custom/V8PromiseCustom.h"

#include <v8.h>

namespace WebCore {

ScriptPromiseResolver::ScriptPromiseResolver(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
    : m_isolate(isolate)
{
    ASSERT(RuntimeEnabledFeatures::promiseEnabled());
    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(creationContext, &promise, &resolver, isolate);
    m_promise = ScriptPromise(promise, isolate);
    m_resolver.set(isolate, resolver);
}

ScriptPromiseResolver::~ScriptPromiseResolver()
{
    // We don't call "detach" here because it requires a caller
    // to be in a v8 context.

    detachPromise();
    m_resolver.clear();
}

PassRefPtr<ScriptPromiseResolver> ScriptPromiseResolver::create(ScriptExecutionContext* context)
{
    ASSERT(v8::Context::InContext());
    ASSERT(context);
    return adoptRef(new ScriptPromiseResolver(toV8Context(context, DOMWrapperWorld::current())->Global(), toIsolate(context)));
}

PassRefPtr<ScriptPromiseResolver> ScriptPromiseResolver::create()
{
    ASSERT(v8::Context::InContext());
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    return adoptRef(new ScriptPromiseResolver(v8::Object::New(), isolate));
}

bool ScriptPromiseResolver::isPending() const
{
    ASSERT(v8::Context::InContext());
    return isPendingInternal();
}

void ScriptPromiseResolver::detach()
{
    ASSERT(v8::Context::InContext());
    detachPromise();
    reject(v8::Undefined(m_isolate));
    m_resolver.clear();
}

void ScriptPromiseResolver::fulfill(v8::Handle<v8::Value> value)
{
    ASSERT(v8::Context::InContext());
    if (!isPendingInternal())
        return;
    V8PromiseCustom::fulfillResolver(m_resolver.newLocal(m_isolate), value, V8PromiseCustom::Asynchronous, m_isolate);
    m_resolver.clear();
}

void ScriptPromiseResolver::resolve(v8::Handle<v8::Value> value)
{
    ASSERT(v8::Context::InContext());
    if (!isPendingInternal())
        return;
    V8PromiseCustom::resolveResolver(m_resolver.newLocal(m_isolate), value, V8PromiseCustom::Asynchronous, m_isolate);
    m_resolver.clear();
}

void ScriptPromiseResolver::reject(v8::Handle<v8::Value> value)
{
    ASSERT(v8::Context::InContext());
    if (!isPendingInternal())
        return;
    V8PromiseCustom::rejectResolver(m_resolver.newLocal(m_isolate), value, V8PromiseCustom::Asynchronous, m_isolate);
    m_resolver.clear();
}

void ScriptPromiseResolver::fulfill(ScriptValue value)
{
    ASSERT(v8::Context::InContext());
    fulfill(value.v8Value());
}

void ScriptPromiseResolver::resolve(ScriptValue value)
{
    ASSERT(v8::Context::InContext());
    resolve(value.v8Value());
}

void ScriptPromiseResolver::reject(ScriptValue value)
{
    ASSERT(v8::Context::InContext());
    reject(value.v8Value());
}

bool ScriptPromiseResolver::isPendingInternal() const
{
    ASSERT(v8::Context::InContext());
    if (m_resolver.isEmpty())
        return false;
    v8::Local<v8::Object> resolver = m_resolver.newLocal(m_isolate);
    if (V8PromiseCustom::isInternalDetached(resolver))
        return false;
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(resolver);
    V8PromiseCustom::PromiseState state = V8PromiseCustom::getState(internal);
    return state == V8PromiseCustom::Pending;
}

} // namespace WebCore
