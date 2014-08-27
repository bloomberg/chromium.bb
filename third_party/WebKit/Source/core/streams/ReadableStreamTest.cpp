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
#include "core/dom/ExceptionCode.h"
#include "core/streams/ReadableStreamImpl.h"
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
typedef ReadableStreamImpl<ReadableStreamChunkTypeTraits<String> > StringStream;

class StringCapturingFunction FINAL : public ScriptFunction {
public:
    static PassOwnPtrWillBeRawPtr<StringCapturingFunction> create(v8::Isolate* isolate, String* value)
    {
        return adoptPtrWillBeNoop(new StringCapturingFunction(isolate, value));
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

class MockUnderlyingSource : public GarbageCollectedFinalized<MockUnderlyingSource>, public UnderlyingSource {
    USING_GARBAGE_COLLECTED_MIXIN(MockUnderlyingSource);
public:
    virtual ~MockUnderlyingSource() { }

    MOCK_METHOD0(pullSource, void());
    MOCK_METHOD2(cancelSource, ScriptPromise(ScriptState*, ScriptValue));
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

    PassOwnPtrWillBeRawPtr<StringCapturingFunction> createCaptor(String* value)
    {
        return StringCapturingFunction::create(isolate(), value);
    }

    StringStream* construct()
    {
        StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource);
        stream->didSourceStart();
        return stream;
    }

    OwnPtr<DummyPageHolder> m_page;
    ScriptState::Scope m_scope;
    Persistent<MockUnderlyingSource> m_underlyingSource;
    ExceptionState m_exceptionState;
};

TEST_F(ReadableStreamTest, Start)
{
    StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Waiting);

    stream->didSourceStart();

    EXPECT_TRUE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Waiting);
}

TEST_F(ReadableStreamTest, StartFail)
{
    StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Waiting);

    stream->error(DOMException::create(NotFoundError));

    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Errored);
}

TEST_F(ReadableStreamTest, WaitOnWaiting)
{
    StringStream* stream = construct();
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
    StringStream* stream = new StringStream(scriptState()->executionContext(), m_underlyingSource);
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

    stream->didSourceStart();

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isPulling());
}

TEST_F(ReadableStreamTest, WaitAndError)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    ScriptPromise promise = stream->wait(scriptState());
    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
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
    StringStream* stream = construct();

    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->state());

    bool result = stream->enqueue("hello");
    EXPECT_FALSE(result);
    EXPECT_EQ(ReadableStream::Errored, stream->state());
}

TEST_F(ReadableStreamTest, CloseAndEnqueue)
{
    StringStream* stream = construct();

    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->state());

    bool result = stream->enqueue("hello");
    EXPECT_FALSE(result);
    EXPECT_EQ(ReadableStream::Closed, stream->state());
}

TEST_F(ReadableStreamTest, EnqueueAndWait)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->state());

    bool result = stream->enqueue("hello");
    EXPECT_TRUE(result);
    EXPECT_EQ(ReadableStream::Readable, stream->state());

    stream->wait(scriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
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
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->state());

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    stream->wait(scriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
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
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    EXPECT_EQ(ReadableStream::Waiting, stream->state());

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    ScriptPromise promise = stream->wait(scriptState());
    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
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

    EXPECT_NE(promise, stream->wait(scriptState()));
}

TEST_F(ReadableStreamTest, CloseWhenWaiting)
{
    String onWaitFulfilled, onWaitRejected;
    String onClosedFulfilled, onClosedRejected;

    StringStream* stream = construct();

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
    }

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    stream->wait(scriptState()).then(createCaptor(&onWaitFulfilled), createCaptor(&onWaitRejected));
    stream->closed(scriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onWaitFulfilled.isNull());
    EXPECT_TRUE(onWaitRejected.isNull());
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());

    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->state());
    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onWaitFulfilled);
    EXPECT_TRUE(onWaitRejected.isNull());
    EXPECT_EQ("undefined", onClosedFulfilled);
    EXPECT_TRUE(onClosedRejected.isNull());
}

TEST_F(ReadableStreamTest, CloseWhenErrored)
{
    String onFulfilled, onRejected;
    StringStream* stream = construct();
    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    stream->closed(scriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    stream->error(DOMException::create(NotFoundError, "error"));
    stream->close();

    EXPECT_EQ(ReadableStream::Errored, stream->state());
    isolate()->RunMicrotasks();

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("NotFoundError: error", onRejected);
}

TEST_F(ReadableStreamTest, ReadWhenWaiting)
{
    StringStream* stream = construct();
    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_FALSE(m_exceptionState.hadException());

    stream->read(scriptState(), m_exceptionState);
    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(V8TypeError, m_exceptionState.code());
    EXPECT_EQ("read is called while state is waiting", m_exceptionState.message());
}

TEST_F(ReadableStreamTest, ReadWhenClosed)
{
    StringStream* stream = construct();
    stream->close();

    EXPECT_EQ(ReadableStream::Closed, stream->state());
    EXPECT_FALSE(m_exceptionState.hadException());

    stream->read(scriptState(), m_exceptionState);
    EXPECT_EQ(ReadableStream::Closed, stream->state());
    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(V8TypeError, m_exceptionState.code());
    EXPECT_EQ("read is called while state is closed", m_exceptionState.message());
}

TEST_F(ReadableStreamTest, ReadWhenErrored)
{
    // DOMException values specified in the spec are different from enum values
    // defined in ExceptionCode.h.
    const int notFoundExceptionCode = 8;
    StringStream* stream = construct();
    stream->error(DOMException::create(NotFoundError, "error"));

    EXPECT_EQ(ReadableStream::Errored, stream->state());
    EXPECT_FALSE(m_exceptionState.hadException());

    stream->read(scriptState(), m_exceptionState);
    EXPECT_EQ(ReadableStream::Errored, stream->state());
    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(notFoundExceptionCode, m_exceptionState.code());
    EXPECT_EQ("error", m_exceptionState.message());
}

TEST_F(ReadableStreamTest, EnqueuedAndRead)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    Checkpoint checkpoint;

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(checkpoint, Call(1));
    }

    stream->enqueue("hello");
    ScriptPromise promise = stream->wait(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());

    checkpoint.Call(0);
    String chunk;
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    checkpoint.Call(1);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ("hello", chunk);
    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    EXPECT_TRUE(stream->isPulling());
    EXPECT_FALSE(stream->isDraining());

    ScriptPromise newPromise = stream->wait(scriptState());
    newPromise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    isolate()->RunMicrotasks();
    EXPECT_NE(promise, newPromise);
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, EnqueTwiceAndRead)
{
    StringStream* stream = construct();
    Checkpoint checkpoint;

    {
        InSequence s;
        EXPECT_CALL(checkpoint, Call(0));
        EXPECT_CALL(checkpoint, Call(1));
    }

    EXPECT_TRUE(stream->enqueue("hello"));
    EXPECT_TRUE(stream->enqueue("bye"));
    ScriptPromise promise = stream->wait(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());

    checkpoint.Call(0);
    String chunk;
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    checkpoint.Call(1);
    EXPECT_FALSE(m_exceptionState.hadException());
    EXPECT_EQ("hello", chunk);
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_FALSE(stream->isDraining());

    ScriptPromise newPromise = stream->wait(scriptState());
    EXPECT_EQ(promise, newPromise);
}

TEST_F(ReadableStreamTest, CloseWhenReadable)
{
    StringStream* stream = construct();
    String onWaitFulfilled, onWaitRejected;
    String onClosedFulfilled, onClosedRejected;

    stream->closed(scriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));
    EXPECT_TRUE(stream->enqueue("hello"));
    EXPECT_TRUE(stream->enqueue("bye"));
    stream->close();
    EXPECT_FALSE(stream->enqueue("should be ignored"));

    ScriptPromise promise = stream->wait(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    String chunk;
    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    EXPECT_EQ("hello", chunk);
    EXPECT_EQ(promise, stream->wait(scriptState()));

    isolate()->RunMicrotasks();

    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    EXPECT_TRUE(stream->read(scriptState(), m_exceptionState).toString(chunk));
    EXPECT_EQ("bye", chunk);
    EXPECT_FALSE(m_exceptionState.hadException());

    EXPECT_NE(promise, stream->wait(scriptState()));
    stream->wait(scriptState()).then(createCaptor(&onWaitFulfilled), createCaptor(&onWaitRejected));

    EXPECT_EQ(ReadableStream::Closed, stream->state());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_TRUE(stream->isDraining());

    EXPECT_TRUE(onWaitFulfilled.isNull());
    EXPECT_TRUE(onWaitRejected.isNull());
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onWaitFulfilled);
    EXPECT_TRUE(onWaitRejected.isNull());
    EXPECT_EQ("undefined", onClosedFulfilled);
    EXPECT_TRUE(onClosedRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenClosed)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    stream->close();
    EXPECT_EQ(ReadableStream::Closed, stream->state());

    ScriptPromise promise = stream->cancel(scriptState(), ScriptValue());
    EXPECT_EQ(ReadableStream::Closed, stream->state());

    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenErrored)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    stream->error(DOMException::create(NotFoundError, "error"));
    EXPECT_EQ(ReadableStream::Errored, stream->state());

    ScriptPromise promise = stream->cancel(scriptState(), ScriptValue());
    EXPECT_EQ(ReadableStream::Errored, stream->state());

    promise.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("NotFoundError: error", onRejected);
}

TEST_F(ReadableStreamTest, CancelWhenWaiting)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    ScriptValue reason(scriptState(), v8String(scriptState()->isolate(), "reason"));
    ScriptPromise promise = ScriptPromise::cast(scriptState(), v8String(scriptState()->isolate(), "hello"));

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, pullSource()).Times(1);
        EXPECT_CALL(*m_underlyingSource, cancelSource(scriptState(), reason)).WillOnce(Return(promise));
    }

    EXPECT_EQ(ReadableStream::Waiting, stream->state());
    ScriptPromise wait = stream->wait(scriptState());
    EXPECT_EQ(promise, stream->cancel(scriptState(), reason));
    EXPECT_EQ(ReadableStream::Closed, stream->state());
    EXPECT_EQ(stream->wait(scriptState()), wait);

    wait.then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, CancelWhenReadable)
{
    StringStream* stream = construct();
    String onFulfilled, onRejected;
    ScriptValue reason(scriptState(), v8String(scriptState()->isolate(), "reason"));
    ScriptPromise promise = ScriptPromise::cast(scriptState(), v8String(scriptState()->isolate(), "hello"));

    {
        InSequence s;
        EXPECT_CALL(*m_underlyingSource, cancelSource(scriptState(), reason)).WillOnce(Return(promise));
    }

    stream->enqueue("hello");
    ScriptPromise wait = stream->wait(scriptState());
    EXPECT_EQ(ReadableStream::Readable, stream->state());
    EXPECT_EQ(promise, stream->cancel(scriptState(), reason));
    EXPECT_EQ(ReadableStream::Closed, stream->state());

    EXPECT_NE(stream->wait(scriptState()), wait);

    stream->wait(scriptState()).then(createCaptor(&onFulfilled), createCaptor(&onRejected));
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamTest, ReadableArrayBufferCompileTest)
{
    // This test tests if ReadableStreamImpl<ArrayBuffer> can be instantiated.
    new ReadableStreamImpl<ReadableStreamChunkTypeTraits<ArrayBuffer> >(scriptState()->executionContext(), m_underlyingSource);
}

} // namespace blink

