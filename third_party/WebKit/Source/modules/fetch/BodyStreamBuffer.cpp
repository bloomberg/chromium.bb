// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/BodyStreamBuffer.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

BodyStreamBuffer* BodyStreamBuffer::createEmpty()
{
    return BodyStreamBuffer::create(createFetchDataConsumerHandleFromWebHandle(createDoneDataConsumerHandle()));
}

FetchDataConsumerHandle* BodyStreamBuffer::handle() const
{
    ASSERT(!m_fetchDataLoader);
    ASSERT(!m_drainingStreamNotificationClient);
    return m_handle.get();
}

PassOwnPtr<FetchDataConsumerHandle> BodyStreamBuffer::releaseHandle()
{
    ASSERT(!m_fetchDataLoader);
    ASSERT(!m_drainingStreamNotificationClient);
    return m_handle.release();
}

class ClientWithFinishNotification final : public GarbageCollectedFinalized<ClientWithFinishNotification>, public FetchDataLoader::Client {
    USING_GARBAGE_COLLECTED_MIXIN(ClientWithFinishNotification);
public:
    ClientWithFinishNotification(BodyStreamBuffer* buffer, FetchDataLoader::Client* client)
        : m_buffer(buffer)
        , m_client(client)
    {
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_buffer);
        visitor->trace(m_client);
        FetchDataLoader::Client::trace(visitor);
    }

private:
    void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle> blobDataHandle) override
    {
        if (m_client)
            m_client->didFetchDataLoadedBlobHandle(blobDataHandle);
        m_buffer->didFetchDataLoadFinished();
    }
    void didFetchDataLoadedArrayBuffer(PassRefPtr<DOMArrayBuffer> arrayBuffer) override
    {
        if (m_client)
            m_client->didFetchDataLoadedArrayBuffer(arrayBuffer);
        m_buffer->didFetchDataLoadFinished();
    }
    void didFetchDataLoadedString(const String& str) override
    {
        if (m_client)
            m_client->didFetchDataLoadedString(str);
        m_buffer->didFetchDataLoadFinished();
    }
    void didFetchDataLoadedStream() override
    {
        if (m_client)
            m_client->didFetchDataLoadedStream();
        m_buffer->didFetchDataLoadFinished();
    }
    void didFetchDataLoadFailed() override
    {
        if (m_client)
            m_client->didFetchDataLoadFailed();
        m_buffer->didFetchDataLoadFinished();
    }

    Member<BodyStreamBuffer> m_buffer;
    Member<FetchDataLoader::Client> m_client;
};

void BodyStreamBuffer::setDrainingStreamNotificationClient(DrainingStreamNotificationClient* client)
{
    ASSERT(!m_fetchDataLoader);
    ASSERT(!m_drainingStreamNotificationClient);
    m_drainingStreamNotificationClient = client;
}

void BodyStreamBuffer::startLoading(FetchDataLoader* fetchDataLoader, FetchDataLoader::Client* client)
{
    ASSERT(!m_fetchDataLoader);
    m_fetchDataLoader = fetchDataLoader;
    m_fetchDataLoader->start(m_handle.get(), new ClientWithFinishNotification(this, client));
}

void BodyStreamBuffer::doDrainingStreamNotification()
{
    ASSERT(!m_fetchDataLoader);
    DrainingStreamNotificationClient* client = m_drainingStreamNotificationClient;
    m_drainingStreamNotificationClient.clear();
    if (client)
        client->didFetchDataLoadFinishedFromDrainingStream();
}

void BodyStreamBuffer::clearDrainingStreamNotification()
{
    ASSERT(!m_fetchDataLoader);
    m_drainingStreamNotificationClient.clear();
}

void BodyStreamBuffer::didFetchDataLoadFinished()
{
    ASSERT(m_fetchDataLoader);
    m_fetchDataLoader.clear();
    doDrainingStreamNotification();
}

DrainingBodyStreamBuffer::~DrainingBodyStreamBuffer()
{
    if (m_buffer)
        m_buffer->doDrainingStreamNotification();
}

void DrainingBodyStreamBuffer::startLoading(FetchDataLoader* fetchDataLoader, FetchDataLoader::Client* client)
{
    if (!m_buffer)
        return;

    m_buffer->startLoading(fetchDataLoader, client);
    m_buffer.clear();
}

BodyStreamBuffer* DrainingBodyStreamBuffer::leakBuffer()
{
    if (!m_buffer)
        return nullptr;

    m_buffer->clearDrainingStreamNotification();
    BodyStreamBuffer* buffer = m_buffer;
    m_buffer.clear();
    return buffer;
}

PassRefPtr<BlobDataHandle> DrainingBodyStreamBuffer::drainAsBlobDataHandle(FetchDataConsumerHandle::Reader::BlobSizePolicy blobSizePolicy)
{
    if (!m_buffer)
        return nullptr;

    RefPtr<BlobDataHandle> blobDataHandle = m_buffer->m_handle->obtainReader(nullptr)->drainAsBlobDataHandle(blobSizePolicy);
    if (!blobDataHandle)
        return nullptr;
    m_buffer->doDrainingStreamNotification();
    m_buffer.clear();
    return blobDataHandle.release();
}

DrainingBodyStreamBuffer::DrainingBodyStreamBuffer(BodyStreamBuffer* buffer, BodyStreamBuffer::DrainingStreamNotificationClient* client)
    : m_buffer(buffer)
{
    ASSERT(client);
    m_buffer->setDrainingStreamNotificationClient(client);
}

} // namespace blink
