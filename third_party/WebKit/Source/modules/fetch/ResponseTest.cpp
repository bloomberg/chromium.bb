// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/fetch/Response.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/FetchResponseData.h"
#include "platform/blob/BlobData.h"
#include "public/platform/WebServiceWorkerResponse.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

const char kTestData[] = "Here is sample text for the blob.";

PassOwnPtr<WebServiceWorkerResponse> createTestWebServiceWorkerResponse()
{
    const KURL url(ParsedURLString, "http://www.webresponse.com/");
    const unsigned short status = 200;
    const String statusText = "the best status text";
    struct {
        const char* key;
        const char* value;
    } headers[] = { { "cache-control", "no-cache" }, { "set-cookie", "foop" }, { "foo", "bar" }, { 0, 0 } };

    OwnPtr<WebServiceWorkerResponse> webResponse = adoptPtr(new WebServiceWorkerResponse());
    webResponse->setURL(url);
    webResponse->setStatus(status);
    webResponse->setStatusText(statusText);
    webResponse->setResponseType(WebServiceWorkerResponseTypeDefault);
    for (int i = 0; headers[i].key; ++i)
        webResponse->setHeader(WebString::fromUTF8(headers[i].key), WebString::fromUTF8(headers[i].value));
    return webResponse.release();
}

class BlobHandleCreatorClient final : public BodyStreamBuffer::BlobHandleCreatorClient {
public:
    BlobHandleCreatorClient()
    {
    }
    ~BlobHandleCreatorClient() override { }
    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_exception);
        BodyStreamBuffer::BlobHandleCreatorClient::trace(visitor);
    }
    void didCreateBlobHandle(PassRefPtr<BlobDataHandle> blobHandle) override
    {
        m_blobHandle = blobHandle;
    }
    void didFail(PassRefPtrWillBeRawPtr<DOMException> exception) override
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


class ServiceWorkerResponseTest : public ::testing::Test {
public:
    ServiceWorkerResponseTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1))) { }

    ScriptState* scriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExecutionContext* executionContext() { return scriptState()->executionContext(); }

private:
    OwnPtr<DummyPageHolder> m_page;
};


TEST_F(ServiceWorkerResponseTest, FromFetchResponseData)
{
    const KURL url(ParsedURLString, "http://www.response.com");

    FetchResponseData* fetchResponseData = FetchResponseData::create();
    fetchResponseData->setURL(url);

    Response* response = Response::create(executionContext(), fetchResponseData);
    ASSERT(response);
    EXPECT_EQ(url, response->url());
}

TEST_F(ServiceWorkerResponseTest, FromWebServiceWorkerResponse)
{
    OwnPtr<WebServiceWorkerResponse> webResponse = createTestWebServiceWorkerResponse();
    Response* response = Response::create(executionContext(), *webResponse);
    ASSERT(response);
    EXPECT_EQ(webResponse->url(), response->url());
    EXPECT_EQ(webResponse->status(), response->status());
    EXPECT_STREQ(webResponse->statusText().utf8().c_str(), response->statusText().utf8().data());

    Headers* responseHeaders = response->headers();

    WebVector<WebString> keys = webResponse->getHeaderKeys();
    EXPECT_EQ(keys.size(), responseHeaders->headerList()->size());
    for (size_t i = 0, max = keys.size(); i < max; ++i) {
        WebString key = keys[i];
        TrackExceptionState exceptionState;
        EXPECT_STREQ(webResponse->getHeader(key).utf8().c_str(), responseHeaders->get(key, exceptionState).utf8().data());
        EXPECT_FALSE(exceptionState.hadException());
    }
}

TEST_F(ServiceWorkerResponseTest, FromWebServiceWorkerResponseDefault)
{
    OwnPtr<WebServiceWorkerResponse> webResponse = createTestWebServiceWorkerResponse();
    webResponse->setResponseType(WebServiceWorkerResponseTypeDefault);
    Response* response = Response::create(executionContext(), *webResponse);

    Headers* responseHeaders = response->headers();
    TrackExceptionState exceptionState;
    EXPECT_STREQ("foop", responseHeaders->get("set-cookie", exceptionState).utf8().data());
    EXPECT_STREQ("bar", responseHeaders->get("foo", exceptionState).utf8().data());
    EXPECT_STREQ("no-cache", responseHeaders->get("cache-control", exceptionState).utf8().data());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ServiceWorkerResponseTest, FromWebServiceWorkerResponseBasic)
{
    OwnPtr<WebServiceWorkerResponse> webResponse = createTestWebServiceWorkerResponse();
    webResponse->setResponseType(WebServiceWorkerResponseTypeBasic);
    Response* response = Response::create(executionContext(), *webResponse);

    Headers* responseHeaders = response->headers();
    TrackExceptionState exceptionState;
    EXPECT_STREQ("", responseHeaders->get("set-cookie", exceptionState).utf8().data());
    EXPECT_STREQ("bar", responseHeaders->get("foo", exceptionState).utf8().data());
    EXPECT_STREQ("no-cache", responseHeaders->get("cache-control", exceptionState).utf8().data());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ServiceWorkerResponseTest, FromWebServiceWorkerResponseCORS)
{
    OwnPtr<WebServiceWorkerResponse> webResponse = createTestWebServiceWorkerResponse();
    webResponse->setResponseType(WebServiceWorkerResponseTypeCORS);
    Response* response = Response::create(executionContext(), *webResponse);

    Headers* responseHeaders = response->headers();
    TrackExceptionState exceptionState;
    EXPECT_STREQ("", responseHeaders->get("set-cookie", exceptionState).utf8().data());
    EXPECT_STREQ("", responseHeaders->get("foo", exceptionState).utf8().data());
    EXPECT_STREQ("no-cache", responseHeaders->get("cache-control", exceptionState).utf8().data());
    EXPECT_FALSE(exceptionState.hadException());
}

TEST_F(ServiceWorkerResponseTest, FromWebServiceWorkerResponseOpaque)
{
    OwnPtr<WebServiceWorkerResponse> webResponse = createTestWebServiceWorkerResponse();
    webResponse->setResponseType(WebServiceWorkerResponseTypeOpaque);
    Response* response = Response::create(executionContext(), *webResponse);

    Headers* responseHeaders = response->headers();
    TrackExceptionState exceptionState;
    EXPECT_STREQ("", responseHeaders->get("set-cookie", exceptionState).utf8().data());
    EXPECT_STREQ("", responseHeaders->get("foo", exceptionState).utf8().data());
    EXPECT_STREQ("", responseHeaders->get("cache-control", exceptionState).utf8().data());
    EXPECT_FALSE(exceptionState.hadException());
}

void checkResponseBlobHandle(Response* response, bool hasNonInternalBlobHandle, const unsigned long long blobSize)
{
    EXPECT_TRUE(response->internalBlobDataHandle());
    EXPECT_EQ(blobSize, response->internalBlobDataHandle()->size());
    EXPECT_FALSE(response->internalBuffer());
    if (hasNonInternalBlobHandle) {
        EXPECT_TRUE(response->blobDataHandle());
        EXPECT_EQ(response->blobDataHandle(), response->internalBlobDataHandle());
        EXPECT_EQ(blobSize, response->blobDataHandle()->size());
    } else {
        EXPECT_FALSE(response->blobDataHandle());
    }
    EXPECT_FALSE(response->buffer());

    TrackExceptionState exceptionState;
    Response* clonedResponse = response->clone(exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    EXPECT_TRUE(response->internalBlobDataHandle());
    EXPECT_EQ(blobSize, clonedResponse->internalBlobDataHandle()->size());
    EXPECT_EQ(response->internalBlobDataHandle(), clonedResponse->internalBlobDataHandle());
    EXPECT_FALSE(response->internalBuffer());
    EXPECT_FALSE(clonedResponse->internalBuffer());
    if (hasNonInternalBlobHandle) {
        EXPECT_EQ(response->internalBlobDataHandle(), response->blobDataHandle());
        EXPECT_EQ(clonedResponse->internalBlobDataHandle(), clonedResponse->blobDataHandle());
    } else {
        EXPECT_FALSE(response->blobDataHandle());
        EXPECT_FALSE(clonedResponse->blobDataHandle());
    }
    EXPECT_FALSE(response->buffer());
    EXPECT_FALSE(clonedResponse->buffer());
}

TEST_F(ServiceWorkerResponseTest, BlobHandleCloneDefault)
{
    FetchResponseData* fetchResponseData = FetchResponseData::create();
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    OwnPtr<BlobData> blobData(BlobData::create());
    blobData->appendBytes(kTestData, sizeof(kTestData) - 1);
    const unsigned long long size = blobData->length();
    fetchResponseData->setBlobDataHandle(BlobDataHandle::create(blobData.release(), size));
    checkResponseBlobHandle(Response::create(executionContext(), fetchResponseData), true, size);
}

TEST_F(ServiceWorkerResponseTest, BlobHandleCloneBasic)
{
    FetchResponseData* fetchResponseData = FetchResponseData::create();
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    OwnPtr<BlobData> blobData(BlobData::create());
    blobData->appendBytes(kTestData, sizeof(kTestData) - 1);
    const unsigned long long size = blobData->length();
    fetchResponseData->setBlobDataHandle(BlobDataHandle::create(blobData.release(), size));
    fetchResponseData = fetchResponseData->createBasicFilteredResponse();
    checkResponseBlobHandle(Response::create(executionContext(), fetchResponseData), true, size);
}

TEST_F(ServiceWorkerResponseTest, BlobHandleCloneCORS)
{
    FetchResponseData* fetchResponseData = FetchResponseData::create();
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    OwnPtr<BlobData> blobData(BlobData::create());
    blobData->appendBytes(kTestData, sizeof(kTestData) - 1);
    const unsigned long long size = blobData->length();
    fetchResponseData->setBlobDataHandle(BlobDataHandle::create(blobData.release(), size));
    fetchResponseData = fetchResponseData->createCORSFilteredResponse();
    checkResponseBlobHandle(Response::create(executionContext(), fetchResponseData), true, size);
}

TEST_F(ServiceWorkerResponseTest, BlobHandleCloneOpaque)
{
    FetchResponseData* fetchResponseData = FetchResponseData::create();
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    OwnPtr<BlobData> blobData(BlobData::create());
    blobData->appendBytes(kTestData, sizeof(kTestData) - 1);
    const unsigned long long size = blobData->length();
    fetchResponseData->setBlobDataHandle(BlobDataHandle::create(blobData.release(), size));
    fetchResponseData = fetchResponseData->createOpaqueFilteredResponse();
    checkResponseBlobHandle(Response::create(executionContext(), fetchResponseData), false, size);
}

void checkResponseStream(Response* response, bool checkResponseBodyStreamBuffer)
{
    BodyStreamBuffer* buffer = response->internalBuffer();
    EXPECT_FALSE(response->internalBlobDataHandle());
    EXPECT_FALSE(response->blobDataHandle());
    if (checkResponseBodyStreamBuffer) {
        EXPECT_EQ(response->buffer(), buffer);
    } else {
        EXPECT_FALSE(response->buffer());
    }

    TrackExceptionState exceptionState;
    Response* clonedResponse = response->clone(exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    EXPECT_FALSE(response->internalBlobDataHandle());
    EXPECT_FALSE(response->blobDataHandle());
    EXPECT_FALSE(clonedResponse->internalBlobDataHandle());
    EXPECT_FALSE(clonedResponse->blobDataHandle());

    EXPECT_TRUE(response->internalBuffer());
    EXPECT_TRUE(clonedResponse->internalBuffer());
    EXPECT_NE(response->internalBuffer(), buffer);
    EXPECT_NE(clonedResponse->internalBuffer(), buffer);
    EXPECT_NE(response->internalBuffer(), clonedResponse->internalBuffer());
    if (checkResponseBodyStreamBuffer) {
        EXPECT_EQ(response->buffer(), response->internalBuffer());
        EXPECT_EQ(clonedResponse->buffer(), clonedResponse->internalBuffer());
    } else {
        EXPECT_FALSE(response->buffer());
        EXPECT_FALSE(clonedResponse->buffer());
    }
    BlobHandleCreatorClient* client1 = new BlobHandleCreatorClient();
    BlobHandleCreatorClient* client2 = new BlobHandleCreatorClient();
    EXPECT_TRUE(response->internalBuffer()->readAllAndCreateBlobHandle(response->internalContentTypeForBuffer(), client1));
    EXPECT_TRUE(clonedResponse->internalBuffer()->readAllAndCreateBlobHandle(clonedResponse->internalContentTypeForBuffer(), client2));
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    buffer->write(DOMArrayBuffer::create("piyo", 4));
    EXPECT_FALSE(client1->blobHandle());
    EXPECT_FALSE(client2->blobHandle());
    buffer->close();
    EXPECT_TRUE(client1->blobHandle());
    EXPECT_TRUE(client2->blobHandle());
    EXPECT_EQ(10u, client1->blobHandle()->size());
    EXPECT_EQ(10u, client2->blobHandle()->size());
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneDefault)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    Response* response = Response::create(executionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBuffer(), buffer);
    checkResponseStream(response, true);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneBasic)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    fetchResponseData = fetchResponseData->createBasicFilteredResponse();
    Response* response = Response::create(executionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBuffer(), buffer);
    checkResponseStream(response, true);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneCORS)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    fetchResponseData = fetchResponseData->createCORSFilteredResponse();
    Response* response = Response::create(executionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBuffer(), buffer);
    checkResponseStream(response, true);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneOpaque)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    fetchResponseData = fetchResponseData->createOpaqueFilteredResponse();
    Response* response = Response::create(executionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBuffer(), buffer);
    checkResponseStream(response, false);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneError)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    Response* response = Response::create(executionContext(), fetchResponseData);
    TrackExceptionState exceptionState;
    Response* clonedResponse = response->clone(exceptionState);
    EXPECT_FALSE(exceptionState.hadException());
    BlobHandleCreatorClient* client1 = new BlobHandleCreatorClient();
    BlobHandleCreatorClient* client2 = new BlobHandleCreatorClient();
    EXPECT_TRUE(response->internalBuffer()->readAllAndCreateBlobHandle(response->internalContentTypeForBuffer(), client1));
    EXPECT_TRUE(clonedResponse->internalBuffer()->readAllAndCreateBlobHandle(clonedResponse->internalContentTypeForBuffer(), client2));
    buffer->write(DOMArrayBuffer::create("foobar", 6));
    buffer->write(DOMArrayBuffer::create("piyo", 4));
    EXPECT_FALSE(client1->blobHandle());
    EXPECT_FALSE(client2->blobHandle());
    buffer->error(DOMException::create(NetworkError, "Error Message"));
    EXPECT_EQ("NetworkError", client1->exception()->name());
    EXPECT_EQ("Error Message", client1->exception()->message());
    EXPECT_EQ("NetworkError", client2->exception()->name());
    EXPECT_EQ("Error Message", client2->exception()->message());
}

} // namespace
} // namespace blink
