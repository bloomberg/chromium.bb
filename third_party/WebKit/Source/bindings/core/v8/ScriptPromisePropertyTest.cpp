// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/ScriptPromiseProperty.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/dom/Document.h"
#include "core/events/Event.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/GCObservation.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include <gtest/gtest.h>
#include <v8.h>

using namespace blink;

namespace {

class NotReached : public ScriptFunction {
public:
    NotReached() : ScriptFunction(v8::Isolate::GetCurrent()) { }

private:
    virtual ScriptValue call(ScriptValue) OVERRIDE;
};

ScriptValue NotReached::call(ScriptValue)
{
    EXPECT_TRUE(false) << "'Unreachable' code was reached";
    return ScriptValue();
}

class StubFunction : public ScriptFunction {
public:
    StubFunction(ScriptValue&, size_t& callCount);

private:
    virtual ScriptValue call(ScriptValue) OVERRIDE;

    ScriptValue& m_value;
    size_t& m_callCount;
};

StubFunction::StubFunction(ScriptValue& value, size_t& callCount)
    : ScriptFunction(v8::Isolate::GetCurrent())
    , m_value(value)
    , m_callCount(callCount)
{
}

ScriptValue StubFunction::call(ScriptValue arg)
{
    m_value = arg;
    m_callCount++;
    return ScriptValue();
}

class ScriptPromisePropertyTest : public ::testing::Test {
protected:
    ScriptPromisePropertyTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
    {
    }

    virtual ~ScriptPromisePropertyTest() { destroyContext(); }

    Document& document() { return m_page->document(); }
    v8::Isolate* isolate() { return toIsolate(&document()); }
    ScriptState* scriptState() { return ScriptState::forMainWorld(document().frame()); }

    void destroyContext()
    {
        m_page.clear();
        gc();
    }

    void gc() { V8GCController::collectGarbage(v8::Isolate::GetCurrent()); }

    PassOwnPtr<ScriptFunction> notReached() { return adoptPtr(new NotReached()); }
    PassOwnPtr<ScriptFunction> stub(ScriptValue& value, size_t& callCount) { return adoptPtr(new StubFunction(value, callCount)); }

    // These tests use Event because it is simple to manufacture lots
    // of events, and 'Ready' because it is an available property name
    // that won't bloat V8HiddenValue with a test property name.

    ScriptValue wrap(PassRefPtrWillBeRawPtr<Event> event)
    {
        ScriptState::Scope scope(scriptState());
        return ScriptValue(scriptState(), V8ValueTraits<PassRefPtrWillBeRawPtr<Event> >::toV8Value(event, scriptState()->context()->Global(), isolate()));
    }

    typedef ScriptPromiseProperty<RefPtrWillBeMember<Event>, RefPtrWillBeMember<Event>, RefPtrWillBeMember<Event> > Property;
    PassRefPtrWillBeRawPtr<Property> newProperty() { return Property::create(&document(), Event::create(), Property::Ready); }

private:
    OwnPtr<DummyPageHolder> m_page;
};

TEST_F(ScriptPromisePropertyTest, Promise_IsStableObject)
{
    RefPtr<Property> p(newProperty());
    ScriptPromise v = p->promise(DOMWrapperWorld::mainWorld());
    ScriptPromise w = p->promise(DOMWrapperWorld::mainWorld());
    EXPECT_EQ(v, w);
    EXPECT_FALSE(v.isEmpty());
    EXPECT_EQ(Property::Pending, p->state());
}

TEST_F(ScriptPromisePropertyTest, Promise_IsStableObjectAfterSettling)
{
    RefPtr<Property> p(newProperty());
    ScriptPromise v = p->promise(DOMWrapperWorld::mainWorld());

    RefPtrWillBeRawPtr<Event> value(Event::create());
    p->resolve(value.get());
    EXPECT_EQ(Property::Resolved, p->state());

    ScriptPromise w = p->promise(DOMWrapperWorld::mainWorld());
    EXPECT_EQ(v, w);
    EXPECT_FALSE(v.isEmpty());
}

TEST_F(ScriptPromisePropertyTest, Promise_DoesNotImpedeGarbageCollection)
{
    RefPtrWillBePersistent<Event> holder(Event::create());
    ScriptValue holderWrapper = wrap(holder);

    RefPtr<Property> p(Property::create(&document(), holder.get(), Property::Ready));

    RefPtrWillBePersistent<GCObservation> observation;
    {
        ScriptState::Scope scope(scriptState());
        observation = GCObservation::create(p->promise(DOMWrapperWorld::mainWorld()).v8Value());
    }

    gc();
    EXPECT_FALSE(observation->wasCollected());

    holderWrapper.clear();
    gc();
    EXPECT_TRUE(observation->wasCollected());

    EXPECT_EQ(Property::Pending, p->state());
}

TEST_F(ScriptPromisePropertyTest, Resolve_ResolvesScriptPromise)
{
    RefPtr<Property> p(newProperty());

    ScriptPromise promise = p->promise(DOMWrapperWorld::mainWorld());
    ScriptValue value;
    size_t nResolveCalls = 0;

    {
        ScriptState::Scope scope(scriptState());
        promise.then(stub(value, nResolveCalls), notReached());
    }

    RefPtrWillBeRawPtr<Event> event(Event::create());
    p->resolve(event.get());
    EXPECT_EQ(Property::Resolved, p->state());

    isolate()->RunMicrotasks();
    EXPECT_EQ(1u, nResolveCalls);
    EXPECT_EQ(wrap(event), value);
}

TEST_F(ScriptPromisePropertyTest, Reject_RejectsScriptPromise)
{
    RefPtr<Property> p(newProperty());

    RefPtrWillBeRawPtr<Event> event(Event::create());
    p->reject(event.get());
    EXPECT_EQ(Property::Rejected, p->state());

    ScriptValue value;
    size_t nRejectCalls = 0;

    {
        ScriptState::Scope scope(scriptState());
        p->promise(DOMWrapperWorld::mainWorld()).then(notReached(), stub(value, nRejectCalls));
    }

    isolate()->RunMicrotasks();
    EXPECT_EQ(1u, nRejectCalls);
    EXPECT_EQ(wrap(event), value);
}

TEST_F(ScriptPromisePropertyTest, Promise_DeadContext)
{
    RefPtr<Property> p(newProperty());

    RefPtrWillBeRawPtr<Event> event(Event::create());
    p->resolve(event.get());
    EXPECT_EQ(Property::Resolved, p->state());

    destroyContext();

    EXPECT_TRUE(p->promise(DOMWrapperWorld::mainWorld()).isEmpty());
}

TEST_F(ScriptPromisePropertyTest, Resolve_DeadContext)
{
    RefPtr<Property> p(newProperty());

    {
        ScriptState::Scope scope(scriptState());
        p->promise(DOMWrapperWorld::mainWorld()).then(notReached(), notReached());
    }

    destroyContext();

    RefPtrWillBeRawPtr<Event> event(Event::create());
    p->resolve(event.get());
    EXPECT_EQ(Property::Pending, p->state());

    v8::Isolate::GetCurrent()->RunMicrotasks();
}

} // namespace
