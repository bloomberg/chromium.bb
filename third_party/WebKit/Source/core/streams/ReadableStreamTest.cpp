// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStream.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "core/streams/UnderlyingSource.h"
#include "core/testing/DummyPageHolder.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace blink {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

namespace {

typedef ::testing::StrictMock<::testing::MockFunction<void(int)> > Checkpoint;

class StringCapturingFunction : public ScriptFunction {
public:
    static PassOwnPtr<StringCapturingFunction> create(v8::Isolate* isolate, String* value)
    {
        return adoptPtr(new StringCapturingFunction(isolate, value));
    }

    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        ASSERT(!value.isEmpty());
        *m_value = toCoreString(value.v8Value()->ToString());
        return value;
    }

private:
    StringCapturingFunction(v8::Isolate* isolate, String* value) : ScriptFunction(isolate), m_value(value) { }

    String* m_value;
};

class MockUnderlyingSource : public UnderlyingSource {
public:
    virtual ~MockUnderlyingSource() { }

    MOCK_METHOD1(startSource, ScriptPromise(ExceptionState*));
    MOCK_METHOD0(pullSource, void());
    MOCK_METHOD0(cancelSource, void());
};

class ThrowError {
public:
    explicit ThrowError(const String& message)
        : m_message(message) { }

    void operator()(ExceptionState* exceptionState)
    {
        exceptionState->throwTypeError(m_message);
    }

private:
    String m_message;
};

} // unnamed namespace

class ReadableStreamTest : public ::testing::Test {
public:
    ReadableStreamTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
        , m_scope(scriptState())
        , m_underlyingSource(new ::testing::StrictMock<MockUnderlyingSource>)
        , m_exceptionState(ExceptionState::ConstructionContext, "property", "interface", scriptState()->context()->Global(), isolate())
    {
    }

    virtual ~ReadableStreamTest()
    {
    }

    ScriptState* scriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    v8::Isolate* isolate() { return scriptState()->isolate(); }

    // Note: This function calls RunMicrotasks.
    ReadableStream* construct()
    {
        RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState());
        ScriptPromise promise = resolver->promise();
        resolver->resolve();
        EXPECT_CALL(*m_underlyingSource, startSource(&m_exceptionState)).WillOnce(Return(promise));

        ReadableStream* stream = new ReadableStream(scriptState(), m_underlyingSource, &m_exceptionState);
        isolate()->RunMicrotasks();
        return stream;
    }

    OwnPtr<DummyPageHolder> m_page;
    ScriptState::Scope m_scope;
    Persistent<MockUnderlyingSource> m_underlyingSource;
    ExceptionState m_exceptionState;
};

TEST_F(ReadableStreamTest, Construct)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState());
    ScriptPromise promise = resolver->promise();
    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, startSource(&m_exceptionState)).WillOnce(Return(promise));
    }
    ReadableStream* stream = new ReadableStream(scriptState(), m_underlyingSource, &m_exceptionState);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Waiting);

    isolate()->RunMicrotasks();

    EXPECT_FALSE(stream->isStarted());

    resolver->resolve();
    isolate()->RunMicrotasks();

    EXPECT_TRUE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Waiting);
}

TEST_F(ReadableStreamTest, ConstructError)
{
    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, startSource(&m_exceptionState))
            .WillOnce(DoAll(Invoke(ThrowError("hello")), Return(ScriptPromise())));
    }
    new ReadableStream(scriptState(), m_underlyingSource, &m_exceptionState);
    EXPECT_TRUE(m_exceptionState.hadException());
}

TEST_F(ReadableStreamTest, StartFailAsynchronously)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState());
    ScriptPromise promise = resolver->promise();
    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, startSource(&m_exceptionState)).WillOnce(Return(promise));
    }
    ReadableStream* stream = new ReadableStream(scriptState(), m_underlyingSource, &m_exceptionState);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Waiting);

    isolate()->RunMicrotasks();

    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Waiting);

    resolver->reject();
    stream->error(DOMException::create(NotFoundError));
    isolate()->RunMicrotasks();

    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Errored);
}

TEST_F(ReadableStreamTest, WaitOnWaiting)
{
    ReadableStream* stream = construct();
    Checkpoint checkpoint;

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isStarted());
    EXPECT_FALSE(stream->isPulling());

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    checkpoint.Call(0);
    ScriptPromise p = stream->wait(scriptState());
    ScriptPromise q = stream->wait(scriptState());
    checkpoint.Call(1);

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_EQ(q, p);
}

TEST_F(ReadableStreamTest, WaitDuringStarting)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState());
    ScriptPromise promise = resolver->promise();
    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, startSource(&m_exceptionState)).WillOnce(Return(promise));
    }
    ReadableStream* stream = new ReadableStream(scriptState(), m_underlyingSource, &m_exceptionState);
    Checkpoint checkpoint;

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isPulling());

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(checkpoint, Call(1));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    checkpoint.Call(0);
    stream->wait(scriptState());
    checkpoint.Call(1);

    EXPECT_TRUE(stream->isPulling());

    resolver->resolve();
    isolate()->RunMicrotasks();

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isPulling());
}

TEST_F(ReadableStreamTest, WaitAndError)
{
    ReadableStream* stream = construct();
    String onFulfilled, onRejected;

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    ScriptPromise promise = stream->wait(scriptState());
    promise.then(StringCapturingFunction::create(isolate(), &onFulfilled), StringCapturingFunction::create(isolate(), &onRejected));
    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isPulling());
    stream->error(DOMException::create(NotFoundError, "hello, error"));
    EXPECT_EQ(ReadableStream::Errored, stream->state());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ(promise, stream->wait(scriptState()));
    EXPECT_EQ("NotFoundError: hello, error", onRejected);
}

TEST_F(ReadableStreamTest, ErrorAndEnqueue)
{
    ReadableStream* stream = construct();

    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->state());

    bool result = stream->enqueue("hello");
    EXPECT_FALSE(result);
    EXPECT_EQ(ReadableStream::Errored, stream->state());
}

TEST_F(ReadableStreamTest, EnqueueAndWait)
{
    ReadableStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->state());

    bool result = stream->enqueue("hello");
    EXPECT_TRUE(result);
    EXPECT_EQ(ReadableStream::Readable, stream->state());

    stream->wait(scriptState()).then(StringCapturingFunction::create(isolate(), &onFulfilled), StringCapturingFunction::create(isolate(), &onRejected));
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, WaitAndEnqueue)
{
    ReadableStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->state());

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    stream->wait(scriptState()).then(StringCapturingFunction::create(isolate(), &onFulfilled), StringCapturingFunction::create(isolate(), &onRejected));
    isolate()->RunMicrotasks();

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    bool result = stream->enqueue("hello");
    EXPECT_TRUE(result);
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, WaitAndEnqueueAndError)
{
    ReadableStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->state());

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    ScriptPromise promise = stream->wait(scriptState());
    promise.then(StringCapturingFunction::create(isolate(), &onFulfilled), StringCapturingFunction::create(isolate(), &onRejected));
    isolate()->RunMicrotasks();

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    bool result = stream->enqueue("hello");
    EXPECT_TRUE(result);
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());

    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->state());

    // FIXME: This expectation should hold but doesn't because of
    // a ScriptPromiseProperty bug. Enable it when the defect is fixed.
    // EXPECT_NE(promise, stream->wait(scriptState()));
}

} // namespace blink

