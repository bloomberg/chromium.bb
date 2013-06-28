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
#include "bindings/v8/custom/V8PromiseCustom.h"

#include "V8Promise.h"
#include "V8PromiseResolver.h"
#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptFunctionCall.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/DOMWindow.h"
#include "core/platform/Task.h"
#include "wtf/Functional.h"
#include "wtf/PassOwnPtr.h"
#include <v8.h>

namespace WebCore {

namespace {

class PromiseTask : public ScriptExecutionContext::Task {
public:
    PromiseTask(v8::Handle<v8::Function> callback, v8::Handle<v8::Object> receiver, v8::Handle<v8::Value> result)
        : m_callback(callback)
        , m_receiver(receiver)
        , m_result(result)
    {
        ASSERT(!m_callback.isEmpty());
        ASSERT(!m_receiver.isEmpty());
        ASSERT(!m_result.isEmpty());
    }
    virtual ~PromiseTask() { }

    virtual void performTask(ScriptExecutionContext*) OVERRIDE;

private:
    ScopedPersistent<v8::Function> m_callback;
    ScopedPersistent<v8::Object> m_receiver;
    ScopedPersistent<v8::Value> m_result;
};

void PromiseTask::performTask(ScriptExecutionContext* context)
{
    ASSERT(context && context->isDocument());
    if (context->activeDOMObjectsAreStopped())
        return;
    ScriptState* state = mainWorldScriptState(static_cast<Document*>(context)->frame());
    v8::HandleScope handleScope;
    ASSERT(state);
    v8::Handle<v8::Context> v8Context = state->context();
    v8::Context::Scope scope(v8Context);
    v8::Isolate* isolate = v8Context->GetIsolate();
    v8::Handle<v8::Value> args[] = { m_result.newLocal(isolate) };
    V8ScriptRunner::callFunction(m_callback.newLocal(isolate), context, m_receiver.newLocal(isolate), WTF_ARRAY_LENGTH(args), args);
};

v8::Handle<v8::Value> postTask(v8::Handle<v8::Function> callback, v8::Handle<v8::Object> receiver, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    DOMWindow* window = activeDOMWindow();
    ASSERT(window);
    Document* document = window->document();
    ASSERT(document);
    document->postTask(adoptPtr(new PromiseTask(callback, receiver, value)));
    return v8::Undefined(isolate);
}

void callCallbacks(v8::Handle<v8::Array> callbacks, v8::Handle<v8::Value> result, V8PromiseCustom::SynchronousMode mode, v8::Isolate* isolate)
{
    v8::Local<v8::Object> global = isolate->GetCurrentContext()->Global();
    for (uint32_t i = 0, length = callbacks->Length(); i < length; ++i) {
        v8::Local<v8::Value> value = callbacks->Get(i);
        v8::Local<v8::Function> callback = value.As<v8::Function>();
        V8PromiseCustom::call(callback, global, result, mode, isolate);
    }
}

} // namespace

void V8Promise::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8SetReturnValue(args, v8::Local<v8::Value>());
    v8::Isolate* isolate = args.GetIsolate();
    if (!args.Length() || !args[0]->IsFunction()) {
        throwTypeError("Promise constructor takes a function argument", isolate);
        return;
    }
    v8::Local<v8::Function> init = args[0].As<v8::Function>();
    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);
    v8::Handle<v8::Value> argv[] = {
        resolver,
    };
    v8::TryCatch trycatch;
    if (V8ScriptRunner::callFunction(init, getScriptExecutionContext(), promise, WTF_ARRAY_LENGTH(argv), argv).IsEmpty()) {
        // An exception is thrown. Reject the promise.
        V8PromiseCustom::rejectResolver(resolver, trycatch.Exception(), V8PromiseCustom::Asynchronous, isolate);
    }
    v8SetReturnValue(args, promise);
    return;
}

//
// -- V8PromiseCustom --
void V8PromiseCustom::createPromise(v8::Handle<v8::Object> creationContext, v8::Local<v8::Object>* promise, v8::Local<v8::Object>* resolver, v8::Isolate* isolate)
{
    // FIXME: v8::ObjectTemplate::New should be cached.
    v8::Local<v8::ObjectTemplate> internalTemplate = v8::ObjectTemplate::New();
    internalTemplate->SetInternalFieldCount(InternalFieldCount);
    v8::Local<v8::Object> internal = internalTemplate->NewInstance();
    *promise = V8DOMWrapper::createWrapper(creationContext, &V8Promise::info, 0, isolate);
    *resolver = V8DOMWrapper::createWrapper(creationContext, &V8PromiseResolver::info, 0, isolate);

    clearInternal(internal, V8PromiseCustom::Pending, v8::Undefined(isolate));

    (*promise)->SetInternalField(v8DOMWrapperObjectIndex, internal);
    (*resolver)->SetInternalField(v8DOMWrapperObjectIndex, internal);
}

void V8PromiseCustom::fulfillResolver(v8::Handle<v8::Object> resolver, v8::Handle<v8::Value> result, SynchronousMode mode, v8::Isolate* isolate)
{
    if (isInternalDetached(resolver))
        return;
    v8::Local<v8::Object> internal = getInternal(resolver);
    if (getState(internal) != Pending)
        return;

    v8::Local<v8::Array> callbacks = internal->GetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex).As<v8::Array>();
    clearInternal(internal, Fulfilled, result);
    detachInternal(resolver, isolate);

    callCallbacks(callbacks, result, mode, isolate);
}

void V8PromiseCustom::rejectResolver(v8::Handle<v8::Object> resolver, v8::Handle<v8::Value> result, SynchronousMode mode, v8::Isolate* isolate)
{
    if (isInternalDetached(resolver))
        return;
    v8::Local<v8::Object> internal = getInternal(resolver);
    if (getState(internal) != Pending)
        return;

    v8::Local<v8::Array> callbacks = internal->GetInternalField(V8PromiseCustom::InternalRejectCallbackIndex).As<v8::Array>();
    clearInternal(internal, Rejected, result);
    detachInternal(resolver, isolate);

    callCallbacks(callbacks, result, mode, isolate);
}

void V8PromiseCustom::append(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> fulfillCallback, v8::Handle<v8::Function> rejectCallback, v8::Isolate* isolate)
{
    // fulfillCallback and rejectCallback can be empty.
    v8::Local<v8::Object> internal = getInternal(promise).As<v8::Object>();

    PromiseState state = getState(internal);
    if (state == Fulfilled) {
        if (!fulfillCallback.IsEmpty()) {
            v8::Local<v8::Value> result = internal->GetInternalField(V8PromiseCustom::InternalResultIndex);
            v8::Local<v8::Object> global = isolate->GetCurrentContext()->Global();
            call(fulfillCallback, global, result, Asynchronous, isolate);
        }
        return;
    }
    if (state == Rejected) {
        if (!rejectCallback.IsEmpty()) {
            v8::Local<v8::Value> result = internal->GetInternalField(V8PromiseCustom::InternalResultIndex);
            v8::Local<v8::Object> global = isolate->GetCurrentContext()->Global();
            call(rejectCallback, global, result, Asynchronous, isolate);
        }
        return;
    }

    ASSERT(state == Pending);
    if (!fulfillCallback.IsEmpty()) {
        v8::Local<v8::Array> callbacks = internal->GetInternalField(InternalFulfillCallbackIndex).As<v8::Array>();
        callbacks->Set(callbacks->Length(), fulfillCallback);
    }
    if (!rejectCallback.IsEmpty()) {
        v8::Local<v8::Array> callbacks = internal->GetInternalField(InternalRejectCallbackIndex).As<v8::Array>();
        callbacks->Set(callbacks->Length(), rejectCallback);
    }
}

v8::Local<v8::Object> V8PromiseCustom::getInternal(v8::Handle<v8::Object> promiseOrResolver)
{
    v8::Local<v8::Value> value = promiseOrResolver->GetInternalField(v8DOMWrapperObjectIndex);
    // This function cannot be called when the internal object is detached, so the value must be an object.
    return value.As<v8::Object>();
}

bool V8PromiseCustom::isInternalDetached(v8::Handle<v8::Object> resolver)
{
    v8::Local<v8::Value> value = resolver->GetInternalField(v8DOMWrapperObjectIndex);
    ASSERT(!value.IsEmpty());
    return value->IsUndefined();
}

void V8PromiseCustom::detachInternal(v8::Handle<v8::Object> resolver, v8::Isolate* isolate)
{
    resolver->SetInternalField(v8DOMWrapperObjectIndex, v8::Undefined(isolate));
}

void V8PromiseCustom::clearInternal(v8::Handle<v8::Object> internal, PromiseState state, v8::Handle<v8::Value> value)
{
    setState(internal, state);
    internal->SetInternalField(V8PromiseCustom::InternalResultIndex, value);
    internal->SetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex, v8::Array::New());
    internal->SetInternalField(V8PromiseCustom::InternalRejectCallbackIndex, v8::Array::New());
}

V8PromiseCustom::PromiseState V8PromiseCustom::getState(v8::Handle<v8::Object> internal)
{
    v8::Handle<v8::Value> value = internal->GetInternalField(V8PromiseCustom::InternalStateIndex);
    bool ok = false;
    uint32_t number = toInt32(value, ok);
    ASSERT(ok && (number == Pending || number == Fulfilled || number == Rejected));
    return static_cast<PromiseState>(number);
}

void V8PromiseCustom::setState(v8::Handle<v8::Object> internal, PromiseState state)
{
    ASSERT(state == Pending || state == Fulfilled || state == Rejected);
    internal->SetInternalField(V8PromiseCustom::InternalStateIndex, v8::Integer::New(state));
}

void V8PromiseCustom::call(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, v8::Handle<v8::Value> result, SynchronousMode mode, v8::Isolate* isolate)
{
    if (mode == Synchronous) {
        v8::Context::Scope scope(isolate->GetCurrentContext());
        // If an exception is thrown, catch it and do nothing.
        v8::TryCatch trycatch;
        v8::Handle<v8::Value> args[] = { result };
        V8ScriptRunner::callFunction(function, getScriptExecutionContext(), receiver, WTF_ARRAY_LENGTH(args), args);
    } else {
        ASSERT(mode == Asynchronous);
        postTask(function, receiver, result, isolate);
    }
}

} // namespace WebCore
