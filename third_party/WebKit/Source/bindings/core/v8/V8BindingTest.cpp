// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/V8Binding.h"

#include "core/testing/GarbageCollectedScriptWrappable.h"
#include "core/testing/RefCountedScriptWrappable.h"
#include "platform/heap/Heap.h"
#include "wtf/Vector.h"

#include <gtest/gtest.h>
#include <v8.h>

#define CHECK_TOV8VALUE(expected, value) check(expected, value, __FILE__, __LINE__)

namespace blink {

namespace {

class V8ValueTraitsTest : public ::testing::Test {
public:

    V8ValueTraitsTest()
        : m_scope(v8::Isolate::GetCurrent())
    {
    }

    virtual ~V8ValueTraitsTest()
    {
    }

    template <typename T> v8::Local<v8::Value> toV8Value(const T& value)
    {
        return V8ValueTraits<T>::toV8Value(value, m_scope.scriptState()->context()->Global(), m_scope.isolate());
    }

    template <typename T>
    void check(const char* expected, const T& value, const char* path, int lineNumber)
    {
        v8::Local<v8::Value> actual = toV8Value(value);
        if (actual.IsEmpty()) {
            ADD_FAILURE_AT(path, lineNumber) << "toV8Value returns an empty value.";
            return;
        }
        String actualString = toCoreString(actual->ToString());
        if (String(expected) != actualString) {
            ADD_FAILURE_AT(path, lineNumber) << "toV8Value returns an incorrect value.\n  Actual: " << actualString.utf8().data() << "\nExpected: " << expected;
            return;
        }
    }

    V8TestingScope m_scope;
};

class GarbageCollectedHolder : public GarbageCollectedFinalized<GarbageCollectedHolder> {
public:
    GarbageCollectedHolder(GarbageCollectedScriptWrappable* scriptWrappable)
        : m_scriptWrappable(scriptWrappable) { }

    void trace(Visitor* visitor) { visitor->trace(m_scriptWrappable); }

    // This should be public in order to access a Member<X> object.
    Member<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

class OffHeapGarbageCollectedHolder {
public:
    OffHeapGarbageCollectedHolder(GarbageCollectedScriptWrappable* scriptWrappable)
        : m_scriptWrappable(scriptWrappable) { }

    // This should be public in order to access a Persistent<X> object.
    Persistent<GarbageCollectedScriptWrappable> m_scriptWrappable;
};

TEST_F(V8ValueTraitsTest, numeric)
{
    CHECK_TOV8VALUE("0", static_cast<int>(0));
    CHECK_TOV8VALUE("1", static_cast<int>(1));
    CHECK_TOV8VALUE("-1", static_cast<int>(-1));

    CHECK_TOV8VALUE("-2", static_cast<long>(-2));
    CHECK_TOV8VALUE("2", static_cast<unsigned>(2));
    CHECK_TOV8VALUE("2", static_cast<unsigned long>(2));

    CHECK_TOV8VALUE("0.5", static_cast<double>(0.5));
    CHECK_TOV8VALUE("-0.5", static_cast<float>(-0.5));
}

TEST_F(V8ValueTraitsTest, boolean)
{
    CHECK_TOV8VALUE("true", true);
    CHECK_TOV8VALUE("false", false);
}

TEST_F(V8ValueTraitsTest, string)
{
    char arrayString[] = "arrayString";
    const char constArrayString[] = "constArrayString";
    CHECK_TOV8VALUE("arrayString", arrayString);
    CHECK_TOV8VALUE("constArrayString", constArrayString);
    CHECK_TOV8VALUE("pointer", const_cast<char*>(static_cast<const char*>("pointer")));
    CHECK_TOV8VALUE("constpointer", static_cast<const char*>("constpointer"));
    CHECK_TOV8VALUE("coreString", String("coreString"));
    CHECK_TOV8VALUE("atomicString", AtomicString("atomicString"));

    // Null strings are converted to empty strings.
    CHECK_TOV8VALUE("", static_cast<char*>(nullptr));
    CHECK_TOV8VALUE("", static_cast<const char*>(nullptr));
    CHECK_TOV8VALUE("", String());
    CHECK_TOV8VALUE("", AtomicString());
}

TEST_F(V8ValueTraitsTest, nullType)
{
    CHECK_TOV8VALUE("null", V8NullType());
}

TEST_F(V8ValueTraitsTest, undefinedType)
{
    CHECK_TOV8VALUE("undefined", V8UndefinedType());
}

TEST_F(V8ValueTraitsTest, refCountedScriptWrappable)
{
    RefPtr<RefCountedScriptWrappable> object = RefCountedScriptWrappable::create("hello");

    CHECK_TOV8VALUE("hello", object);
    CHECK_TOV8VALUE("hello", object.get());
    CHECK_TOV8VALUE("hello", object.release());

    ASSERT_FALSE(object);

    CHECK_TOV8VALUE("null", object);
    CHECK_TOV8VALUE("null", object.get());
    CHECK_TOV8VALUE("null", object.release());
}

TEST_F(V8ValueTraitsTest, garbageCollectedScriptWrappable)
{
    GarbageCollectedScriptWrappable* object = new GarbageCollectedScriptWrappable("world");
    GarbageCollectedHolder holder(object);
    OffHeapGarbageCollectedHolder offHeapHolder(object);

    CHECK_TOV8VALUE("world", object);
    CHECK_TOV8VALUE("world", RawPtr<GarbageCollectedScriptWrappable>(object));
    CHECK_TOV8VALUE("world", holder.m_scriptWrappable);
    CHECK_TOV8VALUE("world", offHeapHolder.m_scriptWrappable);

    object = nullptr;
    holder.m_scriptWrappable = nullptr;
    offHeapHolder.m_scriptWrappable = nullptr;

    CHECK_TOV8VALUE("null", object);
    CHECK_TOV8VALUE("null", RawPtr<GarbageCollectedScriptWrappable>(object));
    CHECK_TOV8VALUE("null", holder.m_scriptWrappable);
    CHECK_TOV8VALUE("null", offHeapHolder.m_scriptWrappable);
}

TEST_F(V8ValueTraitsTest, vector)
{
    Vector<RefPtr<RefCountedScriptWrappable> > v;
    v.append(RefCountedScriptWrappable::create("foo"));
    v.append(RefCountedScriptWrappable::create("bar"));

    CHECK_TOV8VALUE("foo,bar", v);
}

TEST_F(V8ValueTraitsTest, heapVector)
{
    HeapVector<Member<GarbageCollectedScriptWrappable> > v;
    v.append(new GarbageCollectedScriptWrappable("hoge"));
    v.append(new GarbageCollectedScriptWrappable("fuga"));
    v.append(nullptr);

    CHECK_TOV8VALUE("hoge,fuga,", v);
}

TEST_F(V8ValueTraitsTest, scriptValue)
{
    ScriptValue value(m_scope.scriptState(), v8::Number::New(m_scope.isolate(), 1234));

    CHECK_TOV8VALUE("1234", value);
}

TEST_F(V8ValueTraitsTest, v8Value)
{
    v8::Local<v8::Value> localValue(v8::Number::New(m_scope.isolate(), 1234));
    v8::Handle<v8::Value> handleValue(v8::Number::New(m_scope.isolate(), 5678));

    CHECK_TOV8VALUE("1234", localValue);
    CHECK_TOV8VALUE("5678", handleValue);
}

} // namespace

} // namespace blink
