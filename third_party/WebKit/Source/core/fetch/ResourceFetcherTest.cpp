/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/fetch/ResourceFetcher.h"

#include "core/fetch/FetchInitiatorInfo.h"
#include "core/fetch/FetchInitiatorTypeNames.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/RawResource.h"
#include "core/fetch/ResourceLoader.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/heap/Handle.h"
#include "platform/network/ResourceRequest.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockTaskRunner : public blink::WebTaskRunner {
    void postTask(const WebTraceLocation&, Task*) override { }
    void postDelayedTask(const WebTraceLocation&, Task*, double) override { }
    WebTaskRunner* clone() override { return nullptr; }
    double virtualTimeSeconds() const override { return 0.0; }
    double monotonicallyIncreasingVirtualTimeSeconds() const override { return 0.0; }
};

}

class ResourceFetcherTestMockFetchContext : public FetchContext {
public:
    static ResourceFetcherTestMockFetchContext* create()
    {
        return new ResourceFetcherTestMockFetchContext;
    }

    virtual ~ResourceFetcherTestMockFetchContext() { }

    bool allowImage(bool imagesEnabled, const KURL&) const override { return true; }
    bool canRequest(Resource::Type, const ResourceRequest&, const KURL&, const ResourceLoaderOptions&, bool forPreload, FetchRequest::OriginRestriction) const override { return true; }
    bool shouldLoadNewResource(Resource::Type) const override { return true; }
    WebTaskRunner* loadingTaskRunner() const override { return m_runner.get(); }

    void setCachePolicy(CachePolicy policy) { m_policy = policy; }
    CachePolicy getCachePolicy() const override { return m_policy; }

private:
    ResourceFetcherTestMockFetchContext()
        : m_policy(CachePolicyVerify)
        , m_runner(adoptPtr(new MockTaskRunner))
    { }

    CachePolicy m_policy;
    OwnPtr<MockTaskRunner> m_runner;
};

class ResourceFetcherTest : public ::testing::Test {
};

class TestResourceFactory : public ResourceFactory {
public:
    TestResourceFactory(Resource::Type type = Resource::Raw)
        : ResourceFactory(type) { }

    Resource* create(const ResourceRequest& request, const ResourceLoaderOptions& options, const String& charset) const override
    {
        return Resource::create(request, type(), options);
    }
};

TEST_F(ResourceFetcherTest, StartLoadAfterFrameDetach)
{
    KURL secureURL(ParsedURLString, "https://secureorigin.test/image.png");
    // Try to request a url. The request should fail, no resource should be returned,
    // and no resource should be present in the cache.
    ResourceFetcher* fetcher = ResourceFetcher::create(nullptr);
    FetchRequest fetchRequest = FetchRequest(ResourceRequest(secureURL), FetchInitiatorInfo());
    Resource* resource = fetcher->requestResource(fetchRequest, TestResourceFactory());
    EXPECT_EQ(resource, static_cast<Resource*>(nullptr));
    EXPECT_EQ(memoryCache()->resourceForURL(secureURL), static_cast<Resource*>(nullptr));

    // Try calling Resource::load directly. This shouldn't crash.
    Resource* resource2 = Resource::create(secureURL, Resource::Raw);
    resource2->load(fetcher);
}

TEST_F(ResourceFetcherTest, UseExistingResource)
{
    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());

    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    Resource* resource = Resource::create(url, Resource::Image);
    memoryCache()->add(resource);
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    resource->responseReceived(response, nullptr);
    resource->finish();

    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Image));
    EXPECT_EQ(resource, newResource);
    memoryCache()->remove(resource);
}

TEST_F(ResourceFetcherTest, Vary)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    Resource* resource = Resource::create(url, Resource::Raw);
    memoryCache()->add(resource);
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::Vary, "*");
    resource->responseReceived(response, nullptr);
    resource->finish();
    ASSERT_TRUE(resource->hasVaryHeader());

    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Platform::current()->getURLLoaderMockFactory()->registerURL(url, WebURLResponse(), "");
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory());
    EXPECT_NE(resource, newResource);
    newResource->loader()->cancel();
    memoryCache()->remove(newResource);
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);

    memoryCache()->remove(resource);
}

TEST_F(ResourceFetcherTest, VaryOnBack)
{
    ResourceFetcherTestMockFetchContext* context = ResourceFetcherTestMockFetchContext::create();
    context->setCachePolicy(CachePolicyHistoryBuffer);
    ResourceFetcher* fetcher = ResourceFetcher::create(context);

    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    Resource* resource = Resource::create(url, Resource::Raw);
    memoryCache()->add(resource);
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::Vary, "*");
    resource->responseReceived(response, nullptr);
    resource->finish();
    ASSERT_TRUE(resource->hasVaryHeader());

    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory());
    EXPECT_EQ(resource, newResource);

    memoryCache()->remove(newResource);
}

TEST_F(ResourceFetcherTest, VaryImage)
{
    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());

    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::Vary, "*");
    URLTestHelpers::registerMockedURLLoadWithCustomResponse(url, "white-1x1.png", WebString::fromUTF8(""), WrappedResourceResponse(response));

    FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
    Resource* resource = fetcher->requestResource(fetchRequestOriginal, TestResourceFactory(Resource::Image));
    ASSERT_TRUE(resource);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    ASSERT_TRUE(resource->hasVaryHeader());

    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* newResource = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Image));
    EXPECT_EQ(resource, newResource);

    memoryCache()->remove(newResource);
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

TEST_F(ResourceFetcherTest, RevalidateWhileLoading)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    Platform::current()->getURLLoaderMockFactory()->registerURL(url, WebURLResponse(), "");

    ResourceFetcher* fetcher1 = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    ResourceRequest request1(url);
    request1.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
    FetchRequest fetchRequest1 = FetchRequest(request1, FetchInitiatorInfo());
    Resource* resource1 = fetcher1->requestResource(fetchRequest1, TestResourceFactory(Resource::Image));
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
    response.setHTTPHeaderField(HTTPNames::ETag, "1234567890");
    resource1->responseReceived(response, nullptr);
    resource1->finish();

    ResourceFetcherTestMockFetchContext* context = ResourceFetcherTestMockFetchContext::create();
    context->setCachePolicy(CachePolicyRevalidate);
    ResourceFetcher* fetcher2 = ResourceFetcher::create(context);
    FetchRequest fetchRequest2(url, FetchInitiatorInfo());
    Resource* resource2 = fetcher2->requestResource(fetchRequest2, TestResourceFactory(Resource::Image));
    EXPECT_EQ(resource1, resource2);

    // Tidily(?) shut down the ResourceLoader.
    resource1->loader()->cancel();
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

TEST_F(ResourceFetcherTest, DontReuseMediaDataUrl)
{
    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    ResourceRequest request(KURL(ParsedURLString, "data:text/html,foo"));
    ResourceLoaderOptions options;
    options.dataBufferingPolicy = DoNotBufferData;
    FetchRequest fetchRequest = FetchRequest(request, FetchInitiatorTypeNames::internal, options);
    Resource* resource1 = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Media));
    Resource* resource2 = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Media));
    EXPECT_NE(resource1, resource2);
    memoryCache()->remove(resource2);
}

class ServeRequestsOnCompleteClient : public RawResourceClient {
public:
    void notifyFinished(Resource*) override
    {
        Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
    }

    // No callbacks should be received except for the notifyFinished()
    // triggered by ResourceLoader::cancel().
    void dataSent(Resource*, unsigned long long, unsigned long long) override { ASSERT_TRUE(false); }
    void responseReceived(Resource*, const ResourceResponse&, PassOwnPtr<WebDataConsumerHandle>) override { ASSERT_TRUE(false); }
    void setSerializedCachedMetadata(Resource*, const char*, size_t) override { ASSERT_TRUE(false); }
    void dataReceived(Resource*, const char*, size_t) override { ASSERT_TRUE(false); }
    void redirectReceived(Resource*, ResourceRequest&, const ResourceResponse&) override { ASSERT_TRUE(false); }
    void dataDownloaded(Resource*, int) override { ASSERT_TRUE(false); }
    void didReceiveResourceTiming(Resource*, const ResourceTimingInfo&) override { ASSERT_TRUE(false); }

    String debugName() const override { return "ServeRequestsOnCompleteClient"; }
};

// Regression test for http://crbug.com/594072.
// This emulates a modal dialog triggering a nested run loop inside
// ResourceLoader::cancel(). If the ResourceLoader doesn't promptly cancel its
// WebURLLoader before notifying its clients, a nested run loop  may send a
// network response, leading to an invalid state transition in ResourceLoader.
TEST_F(ResourceFetcherTest, ResponseOnCancel)
{
    KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
    ResourceResponse response;
    response.setURL(url);
    response.setHTTPStatusCode(200);
    URLTestHelpers::registerMockedURLLoadWithCustomResponse(url, "white-1x1.png", WebString::fromUTF8(""), WrappedResourceResponse(response));

    ResourceFetcher* fetcher = ResourceFetcher::create(ResourceFetcherTestMockFetchContext::create());
    FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
    Resource* resource = fetcher->requestResource(fetchRequest, TestResourceFactory(Resource::Raw));
    ServeRequestsOnCompleteClient client;
    resource->addClient(&client);
    resource->loader()->cancel();
    Platform::current()->getURLLoaderMockFactory()->unregisterURL(url);
}

} // namespace blink
