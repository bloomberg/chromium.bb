// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/BodyStreamBuffer.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/ExceptionCode.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"

namespace blink {

class BodyStreamBuffer::LoaderHolder final : public GarbageCollectedFinalized<LoaderHolder>, public ActiveDOMObject, public FetchDataLoader::Client {
    WTF_MAKE_NONCOPYABLE(LoaderHolder);
    USING_GARBAGE_COLLECTED_MIXIN(LoaderHolder);
public:
    LoaderHolder(ExecutionContext* executionContext, BodyStreamBuffer* buffer, FetchDataLoader* loader, FetchDataLoader::Client* client)
        : ActiveDOMObject(executionContext)
        , m_buffer(buffer)
        , m_loader(loader)
        , m_client(client)
    {
        suspendIfNeeded();
    }

    void start(PassOwnPtr<FetchDataConsumerHandle> handle) { m_loader->start(handle.get(), this); }

    void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle) override
    {
        m_loader.clear();
        m_buffer->endLoading(this, EndLoadingDone);
        m_client->didFetchDataLoadedBlobHandle(blobDataHandle);
    }

    void didFetchDataLoadedArrayBuffer(PassRefPtr<DOMArrayBuffer> arrayBuffer) override
    {
        m_loader.clear();
        m_buffer->endLoading(this, EndLoadingDone);
        m_client->didFetchDataLoadedArrayBuffer(arrayBuffer);
    }

    void didFetchDataLoadedString(const String& string) override
    {
        m_loader.clear();
        m_buffer->endLoading(this, EndLoadingDone);
        m_client->didFetchDataLoadedString(string);
    }

    void didFetchDataLoadedStream() override
    {
        m_loader.clear();
        m_buffer->endLoading(this, EndLoadingDone);
        m_client->didFetchDataLoadedStream();
    }

    void didFetchDataLoadFailed() override
    {
        m_loader.clear();
        m_buffer->endLoading(this, EndLoadingErrored);
        m_client->didFetchDataLoadFailed();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_buffer);
        visitor->trace(m_loader);
        visitor->trace(m_client);
        ActiveDOMObject::trace(visitor);
        FetchDataLoader::Client::trace(visitor);
    }

private:
    void stop() override
    {
        if (m_loader) {
            m_loader->cancel();
            m_loader.clear();
            m_buffer->endLoading(this, EndLoadingErrored);
        }
    }

    Member<BodyStreamBuffer> m_buffer;
    Member<FetchDataLoader> m_loader;
    Member<FetchDataLoader::Client> m_client;
};

BodyStreamBuffer::BodyStreamBuffer(PassOwnPtr<FetchDataConsumerHandle> handle)
    : m_handle(handle)
    , m_reader(m_handle->obtainReader(this))
    , m_stream(new ReadableByteStream(this, new ReadableByteStream::StrictStrategy))
    , m_lockLevel(0)
    , m_streamNeedsMore(false)
{
    m_stream->didSourceStart();
}

PassRefPtr<BlobDataHandle> BodyStreamBuffer::drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy policy)
{
    ASSERT(!isLocked());
    if (ReadableStream::Closed == m_stream->stateInternal() || ReadableStream::Errored == m_stream->stateInternal())
        return nullptr;

    RefPtr<BlobDataHandle> blobDataHandle = m_reader->drainAsBlobDataHandle(policy);
    if (blobDataHandle) {
        close();
        return blobDataHandle.release();
    }
    return nullptr;
}

PassRefPtr<EncodedFormData> BodyStreamBuffer::drainAsFormData()
{
    ASSERT(!isLocked());
    if (ReadableStream::Closed == m_stream->stateInternal() || ReadableStream::Errored == m_stream->stateInternal())
        return nullptr;

    RefPtr<EncodedFormData> formData = m_reader->drainAsFormData();
    if (formData) {
        close();
        return formData.release();
    }
    return nullptr;
}

PassOwnPtr<FetchDataConsumerHandle> BodyStreamBuffer::lock(ExecutionContext* executionContext)
{
    ASSERT(!isLocked());
    ++m_lockLevel;
    m_reader = nullptr;
    OwnPtr<FetchDataConsumerHandle> handle = m_handle.release();
    if (ReadableStream::Closed == m_stream->stateInternal())
        return createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle());
    if (ReadableStream::Errored == m_stream->stateInternal())
        return createFetchDataConsumerHandleFromWebHandle(createUnexpectedErrorDataConsumerHandle());

    TrackExceptionState exceptionState;
    m_streamReader = m_stream->getBytesReader(executionContext, exceptionState);
    return handle.release();
}

void BodyStreamBuffer::startLoading(ExecutionContext* executionContext, FetchDataLoader* loader, FetchDataLoader::Client* client)
{
    OwnPtr<FetchDataConsumerHandle> handle = lock(executionContext);
    auto holder = new LoaderHolder(executionContext, this, loader, client);
    m_loaders.add(holder);
    holder->start(handle.release());
}

void BodyStreamBuffer::pullSource()
{
    ASSERT(!m_streamNeedsMore);
    m_streamNeedsMore = true;
    processData();
}

ScriptPromise BodyStreamBuffer::cancelSource(ScriptState* scriptState, ScriptValue)
{
    close();
    return ScriptPromise::cast(scriptState, v8::Undefined(scriptState->isolate()));
}

void BodyStreamBuffer::didGetReadable()
{
    if (!m_reader)
        return;

    if (!m_streamNeedsMore) {
        // Perform zero-length read to call close()/error() early.
        size_t readSize;
        WebDataConsumerHandle::Result result = m_reader->read(nullptr, 0, WebDataConsumerHandle::FlagNone, &readSize);
        switch (result) {
        case WebDataConsumerHandle::Ok:
        case WebDataConsumerHandle::ShouldWait:
            return;
        case WebDataConsumerHandle::Done:
            close();
            return;
        case WebDataConsumerHandle::Busy:
        case WebDataConsumerHandle::ResourceExhausted:
        case WebDataConsumerHandle::UnexpectedError:
            error();
            return;
        }
        return;
    }
    processData();
}

void BodyStreamBuffer::close()
{
    m_reader = nullptr;
    m_stream->close();
    m_handle.clear();
}

void BodyStreamBuffer::error()
{
    m_reader = nullptr;
    m_stream->error(DOMException::create(NetworkError, "network error"));
    m_handle.clear();
}

void BodyStreamBuffer::processData()
{
    ASSERT(m_reader);
    while (m_streamNeedsMore) {
        const void* buffer;
        size_t available;
        WebDataConsumerHandle::Result result = m_reader->beginRead(&buffer, WebDataConsumerHandle::FlagNone, &available);
        switch (result) {
        case WebDataConsumerHandle::Ok:
            m_streamNeedsMore = m_stream->enqueue(DOMUint8Array::create(static_cast<const unsigned char*>(buffer), available));
            m_reader->endRead(available);
            break;

        case WebDataConsumerHandle::Done:
            close();
            return;

        case WebDataConsumerHandle::ShouldWait:
            return;

        case WebDataConsumerHandle::Busy:
        case WebDataConsumerHandle::ResourceExhausted:
        case WebDataConsumerHandle::UnexpectedError:
            error();
            return;
        }
    }
}

void BodyStreamBuffer::unlock()
{
    ASSERT(m_lockLevel > 0);
    if (m_streamReader) {
        m_streamReader->releaseLock();
        m_streamReader = nullptr;
    }
    --m_lockLevel;
}

void BodyStreamBuffer::endLoading(FetchDataLoader::Client* client, EndLoadingMode mode)
{
    ASSERT(m_loaders.contains(client));
    m_loaders.remove(client);
    unlock();
    if (mode == EndLoadingDone) {
        close();
    } else {
        ASSERT(mode == EndLoadingErrored);
        error();
    }
}

} // namespace blink
