// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStream.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/streams/ReadableStreamReader.h"
#include "core/streams/UnderlyingSource.h"

namespace blink {

namespace {

class ConstUndefined : public ScriptFunction {
public:
    static v8::Handle<v8::Function> create(ScriptState* scriptState)
    {
        return (new ConstUndefined(scriptState))->bindToV8Function();
    }

private:
    explicit ConstUndefined(ScriptState* scriptState) : ScriptFunction(scriptState) { }
    ScriptValue call(ScriptValue value) override
    {
        return ScriptValue(scriptState(), v8::Undefined(scriptState()->isolate()));
    }
};

} // namespace

ReadableStream::ReadableStream(ExecutionContext* executionContext, UnderlyingSource* source)
    : ActiveDOMObject(executionContext)
    , m_source(source)
    , m_isStarted(false)
    , m_isDraining(false)
    , m_isPulling(false)
    , m_state(Readable)
{
    suspendIfNeeded();
}

ReadableStream::~ReadableStream()
{
}

bool ReadableStream::enqueuePreliminaryCheck()
{
    // This is a bit different from what spec says: it says we should throw
    // an exception here. But sometimes a caller is not in any JavaScript
    // context, and we don't want to throw an exception in such a case.
    if (m_state == Errored || m_state == Closed || m_isDraining)
        return false;

    return true;
}

bool ReadableStream::enqueuePostAction()
{
    m_isPulling = false;

    bool shouldApplyBackpressure = this->shouldApplyBackpressure();
    // this->shouldApplyBackpressure may call this->error().
    if (m_state == Errored)
        return false;

    return !shouldApplyBackpressure;
}

void ReadableStream::close()
{
    if (m_state != Readable)
        return;

    if (isQueueEmpty())
        closeInternal();
    else
        m_isDraining = true;
}

void ReadableStream::readInternalPostAction()
{
    ASSERT(m_state == Readable);
    if (isQueueEmpty() && m_isDraining)
        closeInternal();
    callPullIfNeeded();
}

ScriptPromise ReadableStream::cancel(ScriptState* scriptState)
{
    return cancel(scriptState, ScriptValue(scriptState, v8::Undefined(scriptState->isolate())));
}

ScriptPromise ReadableStream::cancel(ScriptState* scriptState, ScriptValue reason)
{
    if (m_reader)
        return ScriptPromise::reject(scriptState, V8ThrowException::createTypeError(scriptState->isolate(), "this stream is locked to a ReadableStreamReader"));
    if (m_state == Closed)
        return ScriptPromise::cast(scriptState, v8::Undefined(scriptState->isolate()));
    if (m_state == Errored)
        return ScriptPromise::rejectWithDOMException(scriptState, m_exception);

    return cancelInternal(scriptState, reason);
}

ScriptPromise ReadableStream::cancelInternal(ScriptState* scriptState, ScriptValue reason)
{
    closeInternal();
    return m_source->cancelSource(scriptState, reason).then(ConstUndefined::create(scriptState));
}

void ReadableStream::error(PassRefPtrWillBeRawPtr<DOMException> exception)
{
    if (m_state != ReadableStream::Readable)
        return;

    m_exception = exception;
    clearQueue();
    rejectAllPendingReads(m_exception);
    m_state = Errored;
    if (m_reader)
        m_reader->releaseLock();
}

void ReadableStream::didSourceStart()
{
    m_isStarted = true;
    callPullIfNeeded();
}

ReadableStreamReader* ReadableStream::getReader(ExceptionState& exceptionState)
{
    if (m_reader) {
        exceptionState.throwTypeError("already locked to a ReadableStreamReader");
        return nullptr;
    }
    return new ReadableStreamReader(this);
}

void ReadableStream::setReader(ReadableStreamReader* reader)
{
    ASSERT((reader && !m_reader) || (!reader && m_reader));
    m_reader = reader;
}

void ReadableStream::callPullIfNeeded()
{
    if (m_isPulling || m_isDraining || !m_isStarted || m_state == Closed || m_state == Errored)
        return;

    bool shouldApplyBackpressure = this->shouldApplyBackpressure();
    // this->shouldApplyBackpressure may call this->error().
    if (shouldApplyBackpressure || m_state == Errored)
        return;
    m_isPulling = true;
    m_source->pullSource();
}

void ReadableStream::closeInternal()
{
    ASSERT(m_state == Readable);
    m_state = Closed;
    resolveAllPendingReadsAsDone();
    clearQueue();
    if (m_reader)
        m_reader->releaseLock();
}

bool ReadableStream::hasPendingActivity() const
{
    return m_state == Readable;
}

void ReadableStream::stop()
{
    error(DOMException::create(AbortError, "execution context is stopped"));
    ActiveDOMObject::stop();
}

DEFINE_TRACE(ReadableStream)
{
    visitor->trace(m_source);
    visitor->trace(m_exception);
    visitor->trace(m_reader);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
