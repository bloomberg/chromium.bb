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
#include "bindings/v8/DOMRequestState.h"
#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/ScriptFunctionCall.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenValue.h"
#include "bindings/v8/V8PerIsolateData.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContextTask.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorPromiseInstrumentation.h"
#include "core/workers/WorkerGlobalScope.h"
#include "platform/Task.h"
#include "wtf/Deque.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include <v8.h>

#define V8TRYCATCH_VOID_EMPTY(type, var, value)    \
    type var;                                      \
    {                                              \
        v8::TryCatch block;                        \
        var = (value);                             \
        if (UNLIKELY(block.HasCaught())) {         \
            return;                                \
        }                                          \
    }

namespace WebCore {

namespace {

v8::Local<v8::ObjectTemplate> cachedObjectTemplate(void* domTemplateKey, int internalFieldCount, v8::Isolate* isolate)
{
    V8PerIsolateData* data = V8PerIsolateData::from(isolate);
    v8::Handle<v8::FunctionTemplate> functionDescriptor = data->existingDOMTemplate(domTemplateKey);
    if (!functionDescriptor.IsEmpty())
        return functionDescriptor->InstanceTemplate();

    functionDescriptor = v8::FunctionTemplate::New(isolate);
    v8::Local<v8::ObjectTemplate> instanceTemplate = functionDescriptor->InstanceTemplate();
    instanceTemplate->SetInternalFieldCount(internalFieldCount);
    data->setDOMTemplate(domTemplateKey, functionDescriptor);
    return instanceTemplate;
}

v8::Local<v8::ObjectTemplate> promiseAllEnvironmentObjectTemplate(v8::Isolate* isolate)
{
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    return cachedObjectTemplate(&domTemplateKey, V8PromiseCustom::PromiseAllEnvironmentFieldCount, isolate);
}

v8::Local<v8::ObjectTemplate> primitiveWrapperObjectTemplate(v8::Isolate* isolate)
{
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    return cachedObjectTemplate(&domTemplateKey, V8PromiseCustom::PrimitiveWrapperFieldCount, isolate);
}

v8::Local<v8::ObjectTemplate> internalObjectTemplate(v8::Isolate* isolate)
{
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    return cachedObjectTemplate(&domTemplateKey, V8PromiseCustom::InternalFieldCount, isolate);
}

void promiseResolveCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ASSERT(!info.Data().IsEmpty());
    v8::Local<v8::Object> promise = info.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(info.GetIsolate());
    if (info.Length() > 0)
        result = info[0];

    V8PromiseCustom::resolve(promise, result, info.GetIsolate());
}

void promiseRejectCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ASSERT(!info.Data().IsEmpty());
    v8::Local<v8::Object> promise = info.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(info.GetIsolate());
    if (info.Length() > 0)
        result = info[0];

    V8PromiseCustom::reject(promise, result, info.GetIsolate());
}

void promiseAllFulfillCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    ASSERT(!info.Data().IsEmpty());
    v8::Local<v8::Object> environment = info.Data().As<v8::Object>();
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (info.Length() > 0)
        result = info[0];

    v8::Local<v8::Object> promise = environment->GetInternalField(V8PromiseCustom::PromiseAllEnvironmentPromiseIndex).As<v8::Object>();
    v8::Local<v8::Object> countdownWrapper = environment->GetInternalField(V8PromiseCustom::PromiseAllEnvironmentCountdownIndex).As<v8::Object>();
    v8::Local<v8::Integer> index = environment->GetInternalField(V8PromiseCustom::PromiseAllEnvironmentIndexIndex).As<v8::Integer>();
    v8::Local<v8::Array> results = environment->GetInternalField(V8PromiseCustom::PromiseAllEnvironmentResultsIndex).As<v8::Array>();

    results->Set(index->Value(), result);

    v8::Local<v8::Integer> countdown = countdownWrapper->GetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex).As<v8::Integer>();
    ASSERT(countdown->Value() >= 1);
    if (countdown->Value() == 1) {
        V8PromiseCustom::resolve(promise, results, isolate);
        return;
    }
    countdownWrapper->SetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex, v8::Integer::New(isolate, countdown->Value() - 1));
}

v8::Local<v8::Object> promiseAllEnvironment(v8::Handle<v8::Object> promise, v8::Handle<v8::Object> countdownWrapper, int index, v8::Handle<v8::Array> results, v8::Isolate* isolate)
{
    v8::Local<v8::ObjectTemplate> objectTemplate = promiseAllEnvironmentObjectTemplate(isolate);
    v8::Local<v8::Object> environment = objectTemplate->NewInstance();
    if (environment.IsEmpty())
        return v8::Local<v8::Object>();

    environment->SetInternalField(V8PromiseCustom::PromiseAllEnvironmentPromiseIndex, promise);
    environment->SetInternalField(V8PromiseCustom::PromiseAllEnvironmentCountdownIndex, countdownWrapper);
    environment->SetInternalField(V8PromiseCustom::PromiseAllEnvironmentIndexIndex, v8::Integer::New(isolate, index));
    environment->SetInternalField(V8PromiseCustom::PromiseAllEnvironmentResultsIndex, results);
    return environment;
}

// Clear |internal|'s derived array.
void clearDerived(v8::Handle<v8::Object> internal, v8::Isolate* isolate)
{
    internal->SetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex, v8::Array::New(isolate));
    internal->SetInternalField(V8PromiseCustom::InternalRejectCallbackIndex, v8::Array::New(isolate));
    internal->SetInternalField(V8PromiseCustom::InternalDerivedPromiseIndex, v8::Array::New(isolate));
}

// Add a tuple (|derivedPromise|, |onFulfilled|, |onRejected|) to
// |internal|'s derived array.
// |internal| must be a Promise internal object.
// |derivedPromise| must be a Promise instance.
// |onFulfilled| and |onRejected| can be an empty value respectively.
void addToDerived(v8::Handle<v8::Object> internal, v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Isolate* isolate)
{
    v8::Local<v8::Array> fulfillCallbacks = internal->GetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex).As<v8::Array>();
    v8::Local<v8::Array> rejectCallbacks = internal->GetInternalField(V8PromiseCustom::InternalRejectCallbackIndex).As<v8::Array>();
    v8::Local<v8::Array> derivedPromises = internal->GetInternalField(V8PromiseCustom::InternalDerivedPromiseIndex).As<v8::Array>();

    if (onFulfilled.IsEmpty()) {
        fulfillCallbacks->Set(fulfillCallbacks->Length(), v8::Undefined(isolate));
    } else {
        fulfillCallbacks->Set(fulfillCallbacks->Length(), onFulfilled);
    }

    if (onRejected.IsEmpty()) {
        rejectCallbacks->Set(rejectCallbacks->Length(), v8::Undefined(isolate));
    } else {
        rejectCallbacks->Set(rejectCallbacks->Length(), onRejected);
    }

    ASSERT(!derivedPromise.IsEmpty());
    derivedPromises->Set(derivedPromises->Length(), derivedPromise);

    // Since they are treated as a tuple,
    // we need to guaranteed that the length of these arrays are same.
    ASSERT(fulfillCallbacks->Length() == rejectCallbacks->Length() && rejectCallbacks->Length() == derivedPromises->Length());
}

// Set a |promise|'s state and result that correspond to the state.
// |promise| must be a Promise instance.
void setStateForPromise(v8::Handle<v8::Object> promise, V8PromiseCustom::PromiseState state, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    ASSERT(!value.IsEmpty());
    ASSERT(state == V8PromiseCustom::Pending || state == V8PromiseCustom::Fulfilled || state == V8PromiseCustom::Rejected || state == V8PromiseCustom::Following);
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(promise);
    internal->SetInternalField(V8PromiseCustom::InternalStateIndex, v8::Integer::New(isolate, state));
    internal->SetInternalField(V8PromiseCustom::InternalResultIndex, value);
    ExecutionContext* context = currentExecutionContext(isolate);
    if (InspectorInstrumentation::isPromiseTrackerEnabled(context))
        InspectorInstrumentation::didUpdatePromiseState(context, ScriptObject(ScriptState::forContext(isolate->GetCurrentContext()), promise), state, ScriptValue(value, isolate));
}

class TaskPerformScopeForInstrumentation {
public:
    TaskPerformScopeForInstrumentation(ExecutionContext* context, ExecutionContextTask* task)
        : m_cookie(InspectorInstrumentation::willPerformPromiseTask(context, task))
    {
    }

    ~TaskPerformScopeForInstrumentation()
    {
        InspectorInstrumentation::didPerformPromiseTask(m_cookie);
    }

private:
    InspectorInstrumentationCookie m_cookie;
};

class CallHandlerTask FINAL : public ExecutionContextTask {
public:
    CallHandlerTask(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> handler, v8::Handle<v8::Value> argument, V8PromiseCustom::PromiseState originatorState, v8::Isolate* isolate, ExecutionContext* context)
        : m_promise(isolate, promise)
        , m_handler(isolate, handler)
        , m_argument(isolate, argument)
        , m_requestState(isolate)
    {
        ASSERT(!m_promise.isEmpty());
        ASSERT(!m_handler.isEmpty());
        ASSERT(!m_argument.isEmpty());
        InspectorInstrumentation::didPostPromiseTask(context, this, originatorState == V8PromiseCustom::Fulfilled);
    }
    virtual ~CallHandlerTask() { }

    virtual void performTask(ExecutionContext*) OVERRIDE;

private:
    ScopedPersistent<v8::Object> m_promise;
    ScopedPersistent<v8::Function> m_handler;
    ScopedPersistent<v8::Value> m_argument;
    DOMRequestState m_requestState;
};

void CallHandlerTask::performTask(ExecutionContext* context)
{
    TaskPerformScopeForInstrumentation performTaskScope(context, this);

    ASSERT(context);
    if (context->activeDOMObjectsAreStopped())
        return;

    DOMRequestState::Scope scope(m_requestState);
    v8::Isolate* isolate = m_requestState.isolate();
    v8::Handle<v8::Value> info[] = { m_argument.newLocal(isolate) };
    v8::TryCatch trycatch;
    v8::Local<v8::Value> value = V8ScriptRunner::callFunction(m_handler.newLocal(isolate), context, v8::Undefined(isolate), WTF_ARRAY_LENGTH(info), info, isolate);
    if (value.IsEmpty()) {
        V8PromiseCustom::reject(m_promise.newLocal(isolate), trycatch.Exception(), isolate);
    } else {
        V8PromiseCustom::resolve(m_promise.newLocal(isolate), value, isolate);
    }
}

class UpdateDerivedTask FINAL : public ExecutionContextTask {
public:
    UpdateDerivedTask(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> originatorValueObject, v8::Isolate* isolate, ExecutionContext* context)
        : m_promise(isolate, promise)
        , m_onFulfilled(isolate, onFulfilled)
        , m_onRejected(isolate, onRejected)
        , m_originatorValueObject(isolate, originatorValueObject)
        , m_requestState(isolate)
    {
        ASSERT(!m_promise.isEmpty());
        ASSERT(!m_originatorValueObject.isEmpty());
        InspectorInstrumentation::didPostPromiseTask(context, this, true);
    }
    virtual ~UpdateDerivedTask() { }

    virtual void performTask(ExecutionContext*) OVERRIDE;

private:
    ScopedPersistent<v8::Object> m_promise;
    ScopedPersistent<v8::Function> m_onFulfilled;
    ScopedPersistent<v8::Function> m_onRejected;
    ScopedPersistent<v8::Object> m_originatorValueObject;
    DOMRequestState m_requestState;
};

void UpdateDerivedTask::performTask(ExecutionContext* context)
{
    TaskPerformScopeForInstrumentation performTaskScope(context, this);

    ASSERT(context);
    if (context->activeDOMObjectsAreStopped())
        return;

    DOMRequestState::Scope scope(m_requestState);
    v8::Isolate* isolate = m_requestState.isolate();
    v8::Local<v8::Object> originatorValueObject = m_originatorValueObject.newLocal(isolate);
    v8::Local<v8::Value> coercedAlready = V8HiddenValue::getHiddenValue(isolate, originatorValueObject, V8HiddenValue::thenableHiddenPromise(isolate));
    if (!coercedAlready.IsEmpty() && coercedAlready->IsObject()) {
        ASSERT(V8PromiseCustom::isPromise(coercedAlready.As<v8::Object>(), isolate));
        V8PromiseCustom::updateDerivedFromPromise(m_promise.newLocal(isolate), m_onFulfilled.newLocal(isolate), m_onRejected.newLocal(isolate), coercedAlready.As<v8::Object>(), isolate);
        return;
    }

    v8::Local<v8::Value> then;
    v8::TryCatch trycatch;
    then = originatorValueObject->Get(v8AtomicString(isolate, "then"));
    if (then.IsEmpty()) {
        // If calling the [[Get]] internal method threw an exception, catch it and run updateDerivedFromReason.
        V8PromiseCustom::updateDerivedFromReason(m_promise.newLocal(isolate), m_onRejected.newLocal(isolate), trycatch.Exception(), isolate);
        return;
    }

    if (then->IsFunction()) {
        ASSERT(then->IsObject());
        v8::Local<v8::Object> coerced = V8PromiseCustom::coerceThenable(originatorValueObject, then.As<v8::Function>(), isolate);
        // If the stack is exhausted coerced can be empty, but it is impossible
        // because this function is executed on a fresh stack.
        ASSERT(!coerced.IsEmpty());
        V8PromiseCustom::updateDerivedFromPromise(m_promise.newLocal(isolate), m_onFulfilled.newLocal(isolate), m_onRejected.newLocal(isolate), coerced, isolate);
        return;
    }

    V8PromiseCustom::updateDerivedFromValue(m_promise.newLocal(isolate), m_onFulfilled.newLocal(isolate), originatorValueObject, isolate);
}

// Since Promises state propagation routines are executed recursively, they cause
// stack overflow.
// (e.g. UpdateDerived -> UpdateDerivedFromValue -> SetValue ->
//  PropagateToDerived -> UpdateDerived)
//
// To fix that, we introduce PromisePropagator. It holds the stack. When
// propagating the result to the derived tuples, we append the derived tuples
// to the stack. After that, we drain the stack to propagate the result to
// the stored tuples.
//
// PromisePropagator should be held on the stack and should not be held
// as other object's member. PromisePropagator holds Derived tuples. Since
// Derived tuples hold persistent handles to JS objects, retaining
// PromisePropagator in the heap causes memory leaks.
//
class PromisePropagator {
    WTF_MAKE_NONCOPYABLE(PromisePropagator);
public:
    PromisePropagator() { }

    void performPropagation(v8::Isolate*);

    void setValue(v8::Handle<v8::Object> promise, v8::Handle<v8::Value>, v8::Isolate*);
    void setReason(v8::Handle<v8::Object> promise, v8::Handle<v8::Value>, v8::Isolate*);
    void propagateToDerived(v8::Handle<v8::Object> promise, v8::Isolate*);
    void updateDerived(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> originator, v8::Isolate*);
    void updateDerivedFromValue(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Value>, v8::Isolate*);
    void updateDerivedFromReason(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Value>, v8::Isolate*);
    void updateDerivedFromPromise(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> promise, v8::Isolate*);

private:
    class Derived {
        WTF_MAKE_NONCOPYABLE(Derived);
    public:
        Derived(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> originator, v8::Isolate* isolate)
            : m_promise(isolate, promise)
            , m_onFulfilled(isolate, onFulfilled)
            , m_onRejected(isolate, onRejected)
            , m_originator(isolate, originator)
        {
            ASSERT(!m_promise.isEmpty());
            ASSERT(!m_originator.isEmpty());
        }

        static PassOwnPtr<Derived> create(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> originator, v8::Isolate* isolate)
        {
            return adoptPtr(new Derived(promise, onFulfilled, onRejected, originator, isolate));
        }

        v8::Local<v8::Object> promise(v8::Isolate* isolate) const { return m_promise.newLocal(isolate); }
        v8::Local<v8::Function> onFulfilled(v8::Isolate* isolate) const { return m_onFulfilled.newLocal(isolate); }
        v8::Local<v8::Function> onRejected(v8::Isolate* isolate) const { return m_onRejected.newLocal(isolate); }
        v8::Local<v8::Object> originator(v8::Isolate* isolate) const { return m_originator.newLocal(isolate); }

    private:
        ScopedPersistent<v8::Object> m_promise;
        ScopedPersistent<v8::Function> m_onFulfilled;
        ScopedPersistent<v8::Function> m_onRejected;
        ScopedPersistent<v8::Object> m_originator;
    };

    Deque<OwnPtr<Derived> > m_derivedStack;
};

void PromisePropagator::performPropagation(v8::Isolate* isolate)
{
    while (!m_derivedStack.isEmpty()) {
        v8::HandleScope handleScope(isolate);
        OwnPtr<Derived> derived = m_derivedStack.takeLast();
        updateDerived(derived->promise(isolate), derived->onFulfilled(isolate), derived->onRejected(isolate), derived->originator(isolate), isolate);
    }
}

void PromisePropagator::setValue(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    ASSERT(V8PromiseCustom::getState(V8PromiseCustom::getInternal(promise)) != V8PromiseCustom::Fulfilled && V8PromiseCustom::getState(V8PromiseCustom::getInternal(promise)) != V8PromiseCustom::Rejected);
    setStateForPromise(promise, V8PromiseCustom::Fulfilled, value, isolate);
    propagateToDerived(promise, isolate);
}

void PromisePropagator::setReason(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> reason, v8::Isolate* isolate)
{
    ASSERT(V8PromiseCustom::getState(V8PromiseCustom::getInternal(promise)) != V8PromiseCustom::Fulfilled && V8PromiseCustom::getState(V8PromiseCustom::getInternal(promise)) != V8PromiseCustom::Rejected);
    setStateForPromise(promise, V8PromiseCustom::Rejected, reason, isolate);
    propagateToDerived(promise, isolate);
}

void PromisePropagator::propagateToDerived(v8::Handle<v8::Object> promise, v8::Isolate* isolate)
{
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(promise);
    ASSERT(V8PromiseCustom::getState(internal) == V8PromiseCustom::Fulfilled || V8PromiseCustom::getState(internal) == V8PromiseCustom::Rejected);
    v8::Local<v8::Array> fulfillCallbacks = internal->GetInternalField(V8PromiseCustom::InternalFulfillCallbackIndex).As<v8::Array>();
    v8::Local<v8::Array> rejectCallbacks = internal->GetInternalField(V8PromiseCustom::InternalRejectCallbackIndex).As<v8::Array>();
    v8::Local<v8::Array> derivedPromises = internal->GetInternalField(V8PromiseCustom::InternalDerivedPromiseIndex).As<v8::Array>();
    // Since they are treated as a tuple,
    // we need to guaranteed that the length of these arrays are same.
    ASSERT(fulfillCallbacks->Length() == rejectCallbacks->Length() && rejectCallbacks->Length() == derivedPromises->Length());

    // Append Derived tuple to the stack in reverse order.
    for (uint32_t count = 0, length = derivedPromises->Length(); count < length; ++count) {
        uint32_t i = length - count - 1;
        v8::Local<v8::Object> derivedPromise = derivedPromises->Get(i).As<v8::Object>();

        v8::Local<v8::Function> onFulfilled, onRejected;
        v8::Local<v8::Value> onFulfilledValue = fulfillCallbacks->Get(i);
        if (onFulfilledValue->IsFunction()) {
            onFulfilled = onFulfilledValue.As<v8::Function>();
        }
        v8::Local<v8::Value> onRejectedValue = rejectCallbacks->Get(i);
        if (onRejectedValue->IsFunction()) {
            onRejected = onRejectedValue.As<v8::Function>();
        }

        m_derivedStack.append(Derived::create(derivedPromise, onFulfilled, onRejected, promise, isolate));
    }
    clearDerived(internal, isolate);
}

void PromisePropagator::updateDerivedFromValue(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    if (!onFulfilled.IsEmpty()) {
        V8PromiseCustom::callHandler(derivedPromise, onFulfilled, value, V8PromiseCustom::Fulfilled, isolate);
    } else {
        setValue(derivedPromise, value, isolate);
    }
}

void PromisePropagator::updateDerivedFromReason(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Value> reason, v8::Isolate* isolate)
{
    if (!onRejected.IsEmpty()) {
        V8PromiseCustom::callHandler(derivedPromise, onRejected, reason, V8PromiseCustom::Rejected, isolate);
    } else {
        setReason(derivedPromise, reason, isolate);
    }
}

void PromisePropagator::updateDerived(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> originator, v8::Isolate* isolate)
{
    v8::Local<v8::Object> originatorInternal = V8PromiseCustom::getInternal(originator);
    V8PromiseCustom::PromiseState originatorState = V8PromiseCustom::getState(originatorInternal);
    ASSERT(originatorState == V8PromiseCustom::Fulfilled || originatorState == V8PromiseCustom::Rejected);
    v8::Local<v8::Value> originatorValue = originatorInternal->GetInternalField(V8PromiseCustom::InternalResultIndex);
    if (originatorState == V8PromiseCustom::Fulfilled) {
        if (originatorValue->IsObject()) {
            ExecutionContext* executionContext = currentExecutionContext(isolate);
            ASSERT(executionContext && executionContext->isContextThread());
            executionContext->postTask(adoptPtr(new UpdateDerivedTask(derivedPromise, onFulfilled, onRejected, originatorValue.As<v8::Object>(), isolate, executionContext)));
        } else {
            updateDerivedFromValue(derivedPromise, onFulfilled, originatorValue, isolate);
        }
    } else {
        updateDerivedFromReason(derivedPromise, onRejected, originatorValue, isolate);
    }
}

void PromisePropagator::updateDerivedFromPromise(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> promise, v8::Isolate* isolate)
{
    v8::Local<v8::Object> internal = V8PromiseCustom::getInternal(promise);
    V8PromiseCustom::PromiseState state = V8PromiseCustom::getState(internal);
    if (state == V8PromiseCustom::Fulfilled || state == V8PromiseCustom::Rejected) {
        updateDerived(derivedPromise, onFulfilled, onRejected, promise, isolate);
    } else {
        addToDerived(internal, derivedPromise, onFulfilled, onRejected, isolate);
    }
    ExecutionContext* context = currentExecutionContext(isolate);
    if (InspectorInstrumentation::isPromiseTrackerEnabled(context)) {
        ScriptState* scriptState = ScriptState::forContext(isolate->GetCurrentContext());
        InspectorInstrumentation::didUpdatePromiseParent(context, ScriptObject(scriptState, derivedPromise), ScriptObject(scriptState, promise));
    }
}

} // namespace

void V8Promise::constructorCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8SetReturnValue(info, v8::Local<v8::Value>());
    v8::Isolate* isolate = info.GetIsolate();
    ExecutionContext* executionContext = callingExecutionContext(isolate);
    UseCounter::count(executionContext, UseCounter::PromiseConstructor);
    if (!info.Length() || !info[0]->IsFunction()) {
        throwTypeError("Promise constructor takes a function argument", isolate);
        return;
    }
    v8::Local<v8::Function> init = info[0].As<v8::Function>();
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, promise, V8PromiseCustom::createPromise(info.Holder(), isolate));
    V8TRYCATCH_VOID_EMPTY(v8::Handle<v8::Value>, resolve, createClosure(promiseResolveCallback, promise, isolate));
    V8TRYCATCH_VOID_EMPTY(v8::Handle<v8::Value>, reject, createClosure(promiseRejectCallback, promise, isolate));
    v8::Handle<v8::Value> argv[] = { resolve, reject };
    v8::TryCatch trycatch;
    if (V8ScriptRunner::callFunction(init, currentExecutionContext(isolate), v8::Undefined(isolate), WTF_ARRAY_LENGTH(argv), argv, isolate).IsEmpty()) {
        // An exception is thrown. Reject the promise if its resolved flag is unset.
        V8PromiseCustom::reject(promise, trycatch.Exception(), isolate);
    }
    v8SetReturnValue(info, promise);
    return;
}

void V8Promise::thenMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Function> onFulfilled, onRejected;
    if (info.Length() > 0 && info[0]->IsFunction())
        onFulfilled = info[0].As<v8::Function>();
    if (info.Length() > 1 && info[1]->IsFunction())
        onRejected = info[1].As<v8::Function>();
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Value>, newPromise, V8PromiseCustom::then(info.Holder(), onFulfilled, onRejected, isolate));
    v8SetReturnValue(info, newPromise);
}

void V8Promise::castMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    ExecutionContext* executionContext = callingExecutionContext(isolate);
    UseCounter::count(executionContext, UseCounter::PromiseCast);
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (info.Length() > 0)
        result = info[0];

    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Value>, cast, V8PromiseCustom::toPromise(result, isolate));
    v8SetReturnValue(info, cast);
}

void V8Promise::catchMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Function> onFulfilled, onRejected;

    if (info.Length() > 0 && !info[0]->IsUndefined()) {
        if (!info[0]->IsFunction()) {
            v8SetReturnValue(info, throwTypeError("onRejected must be a function or undefined", isolate));
            return;
        }
        onRejected = info[0].As<v8::Function>();
    }
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Value>, newPromise, V8PromiseCustom::then(info.Holder(), onFulfilled, onRejected, isolate));
    v8SetReturnValue(info, newPromise);
}

void V8Promise::resolveMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    ExecutionContext* executionContext = callingExecutionContext(isolate);
    UseCounter::count(executionContext, UseCounter::PromiseResolve);
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (info.Length() > 0)
        result = info[0];

    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, promise, V8PromiseCustom::createPromise(info.Holder(), isolate));
    V8PromiseCustom::resolve(promise, result, isolate);
    v8SetReturnValue(info, promise);
}

void V8Promise::rejectMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    ExecutionContext* executionContext = callingExecutionContext(isolate);
    UseCounter::count(executionContext, UseCounter::PromiseReject);
    v8::Local<v8::Value> result = v8::Undefined(isolate);
    if (info.Length() > 0)
        result = info[0];

    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, promise, V8PromiseCustom::createPromise(info.Holder(), isolate));
    V8PromiseCustom::reject(promise, result, isolate);
    v8SetReturnValue(info, promise);
}

void V8Promise::raceMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, promise, V8PromiseCustom::createPromise(info.Holder(), isolate));

    if (!info.Length() || !info[0]->IsArray()) {
        v8SetReturnValue(info, promise);
        return;
    }

    // FIXME: Now we limit the iterable type to the Array type.
    v8::Local<v8::Array> iterable = info[0].As<v8::Array>();
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Function>, onFulfilled, createClosure(promiseResolveCallback, promise, isolate));
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Function>, onRejected, createClosure(promiseRejectCallback, promise, isolate));

    for (unsigned i = 0, length = iterable->Length(); i < length; ++i) {
        // Array-holes should not be skipped by for-of iteration semantics.
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Value>, nextValue, iterable->Get(i));
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, nextPromise, V8PromiseCustom::toPromise(nextValue, isolate));
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Value>, unused, V8PromiseCustom::then(nextPromise, onFulfilled, onRejected, isolate));
    }
    v8SetReturnValue(info, promise);
}

void V8Promise::allMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    v8::Isolate* isolate = info.GetIsolate();
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, promise, V8PromiseCustom::createPromise(info.Holder(), isolate));
    v8::Local<v8::Array> results = v8::Array::New(info.GetIsolate());

    if (!info.Length() || !info[0]->IsArray()) {
        V8PromiseCustom::resolve(promise, results, isolate);
        v8SetReturnValue(info, promise);
        return;
    }

    // FIXME: Now we limit the iterable type to the Array type.
    v8::Local<v8::Array> iterable = info[0].As<v8::Array>();

    if (!iterable->Length()) {
        V8PromiseCustom::resolve(promise, results, isolate);
        v8SetReturnValue(info, promise);
        return;
    }

    v8::Local<v8::ObjectTemplate> objectTemplate = primitiveWrapperObjectTemplate(isolate);
    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, countdownWrapper, objectTemplate->NewInstance());
    countdownWrapper->SetInternalField(V8PromiseCustom::PrimitiveWrapperPrimitiveIndex, v8::Integer::New(isolate, iterable->Length()));

    V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Function>, onRejected, createClosure(promiseRejectCallback, promise, isolate));
    for (unsigned i = 0, length = iterable->Length(); i < length; ++i) {
        // Array-holes should not be skipped by for-of iteration semantics.
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, environment, promiseAllEnvironment(promise, countdownWrapper, i, results, isolate));
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Function>, onFulfilled, createClosure(promiseAllFulfillCallback, environment, isolate));
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Value>, nextValue, iterable->Get(i));
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Object>, nextPromise, V8PromiseCustom::toPromise(nextValue, isolate));
        V8TRYCATCH_VOID_EMPTY(v8::Local<v8::Value>, unused, V8PromiseCustom::then(nextPromise, onFulfilled, onRejected, isolate));
    }
    v8SetReturnValue(info, promise);
}

//
// -- V8PromiseCustom --
v8::Local<v8::Object> V8PromiseCustom::createPromise(v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    v8::Local<v8::ObjectTemplate> internalTemplate = internalObjectTemplate(isolate);
    v8::Local<v8::Object> internal = internalTemplate->NewInstance();
    if (internal.IsEmpty())
        return v8::Local<v8::Object>();
    v8::Local<v8::Object> promise = V8DOMWrapper::createWrapper(creationContext, &V8Promise::wrapperTypeInfo, 0, isolate);

    clearDerived(internal, isolate);
    promise->SetInternalField(v8DOMWrapperObjectIndex, internal);

    ExecutionContext* context = currentExecutionContext(isolate);
    if (InspectorInstrumentation::isPromiseTrackerEnabled(context))
        InspectorInstrumentation::didCreatePromise(context, ScriptObject(ScriptState::forContext(isolate->GetCurrentContext()), promise));

    setStateForPromise(promise, Pending, v8::Undefined(isolate), isolate);
    return promise;
}

v8::Local<v8::Object> V8PromiseCustom::getInternal(v8::Handle<v8::Object> promise)
{
    v8::Local<v8::Value> value = promise->GetInternalField(v8DOMWrapperObjectIndex);
    return value.As<v8::Object>();
}

V8PromiseCustom::PromiseState V8PromiseCustom::getState(v8::Handle<v8::Object> internal)
{
    v8::Handle<v8::Value> value = internal->GetInternalField(V8PromiseCustom::InternalStateIndex);
    uint32_t number = toInt32(value);
    ASSERT(number == Pending || number == Fulfilled || number == Rejected || number == Following);
    return static_cast<PromiseState>(number);
}

bool V8PromiseCustom::isPromise(v8::Handle<v8::Value> maybePromise, v8::Isolate* isolate)
{
    return V8Promise::domTemplate(isolate)->HasInstance(maybePromise);
}

v8::Local<v8::Object> V8PromiseCustom::toPromise(v8::Handle<v8::Value> maybePromise, v8::Isolate* isolate)
{
    // FIXME: Currently we don't check [[PromiseConstructor]] since we limit
    // the creation of the promise objects only from the Blink Promise
    // constructor.
    if (isPromise(maybePromise, isolate))
        return maybePromise.As<v8::Object>();

    v8::Local<v8::Object> promise = createPromise(v8::Handle<v8::Object>(), isolate);
    if (promise.IsEmpty())
        return v8::Local<v8::Object>();
    resolve(promise, maybePromise, isolate);
    return promise;
}

void V8PromiseCustom::resolve(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> result, v8::Isolate* isolate)
{
    ASSERT(!result.IsEmpty());
    v8::Local<v8::Object> internal = getInternal(promise);
    PromiseState state = getState(internal);
    if (state != Pending)
        return;

    if (isPromise(result, isolate)) {
        v8::Local<v8::Object> valuePromise = result.As<v8::Object>();
        v8::Local<v8::Object> valueInternal = getInternal(valuePromise);
        PromiseState valueState = getState(valueInternal);
        if (promise->SameValue(valuePromise)) {
            v8::Local<v8::Value> reason = V8ThrowException::createTypeError("Resolve a promise with itself", isolate);
            setReason(promise, reason, isolate);
        } else if (valueState == Following) {
            v8::Local<v8::Object> valuePromiseFollowing = valueInternal->GetInternalField(InternalResultIndex).As<v8::Object>();
            setStateForPromise(promise, Following, valuePromiseFollowing, isolate);
            addToDerived(getInternal(valuePromiseFollowing), promise, v8::Handle<v8::Function>(), v8::Handle<v8::Function>(), isolate);
        } else if (valueState == Fulfilled) {
            setValue(promise, valueInternal->GetInternalField(InternalResultIndex), isolate);
        } else if (valueState == Rejected) {
            setReason(promise, valueInternal->GetInternalField(InternalResultIndex), isolate);
        } else {
            ASSERT(valueState == Pending);
            setStateForPromise(promise, Following, valuePromise, isolate);
            addToDerived(valueInternal, promise, v8::Handle<v8::Function>(), v8::Handle<v8::Function>(), isolate);
        }
    } else {
        setValue(promise, result, isolate);
    }
}

void V8PromiseCustom::reject(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> reason, v8::Isolate* isolate)
{
    v8::Local<v8::Object> internal = getInternal(promise);
    PromiseState state = getState(internal);
    if (state != Pending)
        return;
    setReason(promise, reason, isolate);
}

v8::Local<v8::Object> V8PromiseCustom::then(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Isolate* isolate)
{
    v8::Handle<v8::Object> internal = getInternal(promise);
    while (getState(internal) == Following) {
        promise = internal->GetInternalField(InternalResultIndex).As<v8::Object>();
        internal = getInternal(promise);
    }
    // FIXME: Currently we don't lookup "constructor" property since we limit
    // the creation of the promise objects only from the Blink Promise
    // constructor.
    v8::Local<v8::Object> derivedPromise = createPromise(v8::Handle<v8::Object>(), isolate);
    if (derivedPromise.IsEmpty())
        return v8::Local<v8::Object>();
    updateDerivedFromPromise(derivedPromise, onFulfilled, onRejected, promise, isolate);
    return derivedPromise;
}

void V8PromiseCustom::setValue(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    PromisePropagator propagator;
    propagator.setValue(promise, value, isolate);
    propagator.performPropagation(isolate);
}

void V8PromiseCustom::setReason(v8::Handle<v8::Object> promise, v8::Handle<v8::Value> reason, v8::Isolate* isolate)
{
    PromisePropagator propagator;
    propagator.setReason(promise, reason, isolate);
    propagator.performPropagation(isolate);
}

void V8PromiseCustom::propagateToDerived(v8::Handle<v8::Object> promise, v8::Isolate* isolate)
{
    PromisePropagator propagator;
    propagator.propagateToDerived(promise, isolate);
    propagator.performPropagation(isolate);
}

void V8PromiseCustom::updateDerived(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> originator, v8::Isolate* isolate)
{
    PromisePropagator propagator;
    propagator.updateDerived(derivedPromise, onFulfilled, onRejected, originator, isolate);
    propagator.performPropagation(isolate);
}

void V8PromiseCustom::updateDerivedFromValue(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Value> value, v8::Isolate* isolate)
{
    PromisePropagator propagator;
    propagator.updateDerivedFromValue(derivedPromise, onFulfilled, value, isolate);
    propagator.performPropagation(isolate);
}

void V8PromiseCustom::updateDerivedFromReason(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Value> reason, v8::Isolate* isolate)
{
    PromisePropagator propagator;
    propagator.updateDerivedFromReason(derivedPromise, onRejected, reason, isolate);
    propagator.performPropagation(isolate);
}

void V8PromiseCustom::updateDerivedFromPromise(v8::Handle<v8::Object> derivedPromise, v8::Handle<v8::Function> onFulfilled, v8::Handle<v8::Function> onRejected, v8::Handle<v8::Object> promise, v8::Isolate* isolate)
{
    PromisePropagator propagator;
    propagator.updateDerivedFromPromise(derivedPromise, onFulfilled, onRejected, promise, isolate);
    propagator.performPropagation(isolate);
}

v8::Local<v8::Object> V8PromiseCustom::coerceThenable(v8::Handle<v8::Object> thenable, v8::Handle<v8::Function> then, v8::Isolate* isolate)
{
    ASSERT(!thenable.IsEmpty());
    ASSERT(!then.IsEmpty());
    v8::Local<v8::Object> promise = createPromise(v8::Handle<v8::Object>(), isolate);
    if (promise.IsEmpty())
        return v8::Local<v8::Object>();
    v8::Handle<v8::Value> onFulfilled = createClosure(promiseResolveCallback, promise, isolate);
    if (onFulfilled.IsEmpty())
        return v8::Local<v8::Object>();
    v8::Handle<v8::Value> onRejected = createClosure(promiseRejectCallback, promise, isolate);
    if (onRejected.IsEmpty())
        return v8::Local<v8::Object>();
    v8::Handle<v8::Value> argv[] = { onFulfilled, onRejected };

    v8::TryCatch trycatch;
    if (V8ScriptRunner::callFunction(then, currentExecutionContext(isolate), thenable, WTF_ARRAY_LENGTH(argv), argv, isolate).IsEmpty()) {
        reject(promise, trycatch.Exception(), isolate);
    }
    V8HiddenValue::setHiddenValue(isolate, thenable, V8HiddenValue::thenableHiddenPromise(isolate), promise);
    return promise;
}

void V8PromiseCustom::callHandler(v8::Handle<v8::Object> promise, v8::Handle<v8::Function> handler, v8::Handle<v8::Value> argument, PromiseState originatorState, v8::Isolate* isolate)
{
    ASSERT(originatorState == Fulfilled || originatorState == Rejected);
    ExecutionContext* executionContext = currentExecutionContext(isolate);
    ASSERT(executionContext && executionContext->isContextThread());
    executionContext->postTask(adoptPtr(new CallHandlerTask(promise, handler, argument, originatorState, isolate, executionContext)));
}

} // namespace WebCore
