// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/ReadableStreamDataConsumerHandle.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ReadableStreamOperations.h"
#include "bindings/core/v8/ScriptFunction.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8IteratorResultValue.h"
#include "bindings/core/v8/V8RecursionScope.h"
#include "bindings/core/v8/V8Uint8Array.h"
#include "core/dom/DOMTypedArray.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebTraceLocation.h"
#include "wtf/Assertions.h"
#include "wtf/Functional.h"
#include "wtf/RefCounted.h"
#include "wtf/WeakPtr.h"
#include <algorithm>
#include <string.h>
#include <v8.h>

namespace blink {

using Result = WebDataConsumerHandle::Result;
using Flags = WebDataConsumerHandle::Flags;

// This context is not yet thread-safe.
class ReadableStreamDataConsumerHandle::ReadingContext final : public RefCounted<ReadingContext> {
    WTF_MAKE_NONCOPYABLE(ReadingContext);
public:
    class OnFulfilled final : public ScriptFunction {
    public:
        static v8::Local<v8::Function> createFunction(ScriptState* scriptState, WeakPtr<ReadingContext> context)
        {
            return (new OnFulfilled(scriptState, context))->bindToV8Function();
        }

        ScriptValue call(ScriptValue v) override
        {
            RefPtr<ReadingContext> readingContext(m_readingContext.get());
            if (!readingContext)
                return v;
            bool done;
            v8::Local<v8::Value> item = v.v8Value();
            ASSERT(item->IsObject());
            v8::Local<v8::Value> value = v8CallOrCrash(v8UnpackIteratorResult(v.scriptState(), item.As<v8::Object>(), &done));
            if (done) {
                readingContext->onReadDone();
                return v;
            }
            if (!V8Uint8Array::hasInstance(value, v.isolate())) {
                readingContext->onRejected();
                return ScriptValue();
            }
            readingContext->onRead(V8Uint8Array::toImpl(value.As<v8::Object>()));
            return v;
        }

    private:
        OnFulfilled(ScriptState* scriptState, WeakPtr<ReadingContext> context)
            : ScriptFunction(scriptState), m_readingContext(context) {}

        WeakPtr<ReadingContext> m_readingContext;
    };

    class OnRejected final : public ScriptFunction {
    public:
        static v8::Local<v8::Function> createFunction(ScriptState* scriptState, WeakPtr<ReadingContext> context)
        {
            return (new OnRejected(scriptState, context))->bindToV8Function();
        }

        ScriptValue call(ScriptValue v) override
        {
            RefPtr<ReadingContext> readingContext(m_readingContext.get());
            if (!readingContext)
                return v;
            readingContext->onRejected();
            return v;
        }

    private:
        OnRejected(ScriptState* scriptState, WeakPtr<ReadingContext> context)
            : ScriptFunction(scriptState), m_readingContext(context) {}

        WeakPtr<ReadingContext> m_readingContext;
    };

    class ReaderImpl final : public FetchDataConsumerHandle::Reader {
    public:
        ReaderImpl(PassRefPtr<ReadingContext> context, Client* client)
            : m_readingContext(context)
        {
            m_readingContext->attachReader(client);
        }
        ~ReaderImpl() override
        {
            m_readingContext->detachReader();
        }

        Result beginRead(const void** buffer, Flags, size_t* available) override
        {
            return m_readingContext->beginRead(buffer, available);
        }

        Result endRead(size_t readSize) override
        {
            return m_readingContext->endRead(readSize);
        }

    private:
        RefPtr<ReadingContext> m_readingContext;
    };

    static PassRefPtr<ReadingContext> create(ScriptState* scriptState, v8::Local<v8::Value> stream)
    {
        return adoptRef(new ReadingContext(scriptState, stream));
    }

    void attachReader(WebDataConsumerHandle::Client* client)
    {
        m_client = client;
        notifyLater();
    }

    void detachReader()
    {
        m_client = nullptr;
    }

    Result beginRead(const void** buffer, size_t* available)
    {
        *buffer = nullptr;
        *available = 0;
        if (m_hasError)
            return WebDataConsumerHandle::UnexpectedError;
        if (m_isDone)
            return WebDataConsumerHandle::Done;

        if (m_pendingBuffer) {
            ASSERT(m_pendingOffset < m_pendingBuffer->length());
            *buffer = m_pendingBuffer->data() + m_pendingOffset;
            *available = m_pendingBuffer->length() - m_pendingOffset;
            return WebDataConsumerHandle::Ok;
        }
        ASSERT(!m_reader.isEmpty());
        m_isInRecursion = true;
        if (!m_isReading) {
            m_isReading = true;
            ScriptState::Scope scope(m_reader.scriptState());
            V8RecursionScope recursionScope(m_reader.isolate());
            ReadableStreamOperations::read(m_reader.scriptState(), m_reader.v8Value()).then(
                OnFulfilled::createFunction(m_reader.scriptState(), m_weakPtrFactory.createWeakPtr()),
                OnRejected::createFunction(m_reader.scriptState(), m_weakPtrFactory.createWeakPtr()));
            // Note: Microtasks may run here.
        }
        m_isInRecursion = false;
        return WebDataConsumerHandle::ShouldWait;
    }

    Result endRead(size_t readSize)
    {
        ASSERT(m_pendingBuffer);
        ASSERT(m_pendingOffset + readSize <= m_pendingBuffer->length());
        m_pendingOffset += readSize;
        if (m_pendingOffset == m_pendingBuffer->length()) {
            m_pendingBuffer = nullptr;
            m_pendingOffset = 0;
        }
        return WebDataConsumerHandle::Ok;
    }

    void onRead(DOMUint8Array* buffer)
    {
        ASSERT(m_isReading);
        ASSERT(buffer);
        ASSERT(!m_pendingBuffer);
        ASSERT(!m_pendingOffset);
        m_isReading = false;
        m_pendingBuffer = buffer;
        notify();
    }

    void onReadDone()
    {
        ASSERT(m_isReading);
        ASSERT(!m_pendingBuffer);
        m_isReading = false;
        m_isDone = true;
        m_reader.clear();
        notify();
    }

    void onRejected()
    {
        ASSERT(m_isReading);
        ASSERT(!m_pendingBuffer);
        m_hasError = true;
        m_isReading = false;
        m_reader.clear();
        notify();
    }

    void notify()
    {
        if (!m_client)
            return;
        if (m_isInRecursion) {
            notifyLater();
            return;
        }
        m_client->didGetReadable();
    }

    void notifyLater()
    {
        ASSERT(m_client);
        Platform::current()->currentThread()->taskRunner()->postTask(BLINK_FROM_HERE, bind(&ReadingContext::notify, PassRefPtr<ReadingContext>(this)));
    }

private:
    ReadingContext(ScriptState* scriptState, v8::Local<v8::Value> stream)
        : m_client(nullptr)
        , m_weakPtrFactory(this)
        , m_pendingOffset(0)
        , m_isReading(false)
        , m_isDone(false)
        , m_hasError(false)
        , m_isInRecursion(false)
    {
        if (!ReadableStreamOperations::isLocked(scriptState, stream)) {
            // Here the stream implementation must not throw an exception.
            NonThrowableExceptionState es;
            m_reader = ReadableStreamOperations::getReader(scriptState, stream, es);
        }
        m_hasError = m_reader.isEmpty();
    }

    // This ScriptValue is leaky because it stores a strong reference to a
    // JavaScript object.
    // TODO(yhirano): Fix it.
    //
    // Holding a ScriptValue here is safe in terms of cross-world wrapper
    // leakage because we read only Uint8Array chunks from the reader.
    ScriptValue m_reader;
    WebDataConsumerHandle::Client* m_client;
    RefPtr<DOMUint8Array> m_pendingBuffer;
    WeakPtrFactory<ReadingContext> m_weakPtrFactory;
    size_t m_pendingOffset;
    bool m_isReading;
    bool m_isDone;
    bool m_hasError;
    bool m_isInRecursion;
};

ReadableStreamDataConsumerHandle::ReadableStreamDataConsumerHandle(ScriptState* scriptState, v8::Local<v8::Value> stream)
    : m_readingContext(ReadingContext::create(scriptState, stream))
{
}
ReadableStreamDataConsumerHandle::~ReadableStreamDataConsumerHandle() = default;

FetchDataConsumerHandle::Reader* ReadableStreamDataConsumerHandle::obtainReaderInternal(Client* client)
{
    return new ReadingContext::ReaderImpl(m_readingContext, client);
}

} // namespace blink

