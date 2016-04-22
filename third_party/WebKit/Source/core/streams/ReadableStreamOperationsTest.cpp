// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/streams/ReadableStreamOperations.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/Document.h"
#include "core/streams/ReadableStreamController.h"
#include "core/streams/UnderlyingSourceBase.h"
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
        v8::TryCatch block(v.getScriptState()->isolate());
        v8::Local<v8::Value> value;
        v8::Local<v8::Value> item = v.v8Value();
        if (!item->IsObject() || !v8Call(v8UnpackIteratorResult(v.getScriptState(), item.As<v8::Object>(), &m_isDone), value)) {
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

class TestUnderlyingSource final : public UnderlyingSourceBase {
public:
    explicit TestUnderlyingSource(ScriptState* scriptState)
        : UnderlyingSourceBase(scriptState)
    {
    }

    // Just expose the controller methods for easy testing
    void enqueue(ScriptValue value) { controller()->enqueue(value); }
    void close() { controller()->close(); }
    void error(ScriptValue value) { controller()->error(value); }
    double desiredSize() { return controller()->desiredSize(); }
};

class ReadableStreamOperationsTest : public ::testing::Test {
public:
    ReadableStreamOperationsTest()
        : m_scope(v8::Isolate::GetCurrent())
        , m_block(isolate())
        , m_document(Document::create())
    {
        getScriptState()->setExecutionContext(m_document.get());
    }
    ~ReadableStreamOperationsTest() override
    {
        // Execute all pending microtasks
        v8::MicrotasksScope::PerformCheckpoint(isolate());
        EXPECT_FALSE(m_block.HasCaught());
    }

    ScriptState* getScriptState() const { return m_scope.getScriptState(); }
    v8::Isolate* isolate() const { return getScriptState()->isolate(); }

    ScriptValue eval(const char* s)
    {
        v8::Local<v8::String> source;
        v8::Local<v8::Script> script;
        v8::MicrotasksScope microtasks(isolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);
        if (!v8Call(v8::String::NewFromUtf8(isolate(), s, v8::NewStringType::kNormal), source)) {
            ADD_FAILURE();
            return ScriptValue();
        }
        if (!v8Call(v8::Script::Compile(getScriptState()->context(), source), script)) {
            ADD_FAILURE() << "Compilation fails";
            return ScriptValue();
        }
        return ScriptValue(getScriptState(), script->Run(getScriptState()->context()));
    }
    ScriptValue evalWithPrintingError(const char* s)
    {
        v8::TryCatch block(isolate());
        ScriptValue r = eval(s);
        if (block.HasCaught()) {
            ADD_FAILURE() << toCoreString(block.Exception()->ToString(isolate())).utf8().data();
            block.ReThrow();
        }
        return r;
    }

    V8TestingScope m_scope;
    v8::TryCatch m_block;
    Persistent<Document> m_document;
};

TEST_F(ReadableStreamOperationsTest, IsReadableStream)
{
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(getScriptState(), ScriptValue(getScriptState(), v8::Undefined(isolate()))));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(getScriptState(), ScriptValue::createNull(getScriptState())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(getScriptState(), ScriptValue(getScriptState(), v8::Object::New(isolate()))));
    ScriptValue stream = evalWithPrintingError("new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());
    EXPECT_TRUE(ReadableStreamOperations::isReadableStream(getScriptState(), stream));
}

TEST_F(ReadableStreamOperationsTest, IsReadableStreamReaderInvalid)
{
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(getScriptState(), ScriptValue(getScriptState(), v8::Undefined(isolate()))));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(getScriptState(), ScriptValue::createNull(getScriptState())));
    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(getScriptState(), ScriptValue(getScriptState(), v8::Object::New(isolate()))));
    ScriptValue stream = evalWithPrintingError("new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isReadableStreamReader(getScriptState(), stream));
}

TEST_F(ReadableStreamOperationsTest, GetReader)
{
    ScriptValue stream = evalWithPrintingError("new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isLocked(getScriptState(), stream));
    ScriptValue reader;
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(getScriptState(), stream, es);
        ASSERT_FALSE(es.hadException());
    }
    EXPECT_TRUE(ReadableStreamOperations::isLocked(getScriptState(), stream));
    ASSERT_FALSE(reader.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isReadableStream(getScriptState(), reader));
    EXPECT_TRUE(ReadableStreamOperations::isReadableStreamReader(getScriptState(), reader));

    // Already locked!
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(getScriptState(), stream, es);
        ASSERT_TRUE(es.hadException());
    }
    ASSERT_TRUE(reader.isEmpty());
}

TEST_F(ReadableStreamOperationsTest, IsDisturbed)
{
    ScriptValue stream = evalWithPrintingError("stream = new ReadableStream()");
    EXPECT_FALSE(stream.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isDisturbed(getScriptState(), stream));

    ASSERT_FALSE(evalWithPrintingError("stream.cancel()").isEmpty());

    EXPECT_TRUE(ReadableStreamOperations::isDisturbed(getScriptState(), stream));
}

TEST_F(ReadableStreamOperationsTest, Read)
{
    ScriptValue reader = evalWithPrintingError(
        "var controller;"
        "function start(c) { controller = c; }"
        "new ReadableStream({start}).getReader()");
    EXPECT_FALSE(reader.isEmpty());
    ASSERT_TRUE(ReadableStreamOperations::isReadableStreamReader(getScriptState(), reader));

    Iteration* it1 = new Iteration();
    Iteration* it2 = new Iteration();
    ReadableStreamOperations::read(getScriptState(), reader).then(
        Function::createFunction(getScriptState(), it1),
        NotReached::createFunction(getScriptState()));
    ReadableStreamOperations::read(getScriptState(), reader).then(
        Function::createFunction(getScriptState(), it2),
        NotReached::createFunction(getScriptState()));

    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_FALSE(it1->isSet());
    EXPECT_FALSE(it2->isSet());

    ASSERT_FALSE(evalWithPrintingError("controller.enqueue('hello')").isEmpty());
    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("hello", it1->value());
    EXPECT_FALSE(it2->isSet());

    ASSERT_FALSE(evalWithPrintingError("controller.close()").isEmpty());
    v8::MicrotasksScope::PerformCheckpoint(isolate());
    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("hello", it1->value());
    EXPECT_TRUE(it2->isSet());
    EXPECT_TRUE(it2->isValid());
    EXPECT_TRUE(it2->isDone());
}

TEST_F(ReadableStreamOperationsTest, CreateReadableStreamWithCustomUnderlyingSourceAndStrategy)
{
    auto underlyingSource = new TestUnderlyingSource(getScriptState());

    ScriptValue strategy = ReadableStreamOperations::createCountQueuingStrategy(getScriptState(), 10);
    ASSERT_FALSE(strategy.isEmpty());

    ScriptValue stream = ReadableStreamOperations::createReadableStream(getScriptState(), underlyingSource, strategy);
    ASSERT_FALSE(stream.isEmpty());

    EXPECT_EQ(10, underlyingSource->desiredSize());

    underlyingSource->enqueue(ScriptValue::from(getScriptState(), "a"));
    EXPECT_EQ(9, underlyingSource->desiredSize());

    underlyingSource->enqueue(ScriptValue::from(getScriptState(), "b"));
    EXPECT_EQ(8, underlyingSource->desiredSize());

    ScriptValue reader;
    {
        TrackExceptionState es;
        reader = ReadableStreamOperations::getReader(getScriptState(), stream, es);
        ASSERT_FALSE(es.hadException());
    }
    ASSERT_FALSE(reader.isEmpty());

    Iteration* it1 = new Iteration();
    Iteration* it2 = new Iteration();
    Iteration* it3 = new Iteration();
    ReadableStreamOperations::read(getScriptState(), reader).then(Function::createFunction(getScriptState(), it1), NotReached::createFunction(getScriptState()));
    ReadableStreamOperations::read(getScriptState(), reader).then(Function::createFunction(getScriptState(), it2), NotReached::createFunction(getScriptState()));
    ReadableStreamOperations::read(getScriptState(), reader).then(Function::createFunction(getScriptState(), it3), NotReached::createFunction(getScriptState()));

    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_EQ(10, underlyingSource->desiredSize());

    EXPECT_TRUE(it1->isSet());
    EXPECT_TRUE(it1->isValid());
    EXPECT_FALSE(it1->isDone());
    EXPECT_EQ("a", it1->value());

    EXPECT_TRUE(it2->isSet());
    EXPECT_TRUE(it2->isValid());
    EXPECT_FALSE(it2->isDone());
    EXPECT_EQ("b", it2->value());

    EXPECT_FALSE(it3->isSet());

    underlyingSource->close();
    v8::MicrotasksScope::PerformCheckpoint(isolate());

    EXPECT_TRUE(it3->isSet());
    EXPECT_TRUE(it3->isValid());
    EXPECT_TRUE(it3->isDone());
}

TEST_F(ReadableStreamOperationsTest, UnderlyingSourceShouldHavePendingActivityWhenLockedAndControllerIsActive)
{
    auto underlyingSource = new TestUnderlyingSource(getScriptState());

    ScriptValue strategy = ReadableStreamOperations::createCountQueuingStrategy(getScriptState(), 10);
    ASSERT_FALSE(strategy.isEmpty());

    ScriptValue stream = ReadableStreamOperations::createReadableStream(getScriptState(), underlyingSource, strategy);
    ASSERT_FALSE(stream.isEmpty());

    v8::Local<v8::Object> global = getScriptState()->context()->Global();
    ASSERT_TRUE(global->Set(getScriptState()->context(), v8String(getScriptState()->isolate(), "stream"), stream.v8Value()).IsJust());

    EXPECT_FALSE(underlyingSource->hasPendingActivity());
    evalWithPrintingError("let reader = stream.getReader();");
    EXPECT_TRUE(underlyingSource->hasPendingActivity());
    evalWithPrintingError("reader.releaseLock();");
    EXPECT_FALSE(underlyingSource->hasPendingActivity());
    evalWithPrintingError("reader = stream.getReader();");
    EXPECT_TRUE(underlyingSource->hasPendingActivity());
    underlyingSource->enqueue(ScriptValue(getScriptState(), v8::Undefined(getScriptState()->isolate())));
    underlyingSource->close();
    EXPECT_FALSE(underlyingSource->hasPendingActivity());
}

TEST_F(ReadableStreamOperationsTest, SetDisturbed)
{
    ScriptValue stream = evalWithPrintingError("new ReadableStream()");
    ASSERT_FALSE(stream.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isDisturbed(getScriptState(), stream));
    ReadableStreamOperations::setDisturbed(getScriptState(), stream);
    EXPECT_TRUE(ReadableStreamOperations::isDisturbed(getScriptState(), stream));
}

TEST_F(ReadableStreamOperationsTest, IsReadable)
{
    ScriptValue readable = evalWithPrintingError("new ReadableStream()");
    ScriptValue closed = evalWithPrintingError("new ReadableStream({start: c => c.close()})");
    ScriptValue errored = evalWithPrintingError("new ReadableStream({start: c => c.error()})");
    ASSERT_FALSE(readable.isEmpty());
    ASSERT_FALSE(closed.isEmpty());
    ASSERT_FALSE(errored.isEmpty());

    EXPECT_TRUE(ReadableStreamOperations::isReadable(getScriptState(), readable));
    EXPECT_FALSE(ReadableStreamOperations::isReadable(getScriptState(), closed));
    EXPECT_FALSE(ReadableStreamOperations::isReadable(getScriptState(), errored));
}

TEST_F(ReadableStreamOperationsTest, IsClosed)
{
    ScriptValue readable = evalWithPrintingError("new ReadableStream()");
    ScriptValue closed = evalWithPrintingError("new ReadableStream({start: c => c.close()})");
    ScriptValue errored = evalWithPrintingError("new ReadableStream({start: c => c.error()})");
    ASSERT_FALSE(readable.isEmpty());
    ASSERT_FALSE(closed.isEmpty());
    ASSERT_FALSE(errored.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isClosed(getScriptState(), readable));
    EXPECT_TRUE(ReadableStreamOperations::isClosed(getScriptState(), closed));
    EXPECT_FALSE(ReadableStreamOperations::isClosed(getScriptState(), errored));
}

TEST_F(ReadableStreamOperationsTest, IsErrored)
{
    ScriptValue readable = evalWithPrintingError("new ReadableStream()");
    ScriptValue closed = evalWithPrintingError("new ReadableStream({start: c => c.close()})");
    ScriptValue errored = evalWithPrintingError("new ReadableStream({start: c => c.error()})");
    ASSERT_FALSE(readable.isEmpty());
    ASSERT_FALSE(closed.isEmpty());
    ASSERT_FALSE(errored.isEmpty());

    EXPECT_FALSE(ReadableStreamOperations::isErrored(getScriptState(), readable));
    EXPECT_FALSE(ReadableStreamOperations::isErrored(getScriptState(), closed));
    EXPECT_TRUE(ReadableStreamOperations::isErrored(getScriptState(), errored));
}

} // namespace

} // namespace blink

