// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/v8/PromiseTracker.h"

#include "core/inspector/v8/V8DebuggerAgentImpl.h"
#include "core/inspector/v8/V8StackTraceImpl.h"
#include "wtf/CurrentTime.h"
#include "wtf/PassOwnPtr.h"

using blink::TypeBuilder::Array;
using blink::TypeBuilder::Runtime::CallFrame;
using blink::TypeBuilder::Debugger::PromiseDetails;

namespace blink {

class PromiseTracker::PromiseWrapper {
public:
    PromiseWrapper(PromiseTracker* tracker, int id, v8::Local<v8::Object> promise)
        : m_tracker(tracker)
        , m_id(id)
        , m_promise(tracker->m_isolate, promise) { }

    ~PromiseWrapper()
    {
        RefPtr<PromiseDetails> promiseDetails = PromiseDetails::create().setId(m_id);
        m_tracker->m_agent->didUpdatePromise(InspectorFrontend::Debugger::EventType::Gc, promiseDetails.release());
    }

private:
    friend class PromiseTracker;

    PromiseTracker* m_tracker;
    int m_id;
    v8::Global<v8::Object> m_promise;
};

PromiseTracker::PromiseTracker(V8DebuggerAgentImpl* agent, v8::Isolate* isolate)
    : m_circularSequentialId(0)
    , m_isEnabled(false)
    , m_captureStacks(false)
    , m_agent(agent)
    , m_isolate(isolate)
{
    clear();
}

PromiseTracker::~PromiseTracker()
{
}

void PromiseTracker::setEnabled(bool enabled, bool captureStacks)
{
    m_isEnabled = enabled;
    m_captureStacks = captureStacks;
    if (!enabled)
        clear();
}

void PromiseTracker::clear()
{
    v8::HandleScope scope(m_isolate);
    m_promiseToId.Reset(m_isolate, v8::NativeWeakMap::New(m_isolate));
    m_idToPromise.clear();
}

int PromiseTracker::circularSequentialId()
{
    ++m_circularSequentialId;
    if (m_circularSequentialId <= 0)
        m_circularSequentialId = 1;
    return m_circularSequentialId;
}

void PromiseTracker::weakCallback(const v8::WeakCallbackInfo<PromiseWrapper>& data)
{
    PromiseWrapper* wrapper = data.GetParameter();
    wrapper->m_tracker->m_idToPromise.remove(wrapper->m_id);
}

void PromiseTracker::promiseCollected(int id)
{
}

int PromiseTracker::promiseId(v8::Local<v8::Object> promise, bool* isNewPromise)
{
    v8::HandleScope scope(m_isolate);
    v8::Local<v8::NativeWeakMap> map = v8::Local<v8::NativeWeakMap>::New(m_isolate, m_promiseToId);
    v8::Local<v8::Value> value = map->Get(promise);
    if (value->IsInt32()) {
        *isNewPromise = false;
        return value.As<v8::Int32>()->Value();
    }
    *isNewPromise = true;
    int id = circularSequentialId();
    map->Set(promise, v8::Int32::New(m_isolate, id));

    OwnPtr<PromiseWrapper> wrapper = adoptPtr(new PromiseWrapper(this, id, promise));
    wrapper->m_promise.SetWeak(wrapper.get(), weakCallback, v8::WeakCallbackType::kParameter);
    m_idToPromise.set(id, wrapper.release());
    return id;
}

void PromiseTracker::didReceiveV8PromiseEvent(v8::Local<v8::Context> context, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status)
{
    ASSERT(isEnabled());
    ASSERT(!context.IsEmpty());

    bool isNewPromise = false;
    int id = promiseId(promise, &isNewPromise);

    InspectorFrontend::Debugger::EventType::Enum eventType = isNewPromise ? InspectorFrontend::Debugger::EventType::New : InspectorFrontend::Debugger::EventType::Update;

    PromiseDetails::Status::Enum promiseStatus;
    switch (status) {
    case 0:
        promiseStatus = PromiseDetails::Status::Pending;
        break;
    case 1:
        promiseStatus = PromiseDetails::Status::Resolved;
        break;
    default:
        promiseStatus = PromiseDetails::Status::Rejected;
    };
    RefPtr<PromiseDetails> promiseDetails = PromiseDetails::create().setId(id);
    promiseDetails->setStatus(promiseStatus);

    if (!parentPromise.IsEmpty() && parentPromise->IsObject()) {
        v8::Local<v8::Object> handle = parentPromise->ToObject(context->GetIsolate());
        bool parentIsNewPromise = false;
        int parentPromiseId = promiseId(handle, &parentIsNewPromise);
        promiseDetails->setParentId(parentPromiseId);
    } else {
        if (!status) {
            if (isNewPromise) {
                promiseDetails->setCreationTime(currentTimeMS());
                OwnPtr<V8StackTraceImpl> stack = V8StackTraceImpl::capture(m_agent, m_captureStacks ? V8StackTraceImpl::maxCallStackSizeToCapture : 1);
                if (stack)
                    promiseDetails->setCreationStack(stack->buildInspectorObject());
            }
        } else {
            promiseDetails->setSettlementTime(currentTimeMS());
            if (m_captureStacks) {
                OwnPtr<V8StackTraceImpl> stack = V8StackTraceImpl::capture(m_agent, V8StackTrace::maxCallStackSizeToCapture);
                if (stack)
                    promiseDetails->setSettlementStack(stack->buildInspectorObject());
            }
        }
    }

    m_agent->didUpdatePromise(eventType, promiseDetails.release());
}

v8::Local<v8::Object> PromiseTracker::promiseById(int promiseId)
{
    ASSERT(isEnabled());
    PromiseWrapper* wrapper = m_idToPromise.get(promiseId);
    return wrapper ? wrapper->m_promise.Get(m_isolate) : v8::Local<v8::Object>();
}

} // namespace blink
