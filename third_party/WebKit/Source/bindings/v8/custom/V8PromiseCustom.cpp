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
#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptFunctionCall.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/Document.h"
#include "core/page/DOMWindow.h"
#include "core/platform/Task.h"
#include "core/workers/WorkerGlobalScope.h"
#include "wtf/Functional.h"
#include "wtf/PassOwnPtr.h"
#include <v8.h>

namespace WebCore {

namespace {

v8::Local<v8::ObjectTemplate> cachedObjectTemplate(void* privateTemplateUniqueKey, int internalFieldCount, v8::Isolate* isolate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    WrapperWorldType currentWorldType = worldType(isolate);
    v8::Handle<v8::FunctionTemplate> functionDescriptor = data->privateTemplateIfExists(currentWorldType, privateTemplateUniqueKey);
    if (!functionDescriptor.IsEmpty())
        return functionDescriptor->InstanceTemplate();

    functionDescriptor = v8::FunctionTemplate::New();
    v8::Local<v8::ObjectTemplate> instanceTemplate = functionDescriptor->InstanceTemplate();
    instanceTemplate->SetInternalFieldCount(internalFieldCount);
    data->setPrivateTemplate(currentWorldType, privateTemplateUniqueKey, functionDescriptor);
    return instanceTemplate;
}

v8::Local<v8::ObjectTemplate> wrapperCallbackEnvironmentObjectTemplate(v8::Isolate* isolate)
{
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static int privateTemplateUniqueKey = 0;
    return cachedObjectTemplate(&privateTemplateUniqueKey, V8PromiseCustom::WrapperCallbackEnvironmentFieldCount, isolate);
}

v8::Local<v8::ObjectTemplate> promiseEveryEnvironmentObjectTemplate(v8::Isolate* isolate)
{
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static int privateTemplateUniqueKey = 0;
    return cachedObjectTemplate(&privateTemplateUniqueKey, V8PromiseCustom::PromiseEveryEnvironmentFieldCount, isolate);
}

v8::Local<v8::ObjectTemplate> primitiveWrapperObjectTemplate(v8::Isolate* isolate)
{
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static int privateTemplateUniqueKey = 0;
    return cachedObjectTemplate(&privateTemplateUniqueKey, V8PromiseCustom::PrimitiveWrapperFieldCount, isolate);
}

v8::Local<v8::ObjectTemplate> internalObjectTemplate(v8::Isolate* isolate)
{
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static int privateTemplateUniqueKey = 0;
    return cachedObjectTemplate(&privateTemplateUniqueKey, V8PromiseCustom::InternalFieldCount, isolate);
}

v8::Local<v8::ObjectTemplate> resolverObjectTemplate(v8::Isolate* isolate)
{
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static int privateTemplateUniqueKey = 0;
    return cachedObjectTemplate(&privateTemplateUniqueKey, V8PromiseCustom::ResolverFieldCount, isolate);
}

class PromiseTask : public ScriptExecutionContext::Task {
public:
    PromiseTask(v8::Handle<v8::Function> callback, v8::Handle<v8::Object> receiver, v8::Handle<v8::Value> result, v8::Isolate* isolate)
        : m_callback(isolate, callback)
        , m_receiver(isolate, receiver)
        , m_result(isolate, result)
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
    ASSERT(context);
    if (context->activeDOMObjectsAreStopped())
        return;

    ScriptState* state = 0;
    if (context->isDocument()) {
        state = mainWorldScriptState(static_cast<Document*>(context)->frame());
    } else {
        ASSERT(context->isWorkerGlobalScope());
        state = scriptStateFromWorkerGlobalScope(toWorkerGlobalScope(context));
    }
    ASSERT(state);

    v8::Isolate* isolate = state->isolate();
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> v8Context = state->context();
    v8::Context::Scope scope(v8Context);
    v8::Handle<v8::Value> args[] = { m_result.newLocal(isolate) };
    V8ScriptRunner::callFunction(m_callback.newLocal(isolate), context, m_receiver.newLocal(isolate), WTF_ARRAY_LENGTH(args), args, isolate);
};

v8::Handle<v8::Value> postTask(v8::Handle<v8::Function> callback, v8::Handle<v8::Object> receiver, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    ScriptExecutionContext* scriptExecutionContext = getScriptExecutionContext();
    ASSERT(scriptExecutionContext && scriptExecutionContext->isContextThread());
    scriptExecutionContext->postTask(adoptPtr(new PromiseTask(callback, receiver, value, isolate)));
    return v8::Undefined(isolate);
}

void wrapperCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    ASSERT(!args.Data().IsEmpty());
    v8::Local<v8::Object> environment = args.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];

    v8::Local<v8::Object> promise = environment->GetInternalField(V8PromiseCustom::WrapperCallbackEnvironmentPromiseIndex).As<v8::Object>();
    v8::Local<v8::Object> resolver = environment->GetInternalField(V8PromiseCustom::WrapperCallbackEnvironmentPromiseResolverIndex).As<v8::Object>();
    v8::Local<v8::Function> callback = environment->GetInternalField(V8PromiseCustom::WrapperCallbackEnvironmentCallbackIndex).As<v8::Function>();

    v8::Local<v8::Value> argv[] = {
        result,
    };
    v8::TryCatch trycatch;
    result = V8ScriptRunner::callFunction(callback, getScriptExecutionContext(), promise, WTF_ARRAY_LENGTH(argv), argv, isolate);
    if (result.IsEmpty()) {
        V8PromiseCustom::rejectResolver(resolver, trycatch.Exception(), V8PromiseCustom::Synchronous, isolate);
        return;
    }
    V8PromiseCustom::resolveResolver(resolver, result, V8PromiseCustom::Synchronous, isolate);
}

v8::Local<v8::Object> wrapperCallbackEnvironment(v8::Handle<v8::Object> promise, v8::Handle<v8::Object> resolver, v8::Handle<v8::Function> callback, v8::Isolate* isolate)
{
    v8::Local<v8::ObjectTemplate> objectTemplate = wrapperCallbackEnvironmentObjectTemplate(isolate);
    v8::Local<v8::Object> environment = objectTemplate->NewInstance();
    environment->SetInternalField(V8PromiseCustom::WrapperCallbackEnvironmentPromiseIndex, promise);
    environment->SetInternalField(V8PromiseCustom::WrapperCallbackEnvironmentPromiseResolverIndex, resolver);
    environment->SetInternalField(V8PromiseCustom::WrapperCallbackEnvironmentCallbackIndex, callback);
    return environment;
}

void promiseFulfillCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    ASSERT(!args.Data().IsEmpty());
    v8::Local<v8::Object> resolver = args.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(args.GetIsolate());
    if (args.Length() > 0)
        result = args[0];

    V8PromiseCustom::fulfillResolver(resolver, result, V8PromiseCustom::Synchronous, args.GetIsolate());
}

void promiseResolveCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    ASSERT(!args.Data().IsEmpty());
    v8::Local<v8::Object> resolver = args.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(args.GetIsolate());
    if (args.Length() > 0)
        result = args[0];

    V8PromiseCustom::resolveResolver(resolver, result, V8PromiseCustom::Synchronous, args.GetIsolate());
}

void promiseRejectCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    ASSERT(!args.Data().IsEmpty());
    v8::Local<v8::Object> resolver = args.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(args.GetIsolate());
    if (args.Length() > 0)
        result = args[0];

    V8PromiseCustom::rejectResolver(resolver, result, V8PromiseCustom::Synchronous, args.GetIsolate());
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

void promiseEveryFulfillCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    ASSERT(!args.Data().IsEmpty());
    v8::Local<v8::Object> environment = args.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];

    v8::Local<v8::Object> resolver = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentPromiseResolverIndex).As<v8::Object>();
    v8::Local<v8::Object> countdownWrapper = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentCountdownIndex).As<v8::Object>();
    v8::Local<v8::Integer> index = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentIndexIndex).As<v8::Integer>();
    v8::Local<v8::Array> results = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentResultsIndex).As<v8::Array>();

    results->Set(index->Value(), result);

    v8::Local<v8::Integer> countdown = countdownWrapper->GetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex).As<v8::Integer>();
    ASSERT(countdown->Value() >= 1);
    if (countdown->Value() == 1) {
        V8PromiseCustom::resolveResolver(resolver, results, V8PromiseCustom::Synchronous, isolate);
        return;
    }
    countdownWrapper->SetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex, v8::Integer::New(countdown->Value() - 1, isolate));
}

void promiseSomeRejectCallback(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    ASSERT(!args.Data().IsEmpty());
    v8::Local<v8::Object> environment = args.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];

    v8::Local<v8::Object> resolver = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentPromiseResolverIndex).As<v8::Object>();
    v8::Local<v8::Object> countdownWrapper = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentCountdownIndex).As<v8::Object>();
    v8::Local<v8::Integer> index = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentIndexIndex).As<v8::Integer>();
    v8::Local<v8::Array> results = environment->GetInternalField(V8PromiseCustom::PromiseEveryEnvironmentResultsIndex).As<v8::Array>();

    results->Set(index->Value(), result);

    v8::Local<v8::Integer> countdown = countdownWrapper->GetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex).As<v8::Integer>();
    ASSERT(countdown->Value() >= 1);
    if (countdown->Value() == 1) {
        V8PromiseCustom::rejectResolver(resolver, results, V8PromiseCustom::Synchronous, isolate);
        return;
    }
    countdownWrapper->SetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex, v8::Integer::New(countdown->Value() - 1, isolate));
}

v8::Local<v8::Object> promiseEveryEnvironment(v8::Handle<v8::Object> resolver, v8::Handle<v8::Object> countdownWrapper, int index, v8::Handle<v8::Array> results, v8::Isolate* isolate)
{
    v8::Local<v8::ObjectTemplate> objectTemplate = promiseEveryEnvironmentObjectTemplate(isolate);
    v8::Local<v8::Object> environment = objectTemplate->NewInstance();

    environment->SetInternalField(V8PromiseCustom::PromiseEveryEnvironmentPromiseResolverIndex, resolver);
    environment->SetInternalField(V8PromiseCustom::PromiseEveryEnvironmentCountdownIndex, countdownWrapper);
    environment->SetInternalField(V8PromiseCustom::PromiseEveryEnvironmentIndexIndex, v8::Integer::New(index, isolate));
    environment->SetInternalField(V8PromiseCustom::PromiseEveryEnvironmentResultsIndex, results);
    return environment;
}

void promiseResolve(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Local<v8::Object> resolver = args.Data().As<v8::Object>();
    ASSERT(!resolver.IsEmpty());
    if (V8PromiseCustom::isInternalDetached(resolver))
        return;
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(resolver);
    if (V8PromiseCustom::getState(internal) != V8PromiseCustom::Pending)
        return;
    v8::Isolate* isolate = args.GetIsolate();
    V8PromiseCustom::setState(internal, V8PromiseCustom::PendingWithResolvedFlagSet, isolate);

    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];
    V8PromiseCustom::resolveResolver(resolver, result, V8PromiseCustom::Asynchronous, isolate);
}

void promiseReject(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Local<v8::Object> resolver = args.Data().As<v8::Object>();
    ASSERT(!resolver.IsEmpty());
    if (V8PromiseCustom::isInternalDetached(resolver))
        return;
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(resolver);
    if (V8PromiseCustom::getState(internal) != V8PromiseCustom::Pending)
        return;
    v8::Isolate* isolate = args.GetIsolate();
    V8PromiseCustom::setState(internal, V8PromiseCustom::PendingWithResolvedFlagSet, isolate);

    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];
    V8PromiseCustom::rejectResolver(resolver, result, V8PromiseCustom::Asynchronous, isolate);
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
        createClosure(promiseResolve, resolver),
        createClosure(promiseReject, resolver)
    };
    v8::TryCatch trycatch;
    if (V8ScriptRunner::callFunction(init, getScriptExecutionContext(), promise, WTF_ARRAY_LENGTH(argv), argv, isolate).IsEmpty()) {
        // An exception is thrown. Reject the promise if its resolved flag is unset.
        if (!V8PromiseCustom::isInternalDetached(resolver) && V8PromiseCustom::getState(V8PromiseCustom::getInternal(resolver)) == V8PromiseCustom::Pending)
            V8PromiseCustom::rejectResolver(resolver, trycatch.Exception(), V8PromiseCustom::Asynchronous, isolate);
    }
    v8SetReturnValue(args, promise);
    return;
}

void V8Promise::thenMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Function> fulfillWrapper, rejectWrapper;
    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);
    if (args.Length() > 0 && !args[0]->IsUndefined()) {
        if (!args[0]->IsFunction()) {
            v8SetReturnValue(args, throwTypeError("fulfillCallback must be a function or undefined", isolate));
            return;
        }
        fulfillWrapper = createClosure(wrapperCallback, wrapperCallbackEnvironment(promise, resolver, args[0].As<v8::Function>(), isolate));
    } else {
        fulfillWrapper = createClosure(promiseFulfillCallback, resolver);
    }
    if (args.Length() > 1 && !args[1]->IsUndefined()) {
        if (!args[1]->IsFunction()) {
            v8SetReturnValue(args, throwTypeError("rejectCallback must be a function or undefined", isolate));
            return;
        }
        rejectWrapper = createClosure(wrapperCallback, wrapperCallbackEnvironment(promise, resolver, args[1].As<v8::Function>(), isolate));
    } else {
        rejectWrapper = createClosure(promiseRejectCallback, resolver);
    }
    V8PromiseCustom::append(args.Holder(), fulfillWrapper, rejectWrapper, isolate);
    v8SetReturnValue(args, promise);
}

void V8Promise::catchMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Function> fulfillWrapper, rejectWrapper;
    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);

    if (args.Length() > 0 && !args[0]->IsUndefined()) {
        if (!args[0]->IsFunction()) {
            v8SetReturnValue(args, throwTypeError("rejectCallback must be a function or undefined", isolate));
            return;
        }
        rejectWrapper = createClosure(wrapperCallback, wrapperCallbackEnvironment(promise, resolver, args[0].As<v8::Function>(), isolate));
    } else {
        rejectWrapper = createClosure(promiseRejectCallback, resolver);
    }
    fulfillWrapper = createClosure(promiseFulfillCallback, resolver);
    V8PromiseCustom::append(args.Holder(), fulfillWrapper, rejectWrapper, isolate);
    v8SetReturnValue(args, promise);
}

void V8Promise::fulfillMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];

    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);
    V8PromiseCustom::fulfillResolver(resolver, result, V8PromiseCustom::Asynchronous, isolate);
    v8SetReturnValue(args, promise);
}

void V8Promise::resolveMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];

    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);
    V8PromiseCustom::resolveResolver(resolver, result, V8PromiseCustom::Asynchronous, isolate);
    v8SetReturnValue(args, promise);
}

void V8Promise::rejectMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (args.Length() > 0)
        result = args[0];

    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);
    V8PromiseCustom::rejectResolver(resolver, result, V8PromiseCustom::Asynchronous, isolate);
    v8SetReturnValue(args, promise);
}

void V8Promise::anyMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);

    if (!args.Length()) {
        V8PromiseCustom::resolveResolver(resolver, v8::Undefined(isolate), V8PromiseCustom::Asynchronous, isolate);
        v8SetReturnValue(args, promise);
        return;
    }

    v8::Local<v8::Function> fulfillCallback = createClosure(promiseResolveCallback, resolver);
    v8::Local<v8::Function> rejectCallback = createClosure(promiseRejectCallback, resolver);

    for (int i = 0; i < args.Length(); ++i) {
        v8::Local<v8::Object> eachPromise, eachResolver;
        V8PromiseCustom::createPromise(args.Holder(), &eachPromise, &eachResolver, isolate);
        V8PromiseCustom::resolveResolver(eachResolver, args[i], V8PromiseCustom::Asynchronous, isolate);
        V8PromiseCustom::append(eachPromise, fulfillCallback, rejectCallback, isolate);
    }
    v8SetReturnValue(args, promise);
}

void V8Promise::everyMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);

    if (!args.Length()) {
        V8PromiseCustom::resolveResolver(resolver, v8::Undefined(isolate), V8PromiseCustom::Asynchronous, isolate);
        v8SetReturnValue(args, promise);
        return;
    }

    v8::Local<v8::ObjectTemplate> objectTemplate = primitiveWrapperObjectTemplate(isolate);
    v8::Local<v8::Object> countdownWrapper = objectTemplate->NewInstance();
    countdownWrapper->SetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex, v8::Integer::New(args.Length(), isolate));
    v8::Local<v8::Array> results = v8::Array::New();

    v8::Local<v8::Function> rejectCallback = createClosure(promiseRejectCallback, resolver);
    for (int i = 0; i < args.Length(); ++i) {
        v8::Local<v8::Object> environment = promiseEveryEnvironment(resolver, countdownWrapper, i, results, isolate);
        v8::Local<v8::Function> fulfillCallback = v8::FunctionTemplate::New(promiseEveryFulfillCallback, environment)->GetFunction();
        v8::Local<v8::Object> eachPromise, eachResolver;
        V8PromiseCustom::createPromise(args.Holder(), &eachPromise, &eachResolver, isolate);
        V8PromiseCustom::resolveResolver(eachResolver, args[i], V8PromiseCustom::Asynchronous, isolate);
        V8PromiseCustom::append(eachPromise, fulfillCallback, rejectCallback, isolate);
    }
    v8SetReturnValue(args, promise);
}

void V8Promise::someMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();
    v8::Local<v8::Object> promise, resolver;
    V8PromiseCustom::createPromise(args.Holder(), &promise, &resolver, isolate);

    if (!args.Length()) {
        V8PromiseCustom::resolveResolver(resolver, v8::Undefined(isolate), V8PromiseCustom::Asynchronous, isolate);
        v8SetReturnValue(args, promise);
        return;
    }

    // Promise.some also uses PromiseEveryEnvironment.
    v8::Local<v8::ObjectTemplate> objectTemplate = primitiveWrapperObjectTemplate(isolate);
    v8::Local<v8::Object> countdownWrapper = objectTemplate->NewInstance();
    countdownWrapper->SetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex, v8::Integer::New(args.Length(), isolate));
    v8::Local<v8::Array> results = v8::Array::New();

    v8::Local<v8::Function> fulfillCallback = createClosure(promiseResolveCallback, resolver);
    for (int i = 0; i < args.Length(); ++i) {
        v8::Local<v8::Object> environment = promiseEveryEnvironment(resolver, countdownWrapper, i, results, isolate);
        v8::Local<v8::Object> eachPromise, eachResolver;
        v8::Local<v8::Function> rejectCallback = v8::FunctionTemplate::New(promiseSomeRejectCallback, environment)->GetFunction();
        V8PromiseCustom::createPromise(args.Holder(), &eachPromise, &eachResolver, isolate);
        V8PromiseCustom::resolveResolver(eachResolver, args[i], V8PromiseCustom::Asynchronous, isolate);
        V8PromiseCustom::append(eachPromise, fulfillCallback, rejectCallback, isolate);
    }
    v8SetReturnValue(args, promise);
}

//
// -- V8PromiseCustom --
void V8PromiseCustom::createPromise(v8::Handle<v8::Object> creationContext, v8::Local<v8::Object>* promise, v8::Local<v8::Object>* resolver, v8::Isolate* isolate)
{
    v8::Local<v8::ObjectTemplate> internalTemplate = internalObjectTemplate(isolate);
    v8::Local<v8::ObjectTemplate> resolverTemplate = resolverObjectTemplate(isolate);
    v8::Local<v8::Object> internal = internalTemplate->NewInstance();
    *resolver = resolverTemplate->NewInstance();
    *promise = V8DOMWrapper::createWrapper(creationContext, &V8Promise::info, 0, isolate);

    clearInternal(internal, V8PromiseCustom::Pending, v8::Undefined(isolate), isolate);

    (*promise)->SetInternalField(v8DOMWrapperObjectIndex, internal);
    // v8DOMWrapperObjectIndex is the same value as ResolverInternalIndex.
    // To extract a internal object from a promise and resolver object
    // by the same internal field index, use v8DOMWrapperObjectIndex
    // for resolver objects.
    (*resolver)->SetInternalField(v8DOMWrapperObjectIndex, internal);
}

void V8PromiseCustom::fulfillResolver(v8::Handle<v8::Object> resolver, v8::Handle<v8::Value> result, SynchronousMode mode, v8::Isolate* isolate)
{
    if (isInternalDetached(resolver))
        return;
    v8::Local<v8::Object> internal = getInternal(resolver);
    ASSERT(getState(internal) == Pending || getState(internal) == PendingWithResolvedFlagSet);
    v8::Local<v8::Array> callbacks = internal->GetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex).As<v8::Array>();
    clearInternal(internal, Fulfilled, result, isolate);
    detachInternal(resolver, isolate);

    callCallbacks(callbacks, result, mode, isolate);
}

void V8PromiseCustom::resolveResolver(v8::Handle<v8::Object> resolver, v8::Handle<v8::Value> result, SynchronousMode mode, v8::Isolate* isolate)
{
    ASSERT(!result.IsEmpty());
    v8::Local<v8::Value> then;
    if (result->IsObject()) {
        v8::TryCatch trycatch;
        then = result.As<v8::Object>()->Get(v8::String::NewSymbol("then"));
        if (then.IsEmpty()) {
            // If calling the [[Get]] internal method threw an exception, catch it and run reject.
            rejectResolver(resolver, trycatch.Exception(), mode, isolate);
            return;
        }
    }

    if (!then.IsEmpty() && then->IsFunction()) {
        ASSERT(result->IsObject());
        v8::TryCatch trycatch;
        v8::Handle<v8::Value> argv[] = {
            createClosure(promiseResolveCallback, resolver),
            createClosure(promiseRejectCallback, resolver),
        };
        if (V8ScriptRunner::callFunction(then.As<v8::Function>(), getScriptExecutionContext(), result.As<v8::Object>(), WTF_ARRAY_LENGTH(argv), argv, isolate).IsEmpty())
            rejectResolver(resolver, trycatch.Exception(), mode, isolate);
        return;
    }

    fulfillResolver(resolver, result, mode, isolate);
}

void V8PromiseCustom::rejectResolver(v8::Handle<v8::Object> resolver, v8::Handle<v8::Value> result, SynchronousMode mode, v8::Isolate* isolate)
{
    if (isInternalDetached(resolver))
        return;
    v8::Local<v8::Object> internal = getInternal(resolver);
    ASSERT(getState(internal) == Pending || getState(internal) == PendingWithResolvedFlagSet);
    v8::Local<v8::Array> callbacks = internal->GetInternalField(V8PromiseCustom::InternalRejectCallbackIndex).As<v8::Array>();
    clearInternal(internal, Rejected, result, isolate);
    detachInternal(resolver, isolate);

    callCallbacks(callbacks, result, mode, isolate);
}

void V8PromiseCustom::append(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> fulfillCallback, v8::Handle<v8::Function> rejectCallback, v8::Isolate* isolate)
{
    // fulfillCallback and rejectCallback can be empty.
    v8::Local<v8::Object> internal = getInternal(promise);

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

    ASSERT(state == Pending || state == PendingWithResolvedFlagSet);
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

void V8PromiseCustom::clearInternal(v8::Handle<v8::Object> internal, PromiseState state, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    setState(internal, state, isolate);
    internal->SetInternalField(V8PromiseCustom::InternalResultIndex, value);
    internal->SetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex, v8::Array::New());
    internal->SetInternalField(V8PromiseCustom::InternalRejectCallbackIndex, v8::Array::New());
}

V8PromiseCustom::PromiseState V8PromiseCustom::getState(v8::Handle<v8::Object> internal)
{
    v8::Handle<v8::Value> value = internal->GetInternalField(V8PromiseCustom::InternalStateIndex);
    bool ok = false;
    uint32_t number = toInt32(value, ok);
    ASSERT(ok && (number == Pending || number == Fulfilled || number == Rejected || number == PendingWithResolvedFlagSet));
    return static_cast<PromiseState>(number);
}

void V8PromiseCustom::setState(v8::Handle<v8::Object> internal, PromiseState state, v8::Isolate* isolate)
{
    ASSERT(state == Pending || state == Fulfilled || state == Rejected || state == PendingWithResolvedFlagSet);
    internal->SetInternalField(V8PromiseCustom::InternalStateIndex, v8::Integer::New(state, isolate));
}

void V8PromiseCustom::call(v8::Handle<v8::Function> function, v8::Handle<v8::Object> receiver, v8::Handle<v8::Value> result, SynchronousMode mode, v8::Isolate* isolate)
{
    if (mode == Synchronous) {
        v8::Context::Scope scope(isolate->GetCurrentContext());
        // If an exception is thrown, catch it and do nothing.
        v8::TryCatch trycatch;
        v8::Handle<v8::Value> args[] = { result };
        V8ScriptRunner::callFunction(function, getScriptExecutionContext(), receiver, WTF_ARRAY_LENGTH(args), args, isolate);
    } else {
        ASSERT(mode == Asynchronous);
        postTask(function, receiver, result, isolate);
    }
}

} // namespace WebCore
