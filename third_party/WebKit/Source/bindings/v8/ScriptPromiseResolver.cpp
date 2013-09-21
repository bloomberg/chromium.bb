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
    , m_promiseForExposeDetached(false)
    , m_promiseForResolveDetached(false)
{
    ASSERT(RuntimeEnabledFeatures::promiseEnabled());
    v8::Local<v8::Object> promise = V8PromiseCustom::createPromise(creationContext, isolate);
    m_promise = ScriptPromise(promise, isolate);
}

ScriptPromiseResolver::~ScriptPromiseResolver()
{
    // We don't call "detach" here because it requires a caller
    // to be in a v8 context.

    detachPromiseForExpose();
    detachPromiseForResolve();
}

void ScriptPromiseResolver::detachPromise()
{
    detachPromiseForExpose();
}

void ScriptPromiseResolver::detachPromiseForExpose()
{
    m_promiseForExposeDetached = true;
    if (m_promiseForResolveDetached)
        m_promise.clear();
}

void ScriptPromiseResolver::detachPromiseForResolve()
{
    m_promiseForResolveDetached = true;
    if (m_promiseForExposeDetached)
        m_promise.clear();
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
    if (m_promiseForResolveDetached)
        return false;
    ASSERT(!m_promise.hasNoValue());
    v8::Local<v8::Object> promise = m_promise.v8Value().As<v8::Object>();
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(promise);
    V8PromiseCustom::PromiseState state = V8PromiseCustom::getState(internal);
    return state == V8PromiseCustom::Pending;
}

void ScriptPromiseResolver::detach()
{
    ASSERT(v8::Context::InContext());
    detachPromiseForExpose();
    reject(v8::Undefined(m_isolate));
    detachPromiseForResolve();
}

void ScriptPromiseResolver::fulfill(v8::Handle<v8::Value> value)
{
    resolve(value);
}

void ScriptPromiseResolver::resolve(v8::Handle<v8::Value> value)
{
    ASSERT(v8::Context::InContext());
    if (!isPending())
        return;
    V8PromiseCustom::resolve(m_promise.v8Value().As<v8::Object>(), value, V8PromiseCustom::Asynchronous, m_isolate);
    detachPromiseForResolve();
}

void ScriptPromiseResolver::reject(v8::Handle<v8::Value> value)
{
    ASSERT(v8::Context::InContext());
    if (!isPending())
        return;
    V8PromiseCustom::reject(m_promise.v8Value().As<v8::Object>(), value, V8PromiseCustom::Asynchronous, m_isolate);
    detachPromiseForResolve();
}

void ScriptPromiseResolver::fulfill(ScriptValue value)
{
    resolve(value);
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

} // namespace WebCore
