// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/BodyStreamBuffer.h"

#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "platform/heap/Heap.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include "public/platform/WebVector.h"
#include "wtf/RefPtr.h"
#include "wtf/testing/WTFTestHelpers.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

class MockObserver final : public BodyStreamBuffer::Observer {
public:
    MockObserver()
        : m_writeCount(0)
        , m_closeCount(0)
        , m_errorCount(0)
    {
    }
    ~MockObserver() override { }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        BodyStreamBuffer::Observer::trace(visitor);
    }
    void onWrite() override { ++m_writeCount; }
    void onClose() override { ++m_closeCount; }
    void onError() override { ++m_errorCount; }
    int writeCount() const { return m_writeCount; }
    int closeCount() const { return m_closeCount; }
    int errorCount() const { return m_errorCount; }

private:
    int m_writeCount;
    int m_closeCount;
    int m_errorCount;
};

class BlobHandleCallback final : public BodyStreamBuffer::BlobHandleCreatorClient {
public:
    BlobHandleCallback()
    {
    }
    virtual ~BlobHandleCallback() override { }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_exception);
        BodyStreamBuffer::BlobHandleCreatorClient::trace(visitor);
    }
    virtual void didCreateBlobHandle(PassRefPtr<BlobDataHandle> blobHandle) override
    {
        m_blobHandle = blobHandle;
    }
    virtual void didFail(PassRefPtrWillBeRawPtr<DOMException> exception) override
    {
        m_exception = exception;
    }
    PassRefPtr<BlobDataHandle> blobHandle()
    {
        return m_blobHandle;
    }
    PassRefPtrWillBeRawPtr<DOMException> exception()
    {
        return m_exception;
    }

private:
    RefPtr<BlobDataHandle> m_blobHandle;
    RefPtrWillBeMember<DOMException> m_exception;
};

} // namespace

TEST(BodyStreamBufferTest, Read)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    RefPtr<DOMArrayBuffer> arrayBuffer1 = DOMArrayBuffer::create("foobar", 6);
    RefPtr<DOMArrayBuffer> arrayBuffer2 = DOMArrayBuffer::create("abc", 3);
    RefPtr<DOMArrayBuffer> arrayBuffer3 = DOMArrayBuffer::create("piyo", 4);
    buffer->write(arrayBuffer1);
    buffer->write(arrayBuffer2);
    EXPECT_EQ(arrayBuffer1, buffer->read());
    EXPECT_EQ(arrayBuffer2, buffer->read());
    EXPECT_FALSE(buffer->read());
    buffer->write(arrayBuffer3);
    EXPECT_EQ(arrayBuffer3, buffer->read());
    EXPECT_FALSE(buffer->read());
}

TEST(BodyStreamBufferTest, Exception)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    EXPECT_FALSE(buffer->exception());
    buffer->error(DOMException::create(NetworkError, "Error Message"));
    EXPECT_TRUE(buffer->exception());
    EXPECT_EQ("NetworkError", buffer->exception()->name());
    EXPECT_EQ("Error Message", buffer->exception()->message());
}

TEST(BodyStreamBufferTest, Observer)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    MockObserver* observer1 = new MockObserver();
    MockObserver* observer2 = new MockObserver();
    EXPECT_TRUE(buffer->registerObserver(observer1));
    EXPECT_FALSE(buffer->registerObserver(observer2));
    EXPECT_EQ(0, observer1->writeCount());
    EXPECT_EQ(0, observer1->closeCount());
    EXPECT_EQ(0, observer1->errorCount());
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    EXPECT_EQ(1, observer1->writeCount());
    EXPECT_EQ(0, observer1->closeCount());
    EXPECT_EQ(0, observer1->errorCount());
    buffer->write(DOMArrayBuffer::create("piyo", 4));
    EXPECT_EQ(2, observer1->writeCount());
    EXPECT_EQ(0, observer1->closeCount());
    EXPECT_EQ(0, observer1->errorCount());
    EXPECT_FALSE(buffer->isClosed());
    buffer->close();
    EXPECT_TRUE(buffer->isClosed());
    EXPECT_EQ(2, observer1->writeCount());
    EXPECT_EQ(1, observer1->closeCount());
    EXPECT_EQ(0, observer1->errorCount());
    EXPECT_EQ(0, observer2->writeCount());
    EXPECT_EQ(0, observer2->closeCount());
    EXPECT_EQ(0, observer2->errorCount());
}

TEST(BodyStreamBufferTest, CreateBlob)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    BlobHandleCallback* callback1 = new BlobHandleCallback();
    BlobHandleCallback* callback2 = new BlobHandleCallback();
    EXPECT_TRUE(buffer->readAllAndCreateBlobHandle("text/html", callback1));
    EXPECT_FALSE(buffer->readAllAndCreateBlobHandle("text/html", callback2));
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    EXPECT_FALSE(callback1->blobHandle());
    buffer->write(DOMArrayBuffer::create("piyo", 4));
    EXPECT_FALSE(callback1->blobHandle());
    buffer->close();
    EXPECT_TRUE(callback1->blobHandle());
    EXPECT_EQ("text/html", callback1->blobHandle()->type());
    EXPECT_FALSE(callback2->blobHandle());
    EXPECT_EQ(10u, callback1->blobHandle()->size());
    WebVector<WebBlobData::Item*> items;
    EXPECT_TRUE(Platform::current()->unitTestSupport()->getBlobItems(callback1->blobHandle()->uuid(), &items));
    EXPECT_EQ(2u, items.size());
    EXPECT_EQ(6u, items[0]->data.size());
    EXPECT_EQ(0, memcmp(items[0]->data.data(), "foobar", 6));
    EXPECT_EQ(4u, items[1]->data.size());
    EXPECT_EQ(0, memcmp(items[1]->data.data(), "piyo", 4));
    EXPECT_FALSE(callback1->exception());
    EXPECT_FALSE(callback2->exception());
}

TEST(BodyStreamBufferTest, CreateBlobAfterWrite)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    BlobHandleCallback* callback = new BlobHandleCallback();
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    EXPECT_TRUE(buffer->readAllAndCreateBlobHandle("", callback));
    buffer->close();
    EXPECT_TRUE(callback->blobHandle());
    EXPECT_EQ(6u, callback->blobHandle()->size());
    WebVector<WebBlobData::Item*> items;
    EXPECT_TRUE(Platform::current()->unitTestSupport()->getBlobItems(callback->blobHandle()->uuid(), &items));
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(6u, items[0]->data.size());
    EXPECT_EQ(0, memcmp(items[0]->data.data(), "foobar", 6));
}

TEST(BodyStreamBufferTest, CreateBlobAfterClose)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    BlobHandleCallback* callback = new BlobHandleCallback();
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    buffer->close();
    EXPECT_TRUE(buffer->readAllAndCreateBlobHandle("", callback));
    EXPECT_TRUE(callback->blobHandle());
    EXPECT_EQ(6u, callback->blobHandle()->size());
    WebVector<WebBlobData::Item*> items;
    EXPECT_TRUE(Platform::current()->unitTestSupport()->getBlobItems(callback->blobHandle()->uuid(), &items));
    EXPECT_EQ(1u, items.size());
    EXPECT_EQ(6u, items[0]->data.size());
    EXPECT_EQ(0, memcmp(items[0]->data.data(), "foobar", 6));
}

TEST(BodyStreamBufferTest, CreateBlobException)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    BlobHandleCallback* callback1 = new BlobHandleCallback();
    BlobHandleCallback* callback2 = new BlobHandleCallback();
    EXPECT_TRUE(buffer->readAllAndCreateBlobHandle("", callback1));
    EXPECT_FALSE(buffer->readAllAndCreateBlobHandle("", callback2));
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    buffer->write(DOMArrayBuffer::create("piyo", 4));
    EXPECT_FALSE(buffer->hasError());
    buffer->error(DOMException::create(NetworkError, "Error Message"));
    EXPECT_TRUE(buffer->hasError());
    EXPECT_FALSE(callback1->blobHandle());
    EXPECT_FALSE(callback2->blobHandle());
    EXPECT_TRUE(callback1->exception());
    EXPECT_FALSE(callback2->exception());
    EXPECT_EQ("NetworkError", callback1->exception()->name());
    EXPECT_EQ("Error Message", callback1->exception()->message());
}

TEST(BodyStreamBufferTest, CreateBlobExceptionAfterWrite)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    BlobHandleCallback* callback = new BlobHandleCallback();
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    EXPECT_TRUE(buffer->readAllAndCreateBlobHandle("", callback));
    buffer->error(DOMException::create(NetworkError, "Error Message"));
    EXPECT_TRUE(callback->exception());
    EXPECT_EQ("NetworkError", callback->exception()->name());
    EXPECT_EQ("Error Message", callback->exception()->message());
}

TEST(BodyStreamBufferTest, CreateBlobExceptionAfterError)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    BlobHandleCallback* callback = new BlobHandleCallback();
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    buffer->error(DOMException::create(NetworkError, "Error Message"));
    EXPECT_TRUE(buffer->readAllAndCreateBlobHandle("", callback));
    EXPECT_TRUE(callback->exception());
    EXPECT_EQ("NetworkError", callback->exception()->name());
    EXPECT_EQ("Error Message", callback->exception()->message());
}

} // namespace blink
