// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"
#include "core/events/PromiseRejectionEvent.h"

namespace blink {

PromiseRejectionEvent::PromiseRejectionEvent()
{
}

PromiseRejectionEvent::PromiseRejectionEvent(const AtomicString& type, const PromiseRejectionEventInit& initializer)
    : Event(type, initializer)
{
    if (initializer.hasPromise()) {
        m_promise.set(initializer.promise().isolate(), initializer.promise().v8Value());
        m_promise.setWeak(this, &PromiseRejectionEvent::didCollectPromise);
    }
    if (initializer.hasReason()) {
        m_reason.set(initializer.reason().isolate(), initializer.reason().v8Value());
        m_reason.setWeak(this, &PromiseRejectionEvent::didCollectReason);
    }
}

PromiseRejectionEvent::~PromiseRejectionEvent()
{
}

ScriptPromise PromiseRejectionEvent::promise(ScriptState* state) const
{
    v8::Local<v8::Value> value = m_promise.newLocal(state->isolate());
    return ScriptPromise(state, value);
}

ScriptValue PromiseRejectionEvent::reason(ScriptState* state) const
{
    v8::Local<v8::Value> value = m_reason.newLocal(state->isolate());
    return ScriptValue(state, value);
}

const AtomicString& PromiseRejectionEvent::interfaceName() const
{
    return EventNames::PromiseRejectionEvent;
}

DEFINE_TRACE(PromiseRejectionEvent)
{
    Event::trace(visitor);
}

void PromiseRejectionEvent::didCollectPromise(const v8::WeakCallbackInfo<PromiseRejectionEvent>& data)
{
    data.GetParameter()->m_promise.clear();
}

void PromiseRejectionEvent::didCollectReason(const v8::WeakCallbackInfo<PromiseRejectionEvent>& data)
{
    data.GetParameter()->m_reason.clear();
}

} // namespace blink
