/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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
#include "bindings/v8/V8AbstractEventListener.h"

#include "V8Event.h"
#include "V8EventTarget.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8EventListenerList.h"
#include "bindings/v8/V8HiddenValue.h"
#include "core/events/BeforeUnloadEvent.h"
#include "core/events/Event.h"
#include "core/inspector/InspectorCounters.h"
#include "core/workers/WorkerGlobalScope.h"

namespace WebCore {

V8AbstractEventListener::V8AbstractEventListener(bool isAttribute, DOMWrapperWorld* world, v8::Isolate* isolate)
    : EventListener(JSEventListenerType)
    , m_isAttribute(isAttribute)
    , m_world(world)
    , m_isolate(isolate)
{
    if (isMainThread())
        InspectorCounters::incrementCounter(InspectorCounters::JSEventListenerCounter);
}

V8AbstractEventListener::~V8AbstractEventListener()
{
    if (!m_listener.isEmpty()) {
        v8::HandleScope scope(m_isolate);
        V8EventListenerList::clearWrapper(m_listener.newLocal(m_isolate), m_isAttribute, m_isolate);
    }
    if (isMainThread())
        InspectorCounters::decrementCounter(InspectorCounters::JSEventListenerCounter);
}

void V8AbstractEventListener::handleEvent(ExecutionContext* context, Event* event)
{
    // Don't reenter V8 if execution was terminated in this instance of V8.
    if (context->isJSExecutionForbidden())
        return;

    ASSERT(event);

    // The callback function on XMLHttpRequest can clear the event listener and destroys 'this' object. Keep a local reference to it.
    // See issue 889829.
    RefPtr<V8AbstractEventListener> protect(this);

    v8::HandleScope handleScope(m_isolate);

    v8::Local<v8::Context> v8Context = toV8Context(context, world());
    if (v8Context.IsEmpty())
        return;

    // Enter the V8 context in which to perform the event handling.
    v8::Context::Scope scope(v8Context);

    // Get the V8 wrapper for the event object.
    v8::Isolate* isolate = v8Context->GetIsolate();
    v8::Handle<v8::Value> jsEvent = toV8(event, v8::Handle<v8::Object>(), isolate);
    if (jsEvent.IsEmpty())
        return;
    invokeEventHandler(context, event, v8::Local<v8::Value>::New(isolate, jsEvent));
}

void V8AbstractEventListener::setListenerObject(v8::Handle<v8::Object> listener)
{
    m_listener.set(m_isolate, listener);
    m_listener.setWeak(this, &setWeakCallback);
}

void V8AbstractEventListener::invokeEventHandler(ExecutionContext* context, Event* event, v8::Local<v8::Value> jsEvent)
{
    // If jsEvent is empty, attempt to set it as a hidden value would crash v8.
    if (jsEvent.IsEmpty())
        return;

    v8::Local<v8::Context> v8Context = toV8Context(context, world());
    if (v8Context.IsEmpty())
        return;
    v8::Isolate* isolate = v8Context->GetIsolate();

    v8::Local<v8::Value> returnValue;
    {
        // Catch exceptions thrown in the event handler so they do not propagate to javascript code that caused the event to fire.
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);

        // Save the old 'event' property so we can restore it later.
        v8::Local<v8::Value> savedEvent = V8HiddenValue::getHiddenValue(v8Context->GetIsolate(), v8Context->Global(), V8HiddenValue::event(isolate));
        tryCatch.Reset();

        // Make the event available in the global object, so DOMWindow can expose it.
        V8HiddenValue::setHiddenValue(v8Context->GetIsolate(), v8Context->Global(), V8HiddenValue::event(isolate), jsEvent);
        tryCatch.Reset();

        returnValue = callListenerFunction(context, jsEvent, event);
        if (tryCatch.HasCaught())
            event->target()->uncaughtExceptionInEventHandler();

        if (!tryCatch.CanContinue()) { // Result of TerminateExecution().
            if (context->isWorkerGlobalScope())
                toWorkerGlobalScope(context)->script()->forbidExecution();
            return;
        }
        tryCatch.Reset();

        // Restore the old event. This must be done for all exit paths through this method.
        if (savedEvent.IsEmpty())
            V8HiddenValue::setHiddenValue(v8Context->GetIsolate(), v8Context->Global(), V8HiddenValue::event(isolate), v8::Undefined(v8Context->GetIsolate()));
        else
            V8HiddenValue::setHiddenValue(v8Context->GetIsolate(), v8Context->Global(), V8HiddenValue::event(isolate), savedEvent);
        tryCatch.Reset();
    }

    if (returnValue.IsEmpty())
        return;

    if (m_isAttribute && !returnValue->IsNull() && !returnValue->IsUndefined() && event->isBeforeUnloadEvent()) {
        V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, stringReturnValue, returnValue);
        toBeforeUnloadEvent(event)->setReturnValue(stringReturnValue);
    }

    if (m_isAttribute && shouldPreventDefault(returnValue))
        event->preventDefault();
}

bool V8AbstractEventListener::shouldPreventDefault(v8::Local<v8::Value> returnValue)
{
    // Prevent default action if the return value is false in accord with the spec
    // http://www.w3.org/TR/html5/webappapis.html#event-handler-attributes
    return returnValue->IsBoolean() && !returnValue->BooleanValue();
}

v8::Local<v8::Object> V8AbstractEventListener::getReceiverObject(ExecutionContext* context, Event* event)
{
    v8::Isolate* isolate = toV8Context(context, world())->GetIsolate();
    v8::Local<v8::Object> listener = m_listener.newLocal(isolate);
    if (!m_listener.isEmpty() && !listener->IsFunction())
        return listener;

    EventTarget* target = event->currentTarget();
    v8::Handle<v8::Value> value = toV8(target, v8::Handle<v8::Object>(), isolate);
    if (value.IsEmpty())
        return v8::Local<v8::Object>();
    return v8::Local<v8::Object>::New(isolate, v8::Handle<v8::Object>::Cast(value));
}

bool V8AbstractEventListener::belongsToTheCurrentWorld() const
{
    return m_isolate->InContext() && m_world == DOMWrapperWorld::current(m_isolate);
}

void V8AbstractEventListener::setWeakCallback(const v8::WeakCallbackData<v8::Object, V8AbstractEventListener> &data)
{
    data.GetParameter()->m_listener.clear();
}

} // namespace WebCore
