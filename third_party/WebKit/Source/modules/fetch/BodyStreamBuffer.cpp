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
    DEFINE_INLINE_VIRTUAL_TRACE()
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

class StreamTeePump : public BodyStreamBuffer::Observer {
public:
    StreamTeePump(BodyStreamBuffer* inBuffer, BodyStreamBuffer* outBuffer1, BodyStreamBuffer* outBuffer2)
        : m_inBuffer(inBuffer)
        , m_outBuffer1(outBuffer1)
        , m_outBuffer2(outBuffer2)
    {
    }
    void onWrite() override
    {
        while (RefPtr<DOMArrayBuffer> buf = m_inBuffer->read()) {
            m_outBuffer1->write(buf);
            m_outBuffer2->write(buf);
        }
    }
    void onClose() override
    {
        m_outBuffer1->close();
        m_outBuffer2->close();
        cleanup();
    }
    void onError() override
    {
        m_outBuffer1->error(m_inBuffer->exception());
        m_outBuffer2->error(m_inBuffer->exception());
        cleanup();
    }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        BodyStreamBuffer::Observer::trace(visitor);
        visitor->trace(m_inBuffer);
        visitor->trace(m_outBuffer1);
        visitor->trace(m_outBuffer2);
    }
    void start()
    {
        m_inBuffer->registerObserver(this);
        onWrite();
        if (m_inBuffer->hasError())
            return onError();
        if (m_inBuffer->isClosed())
            return onClose();
    }

private:
    void cleanup()
    {
        m_inBuffer->unregisterObserver();
        m_inBuffer.clear();
        m_outBuffer1.clear();
        m_outBuffer2.clear();
    }
    Member<BodyStreamBuffer> m_inBuffer;
    Member<BodyStreamBuffer> m_outBuffer1;
    Member<BodyStreamBuffer> m_outBuffer2;
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

bool BodyStreamBuffer::startTee(BodyStreamBuffer* out1, BodyStreamBuffer* out2)
{
    if (m_observer)
        return false;
    StreamTeePump* teePump = new StreamTeePump(this, out1, out2);
    teePump->start();
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

DEFINE_TRACE(BodyStreamBuffer)
{
    visitor->trace(m_exception);
    visitor->trace(m_observer);
}

BodyStreamBuffer::BodyStreamBuffer()
    : m_isClosed(false)
{
}

} // namespace blink
