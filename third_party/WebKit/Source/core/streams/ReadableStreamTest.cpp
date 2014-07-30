// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStream.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/streams/UnderlyingSource.h"
#include "core/testing/DummyPageHolder.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace blink {

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

class MockUnderlyingSource : public UnderlyingSource {
public:
    virtual ~MockUnderlyingSource() { }

    MOCK_METHOD1(startSource, ScriptPromise(ExceptionState*));
    MOCK_METHOD1(pullSource, void(ExceptionState*));
    MOCK_METHOD1(cancelSource, void(ExceptionState*));
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

class ReadableStreamTest : public ::testing::Test {
public:
    virtual ~ReadableStreamTest() { }

    ReadableStreamTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1)))
        , m_scope(scriptState())
        , m_underlyingSource(new ::testing::StrictMock<MockUnderlyingSource>)
        , m_exceptionState(ExceptionState::ConstructionContext, "property", "interface", scriptState()->context()->Global(), isolate())
    {
    }

    ScriptState* scriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    v8::Isolate* isolate() { return scriptState()->isolate(); }

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
    isolate()->RunMicrotasks();

    EXPECT_FALSE(stream->isStarted());
    EXPECT_FALSE(stream->isDraining());
    EXPECT_FALSE(stream->isPulling());
    EXPECT_EQ(stream->state(), ReadableStream::Errored);
}

} // namespace blink

