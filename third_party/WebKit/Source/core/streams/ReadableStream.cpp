// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/streams/ReadableStream.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/V8Binding.h"
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

class ReadableStream::OnStartFailed : public ScriptFunction {
public:
    OnStartFailed(v8::Isolate* isolate, ReadableStream* stream)
        : ScriptFunction(isolate)
        , m_stream(stream) { }
    virtual ScriptValue call(ScriptValue value) OVERRIDE
    {
        m_stream->error(value);
        return value;
    }

private:
    Persistent<ReadableStream> m_stream;
};

ReadableStream::ReadableStream(ScriptState* scriptState, UnderlyingSource* source, ExceptionState* exceptionState)
    : m_scriptState(scriptState)
    , m_source(source)
    , m_isStarted(false)
    , m_isDraining(false)
    , m_isPulling(false)
    , m_state(Waiting)
{
    ScriptPromise promise = source->startSource(exceptionState);
    promise.then(adoptPtr(new OnStarted(scriptState->isolate(), this)), adoptPtr(new OnStartFailed(scriptState->isolate(), this)));
}

ReadableStream::~ReadableStream()
{
}

bool ReadableStream::enqueueInternal(ScriptValue chunk)
{
    // FIXME: Implemnt this method.
    return false;
}

void ReadableStream::error(ScriptValue error)
{
    // FIXME: Implement this function correctly.
    m_state = Errored;
}

void ReadableStream::onStarted()
{
    m_isStarted = true;
}

void ReadableStream::trace(Visitor* visitor)
{
    visitor->trace(m_source);
}

} // namespace blink

