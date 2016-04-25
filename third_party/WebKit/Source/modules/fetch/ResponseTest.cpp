// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/fetch/Response.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/Document.h"
#include "core/frame/Frame.h"
#include "core/testing/DummyPageHolder.h"
#include "modules/fetch/BodyStreamBuffer.h"
#include "modules/fetch/DataConsumerHandleTestUtil.h"
#include "modules/fetch/DataConsumerHandleUtil.h"
#include "modules/fetch/FetchResponseData.h"
#include "platform/blob/BlobData.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

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

class ServiceWorkerResponseTest : public ::testing::Test {
public:
    ServiceWorkerResponseTest()
        : m_page(DummyPageHolder::create(IntSize(1, 1))) { }

    ScriptState* getScriptState() { return ScriptState::forMainWorld(m_page->document().frame()); }
    ExecutionContext* getExecutionContext() { return getScriptState()->getExecutionContext(); }

private:
    OwnPtr<DummyPageHolder> m_page;
};


TEST_F(ServiceWorkerResponseTest, FromFetchResponseData)
{
    const KURL url(ParsedURLString, "http://www.response.com");

    FetchResponseData* fetchResponseData = FetchResponseData::create();
    fetchResponseData->setURL(url);

    Response* response = Response::create(getExecutionContext(), fetchResponseData);
    ASSERT(response);
    EXPECT_EQ(url, response->url());
}

TEST_F(ServiceWorkerResponseTest, FromWebServiceWorkerResponse)
{
    OwnPtr<WebServiceWorkerResponse> webResponse = createTestWebServiceWorkerResponse();
    Response* response = Response::create(getExecutionContext(), *webResponse);
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
    Response* response = Response::create(getExecutionContext(), *webResponse);

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
    Response* response = Response::create(getExecutionContext(), *webResponse);

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
    Response* response = Response::create(getExecutionContext(), *webResponse);

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
    Response* response = Response::create(getExecutionContext(), *webResponse);

    Headers* responseHeaders = response->headers();
    TrackExceptionState exceptionState;
    EXPECT_STREQ("", responseHeaders->get("set-cookie", exceptionState).utf8().data());
    EXPECT_STREQ("", responseHeaders->get("foo", exceptionState).utf8().data());
    EXPECT_STREQ("", responseHeaders->get("cache-control", exceptionState).utf8().data());
    EXPECT_FALSE(exceptionState.hadException());
}

void checkResponseStream(Response* response, bool checkResponseBodyStreamBuffer)
{
    BodyStreamBuffer* originalInternal = response->internalBodyBuffer();
    if (checkResponseBodyStreamBuffer) {
        EXPECT_EQ(response->bodyBuffer(), originalInternal);
    } else {
        EXPECT_FALSE(response->bodyBuffer());
    }

    TrackExceptionState exceptionState;
    Response* clonedResponse = response->clone(exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    if (!response->internalBodyBuffer())
        FAIL() << "internalBodyBuffer() must not be null.";
    if (!clonedResponse->internalBodyBuffer())
        FAIL() << "internalBodyBuffer() must not be null.";
    EXPECT_TRUE(response->internalBodyBuffer());
    EXPECT_TRUE(clonedResponse->internalBodyBuffer());
    EXPECT_TRUE(response->internalBodyBuffer());
    EXPECT_TRUE(clonedResponse->internalBodyBuffer());
    EXPECT_NE(response->internalBodyBuffer(), originalInternal);
    EXPECT_NE(clonedResponse->internalBodyBuffer(), originalInternal);
    EXPECT_NE(response->internalBodyBuffer(), clonedResponse->internalBodyBuffer());
    if (checkResponseBodyStreamBuffer) {
        EXPECT_EQ(response->bodyBuffer(), response->internalBodyBuffer());
        EXPECT_EQ(clonedResponse->bodyBuffer(), clonedResponse->internalBodyBuffer());
    } else {
        EXPECT_FALSE(response->bodyBuffer());
        EXPECT_FALSE(clonedResponse->bodyBuffer());
    }
    DataConsumerHandleTestUtil::MockFetchDataLoaderClient* client1 = new DataConsumerHandleTestUtil::MockFetchDataLoaderClient();
    DataConsumerHandleTestUtil::MockFetchDataLoaderClient* client2 = new DataConsumerHandleTestUtil::MockFetchDataLoaderClient();
    EXPECT_CALL(*client1, didFetchDataLoadedString(String("Hello, world")));
    EXPECT_CALL(*client2, didFetchDataLoadedString(String("Hello, world")));

    response->internalBodyBuffer()->startLoading(response->getExecutionContext(), FetchDataLoader::createLoaderAsString(), client1);
    clonedResponse->internalBodyBuffer()->startLoading(response->getExecutionContext(), FetchDataLoader::createLoaderAsString(), client2);
    blink::testing::runPendingTasks();
}

BodyStreamBuffer* createHelloWorldBuffer()
{
    using Command = DataConsumerHandleTestUtil::Command;
    OwnPtr<DataConsumerHandleTestUtil::ReplayingHandle> src(DataConsumerHandleTestUtil::ReplayingHandle::create());
    src->add(Command(Command::Data, "Hello, "));
    src->add(Command(Command::Data, "world"));
    src->add(Command(Command::Done));
    return new BodyStreamBuffer(createFetchDataConsumerHandleFromWebHandle(src.release()));
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneDefault)
{
    BodyStreamBuffer* buffer = createHelloWorldBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    Response* response = Response::create(getExecutionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBodyBuffer(), buffer);
    checkResponseStream(response, true);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneBasic)
{
    BodyStreamBuffer* buffer = createHelloWorldBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    fetchResponseData = fetchResponseData->createBasicFilteredResponse();
    Response* response = Response::create(getExecutionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBodyBuffer(), buffer);
    checkResponseStream(response, true);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneCORS)
{
    BodyStreamBuffer* buffer = createHelloWorldBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    fetchResponseData = fetchResponseData->createCORSFilteredResponse();
    Response* response = Response::create(getExecutionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBodyBuffer(), buffer);
    checkResponseStream(response, true);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneOpaque)
{
    BodyStreamBuffer* buffer = createHelloWorldBuffer();
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    fetchResponseData = fetchResponseData->createOpaqueFilteredResponse();
    Response* response = Response::create(getExecutionContext(), fetchResponseData);
    EXPECT_EQ(response->internalBodyBuffer(), buffer);
    checkResponseStream(response, false);
}

TEST_F(ServiceWorkerResponseTest, BodyStreamBufferCloneError)
{
    BodyStreamBuffer* buffer = new BodyStreamBuffer(createFetchDataConsumerHandleFromWebHandle(createUnexpectedErrorDataConsumerHandle()));
    FetchResponseData* fetchResponseData = FetchResponseData::createWithBuffer(buffer);
    fetchResponseData->setURL(KURL(ParsedURLString, "http://www.response.com"));
    Response* response = Response::create(getExecutionContext(), fetchResponseData);
    TrackExceptionState exceptionState;
    Response* clonedResponse = response->clone(exceptionState);
    EXPECT_FALSE(exceptionState.hadException());

    DataConsumerHandleTestUtil::MockFetchDataLoaderClient* client1 = new DataConsumerHandleTestUtil::MockFetchDataLoaderClient();
    DataConsumerHandleTestUtil::MockFetchDataLoaderClient* client2 = new DataConsumerHandleTestUtil::MockFetchDataLoaderClient();
    EXPECT_CALL(*client1, didFetchDataLoadFailed());
    EXPECT_CALL(*client2, didFetchDataLoadFailed());

    response->internalBodyBuffer()->startLoading(response->getExecutionContext(), FetchDataLoader::createLoaderAsString(), client1);
    clonedResponse->internalBodyBuffer()->startLoading(response->getExecutionContext(), FetchDataLoader::createLoaderAsString(), client2);
    blink::testing::runPendingTasks();
}

} // namespace
} // namespace blink
