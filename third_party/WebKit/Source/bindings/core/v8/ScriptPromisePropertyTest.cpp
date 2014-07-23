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
#include "core/testing/DummyPageHolder.h"
#include "core/testing/GCObservation.h"
#include "core/testing/GarbageCollectedScriptWrappable.h"
#include "core/testing/RefCountedScriptWrappable.h"
#include "platform/heap/Handle.h"
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

class GarbageCollectedHolder : public GarbageCollectedScriptWrappable {
public:
    typedef ScriptPromiseProperty<Member<GarbageCollectedScriptWrappable>, Member<GarbageCollectedScriptWrappable>, Member<GarbageCollectedScriptWrappable> > Property;
    GarbageCollectedHolder(ExecutionContext* executionContext)
        : GarbageCollectedScriptWrappable("holder")
        , m_property(new Property(executionContext, toGarbageCollectedScriptWrappable(), Property::Ready)) { }

    Property* property() { return m_property; }
    GarbageCollectedScriptWrappable* toGarbageCollectedScriptWrappable() { return this; }

    virtual void trace(Visitor *visitor) OVERRIDE
    {
        GarbageCollectedScriptWrappable::trace(visitor);
        visitor->trace(m_property);
    }

private:
    Member<Property> m_property;
};

class RefCountedHolder : public RefCountedScriptWrappable {
public:
    typedef ScriptPromiseProperty<RefCountedScriptWrappable*, RefCountedScriptWrappable*, RefCountedScriptWrappable*> Property;
    static PassRefPtr<RefCountedHolder> create(ExecutionContext* executionContext)
    {
        return adoptRef(new RefCountedHolder(executionContext));
    }
    Property* property() { return m_property; }
    RefCountedScriptWrappable* toRefCountedScriptWrappable() { return this; }

private:
    RefCountedHolder(ExecutionContext* executionContext)
        : RefCountedScriptWrappable("holder")
        , m_property(new Property(executionContext, toRefCountedScriptWrappable(), Property::Ready)) { }

    Persistent<Property> m_property;
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

class ScriptPromisePropertyTestBase {
public:
    ScriptPromisePropertyTestBase()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
    {
    }

    virtual ~ScriptPromisePropertyTestBase()
    {
        m_page.clear();
        gc();
        Heap::collectAllGarbage();
    }

    Document& document() { return m_page->document(); }
    v8::Isolate* isolate() { return toIsolate(&document()); }
    ScriptState* scriptState() { return ScriptState::forMainWorld(document().frame()); }

    virtual void destroyContext()
    {
        m_page.clear();
        gc();
        Heap::collectGarbage(ThreadState::HeapPointersOnStack);
    }

    void gc() { V8GCController::collectGarbage(v8::Isolate::GetCurrent()); }

    PassOwnPtr<ScriptFunction> notReached() { return adoptPtr(new NotReached()); }
    PassOwnPtr<ScriptFunction> stub(ScriptValue& value, size_t& callCount) { return adoptPtr(new StubFunction(value, callCount)); }

    template <typename T>
    ScriptValue wrap(const T& value)
    {
        ScriptState::Scope scope(scriptState());
        return ScriptValue(scriptState(), V8ValueTraits<T>::toV8Value(value, scriptState()->context()->Global(), isolate()));
    }

private:
    OwnPtr<DummyPageHolder> m_page;
};

// This is the main test class.
// If you want to examine a testcase independent of holder types, place the
// test on this class.
class ScriptPromisePropertyGarbageCollectedTest : public ScriptPromisePropertyTestBase, public ::testing::Test {
public:
    typedef GarbageCollectedHolder::Property Property;

    ScriptPromisePropertyGarbageCollectedTest()
        : m_holder(new GarbageCollectedHolder(&document()))
    {
    }

    GarbageCollectedHolder* holder() { return m_holder; }
    Property* property() { return m_holder->property(); }
    ScriptPromise promise(DOMWrapperWorld& world) { return property()->promise(world); }

    virtual void destroyContext() OVERRIDE
    {
        m_holder = nullptr;
        ScriptPromisePropertyTestBase::destroyContext();
    }

private:
    Persistent<GarbageCollectedHolder> m_holder;
};

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_IsStableObject)
{
    ScriptPromise v = property()->promise(DOMWrapperWorld::mainWorld());
    ScriptPromise w = property()->promise(DOMWrapperWorld::mainWorld());
    EXPECT_EQ(v, w);
    EXPECT_FALSE(v.isEmpty());
    EXPECT_EQ(Property::Pending, property()->state());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_IsStableObjectAfterSettling)
{
    ScriptPromise v = promise(DOMWrapperWorld::mainWorld());
    GarbageCollectedScriptWrappable* value = new GarbageCollectedScriptWrappable("value");

    property()->resolve(value);
    EXPECT_EQ(Property::Resolved, property()->state());

    ScriptPromise w = promise(DOMWrapperWorld::mainWorld());
    EXPECT_EQ(v, w);
    EXPECT_FALSE(v.isEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_DoesNotImpedeGarbageCollection)
{
    ScriptValue holderWrapper = wrap(holder()->toGarbageCollectedScriptWrappable());

    RefPtrWillBePersistent<GCObservation> observation;
    {
        ScriptState::Scope scope(scriptState());
        observation = GCObservation::create(promise(DOMWrapperWorld::mainWorld()).v8Value());
    }

    gc();
    EXPECT_FALSE(observation->wasCollected());

    holderWrapper.clear();
    gc();
    EXPECT_TRUE(observation->wasCollected());

    EXPECT_EQ(Property::Pending, property()->state());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Resolve_ResolvesScriptPromise)
{
    ScriptValue actual;
    size_t nResolveCalls = 0;

    {
        ScriptState::Scope scope(scriptState());
        promise(DOMWrapperWorld::mainWorld()).then(stub(actual, nResolveCalls), notReached());
    }

    GarbageCollectedScriptWrappable* value = new GarbageCollectedScriptWrappable("value");
    property()->resolve(value);
    EXPECT_EQ(Property::Resolved, property()->state());

    isolate()->RunMicrotasks();
    EXPECT_EQ(1u, nResolveCalls);
    EXPECT_EQ(wrap(value), actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Reject_RejectsScriptPromise)
{
    GarbageCollectedScriptWrappable* reason = new GarbageCollectedScriptWrappable("reason");
    property()->reject(reason);
    EXPECT_EQ(Property::Rejected, property()->state());

    ScriptValue actual;
    size_t nRejectCalls = 0;

    {
        ScriptState::Scope scope(scriptState());
        promise(DOMWrapperWorld::mainWorld()).then(notReached(), stub(actual, nRejectCalls));
    }

    isolate()->RunMicrotasks();
    EXPECT_EQ(1u, nRejectCalls);
    EXPECT_EQ(wrap(reason), actual);
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Promise_DeadContext)
{
    Property* property = this->property();
    property->resolve(new GarbageCollectedScriptWrappable("value"));
    EXPECT_EQ(Property::Resolved, property->state());

    destroyContext();

    EXPECT_TRUE(property->promise(DOMWrapperWorld::mainWorld()).isEmpty());
}

TEST_F(ScriptPromisePropertyGarbageCollectedTest, Resolve_DeadContext)
{
    Property* property = this->property();

    {
        ScriptState::Scope scope(scriptState());
        property->promise(DOMWrapperWorld::mainWorld()).then(notReached(), notReached());
    }

    destroyContext();

    property->resolve(new GarbageCollectedScriptWrappable("value"));
    EXPECT_EQ(Property::Pending, property->state());

    v8::Isolate::GetCurrent()->RunMicrotasks();
}

// Tests that ScriptPromiseProperty works with a ref-counted holder.
class ScriptPromisePropertyRefCountedTest : public ScriptPromisePropertyTestBase, public ::testing::Test {
public:
    typedef RefCountedHolder::Property Property;

    ScriptPromisePropertyRefCountedTest()
        : m_holder(RefCountedHolder::create(&document()))
    {
    }

    RefCountedHolder* holder() { return m_holder.get(); }
    Property* property() { return m_holder->property(); }
    ScriptPromise promise(DOMWrapperWorld& world) { return property()->promise(world); }

private:
    RefPtr<RefCountedHolder> m_holder;
};

TEST_F(ScriptPromisePropertyRefCountedTest, Resolve)
{
    ScriptValue actual;
    size_t nResolveCalls = 0;

    {
        ScriptState::Scope scope(scriptState());
        promise(DOMWrapperWorld::mainWorld()).then(stub(actual, nResolveCalls), notReached());
    }

    RefPtr<RefCountedScriptWrappable> value = RefCountedScriptWrappable::create("value");
    property()->resolve(value.get());
    EXPECT_EQ(Property::Resolved, property()->state());

    isolate()->RunMicrotasks();
    EXPECT_EQ(1u, nResolveCalls);
    EXPECT_EQ(wrap(value), actual);
}

TEST_F(ScriptPromisePropertyRefCountedTest, Reject)
{
    ScriptValue actual;
    size_t nRejectCalls = 0;

    {
        ScriptState::Scope scope(scriptState());
        promise(DOMWrapperWorld::mainWorld()).then(notReached(), stub(actual, nRejectCalls));
    }

    RefPtr<RefCountedScriptWrappable> reason = RefCountedScriptWrappable::create("reason");
    property()->reject(reason.get());
    EXPECT_EQ(Property::Rejected, property()->state());

    isolate()->RunMicrotasks();
    EXPECT_EQ(1u, nRejectCalls);
    EXPECT_EQ(wrap(reason), actual);
}

// Tests that ScriptPromiseProperty works with a non ScriptWrappable resolution
// target.
class ScriptPromisePropertyNonScriptWrappableResolutionTargetTest : public ScriptPromisePropertyTestBase, public ::testing::Test {
public:
    template <typename T>
    void test(const T& value, const char* expected, const char* file, size_t line)
    {
        typedef ScriptPromiseProperty<Member<GarbageCollectedScriptWrappable>, T, V8UndefinedType> Property;
        Property* property = new Property(&document(), new GarbageCollectedScriptWrappable("holder"), Property::Ready);
        size_t nResolveCalls = 0;
        ScriptValue actualValue;
        String actual;
        {
            ScriptState::Scope scope(scriptState());
            property->promise(DOMWrapperWorld::mainWorld()).then(stub(actualValue, nResolveCalls), notReached());
        }
        property->resolve(value);
        isolate()->RunMicrotasks();
        {
            ScriptState::Scope scope(scriptState());
            actual = toCoreString(actualValue.v8Value()->ToString());
        }
        if (expected != actual) {
            ADD_FAILURE_AT(file, line) << "toV8Value returns an incorrect value.\n  Actual: " << actual.utf8().data() << "\nExpected: " << expected;
            return;
        }
    }
};

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest, ResolveWithUndefined)
{
    test(V8UndefinedType(), "undefined", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest, ResolveWithString)
{
    test(String("hello"), "hello", __FILE__, __LINE__);
}

TEST_F(ScriptPromisePropertyNonScriptWrappableResolutionTargetTest, ResolveWithInteger)
{
    test<int>(-1, "-1", __FILE__, __LINE__);
}

} // namespace
