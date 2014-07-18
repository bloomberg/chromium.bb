// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptPromisePropertyBase.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "core/dom/ExecutionContext.h"

namespace blink {

ScriptPromisePropertyBase::ScriptPromisePropertyBase(ExecutionContext* executionContext, Name name)
    : ContextLifecycleObserver(executionContext)
    , m_isolate(toIsolate(executionContext))
    , m_name(name)
    , m_state(Pending)
{
}

ScriptPromisePropertyBase::~ScriptPromisePropertyBase()
{
    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Object> wrapper = m_mainWorldWrapper.newLocal(m_isolate);
    if (!wrapper.IsEmpty()) {
        wrapper->DeleteHiddenValue(resolverName());
        wrapper->DeleteHiddenValue(promiseName());
    }
}

static void clearHandle(const v8::WeakCallbackData<v8::Object, ScopedPersistent<v8::Object> >& data)
{
    data.GetParameter()->clear();
}

ScriptPromise ScriptPromisePropertyBase::promise(DOMWrapperWorld& world)
{
    if (!executionContext())
        return ScriptPromise();

    if (!world.isMainWorld()) {
        // FIXME: Support isolated worlds.
        return ScriptPromise();
    }

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Context> context = toV8Context(executionContext(), world);
    if (context.IsEmpty())
        return ScriptPromise();
    ScriptState* scriptState = ScriptState::from(context);
    ScriptState::Scope scope(scriptState);

    v8::Handle<v8::Object> wrapper = m_mainWorldWrapper.newLocal(m_isolate);
    if (wrapper.IsEmpty()) {
        wrapper = holder(context->Global(), m_isolate);
        ASSERT(!wrapper.IsEmpty());
        ASSERT(V8HiddenValue::getHiddenValue(m_isolate, wrapper, resolverName()).IsEmpty());
        ASSERT(V8HiddenValue::getHiddenValue(m_isolate, wrapper, promiseName()).IsEmpty());
        m_mainWorldWrapper.set(m_isolate, wrapper);
        m_mainWorldWrapper.setWeak(&m_mainWorldWrapper, &clearHandle);
    }
    ASSERT(wrapper->CreationContext() == context);

    v8::Handle<v8::Value> cachedPromise = V8HiddenValue::getHiddenValue(m_isolate, wrapper, promiseName());
    if (!cachedPromise.IsEmpty())
        return ScriptPromise(scriptState, cachedPromise);

    // Create and cache the Promise
    v8::Handle<v8::Promise::Resolver> resolver = v8::Promise::Resolver::New(m_isolate);
    v8::Handle<v8::Promise> promise = resolver->GetPromise();
    V8HiddenValue::setHiddenValue(m_isolate, wrapper, promiseName(), promise);

    switch (m_state) {
    case Pending:
        // Cache the resolver too
        V8HiddenValue::setHiddenValue(m_isolate, wrapper, resolverName(), resolver);
        break;
    case Resolved:
    case Rejected:
        resolveOrRejectInternal(resolver);
        break;
    }

    return ScriptPromise(scriptState, promise);
}

void ScriptPromisePropertyBase::resolveOrReject(State targetState)
{
    ASSERT(executionContext());
    ASSERT(m_state == Pending);
    ASSERT(targetState == Resolved || targetState == Rejected);

    m_state = targetState;

    v8::HandleScope handleScope(m_isolate);
    v8::Handle<v8::Object> wrapper = m_mainWorldWrapper.newLocal(m_isolate);
    if (wrapper.IsEmpty())
        return; // wrapper has died or was never populated
    ScriptState::Scope scope(ScriptState::from(wrapper->CreationContext()));

    v8::Handle<v8::Promise::Resolver> resolver = V8HiddenValue::getHiddenValue(m_isolate, wrapper, resolverName()).As<v8::Promise::Resolver>();

    V8HiddenValue::deleteHiddenValue(m_isolate, wrapper, resolverName());
    resolveOrRejectInternal(resolver);
}

void ScriptPromisePropertyBase::resolveOrRejectInternal(v8::Handle<v8::Promise::Resolver> resolver)
{
    switch (m_state) {
    case Pending:
        ASSERT_NOT_REACHED();
        break;
    case Resolved:
        resolver->Resolve(resolvedValue(resolver->CreationContext()->Global(), m_isolate));
        break;
    case Rejected:
        resolver->Reject(rejectedValue(resolver->CreationContext()->Global(), m_isolate));
        break;
    }
}

v8::Handle<v8::String> ScriptPromisePropertyBase::promiseName()
{
    switch (m_name) {
#define P(Name)                                           \
    case Name:                                            \
        return V8HiddenValue::Name ## Promise(m_isolate);

        SCRIPT_PROMISE_PROPERTIES(P)

#undef P
    }
    ASSERT_NOT_REACHED();
    return v8::Handle<v8::String>();
}

v8::Handle<v8::String> ScriptPromisePropertyBase::resolverName()
{
    switch (m_name) {
#define P(Name)                                            \
    case Name:                                             \
        return V8HiddenValue::Name ## Resolver(m_isolate);

        SCRIPT_PROMISE_PROPERTIES(P)

#undef P
    }
    ASSERT_NOT_REACHED();
    return v8::Handle<v8::String>();
}

} // namespace blink
