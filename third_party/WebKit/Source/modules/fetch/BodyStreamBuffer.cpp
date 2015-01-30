// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/BodyStreamBuffer.h"

#include "core/dom/DOMArrayBuffer.h"

namespace blink {

namespace {

class BlobCreator final : public BodyStreamBuffer::Observer {
public:
    BlobCreator(BodyStreamBuffer* buffer, const String& contentType, BodyStreamBuffer::BlobHandleCreatorClient* client)
        : m_buffer(buffer)
        , m_client(client)
        , m_blobData(BlobData::create())
    {
        m_blobData->setContentType(contentType);
    }
    ~BlobCreator() override { }
    void trace(Visitor* visitor) override
    {
        visitor->trace(m_buffer);
        visitor->trace(m_client);
        BodyStreamBuffer::Observer::trace(visitor);
    }
    void onWrite() override
    {
        ASSERT(m_buffer);
        while (RefPtr<DOMArrayBuffer> buf = m_buffer->read()) {
            m_blobData->appendBytes(buf->data(), buf->byteLength());
        }
    }
    void onClose() override
    {
        ASSERT(m_buffer);
        const long long size = m_blobData->length();
        m_client->didCreateBlobHandle(BlobDataHandle::create(m_blobData.release(), size));
        cleanup();
    }
    void onError() override
    {
        ASSERT(m_buffer);
        m_client->didFail(m_buffer->exception());
        cleanup();
    }
    void start()
    {
        ASSERT(!m_buffer->isObserverRegistered());
        m_buffer->registerObserver(this);
        onWrite();
        if (m_buffer->hasError()) {
            return onError();
        }
        if (m_buffer->isClosed())
            return onClose();
    }
    void cleanup()
    {
        m_buffer->unregisterObserver();
        m_buffer.clear();
        m_client.clear();
        m_blobData.clear();
    }
private:
    Member<BodyStreamBuffer> m_buffer;
    Member<BodyStreamBuffer::BlobHandleCreatorClient> m_client;
    OwnPtr<BlobData> m_blobData;
};

} // namespace

PassRefPtr<DOMArrayBuffer> BodyStreamBuffer::read()
{
    if (m_queue.isEmpty())
        return PassRefPtr<DOMArrayBuffer>();
    return m_queue.takeFirst();
}

void BodyStreamBuffer::write(PassRefPtr<DOMArrayBuffer> chunk)
{
    ASSERT(!m_isClosed);
    ASSERT(!m_exception);
    ASSERT(chunk);
    m_queue.append(chunk);
    if (m_observer)
        m_observer->onWrite();
}

void BodyStreamBuffer::close()
{
    ASSERT(!m_isClosed);
    ASSERT(!m_exception);
    m_isClosed = true;
    if (m_observer)
        m_observer->onClose();
}

void BodyStreamBuffer::error(PassRefPtrWillBeRawPtr<DOMException> exception)
{
    ASSERT(exception);
    ASSERT(!m_isClosed);
    ASSERT(!m_exception);
    m_exception = exception;
    if (m_observer)
        m_observer->onError();
}

bool BodyStreamBuffer::readAllAndCreateBlobHandle(const String& contentType, BlobHandleCreatorClient* client)
{
    if (m_observer)
        return false;
    BlobCreator* blobCreator = new BlobCreator(this, contentType, client);
    blobCreator->start();
    return true;
}

bool BodyStreamBuffer::registerObserver(Observer* observer)
{
    if (m_observer)
        return false;
    ASSERT(observer);
    m_observer = observer;
    return true;
}

void BodyStreamBuffer::unregisterObserver()
{
    m_observer.clear();
}

void BodyStreamBuffer::trace(Visitor* visitor)
{
    visitor->trace(m_exception);
    visitor->trace(m_observer);
}

BodyStreamBuffer::BodyStreamBuffer()
    : m_isClosed(false)
{
}

} // namespace blink
