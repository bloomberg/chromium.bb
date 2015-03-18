// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStreamReader.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ToV8.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/streams/ReadableStream.h"
#include "core/streams/ReadableStreamImpl.h"
#include "core/streams/UnderlyingSource.h"
#include "core/testing/DummyPageHolder.h"
#include <gtest/gtest.h>

namespace blink {

using StringStream = ReadableStreamImpl<ReadableStreamChunkTypeTraits<String>>;

namespace {

class StringCapturingFunction final : public ScriptFunction {
public:
    static v8::Handle<v8::Function> createFunction(ScriptState* scriptState, String* value)
    {
        StringCapturingFunction* self = new StringCapturingFunction(scriptState, value);
        return self->bindToV8Function();
    }

private:
    StringCapturingFunction(ScriptState* scriptState, String* value)
        : ScriptFunction(scriptState)
        , m_value(value)
    {
    }

    ScriptValue call(ScriptValue value) override
    {
        ASSERT(!value.isEmpty());
        *m_value = toCoreString(value.v8Value()->ToString(scriptState()->isolate()));
        return value;
    }

    String* m_value;
};

class NoopUnderlyingSource final : public GarbageCollectedFinalized<NoopUnderlyingSource>, public UnderlyingSource {
    USING_GARBAGE_COLLECTED_MIXIN(NoopUnderlyingSource);
public:
    ~NoopUnderlyingSource() override { }

    void pullSource() override { }
    ScriptPromise cancelSource(ScriptState* scriptState, ScriptValue reason) { return ScriptPromise::cast(scriptState, reason); }
    DEFINE_INLINE_VIRTUAL_TRACE() { UnderlyingSource::trace(visitor); }
};

class PermissiveStrategy final : public StringStream::Strategy {
public:
    bool shouldApplyBackpressure(size_t, ReadableStream*) override { return false; }
};

class ReadableStreamReaderTest : public ::testing::Test {
public:
    ReadableStreamReaderTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
        , m_scope(scriptState())
        , m_exceptionState(ExceptionState::ConstructionContext, "property", "interface", scriptState()->context()->Global(), isolate())
        , m_stream(new StringStream(scriptState()->executionContext(), new NoopUnderlyingSource, new PermissiveStrategy))
    {
        m_stream->didSourceStart();
    }

    ~ReadableStreamReaderTest()
    {
        // We need to call |error| in order to make
        // ActiveDOMObject::hasPendingActivity return false.
        m_stream->error(DOMException::create(AbortError, "done"));
    }

    ScriptState* scriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    v8::Isolate* isolate() { return scriptState()->isolate(); }

    v8::Handle<v8::Function> createCaptor(String* value)
    {
        return StringCapturingFunction::createFunction(scriptState(), value);
    }

    OwnPtr<DummyPageHolder> m_page;
    ScriptState::Scope m_scope;
    ExceptionState m_exceptionState;
    Persistent<StringStream> m_stream;
};

TEST_F(ReadableStreamReaderTest, Construct)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());
}

TEST_F(ReadableStreamReaderTest, Release)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());
    reader->releaseLock();
    EXPECT_FALSE(reader->isActive());

    ReadableStreamReader* another = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(another->isActive());
    EXPECT_FALSE(reader->isActive());
    reader->releaseLock();
    EXPECT_TRUE(another->isActive());
    EXPECT_FALSE(reader->isActive());
}

TEST_F(ReadableStreamReaderTest, MaskState)
{
    m_stream->enqueue("hello");
    EXPECT_EQ("readable", m_stream->stateString());

    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_EQ("waiting", m_stream->stateString());
    EXPECT_EQ("readable", reader->state());

    reader->releaseLock();
    EXPECT_EQ("readable", m_stream->stateString());
    EXPECT_EQ("closed", reader->state());

    ReadableStreamReader* another = new ReadableStreamReader(m_stream);
    EXPECT_EQ("waiting", m_stream->stateString());
    EXPECT_EQ("closed", reader->state());
    EXPECT_EQ("readable", another->state());
}

TEST_F(ReadableStreamReaderTest, MaskReady)
{
    m_stream->enqueue("hello");
    isolate()->RunMicrotasks();

    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    {
        String s1, s2;
        reader->ready(scriptState()).then(createCaptor(&s1));
        m_stream->ready(scriptState()).then(createCaptor(&s2));
        isolate()->RunMicrotasks();
        EXPECT_EQ("undefined", s1);
        EXPECT_TRUE(s2.isNull());

        reader->releaseLock();
        isolate()->RunMicrotasks();
        EXPECT_EQ("undefined", s2);
    }

    {
        String s1, s2;
        reader->ready(scriptState()).then(createCaptor(&s1));
        m_stream->ready(scriptState()).then(createCaptor(&s2));
        isolate()->RunMicrotasks();
        EXPECT_EQ("undefined", s1);
        EXPECT_EQ("undefined", s2);
    }

    ReadableStreamReader* another = new ReadableStreamReader(m_stream);
    {
        String s1, s2, s3;
        reader->ready(scriptState()).then(createCaptor(&s1));
        m_stream->ready(scriptState()).then(createCaptor(&s2));
        another->ready(scriptState()).then(createCaptor(&s3));
        isolate()->RunMicrotasks();
        EXPECT_EQ("undefined", s1);
        EXPECT_TRUE(s2.isNull());
        EXPECT_EQ("undefined", s3);

        // We need to call here to ensure all promises having captors are
        // resolved or rejected.
        m_stream->error(DOMException::create(AbortError, "done"));
        isolate()->RunMicrotasks();
    }
}

TEST_F(ReadableStreamReaderTest, ReaderRead)
{
    m_stream->enqueue("hello");
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);

    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());
    ScriptValue value = reader->read(scriptState(), m_exceptionState);

    EXPECT_FALSE(m_exceptionState.hadException());
    String stringValue;
    EXPECT_TRUE(value.toString(stringValue));
    EXPECT_EQ("hello", stringValue);
}

TEST_F(ReadableStreamReaderTest, StreamReadShouldFailWhenLocked)
{
    m_stream->enqueue("hello");
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());

    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());
    m_stream->read(scriptState(), m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());
}

TEST_F(ReadableStreamReaderTest, ReaderReadShouldFailWhenNotLocked)
{
    m_stream->enqueue("hello");
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    reader->releaseLock();
    EXPECT_FALSE(reader->isActive());

    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());
    reader->read(scriptState(), m_exceptionState);

    EXPECT_TRUE(m_exceptionState.hadException());
    EXPECT_EQ(ReadableStream::Readable, m_stream->stateInternal());
}

TEST_F(ReadableStreamReaderTest, ClosedReader)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);

    m_stream->close();

    EXPECT_EQ("closed", m_stream->stateString());
    EXPECT_EQ("closed", reader->state());
    EXPECT_FALSE(reader->isActive());

    String onClosedFulfilled, onClosedRejected;
    String onReadyFulfilled, onReadyRejected;
    isolate()->RunMicrotasks();
    reader->closed(scriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));
    reader->ready(scriptState()).then(createCaptor(&onReadyFulfilled), createCaptor(&onReadyRejected));
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_TRUE(onReadyFulfilled.isNull());
    EXPECT_TRUE(onReadyRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onClosedFulfilled);
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_EQ("undefined", onReadyFulfilled);
    EXPECT_TRUE(onReadyRejected.isNull());
}

TEST_F(ReadableStreamReaderTest, ErroredReader)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);

    m_stream->error(DOMException::create(SyntaxError, "some error"));

    EXPECT_EQ("errored", m_stream->stateString());
    EXPECT_EQ("errored", reader->state());
    EXPECT_FALSE(reader->isActive());

    String onClosedFulfilled, onClosedRejected;
    String onReadyFulfilled, onReadyRejected;
    reader->closed(scriptState()).then(createCaptor(&onClosedFulfilled), createCaptor(&onClosedRejected));
    reader->ready(scriptState()).then(createCaptor(&onReadyFulfilled), createCaptor(&onReadyRejected));
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_TRUE(onClosedRejected.isNull());
    EXPECT_TRUE(onReadyFulfilled.isNull());
    EXPECT_TRUE(onReadyRejected.isNull());

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onClosedFulfilled.isNull());
    EXPECT_EQ("SyntaxError: some error", onClosedRejected);
    EXPECT_EQ("undefined", onReadyFulfilled);
    EXPECT_TRUE(onReadyRejected.isNull());
}

TEST_F(ReadableStreamReaderTest, ReadyPromiseShouldNotBeResolvedWhenLocked)
{
    String s;
    ScriptPromise ready = m_stream->ready(scriptState());
    ready.then(createCaptor(&s));
    isolate()->RunMicrotasks();
    EXPECT_TRUE(s.isNull());

    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());
    EXPECT_NE(ready, m_stream->ready(scriptState()));

    isolate()->RunMicrotasks();
    EXPECT_TRUE(s.isNull());

    // We need to call here to ensure all promises having captors are resolved
    // or rejected.
    m_stream->error(DOMException::create(AbortError, "done"));
    isolate()->RunMicrotasks();
}

TEST_F(ReadableStreamReaderTest, ReaderShouldBeReleasedWhenClosed)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());
    m_stream->close();
    EXPECT_FALSE(reader->isActive());
}

TEST_F(ReadableStreamReaderTest, ReaderShouldBeReleasedWhenCanceled)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());
    reader->cancel(scriptState(), ScriptValue(scriptState(), v8::Undefined(isolate())));
    EXPECT_FALSE(reader->isActive());
}

TEST_F(ReadableStreamReaderTest, ReaderShouldBeReleasedWhenErrored)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());
    m_stream->error(DOMException::create(SyntaxError, "some error"));
    EXPECT_FALSE(reader->isActive());
}

TEST_F(ReadableStreamReaderTest, StreamCancelShouldFailWhenLocked)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    EXPECT_TRUE(reader->isActive());
    ScriptPromise p = m_stream->cancel(scriptState(), ScriptValue(scriptState(), v8::Undefined(isolate())));
    EXPECT_EQ(ReadableStream::Waiting, m_stream->stateInternal());
    String onFulfilled, onRejected;
    p.then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());
    isolate()->RunMicrotasks();
    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_EQ("TypeError: this stream is locked to a ReadableStreamReader", onRejected);
}

TEST_F(ReadableStreamReaderTest, ReaderCancelShouldNotWorkWhenNotActive)
{
    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);
    reader->releaseLock();
    EXPECT_FALSE(reader->isActive());

    ScriptPromise p = reader->cancel(scriptState(), ScriptValue(scriptState(), v8::Undefined(isolate())));
    EXPECT_EQ(ReadableStream::Waiting, m_stream->stateInternal());
    String onFulfilled, onRejected;
    p.then(createCaptor(&onFulfilled), createCaptor(&onRejected));

    EXPECT_TRUE(onFulfilled.isNull());
    EXPECT_TRUE(onRejected.isNull());
    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
    EXPECT_TRUE(onRejected.isNull());
}

TEST_F(ReadableStreamReaderTest, ReadyShouldNotBeResolvedWhileLocked)
{
    String onFulfilled;
    m_stream->ready(scriptState()).then(createCaptor(&onFulfilled));

    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);

    m_stream->enqueue("hello");
    m_stream->enqueue("world");

    ASSERT_EQ("readable", reader->state());
    reader->read(scriptState(), m_exceptionState);
    ASSERT_EQ("readable", reader->state());
    reader->read(scriptState(), m_exceptionState);
    ASSERT_EQ("waiting", reader->state());

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onFulfilled.isNull());

    // We need to call here to ensure all promises having captors are resolved
    // or rejected.
    m_stream->error(DOMException::create(AbortError, "done"));
    isolate()->RunMicrotasks();
}

TEST_F(ReadableStreamReaderTest, ReadyShouldNotBeResolvedWhenReleasedIfNotReady)
{
    String onFulfilled;
    m_stream->ready(scriptState()).then(createCaptor(&onFulfilled));

    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);

    m_stream->enqueue("hello");
    m_stream->enqueue("world");

    ASSERT_EQ("readable", reader->state());
    reader->read(scriptState(), m_exceptionState);
    ASSERT_EQ("readable", reader->state());
    reader->read(scriptState(), m_exceptionState);
    ASSERT_EQ("waiting", reader->state());

    reader->releaseLock();

    isolate()->RunMicrotasks();
    EXPECT_TRUE(onFulfilled.isNull());

    // We need to call here to ensure all promises having captors are resolved
    // or rejected.
    m_stream->error(DOMException::create(AbortError, "done"));
    isolate()->RunMicrotasks();
}

TEST_F(ReadableStreamReaderTest, ReadyShouldBeResolvedWhenReleasedIfReady)
{
    String onFulfilled;
    m_stream->ready(scriptState()).then(createCaptor(&onFulfilled));

    ReadableStreamReader* reader = new ReadableStreamReader(m_stream);

    m_stream->enqueue("hello");
    m_stream->enqueue("world");

    ASSERT_EQ("readable", reader->state());
    reader->read(scriptState(), m_exceptionState);
    ASSERT_EQ("readable", reader->state());

    isolate()->RunMicrotasks();
    reader->releaseLock();
    EXPECT_TRUE(onFulfilled.isNull());

    isolate()->RunMicrotasks();
    EXPECT_EQ("undefined", onFulfilled);
}

} // namespace

} // namespace blink
