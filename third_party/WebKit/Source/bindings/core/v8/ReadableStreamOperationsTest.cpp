// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "platform/heap/Handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <v8.h>

namespace blink {

namespace {

class NotReached : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState)
    {
        NotReached* self = new NotReached(scriptState);
        return self->bindToV8Function();
    }

private:
    explicit NotReached(ScriptState* scriptState)
        : ScriptFunction(scriptState)
    {
    }

    ScriptValue call(ScriptValue) override;
};

ScriptValue NotReached::call(ScriptValue)
{
    EXPECT_TRUE(false) << "'Unreachable' code was reached";
    return ScriptValue();
}

class Iteration final : public GarbageCollectedFinalized<Iteration> {
public:
    Iteration()
        : m_isSet(false)
        , m_isDone(false)
        , m_isValid(true) {}

    void set(ScriptValue v)
    {
        ASSERT(!v.isEmpty());
        m_isSet = true;
        v8::TryCatch block(v.scriptState()->isolate());
        v8::Local<v8::Value> value;
        v8::Local<v8::Value> item = v.v8Value();
        if (!item->IsObject() || !v8Call(v8UnpackIteratorResult(v.scriptState(), item.As<v8::Object>(), &m_isDone), value)) {
            m_isValid = false;
            return;
        }
        m_value = toCoreString(value->ToString());
    }

    bool isSet() const { return m_isSet; }
    bool isDone() const { return m_isDone; }
    bool isValid() const { return m_isValid; }
    const String& value() const { return m_value; }

    DEFINE_INLINE_TRACE() {}

private:
    bool m_isSet;
    bool m_isDone;
    bool m_isValid;
    String m_value;
};

class Function : public ScriptFunction {
public:
    static v8::Local<v8::Function> createFunction(ScriptState* scriptState, Iteration* iteration)
    {
        Function* self = new Function(scriptState, iteration);
        return self->bindToV8Function();
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_iteration);
        ScriptFunction::trace(visitor);
    }

private:
    Function(ScriptState* scriptState, Iteration* iteration)
        : ScriptFunction(scriptState)
        , m_iteration(iteration)
    {
    }

    ScriptValue call(ScriptValue value) override
    {
        m_iteration->set(value);
        return value;
    }

    Member<Iteration> m_iteration;
};

class ReadableStreamOperationsTest : public ::testing::Test {
public:
    ReadableStreamOperationsTest() : m_scope(v8::Isolate::GetCurrent()), m_block(isolate()) {}
    ~ReadableStreamOperationsTest() override
    {
        // Execute all pending microtasks
        isolate()->RunMicrotasks();
        EXPECT_FALSE(m_block.HasCaught());
    }

    ScriptState* scriptState() const { return m_scope.scriptState(); }
    v8::Isolate* isolate() const { return scriptState()->isolate(); }

    v8::MaybeLocal<v8::Value> eval(const char* s)
    {
        v8::Local<v8::String> source;
        v8::Local<v8::Script> script;
        if (!v8Call(v8::String::NewFromUtf8(isolate(), s, v8::NewStringType::kNormal), source)) {
            ADD_FAILURE();
            return v8::MaybeLocal<v8::Value>();
        }
        if (!v8Call(v8::Script::Compile(scriptState()->context(), source), script)) {
            ADD_FAILURE() << "Compilation fails";
            return v8::MaybeLocal<v8::Value>();
        }
        return script->Run(scriptState()->context());
    }
    v8::MaybeLocal<v8::Value> evalWithPrintingError(const char* s)
    {
        v8::TryCatch block(isolate());
        v8::MaybeLocal<v8::Value> r = eval(s);
        if (block.HasCaught()) {
            ADD_FAILURE() << toCoreString(block.Exception()->ToString(isolate())).utf8().data();
            block.ReThrow();
        }
        return r;
    }

    V8TestingScope m_scope;
    v8::TryCatch m_block;
};

TEST_F(ReadableStreamOperationsTest, IsReadableStream)
{
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scriptState(), v8::Undefined(isolate())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scriptState(), v8::Null(isolate())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scriptState(), v8::Object::New(isolate())));
    v8::Local<v8::Value> stream;
    ASSERT_TRUE(v8Call(evalWithPrintingError("new ReadableStream()"), stream));
    EXPECT_TRUE(ReadableStreamOperations::isReadableStream(scriptState(), stream));
}

TEST_F(ReadableStreamOperationsTest, IsReadableStreamReaderInvalid)
{
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(scriptState(), v8::Undefined(isolate())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(scriptState(), v8::Null(isolate())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(scriptState(), v8::Object::New(isolate())));
    v8::Local<v8::Value> stream;
    ASSERT_TRUE(v8Call(evalWithPrintingError("new ReadableStream()"), stream));

    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(scriptState(), stream));
}

TEST_F(ReadableStreamOperationsTest, GetReader)
{
    v8::Local<v8::Value> stream;
    ASSERT_TRUE(v8Call(evalWithPrintingError("new ReadableStream()"), stream));

    EXPECT_FALSE(ReadableStreamOperations::isLocked(scriptState(), stream));
    ScriptValue reader;
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(scriptState(), stream, es);
        ASSERT_FALSE(es.hadException());
    }
    EXPECT_TRUE(ReadableStreamOperations::isLocked(scriptState(), stream));
    ASSERT_FALSE(reader.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(scriptState(), reader.v8Value()));
    EXPECT_TRUE(ReadableStreamOperations::isReadableStreamReader(scriptState(), reader.v8Value()));

    // Already locked!
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(scriptState(), stream, es);
        ASSERT_TRUE(es.hadException());
    }
    ASSERT_TRUE(reader.isEmpty());
}

TEST_F(ReadableStreamOperationsTest, IsDisturbed)
{
    v8::Local<v8::Value> stream;
    ASSERT_TRUE(v8Call(evalWithPrintingError("stream = new ReadableStream()"), stream));

    EXPECT_FALSE(ReadableStreamOperations::isDisturbed(scriptState(), stream));

    ASSERT_FALSE(evalWithPrintingError("stream.cancel()").IsEmpty());

    EXPECT_TRUE(ReadableStreamOperations::isDisturbed(scriptState(), stream));
}

TEST_F(ReadableStreamOperationsTest, Read)
{
    v8::Local<v8::Value> reader;
    ASSERT_TRUE(v8Call(evalWithPrintingError(
        "var controller;"
        "function start(c) { controller = c; }"
        "new ReadableStream({start}).getReader()"), reader));
    ASSERT_TRUE(ReadableStreamOperations::isReadableStreamReader(scriptState(), reader));

    Iteration* it1 = new Iteration();
    Iteration* it2 = new Iteration();
    ReadableStreamOperations::read(scriptState(), reader).then(
        Function::createFunction(scriptState(), it1),
        NotReached::createFunction(scriptState()));
    ReadableStreamOperations::read(scriptState(), reader).then(
        Function::createFunction(scriptState(), it2),
        NotReached::createFunction(scriptState()));

    isolate()->RunMicrotasks();
    EXPECT_FALSE(it1->isSet());
    EXPECT_FALSE(it2->isSet());

    ASSERT_FALSE(evalWithPrintingError("controller.enqueue('hello')").IsEmpty());
    isolate()->RunMicrotasks();
    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("hello", it1->value());
    EXPECT_FALSE(it2->isSet());

    ASSERT_FALSE(evalWithPrintingError("controller.close()").IsEmpty());
    isolate()->RunMicrotasks();
    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("hello", it1->value());
    EXPECT_TRUE(it2->isSet());
    EXPECT_TRUE(it2->isValid());
    EXPECT_TRUE(it2->isDone());
}

} // namespace

} // namespace blink

