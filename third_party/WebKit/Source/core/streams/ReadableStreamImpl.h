// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ReadableStreamImpl_h
#define ReadableStreamImpl_h

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ArrayBuffer.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/streams/ReadableStream.h"
#include "wtf/Deque.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <utility>

namespace blink {

// We define the default ChunkTypeTraits for frequently used types.
template<typename ChunkType>
class ReadableStreamChunkTypeTraits { };

template<>
class ReadableStreamChunkTypeTraits<String> {
public:
    typedef String HoldType;
    typedef const String& PassType;

    static size_t size(const String& chunk) { return chunk.length(); }
    static ScriptValue toScriptValue(ScriptState* scriptState, const HoldType& value)
    {
        return ScriptValue(scriptState, v8String(scriptState->isolate(), value));
    }
};

template<>
class ReadableStreamChunkTypeTraits<DOMArrayBuffer> {
public:
    typedef RefPtr<DOMArrayBuffer> HoldType;
    typedef PassRefPtr<DOMArrayBuffer> PassType;

    static size_t size(const PassType& chunk) { return chunk->byteLength(); }
    static ScriptValue toScriptValue(ScriptState* scriptState, const HoldType& value)
    {
        return ScriptValue(scriptState, toV8(value.get(), scriptState->context()->Global(), scriptState->isolate()));
    }
};

// ReadableStreamImpl<ChunkTypeTraits> is a ReadableStream subtype. It has a
// queue whose type depends on ChunkTypeTraits and it implements queue-related
// ReadableStream pure virtual methods.
template <typename ChunkTypeTraits>
class ReadableStreamImpl : public ReadableStream {
public:
    class Strategy : public GarbageCollectedFinalized<Strategy> {
    public:
        virtual ~Strategy() { }

        // These functions call ReadableStream::error on error.
        virtual size_t size(const typename ChunkTypeTraits::PassType& chunk, ReadableStream*) { return ChunkTypeTraits::size(chunk); }
        virtual bool shouldApplyBackpressure(size_t totalQueueSize, ReadableStream*) = 0;

        virtual void trace(Visitor*) { }
    };

    class DefaultStrategy : public Strategy {
    public:
        ~DefaultStrategy() override { }
        size_t size(const typename ChunkTypeTraits::PassType& chunk, ReadableStream*) override { return 1; }
        bool shouldApplyBackpressure(size_t totalQueueSize, ReadableStream*) override { return totalQueueSize > 1; }
    };

    ReadableStreamImpl(ExecutionContext* executionContext, UnderlyingSource* source)
        : ReadableStreamImpl(executionContext, source, new DefaultStrategy) { }
    ReadableStreamImpl(ExecutionContext* executionContext, UnderlyingSource* source, Strategy* strategy)
        : ReadableStream(executionContext, source)
        , m_strategy(strategy)
        , m_totalQueueSize(0) { }
    ~ReadableStreamImpl() override { }

    // ReadableStream methods
    ScriptValue read(ScriptState*, ExceptionState&) override;

    bool enqueue(typename ChunkTypeTraits::PassType);

    void trace(Visitor* visitor) override
    {
        visitor->trace(m_strategy);
        ReadableStream::trace(visitor);
    }

private:
    // ReadableStream methods
    bool isQueueEmpty() const override { return m_queue.isEmpty(); }
    void clearQueue() override
    {
        m_queue.clear();
        m_totalQueueSize = 0;
    }
    bool shouldApplyBackpressure() override
    {
        return m_strategy->shouldApplyBackpressure(m_totalQueueSize, this);
    }

    Member<Strategy> m_strategy;
    Deque<std::pair<typename ChunkTypeTraits::HoldType, size_t> > m_queue;
    size_t m_totalQueueSize;
};

template <typename ChunkTypeTraits>
bool ReadableStreamImpl<ChunkTypeTraits>::enqueue(typename ChunkTypeTraits::PassType chunk)
{
    size_t size = m_strategy->size(chunk, this);
    if (!enqueuePreliminaryCheck())
        return false;
    m_queue.append(std::make_pair(chunk, size));
    m_totalQueueSize += size;
    return enqueuePostAction();
}

template <typename ChunkTypeTraits>
ScriptValue ReadableStreamImpl<ChunkTypeTraits>::read(ScriptState* scriptState, ExceptionState& exceptionState)
{
    readPreliminaryCheck(exceptionState);
    if (exceptionState.hadException())
        return ScriptValue();
    ASSERT(state() == Readable);
    ASSERT(!m_queue.isEmpty());
    auto pair = m_queue.takeFirst();
    typename ChunkTypeTraits::HoldType chunk = pair.first;
    size_t size = pair.second;
    ASSERT(m_totalQueueSize >= size);
    m_totalQueueSize -= size;
    readPostAction();
    return ChunkTypeTraits::toScriptValue(scriptState, chunk);
}

} // namespace blink

#endif // ReadableStreamImpl_h
