// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/BodyStreamBuffer.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMTypedArray.h"
#include "core/dom/ExceptionCode.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "platform/blob/BlobData.h"
#include "platform/network/EncodedFormData.h"

namespace blink {

class BodyStreamBuffer::LoaderClient final : public GarbageCollectedFinalized<LoaderClient>, public ActiveDOMObject, public FetchDataLoader::Client {
    WTF_MAKE_NONCOPYABLE(LoaderClient);
    USING_GARBAGE_COLLECTED_MIXIN(LoaderClient);
public:
    LoaderClient(ExecutionContext* executionContext, BodyStreamBuffer* buffer, FetchDataLoader::Client* client)
        : ActiveDOMObject(executionContext)
        , m_buffer(buffer)
        , m_client(client)
    {
        suspendIfNeeded();
    }

    void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle) override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedBlobHandle(blobDataHandle);
    }

    void didFetchDataLoadedArrayBuffer(PassRefPtr<DOMArrayBuffer> arrayBuffer) override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedArrayBuffer(arrayBuffer);
    }

    void didFetchDataLoadedString(const String& string) override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedString(string);
    }

    void didFetchDataLoadedStream() override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadedStream();
    }

    void didFetchDataLoadFailed() override
    {
        m_buffer->endLoading();
        m_client->didFetchDataLoadFailed();
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_buffer);
        visitor->trace(m_client);
        ActiveDOMObject::trace(visitor);
        FetchDataLoader::Client::trace(visitor);
    }

private:
    void stop() override
    {
        m_buffer->stopLoading();
    }

    Member<BodyStreamBuffer> m_buffer;
    Member<FetchDataLoader::Client> m_client;
};

BodyStreamBuffer::BodyStreamBuffer(PassOwnPtr<FetchDataConsumerHandle> handle)
    : m_handle(handle)
    , m_reader(m_handle->obtainReader(this))
    , m_stream(new ReadableByteStream(this, new ReadableByteStream::StrictStrategy))
    , m_streamNeedsMore(false)
{
    m_stream->didSourceStart();
}

PassRefPtr<BlobDataHandle> BodyStreamBuffer::drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy policy)
{
    ASSERT(!stream()->isLocked());
    m_stream->setIsDisturbed();
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
    ASSERT(!stream()->isLocked());
    m_stream->setIsDisturbed();
    if (ReadableStream::Closed == m_stream->stateInternal() || ReadableStream::Errored == m_stream->stateInternal())
        return nullptr;

    RefPtr<EncodedFormData> formData = m_reader->drainAsFormData();
    if (formData) {
        close();
        return formData.release();
    }
    return nullptr;
}

PassOwnPtr<FetchDataConsumerHandle> BodyStreamBuffer::releaseHandle(ExecutionContext* executionContext)
{
    ASSERT(!stream()->isLocked());
    m_reader = nullptr;
    m_stream->setIsDisturbed();
    TrackExceptionState exceptionState;
    m_stream->getBytesReader(executionContext, exceptionState);

    if (ReadableStream::Closed == m_stream->stateInternal())
        return createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle());
    if (ReadableStream::Errored == m_stream->stateInternal())
        return createFetchDataConsumerHandleFromWebHandle(createUnexpectedErrorDataConsumerHandle());

    ASSERT(m_handle);
    OwnPtr<FetchDataConsumerHandle> handle = m_handle.release();
    close();
    return handle.release();
}

void BodyStreamBuffer::startLoading(ExecutionContext* executionContext, FetchDataLoader* loader, FetchDataLoader::Client* client)
{
    ASSERT(!m_loader);
    OwnPtr<FetchDataConsumerHandle> handle = releaseHandle(executionContext);
    m_loader = loader;
    loader->start(handle.get(), new LoaderClient(executionContext, this, client));
}

bool BodyStreamBuffer::hasPendingActivity() const
{
    return m_loader || (m_stream->isLocked() && m_stream->stateInternal() == ReadableStream::Readable);
}

void BodyStreamBuffer::stop()
{
    m_reader = nullptr;
    m_handle = nullptr;
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

void BodyStreamBuffer::endLoading()
{
    ASSERT(m_loader);
    m_loader = nullptr;
}

void BodyStreamBuffer::stopLoading()
{
    if (!m_loader)
        return;
    m_loader->cancel();
    m_loader = nullptr;
}

} // namespace blink
