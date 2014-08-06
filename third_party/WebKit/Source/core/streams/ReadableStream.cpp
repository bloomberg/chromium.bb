// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStream.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/streams/UnderlyingSource.h"

namespace blink {

class ReadableStream::OnStarted : public ScriptFunction {
public:
    OnStarted(v8::Isolate* isolate, ReadableStream* stream)
        : ScriptFunction(isolate)
        , m_stream(stream) { }
    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        m_stream->onStarted();
        return value;
    }

private:
    Persistent<ReadableStream> m_stream;
};

ReadableStream::ReadableStream(ScriptState* scriptState, UnderlyingSource* source, ExceptionState* exceptionState)
    : ContextLifecycleObserver(scriptState->executionContext())
    , m_source(source)
    , m_isStarted(false)
    , m_isDraining(false)
    , m_isPulling(false)
    , m_isSchedulingPull(false)
    , m_state(Waiting)
    , m_wait(new WaitPromise(scriptState->executionContext(), this, WaitPromise::Ready))
{
    ScriptWrappable::init(this);

    ScriptPromise promise = source->startSource(exceptionState);
    // The underlying source calls |this->error| on failure.
    promise.then(adoptPtr(new OnStarted(scriptState->isolate(), this)));
}

ReadableStream::~ReadableStream()
{
}

bool ReadableStream::enqueue(const String& chunk)
{
    if (m_state == Errored || m_state == Closed)
        return false;

    // FIXME: Query strategy.

    enqueueValueWithSize(chunk);
    m_isPulling = false;

    // FIXME: Set needsMore correctly.
    bool needsMore = true;

    if (m_state == Waiting) {
        m_state = Readable;
        m_wait->resolve(V8UndefinedType());
    }

    return needsMore;
}

ScriptPromise ReadableStream::wait(ScriptState* scriptState)
{
    if (m_state == Waiting)
        callOrSchedulePull();
    return m_wait->promise(scriptState->world());
}

void ReadableStream::error(PassRefPtrWillBeRawPtr<DOMException> exception)
{
    if (m_state == Readable)
        m_wait = new WaitPromise(executionContext(), this, WaitPromise::Ready);

    if (m_state == Waiting || m_state == Readable) {
        m_state = Errored;
        m_exception = exception;
        m_wait->reject(m_exception);
    }
}

void ReadableStream::onStarted()
{
    m_isStarted = true;
    if (m_isSchedulingPull)
        m_source->pullSource();
}

void ReadableStream::enqueueValueWithSize(const String& chunk)
{
    // FIXME: implement this.
}

void ReadableStream::callOrSchedulePull()
{
    if (m_isPulling)
        return;
    m_isPulling = true;
    if (m_isStarted)
        m_source->pullSource();
    else
        m_isSchedulingPull = true;
}

void ReadableStream::trace(Visitor* visitor)
{
    visitor->trace(m_source);
    visitor->trace(m_wait);
}

} // namespace blink

