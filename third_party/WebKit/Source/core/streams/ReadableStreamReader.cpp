// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStreamReader.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/streams/ReadableStream.h"

namespace blink {

namespace {

class PromiseRaceFulfillHandler : public ScriptFunction {
public:
    static v8::Handle<v8::Function> create(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
    {
        return (new PromiseRaceFulfillHandler(resolver))->bindToV8Function();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_resolver);
        ScriptFunction::trace(visitor);
    }

private:
    explicit PromiseRaceFulfillHandler(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
        : ScriptFunction(resolver->scriptState())
        , m_resolver(resolver) { }
    ScriptValue call(ScriptValue value) override
    {
        m_resolver->resolve(value);
        return ScriptValue(scriptState(), v8::Undefined(scriptState()->isolate()));
    }

    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
};

class PromiseRaceRejectHandler : public ScriptFunction {
public:
    static v8::Handle<v8::Function> create(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
    {
        return (new PromiseRaceRejectHandler(resolver))->bindToV8Function();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_resolver);
        ScriptFunction::trace(visitor);
    }

private:
    explicit PromiseRaceRejectHandler(PassRefPtrWillBeRawPtr<ScriptPromiseResolver> resolver)
        : ScriptFunction(resolver->scriptState())
        , m_resolver(resolver) { }
    ScriptValue call(ScriptValue value) override
    {
        m_resolver->reject(value);
        return ScriptValue(scriptState(), v8::Undefined(scriptState()->isolate()));
    }

    RefPtrWillBeMember<ScriptPromiseResolver> m_resolver;
};

ScriptPromise race(ScriptState* scriptState, const Vector<ScriptPromise>& promises)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    for (ScriptPromise promise : promises) {
        promise.then(PromiseRaceFulfillHandler::create(resolver), PromiseRaceRejectHandler::create(resolver));
    }
    return resolver->promise();
}

} // namespace

ReadableStreamReader::ReadableStreamReader(ReadableStream* stream)
    : ActiveDOMObject(stream->executionContext())
    , m_stream(stream)
    , m_released(new ReleasedPromise(stream->executionContext(), this, ReleasedPromise::Released))
    , m_stateAfterRelease(ReadableStream::Closed)
{
    suspendIfNeeded();
    ASSERT(m_stream->isLockedTo(nullptr));
    m_stream->setReader(this);
}

ScriptPromise ReadableStreamReader::closed(ScriptState* scriptState)
{
    if (isActive()) {
        Vector<ScriptPromise> promises;
        promises.append(m_stream->closed(scriptState));
        promises.append(m_released->promise(scriptState->world()));
        return race(scriptState, promises);
    }
    ASSERT(m_released);
    return m_closedAfterRelease->promise(scriptState->world());
}

bool ReadableStreamReader::isActive() const
{
    return m_stream->isLockedTo(this);
}

ScriptPromise ReadableStreamReader::ready(ScriptState* scriptState)
{
    if (isActive()) {
        Vector<ScriptPromise> promises;
        promises.append(m_stream->readyInternal(scriptState));
        promises.append(m_released->promise(scriptState->world()));
        return race(scriptState, promises);
    }
    ASSERT(m_readyAfterRelease);
    return m_readyAfterRelease->promise(scriptState->world());
}

String ReadableStreamReader::state() const
{
    if (isActive())
        return ReadableStream::stateToString(m_stream->stateInternal());
    return ReadableStream::stateToString(m_stateAfterRelease);
}

ScriptPromise ReadableStreamReader::cancel(ScriptState* scriptState, ScriptValue reason)
{
    if (isActive()) {
        releaseLock();
        return m_stream->cancel(scriptState, reason);
    }
    return m_closedAfterRelease->promise(scriptState->world());
}

ScriptValue ReadableStreamReader::read(ScriptState* scriptState, ExceptionState& es)
{
    if (!isActive()) {
        es.throwTypeError("The stream is not locked to this reader");
        return ScriptValue();
    }
    return m_stream->readInternal(scriptState, es);
}

void ReadableStreamReader::releaseLock()
{
    if (!isActive())
        return;

    m_stream->setReader(nullptr);

    m_readyAfterRelease = new ReadyPromise(executionContext(), this, ReadyPromise::Ready);
    m_readyAfterRelease->resolve(ToV8UndefinedGenerator());
    m_closedAfterRelease = new ClosedPromise(executionContext(), this, ReadyPromise::Closed);

    if (m_stream->stateInternal() == ReadableStream::Closed) {
        m_stateAfterRelease = ReadableStream::Closed;
        m_closedAfterRelease->resolve(ToV8UndefinedGenerator());
    } else if (m_stream->stateInternal() == ReadableStream::Errored) {
        m_stateAfterRelease = ReadableStream::Errored;
        m_closedAfterRelease->reject(m_stream->storedException());
    } else {
        m_stateAfterRelease = ReadableStream::Closed;
        m_closedAfterRelease->resolve(ToV8UndefinedGenerator());
    }
    m_released->resolve(ToV8UndefinedGenerator());
    ASSERT(!isActive());
}

ScriptPromise ReadableStreamReader::released(ScriptState* scriptState)
{
    return m_released->promise(scriptState->world());
}

bool ReadableStreamReader::hasPendingActivity() const
{
    // We need to extend ReadableStreamReader's wrapper's life while it is
    // active in order to call resolve / reject on ScriptPromiseProperties.
    return isActive();
}

void ReadableStreamReader::stop()
{
    releaseLock();
    ActiveDOMObject::stop();
}

DEFINE_TRACE(ReadableStreamReader)
{
    visitor->trace(m_stream);
    visitor->trace(m_released);
    visitor->trace(m_closedAfterRelease);
    visitor->trace(m_readyAfterRelease);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
