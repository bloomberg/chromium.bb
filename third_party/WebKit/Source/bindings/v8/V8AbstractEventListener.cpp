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
#include "bindings/v8/DateExtension.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8EventListenerList.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "core/dom/BeforeUnloadEvent.h"
#include "core/dom/Event.h"
#include "core/dom/EventNames.h"
#include "core/inspector/InspectorCounters.h"
#include "core/workers/WorkerGlobalScope.h"

namespace WebCore {

V8AbstractEventListener::V8AbstractEventListener(bool isAttribute, PassRefPtr<DOMWrapperWorld> world, v8::Isolate* isolate)
    : EventListener(JSEventListenerType)
    , m_isAttribute(isAttribute)
    , m_world(world)
    , m_isolate(isolate)
{
    ThreadLocalInspectorCounters::current().incrementCounter(ThreadLocalInspectorCounters::JSEventListenerCounter);
}

V8AbstractEventListener::~V8AbstractEventListener()
{
    if (!m_listener.isEmpty()) {
        v8::HandleScope scope(m_isolate);
        V8EventListenerList::clearWrapper(m_listener.newLocal(m_isolate), m_isAttribute);
    }
    ThreadLocalInspectorCounters::current().decrementCounter(ThreadLocalInspectorCounters::JSEventListenerCounter);
}

void V8AbstractEventListener::handleEvent(ScriptExecutionContext* context, Event* event)
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
    m_listener.makeWeak(this, &makeWeakCallback);
}

void V8AbstractEventListener::invokeEventHandler(ScriptExecutionContext* context, Event* event, v8::Local<v8::Value> jsEvent)
{
    // If jsEvent is empty, attempt to set it as a hidden value would crash v8.
    if (jsEvent.IsEmpty())
        return;

    v8::Local<v8::Context> v8Context = toV8Context(context, world());
    if (v8Context.IsEmpty())
        return;

    // We push the event being processed into the global object, so that it can be exposed by DOMWindow's bindings.
    v8::Handle<v8::String> eventSymbol = V8HiddenPropertyName::event();
    v8::Local<v8::Value> returnValue;

    // In beforeunload/unload handlers, we want to avoid sleeps which do tight loops of calling Date.getTime().
    if (event->type() == eventNames().beforeunloadEvent || event->type() == eventNames().unloadEvent)
        DateExtension::get()->setAllowSleep(false, v8Context->GetIsolate());

    {
        // Catch exceptions thrown in the event handler so they do not propagate to javascript code that caused the event to fire.
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);

        // Save the old 'event' property so we can restore it later.
        v8::Local<v8::Value> savedEvent = v8Context->Global()->GetHiddenValue(eventSymbol);
        tryCatch.Reset();

        // Make the event available in the global object, so DOMWindow can expose it.
        v8Context->Global()->SetHiddenValue(eventSymbol, jsEvent);
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
            v8Context->Global()->SetHiddenValue(eventSymbol, v8::Undefined());
        else
            v8Context->Global()->SetHiddenValue(eventSymbol, savedEvent);
        tryCatch.Reset();
    }

    if (event->type() == eventNames().beforeunloadEvent || event->type() == eventNames().unloadEvent)
        DateExtension::get()->setAllowSleep(true, v8Context->GetIsolate());

    ASSERT(!handleOutOfMemory() || returnValue.IsEmpty());

    if (returnValue.IsEmpty())
        return;

    if (!returnValue->IsNull() && !returnValue->IsUndefined() && event->isBeforeUnloadEvent()) {
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

v8::Local<v8::Object> V8AbstractEventListener::getReceiverObject(ScriptExecutionContext* context, Event* event)
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

void V8AbstractEventListener::makeWeakCallback(v8::Isolate*, v8::Persistent<v8::Object>*, V8AbstractEventListener* listener)
{
    listener->m_listener.clear();
}

} // namespace WebCore
