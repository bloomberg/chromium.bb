// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamImpl_h
#define ReadableStreamImpl_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/streams/ReadableStream.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/Deque.h"
#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

// We define the default ChunkTypeTraits for frequently used types.
template<typename ChunkType>
class ReadableStreamChunkTypeTraits { };

template<>
class ReadableStreamChunkTypeTraits<String> {
public:
    typedef String HoldType;
    typedef const String& PassType;

    static size_t size(const String& value) { return value.length(); }
    static ScriptValue toScriptValue(ScriptState* scriptState, const HoldType& value)
    {
        return ScriptValue(scriptState, v8String(scriptState->isolate(), value));
    }
};

template<>
class ReadableStreamChunkTypeTraits<ArrayBuffer> {
public:
    typedef RefPtr<ArrayBuffer> HoldType;
    typedef PassRefPtr<ArrayBuffer> PassType;

    static size_t size(const PassType& value) { return value->byteLength(); }
    static size_t size(const HoldType& value) { return value->byteLength(); }
    static ScriptValue toScriptValue(ScriptState* scriptState, const HoldType& value)
    {
        return ScriptValue(scriptState, toV8NoInline(value.get(), scriptState->context()->Global(), scriptState->isolate()));
    }
};

// ReadableStreamImpl<ChunkTypeTraits> is a ReadableStream subtype. It has a
// queue whose type depends on ChunkTypeTraits and it implements queue-related
// ReadableStream pure virtual methods.
template <typename ChunkTypeTraits>
class ReadableStreamImpl : public ReadableStream {
public:
    ReadableStreamImpl(ExecutionContext* executionContext, UnderlyingSource* source)
        : ReadableStream(executionContext, source)
        , m_totalQueueSize(0) { }
    virtual ~ReadableStreamImpl() { }

    // ReadableStream methods
    virtual ScriptValue read(ScriptState*, ExceptionState&) override;

    bool enqueue(typename ChunkTypeTraits::PassType);

    virtual void trace(Visitor* visitor) override
    {
        ReadableStream::trace(visitor);
    }

private:
    // ReadableStream methods
    virtual bool isQueueEmpty() const override { return m_queue.isEmpty(); }
    virtual void clearQueue() override
    {
        m_queue.clear();
        m_totalQueueSize = 0;
    }

    Deque<typename ChunkTypeTraits::HoldType> m_queue;
    size_t m_totalQueueSize;
};

template <typename ChunkTypeTraits>
bool ReadableStreamImpl<ChunkTypeTraits>::enqueue(typename ChunkTypeTraits::PassType chunk)
{
    size_t size = ChunkTypeTraits::size(chunk);
    if (!enqueuePreliminaryCheck(size))
        return false;
    m_queue.append(chunk);
    m_totalQueueSize += size;
    return enqueuePostAction(m_totalQueueSize);
}

template <typename ChunkTypeTraits>
ScriptValue ReadableStreamImpl<ChunkTypeTraits>::read(ScriptState* scriptState, ExceptionState& exceptionState)
{
    readPreliminaryCheck(exceptionState);
    if (exceptionState.hadException())
        return ScriptValue();
    ASSERT(state() == Readable);
    ASSERT(!m_queue.isEmpty());
    typename ChunkTypeTraits::HoldType chunk = m_queue.takeFirst();
    m_totalQueueSize -= ChunkTypeTraits::size(chunk);
    readPostAction();
    return ChunkTypeTraits::toScriptValue(scriptState, chunk);
}

} // namespace blink

#endif // ReadableStreamImpl_h

