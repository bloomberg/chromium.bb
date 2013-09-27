/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "bindings/v8/V8ErrorHandler.h"

#include "V8ErrorEvent.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "core/dom/Document.h"
#include "core/events/ErrorEvent.h"
#include "core/events/EventNames.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/page/Frame.h"

namespace WebCore {

V8ErrorHandler::V8ErrorHandler(v8::Local<v8::Object> listener, bool isInline, v8::Isolate* isolate)
    : V8EventListener(listener, isInline, isolate)
{
}

v8::Local<v8::Value> V8ErrorHandler::callListenerFunction(ScriptExecutionContext* context, v8::Handle<v8::Value> jsEvent, Event* event)
{
    if (!event->hasInterface(eventNames().interfaceForErrorEvent))
        return V8EventListener::callListenerFunction(context, jsEvent, event);

    ErrorEvent* errorEvent = static_cast<ErrorEvent*>(event);

    v8::Isolate* isolate = toV8Context(context, world())->GetIsolate();
    if (errorEvent->world() && errorEvent->world() != world())
        return v8::Null(isolate);

    v8::Local<v8::Object> listener = getListenerObject(context);
    v8::Local<v8::Value> returnValue;
    if (!listener.IsEmpty() && listener->IsFunction()) {
        v8::Local<v8::Function> callFunction = v8::Local<v8::Function>::Cast(listener);
        v8::Local<v8::Object> thisValue = v8::Context::GetCurrent()->Global();

        v8::Local<v8::Value> error = jsEvent->ToObject()->GetHiddenValue(V8HiddenPropertyName::error(isolate));
        if (error.IsEmpty())
            error = v8::Null(isolate);

        v8::Handle<v8::Value> parameters[5] = { v8String(errorEvent->message(), isolate), v8String(errorEvent->filename(), isolate), v8::Integer::New(errorEvent->lineno(), isolate), v8::Integer::New(errorEvent->colno(), isolate), error };
        v8::TryCatch tryCatch;
        tryCatch.SetVerbose(true);
        if (worldType(isolate) == WorkerWorld)
            returnValue = V8ScriptRunner::callFunction(callFunction, context, thisValue, WTF_ARRAY_LENGTH(parameters), parameters, isolate);
        else
            returnValue = ScriptController::callFunction(context, callFunction, thisValue, WTF_ARRAY_LENGTH(parameters), parameters, isolate);
    }
    return returnValue;
}

// static
void V8ErrorHandler::storeExceptionOnErrorEventWrapper(ErrorEvent* event, v8::Handle<v8::Value> data, v8::Isolate* isolate)
{
    v8::Local<v8::Value> wrappedEvent = toV8(event, v8::Handle<v8::Object>(), isolate);
    if (!wrappedEvent.IsEmpty()) {
        ASSERT(wrappedEvent->IsObject());
        v8::Local<v8::Object>::Cast(wrappedEvent)->SetHiddenValue(V8HiddenPropertyName::error(isolate), data);
    }
}

bool V8ErrorHandler::shouldPreventDefault(v8::Local<v8::Value> returnValue)
{
    return returnValue->IsBoolean() && returnValue->BooleanValue();
}

} // namespace WebCore
