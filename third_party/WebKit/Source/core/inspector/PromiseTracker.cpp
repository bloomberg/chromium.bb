// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/PromiseTracker.h"

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptCallStackFactory.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/inspector/ScriptAsyncCallStack.h"
#include "wtf/CurrentTime.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/WeakPtr.h"

using blink::TypeBuilder::Array;
using blink::TypeBuilder::Console::CallFrame;
using blink::TypeBuilder::Debugger::PromiseDetails;

namespace blink {

class PromiseTracker::PromiseData final : public RefCountedWillBeGarbageCollectedFinalized<PromiseData> {
public:
    static PassRefPtrWillBeRawPtr<PromiseData> create(ScriptState* scriptState, int promiseHash, int promiseId, v8::Local<v8::Object> promise)
    {
        return adoptRefWillBeNoop(new PromiseData(scriptState, promiseHash, promiseId, promise));
    }

    int promiseHash() const { return m_promiseHash; }
    int promiseId() const { return m_promiseId; }
    ScopedPersistent<v8::Object>& promise() { return m_promise; }

#if ENABLE(OILPAN)
    void dispose()
    {
        m_promise.clear();
        m_parentPromise.clear();
    }
#else
    WeakPtr<PromiseData> createWeakPtr()
    {
        return m_weakPtrFactory.createWeakPtr();
    }
#endif

    PassRefPtr<PromiseDetails> toPromiseDetails() const
    {
        PromiseDetails::Status::Enum status;
        if (!m_status)
            status = PromiseDetails::Status::Pending;
        else if (m_status == 1)
            status = PromiseDetails::Status::Resolved;
        else
            status = PromiseDetails::Status::Rejected;
        RefPtr<PromiseDetails> promiseDetails = PromiseDetails::create()
            .setId(m_promiseId)
            .setStatus(status);
        if (m_parentPromiseId)
            promiseDetails->setParentId(m_parentPromiseId);
        if (m_creationStack)
            promiseDetails->setCallFrame(m_creationStack->at(0).buildInspectorObject());
        if (m_creationTime)
            promiseDetails->setCreationTime(m_creationTime);
        if (m_settlementTime)
            promiseDetails->setSettlementTime(m_settlementTime);
        if (m_creationStack && m_fullCreationStack) {
            promiseDetails->setCreationStack(m_creationStack->buildInspectorArray());
            RefPtrWillBeRawPtr<ScriptAsyncCallStack> asyncCallStack = m_creationStack->asyncCallStack();
            if (asyncCallStack)
                promiseDetails->setAsyncCreationStack(asyncCallStack->buildInspectorObject());
        }
        if (m_settlementStack) {
            promiseDetails->setSettlementStack(m_settlementStack->buildInspectorArray());
            RefPtrWillBeRawPtr<ScriptAsyncCallStack> asyncCallStack = m_settlementStack->asyncCallStack();
            if (asyncCallStack)
                promiseDetails->setAsyncSettlementStack(asyncCallStack->buildInspectorObject());
        }
        return promiseDetails.release();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_creationStack);
        visitor->trace(m_settlementStack);
    }

private:
    friend class PromiseTracker;

    PromiseData(ScriptState* scriptState, int promiseHash, int promiseId, v8::Local<v8::Object> promise)
        : m_scriptState(scriptState)
        , m_promiseHash(promiseHash)
        , m_promiseId(promiseId)
        , m_promise(scriptState->isolate(), promise)
        , m_parentPromiseId(0)
        , m_status(0)
        , m_fullCreationStack(false)
        , m_creationTime(0)
        , m_settlementTime(0)
#if !ENABLE(OILPAN)
        , m_weakPtrFactory(this)
#endif
    {
    }

    RefPtr<ScriptState> m_scriptState;
    int m_promiseHash;
    int m_promiseId;
    ScopedPersistent<v8::Object> m_promise;
    int m_parentPromiseId;
    int m_status;
    RefPtrWillBeMember<ScriptCallStack> m_creationStack;
    RefPtrWillBeMember<ScriptCallStack> m_settlementStack;
    bool m_fullCreationStack;
    ScopedPersistent<v8::Object> m_parentPromise;
    double m_creationTime;
    double m_settlementTime;
#if !ENABLE(OILPAN)
    WeakPtrFactory<PromiseData> m_weakPtrFactory;
#endif
};

static int indexOf(PromiseTracker::PromiseDataVector* vector, const ScopedPersistent<v8::Object>& promise)
{
    for (size_t index = 0; index < vector->size(); ++index) {
        if (vector->at(index)->promise() == promise)
            return index;
    }
    return -1;
}

namespace {

class PromiseDataWrapper final : public NoBaseWillBeGarbageCollected<PromiseDataWrapper> {
public:
    static PassOwnPtrWillBeRawPtr<PromiseDataWrapper> create(PromiseTracker::PromiseData* data, PromiseTracker* tracker)
    {
#if ENABLE(OILPAN)
        return new PromiseDataWrapper(data, tracker);
#else
        return adoptPtr(new PromiseDataWrapper(data->createWeakPtr(), tracker));
#endif
    }

#if ENABLE(OILPAN)
    static void didRemovePromise(const v8::WeakCallbackData<v8::Object, Persistent<PromiseDataWrapper> >& data)
#else
    static void didRemovePromise(const v8::WeakCallbackData<v8::Object, PromiseDataWrapper>& data)
#endif
    {
#if ENABLE(OILPAN)
        OwnPtr<Persistent<PromiseDataWrapper> > persistentWrapper = adoptPtr(data.GetParameter());
        RawPtr<PromiseDataWrapper> wrapper = *persistentWrapper;
#else
        OwnPtr<PromiseDataWrapper> wrapper = adoptPtr(data.GetParameter());
#endif
        WeakPtrWillBeRawPtr<PromiseTracker::PromiseData> promiseData = wrapper->m_data;
        if (!promiseData || !wrapper->m_tracker)
            return;

        PromiseTracker::Listener* listener = wrapper->m_tracker->listener();
        if (listener)
            listener->didUpdatePromise(InspectorFrontend::Debugger::EventType::Gc, promiseData->toPromiseDetails());

        wrapper->m_tracker->promiseIdToDataMap().remove(promiseData->promiseId());

#if ENABLE(OILPAN)
        // Oilpan: let go of ScopedPersistent<>s right here (and not wait until the
        // PromiseDataWrapper is GCed later.) The v8 weak callback handling expects
        // to see the callback data upon return.
        promiseData->dispose();
#endif
        PromiseTracker::PromiseDataMap& map = wrapper->m_tracker->promiseDataMap();
        int promiseHash = promiseData->promiseHash();

        PromiseTracker::PromiseDataMap::iterator it = map.find(promiseHash);
        // The PromiseTracker may have been disabled (and, possibly, re-enabled later),
        // leaving the promiseHash as unmapped.
        if (it == map.end())
            return;

        PromiseTracker::PromiseDataVector* vector = &it->value;
        int index = indexOf(vector, promiseData->promise());
        ASSERT(index >= 0);
        vector->remove(index);
        if (vector->isEmpty())
            map.remove(promiseHash);
    }

    DEFINE_INLINE_TRACE()
    {
#if ENABLE(OILPAN)
        visitor->trace(m_data);
        visitor->trace(m_tracker);
#endif
    }

private:
    PromiseDataWrapper(WeakPtrWillBeRawPtr<PromiseTracker::PromiseData> data, PromiseTracker* tracker)
        : m_data(data)
        , m_tracker(tracker)
    {
    }

    WeakPtrWillBeWeakMember<PromiseTracker::PromiseData> m_data;
    RawPtrWillBeWeakMember<PromiseTracker> m_tracker;
};

}

PromiseTracker::PromiseTracker(Listener* listener)
    : m_circularSequentialId(0)
    , m_isEnabled(false)
    , m_captureStacks(false)
    , m_listener(listener)
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(PromiseTracker);

DEFINE_TRACE(PromiseTracker)
{
#if ENABLE(OILPAN)
    visitor->trace(m_promiseDataMap);
    visitor->trace(m_promiseIdToDataMap);
    visitor->trace(m_listener);
#endif
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
    m_promiseDataMap.clear();
    m_promiseIdToDataMap.clear();
}

int PromiseTracker::circularSequentialId()
{
    ++m_circularSequentialId;
    if (m_circularSequentialId <= 0)
        m_circularSequentialId = 1;
    return m_circularSequentialId;
}

PassRefPtrWillBeRawPtr<PromiseTracker::PromiseData> PromiseTracker::createPromiseDataIfNeeded(ScriptState* scriptState, v8::Local<v8::Object> promise)
{
    int promiseHash = promise->GetIdentityHash();
    RawPtr<PromiseDataVector> vector = nullptr;
    PromiseDataMap::iterator it = m_promiseDataMap.find(promiseHash);
    if (it != m_promiseDataMap.end()) {
        vector = &it->value;
        int index = indexOf(vector, ScopedPersistent<v8::Object>(scriptState->isolate(), promise));
        if (index != -1)
            return vector->at(index);
    } else {
        vector = &m_promiseDataMap.add(promiseHash, PromiseDataVector()).storedValue->value;
    }

    // FIXME: Consider using the ScriptState's DOMWrapperWorld instead
    // to handle the lifetime of PromiseDataWrapper, avoiding all this
    // manual labor to achieve the same, with and without Oilpan.
    int promiseId = circularSequentialId();
    RefPtrWillBeRawPtr<PromiseData> data = PromiseData::create(scriptState, promiseHash, promiseId, promise);
    OwnPtrWillBeRawPtr<PromiseDataWrapper> dataWrapper = PromiseDataWrapper::create(data.get(), this);
#if ENABLE(OILPAN)
    OwnPtr<Persistent<PromiseDataWrapper> > wrapper = adoptPtr(new Persistent<PromiseDataWrapper>(dataWrapper));
#else
    OwnPtr<PromiseDataWrapper> wrapper = dataWrapper.release();
#endif
    data->m_promise.setWeak(wrapper.leakPtr(), &PromiseDataWrapper::didRemovePromise);
    vector->append(data);

    m_promiseIdToDataMap.set(promiseId, data);

    return data.release();
}

void PromiseTracker::didReceiveV8PromiseEvent(ScriptState* scriptState, v8::Local<v8::Object> promise, v8::Local<v8::Value> parentPromise, int status)
{
    ASSERT(isEnabled());
    ASSERT(scriptState->contextIsValid());

    ScriptState::Scope scope(scriptState);
    InspectorFrontend::Debugger::EventType::Enum eventType = InspectorFrontend::Debugger::EventType::Update;

    RefPtrWillBeRawPtr<PromiseData> data = createPromiseDataIfNeeded(scriptState, promise);
    if (!parentPromise.IsEmpty() && parentPromise->IsObject()) {
        v8::Local<v8::Object> handle = parentPromise->ToObject(scriptState->isolate());
        RefPtrWillBeRawPtr<PromiseData> parentData = createPromiseDataIfNeeded(scriptState, handle);
        data->m_parentPromiseId = parentData->m_promiseId;
        data->m_parentPromise.set(scriptState->isolate(), handle);
    } else {
        ASSERT(!data->m_status);
        data->m_status = status;
        if (!status) {
            if (!data->m_creationTime) {
                data->m_creationTime = currentTimeMS();
                eventType = InspectorFrontend::Debugger::EventType::New;
            }
            if (!data->m_creationStack) {
                RefPtrWillBeRawPtr<ScriptCallStack> stack = createScriptCallStack(m_captureStacks ? ScriptCallStack::maxCallStackSizeToCapture : 1, true);
                if (stack && stack->size()) {
                    data->m_creationStack = stack;
                    data->m_fullCreationStack = m_captureStacks;
                }
            }
        } else if (!data->m_settlementTime) {
            data->m_settlementTime = currentTimeMS();
            if (m_captureStacks) {
                RefPtrWillBeRawPtr<ScriptCallStack> stack = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);
                if (stack && stack->size())
                    data->m_settlementStack = stack;
            }
        }
    }

    if (m_listener)
        m_listener->didUpdatePromise(eventType, data->toPromiseDetails());
}

PassRefPtr<Array<PromiseDetails> > PromiseTracker::promises()
{
    ASSERT(isEnabled());
    RefPtr<Array<PromiseDetails> > result = Array<PromiseDetails>::create();
    for (auto& data : m_promiseDataMap) {
        PromiseDataVector* vector = &data.value;
        for (size_t index = 0; index < vector->size(); ++index)
            result->addItem(vector->at(index)->toPromiseDetails());
    }
    return result.release();
}

ScriptValue PromiseTracker::promiseById(int promiseId) const
{
    ASSERT(isEnabled());

    PromiseIdToDataMap::const_iterator it = m_promiseIdToDataMap.find(promiseId);
    if (it == m_promiseIdToDataMap.end())
        return ScriptValue();
    RefPtrWillBeRawPtr<PromiseData> data = it->value;
    ASSERT(data && data->m_promiseId == promiseId);
    ScriptState* scriptState = data->m_scriptState.get();
    v8::HandleScope scope(scriptState->isolate());
    return ScriptValue(scriptState, data->m_promise.newLocal(scriptState->isolate()));
}

} // namespace blink
