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

#include "platform/loader/fetch/ResourceFetcher.h"

#include <memory>
#include "platform/WebTaskRunner.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/Member.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchInitiatorTypeNames.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/testing/FetchTestingPlatformSupport.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/loader/testing/MockResource.h"
#include "platform/loader/testing/MockResourceClient.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceTimingInfo.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/testing/weburl_loader_mock.h"
#include "platform/testing/weburl_loader_mock_factory_impl.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Allocator.h"
#include "wtf/PtrUtil.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

constexpr char kTestResourceFilename[] = "white-1x1.png";
constexpr char kTestResourceMimeType[] = "image/png";
constexpr int kTestResourceSize = 103;  // size of white-1x1.png

void registerMockedURLLoadWithCustomResponse(const KURL& url,
                                             const ResourceResponse& response) {
  URLTestHelpers::registerMockedURLLoadWithCustomResponse(
      url, testing::platformTestDataPath(kTestResourceFilename),
      WrappedResourceResponse(response));
}

void registerMockedURLLoad(const KURL& url) {
  URLTestHelpers::registerMockedURLLoad(
      url, testing::platformTestDataPath(kTestResourceFilename),
      kTestResourceMimeType);
}

}  // namespace

class ResourceFetcherTest : public ::testing::Test {
 public:
  ResourceFetcherTest() = default;
  ~ResourceFetcherTest() override { memoryCache()->evictResources(); }

 protected:
  MockFetchContext* context() { return m_platform->context(); }

  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> m_platform;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherTest);
};

TEST_F(ResourceFetcherTest, StartLoadAfterFrameDetach) {
  KURL secureURL(ParsedURLString, "https://secureorigin.test/image.png");
  // Try to request a url. The request should fail, and a resource in an error
  // state should be returned, and no resource should be present in the cache.
  ResourceFetcher* fetcher = ResourceFetcher::create(nullptr);
  ResourceRequest resourceRequest(secureURL);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Resource* resource = RawResource::fetch(fetchRequest, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->errorOccurred());
  EXPECT_TRUE(resource->resourceError().isAccessCheck());
  EXPECT_FALSE(memoryCache()->resourceForURL(secureURL));

  // Start by calling startLoad() directly, rather than via requestResource().
  // This shouldn't crash.
  fetcher->startLoad(RawResource::create(secureURL, Resource::Raw));
}

TEST_F(ResourceFetcherTest, UseExistingResource) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  registerMockedURLLoadWithCustomResponse(url, response);

  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* resource = MockResource::fetch(fetchRequest, fetcher);
  ASSERT_TRUE(resource);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_TRUE(resource->isLoaded());
  EXPECT_TRUE(memoryCache()->contains(resource));

  Resource* newResource = MockResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);
}

TEST_F(ResourceFetcherTest, Vary) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::create(url, Resource::Raw);
  memoryCache()->add(resource);
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::Vary, "*");
  resource->responseReceived(response, nullptr);
  resource->finish();
  ASSERT_TRUE(resource->mustReloadDueToVaryHeader(url));

  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WebURLResponse(), "");
  Resource* newResource = RawResource::fetch(fetchRequest, fetcher);
  EXPECT_NE(resource, newResource);
  newResource->loader()->cancel();
}

TEST_F(ResourceFetcherTest, NavigationTimingInfo) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);

  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceRequest resourceRequest(url);
  resourceRequest.setFrameType(WebURLRequest::FrameTypeNested);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextForm);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WebURLResponse(), "");
  Resource* resource =
      RawResource::fetchMainResource(fetchRequest, fetcher, SubstituteData());
  resource->responseReceived(response, nullptr);
  EXPECT_EQ(resource->getType(), Resource::MainResource);

  ResourceTimingInfo* navigationTimingInfo = fetcher->getNavigationTimingInfo();
  ASSERT_TRUE(navigationTimingInfo);
  long long encodedDataLength = 123;
  resource->loader()->didFinishLoading(0.0, encodedDataLength, 0);
  EXPECT_EQ(navigationTimingInfo->transferSize(), encodedDataLength);

  // When there are redirects.
  KURL redirectURL(ParsedURLString, "http://127.0.0.1:8000/redirect.html");
  ResourceResponse redirectResponse;
  redirectResponse.setURL(redirectURL);
  redirectResponse.setHTTPStatusCode(200);
  long long redirectEncodedDataLength = 123;
  redirectResponse.setEncodedDataLength(redirectEncodedDataLength);
  ResourceRequest redirectResourceRequest(url);
  fetcher->recordResourceTimingOnRedirect(resource, redirectResponse, false);
  EXPECT_EQ(navigationTimingInfo->transferSize(),
            encodedDataLength + redirectEncodedDataLength);
}

TEST_F(ResourceFetcherTest, VaryOnBack) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::create(url, Resource::Raw);
  memoryCache()->add(resource);
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::Vary, "*");
  resource->responseReceived(response, nullptr);
  resource->finish();
  ASSERT_TRUE(resource->mustReloadDueToVaryHeader(url));

  ResourceRequest resourceRequest(url);
  resourceRequest.setCachePolicy(WebCachePolicy::ReturnCacheDataElseLoad);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Resource* newResource = RawResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);
}

TEST_F(ResourceFetcherTest, VaryResource) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::Vary, "*");
  registerMockedURLLoadWithCustomResponse(url, response);

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  Resource* resource = MockResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  ASSERT_TRUE(resource->mustReloadDueToVaryHeader(url));

  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = MockResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);
}

class RequestSameResourceOnComplete
    : public GarbageCollectedFinalized<RequestSameResourceOnComplete>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(RequestSameResourceOnComplete);

 public:
  explicit RequestSameResourceOnComplete(Resource* resource)
      : m_resource(resource), m_notifyFinishedCalled(false) {}

  void notifyFinished(Resource* resource) override {
    EXPECT_EQ(m_resource, resource);
    MockFetchContext* context =
        MockFetchContext::create(MockFetchContext::kShouldLoadNewResource);
    ResourceFetcher* fetcher2 = ResourceFetcher::create(context);
    FetchRequest fetchRequest2(m_resource->url(), FetchInitiatorInfo());
    fetchRequest2.mutableResourceRequest().setCachePolicy(
        WebCachePolicy::ValidatingCacheData);
    Resource* resource2 = MockResource::fetch(fetchRequest2, fetcher2);
    EXPECT_EQ(m_resource, resource2);
    m_notifyFinishedCalled = true;
  }
  bool notifyFinishedCalled() const { return m_notifyFinishedCalled; }

  DEFINE_INLINE_TRACE() {
    visitor->trace(m_resource);
    RawResourceClient::trace(visitor);
  }

  String debugName() const override { return "RequestSameResourceOnComplete"; }

 private:
  Member<Resource> m_resource;
  bool m_notifyFinishedCalled;
};

TEST_F(ResourceFetcherTest, RevalidateWhileFinishingLoading) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.setHTTPHeaderField(HTTPNames::ETag, "1234567890");
  registerMockedURLLoadWithCustomResponse(url, response);
  ResourceFetcher* fetcher1 = ResourceFetcher::create(context());
  ResourceRequest request1(url);
  request1.setHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
  FetchRequest fetchRequest1 = FetchRequest(request1, FetchInitiatorInfo());
  Resource* resource1 = MockResource::fetch(fetchRequest1, fetcher1);
  Persistent<RequestSameResourceOnComplete> client =
      new RequestSameResourceOnComplete(resource1);
  resource1->addClient(client);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_TRUE(client->notifyFinishedCalled());
  resource1->removeClient(client);
}

TEST_F(ResourceFetcherTest, DontReuseMediaDataUrl) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceRequest request(KURL(ParsedURLString, "data:text/html,foo"));
  request.setRequestContext(WebURLRequest::RequestContextVideo);
  ResourceLoaderOptions options;
  options.dataBufferingPolicy = DoNotBufferData;
  FetchRequest fetchRequest =
      FetchRequest(request, FetchInitiatorTypeNames::internal, options);
  Resource* resource1 = RawResource::fetchMedia(fetchRequest, fetcher);
  Resource* resource2 = RawResource::fetchMedia(fetchRequest, fetcher);
  EXPECT_NE(resource1, resource2);
}

class ServeRequestsOnCompleteClient final
    : public GarbageCollectedFinalized<ServeRequestsOnCompleteClient>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(ServeRequestsOnCompleteClient);

 public:
  void notifyFinished(Resource*) override {
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  }

  // No callbacks should be received except for the notifyFinished() triggered
  // by ResourceLoader::cancel().
  void dataSent(Resource*, unsigned long long, unsigned long long) override {
    ASSERT_TRUE(false);
  }
  void responseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override {
    ASSERT_TRUE(false);
  }
  void setSerializedCachedMetadata(Resource*, const char*, size_t) override {
    ASSERT_TRUE(false);
  }
  void dataReceived(Resource*, const char*, size_t) override {
    ASSERT_TRUE(false);
  }
  bool redirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override {
    ADD_FAILURE();
    return true;
  }
  void dataDownloaded(Resource*, int) override { ASSERT_TRUE(false); }
  void didReceiveResourceTiming(Resource*, const ResourceTimingInfo&) override {
    ASSERT_TRUE(false);
  }

  DEFINE_INLINE_TRACE() { RawResourceClient::trace(visitor); }

  String debugName() const override { return "ServeRequestsOnCompleteClient"; }
};

// Regression test for http://crbug.com/594072.
// This emulates a modal dialog triggering a nested run loop inside
// ResourceLoader::cancel(). If the ResourceLoader doesn't promptly cancel its
// WebURLLoader before notifying its clients, a nested run loop  may send a
// network response, leading to an invalid state transition in ResourceLoader.
TEST_F(ResourceFetcherTest, ResponseOnCancel) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  registerMockedURLLoad(url);

  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Resource* resource = RawResource::fetch(fetchRequest, fetcher);
  Persistent<ServeRequestsOnCompleteClient> client =
      new ServeRequestsOnCompleteClient();
  resource->addClient(client);
  resource->loader()->cancel();
  resource->removeClient(client);
}

class ScopedMockRedirectRequester {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScopedMockRedirectRequester);

 public:
  explicit ScopedMockRedirectRequester(MockFetchContext* context)
      : m_context(context) {}

  void registerRedirect(const WebString& fromURL, const WebString& toURL) {
    KURL redirectURL(ParsedURLString, fromURL);
    WebURLResponse redirectResponse;
    redirectResponse.setURL(redirectURL);
    redirectResponse.setHTTPStatusCode(301);
    redirectResponse.setHTTPHeaderField(HTTPNames::Location, toURL);
    redirectResponse.setEncodedDataLength(kRedirectResponseOverheadBytes);
    Platform::current()->getURLLoaderMockFactory()->registerURL(
        redirectURL, redirectResponse, "");
  }

  void registerFinalResource(const WebString& url) {
    KURL finalURL(ParsedURLString, url);
    registerMockedURLLoad(finalURL);
  }

  void request(const WebString& url) {
    ResourceFetcher* fetcher = ResourceFetcher::create(m_context);
    ResourceRequest resourceRequest(url);
    resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
    FetchRequest fetchRequest =
        FetchRequest(resourceRequest, FetchInitiatorInfo());
    RawResource::fetch(fetchRequest, fetcher);
    Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  }

 private:
  Member<MockFetchContext> m_context;
};

TEST_F(ResourceFetcherTest, SameOriginRedirect) {
  const char redirectURL[] = "http://127.0.0.1:8000/redirect.html";
  const char finalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester(context());
  requester.registerRedirect(redirectURL, finalURL);
  requester.registerFinalResource(finalURL);
  requester.request(redirectURL);

  EXPECT_EQ(kRedirectResponseOverheadBytes + kTestResourceSize,
            context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, CrossOriginRedirect) {
  const char redirectURL[] = "http://otherorigin.test/redirect.html";
  const char finalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester(context());
  requester.registerRedirect(redirectURL, finalURL);
  requester.registerFinalResource(finalURL);
  requester.request(redirectURL);

  EXPECT_EQ(kTestResourceSize, context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, ComplexCrossOriginRedirect) {
  const char redirectURL1[] = "http://127.0.0.1:8000/redirect1.html";
  const char redirectURL2[] = "http://otherorigin.test/redirect2.html";
  const char redirectURL3[] = "http://127.0.0.1:8000/redirect3.html";
  const char finalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester(context());
  requester.registerRedirect(redirectURL1, redirectURL2);
  requester.registerRedirect(redirectURL2, redirectURL3);
  requester.registerRedirect(redirectURL3, finalURL);
  requester.registerFinalResource(finalURL);
  requester.request(redirectURL1);

  EXPECT_EQ(kTestResourceSize, context()->getTransferSize());
}

TEST_F(ResourceFetcherTest, SynchronousRequest) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  registerMockedURLLoad(url);

  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest(resourceRequest, FetchInitiatorInfo());
  fetchRequest.makeSynchronous();
  Resource* resource = RawResource::fetch(fetchRequest, fetcher);
  EXPECT_TRUE(resource->isLoaded());
  EXPECT_EQ(ResourceLoadPriorityHighest,
            resource->resourceRequest().priority());
}

TEST_F(ResourceFetcherTest, PreloadResourceTwice) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  registerMockedURLLoad(url);

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  Resource* resource = MockResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher->preloadStarted(resource);

  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = MockResource::fetch(fetchRequest, fetcher);
  EXPECT_EQ(resource, newResource);
  fetcher->preloadStarted(resource);

  fetcher->clearPreloads(ResourceFetcher::ClearAllPreloads);
  EXPECT_FALSE(memoryCache()->contains(resource));
  EXPECT_FALSE(resource->isPreloaded());
}

TEST_F(ResourceFetcherTest, LinkPreloadResourceAndUse) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  registerMockedURLLoad(url);

  // Link preload preload scanner
  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestOriginal.setLinkPreload(true);
  Resource* resource = MockResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->isLinkPreload());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher->preloadStarted(resource);

  // Resource created by preload scanner
  FetchRequest fetchRequestPreloadScanner =
      FetchRequest(url, FetchInitiatorInfo());
  Resource* preloadScannerResource =
      MockResource::fetch(fetchRequestPreloadScanner, fetcher);
  EXPECT_EQ(resource, preloadScannerResource);
  EXPECT_FALSE(resource->isLinkPreload());
  fetcher->preloadStarted(resource);

  // Resource created by parser
  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = MockResource::fetch(fetchRequest, fetcher);
  Persistent<MockResourceClient> client = new MockResourceClient(newResource);
  EXPECT_EQ(resource, newResource);
  EXPECT_FALSE(resource->isLinkPreload());

  // DCL reached
  fetcher->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);
  EXPECT_TRUE(memoryCache()->contains(resource));
  EXPECT_FALSE(resource->isPreloaded());
}

TEST_F(ResourceFetcherTest, LinkPreloadResourceMultipleFetchersAndUse) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceFetcher* fetcher2 = ResourceFetcher::create(context());

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  registerMockedURLLoad(url);

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestOriginal.setLinkPreload(true);
  Resource* resource = MockResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->isLinkPreload());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher->preloadStarted(resource);

  FetchRequest fetchRequestSecond = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestSecond.setLinkPreload(true);
  Resource* secondResource = MockResource::fetch(fetchRequestSecond, fetcher2);
  ASSERT_TRUE(secondResource);
  EXPECT_TRUE(secondResource->isLinkPreload());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  fetcher2->preloadStarted(secondResource);

  // Link rel preload scanner
  FetchRequest fetchRequestLinkPreloadScanner =
      FetchRequest(url, FetchInitiatorInfo());
  fetchRequestLinkPreloadScanner.setLinkPreload(true);
  Resource* linkPreloadScannerResource =
      MockResource::fetch(fetchRequestLinkPreloadScanner, fetcher);
  EXPECT_EQ(resource, linkPreloadScannerResource);
  EXPECT_TRUE(resource->isLinkPreload());
  fetcher->preloadStarted(resource);

  // Resource created by preload scanner
  FetchRequest fetchRequestPreloadScanner =
      FetchRequest(url, FetchInitiatorInfo());
  Resource* preloadScannerResource =
      MockResource::fetch(fetchRequestPreloadScanner, fetcher);
  EXPECT_EQ(resource, preloadScannerResource);
  EXPECT_FALSE(resource->isLinkPreload());
  fetcher->preloadStarted(resource);

  // Resource created by preload scanner on the second fetcher
  FetchRequest fetchRequestPreloadScanner2 =
      FetchRequest(url, FetchInitiatorInfo());
  Resource* preloadScannerResource2 =
      MockResource::fetch(fetchRequestPreloadScanner2, fetcher2);
  EXPECT_EQ(resource, preloadScannerResource2);
  EXPECT_FALSE(resource->isLinkPreload());
  fetcher2->preloadStarted(resource);

  // Resource created by parser
  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource = MockResource::fetch(fetchRequest, fetcher);
  Persistent<MockResourceClient> client = new MockResourceClient(newResource);
  EXPECT_EQ(resource, newResource);
  EXPECT_FALSE(resource->isLinkPreload());

  // Resource created by parser on the second fetcher
  FetchRequest fetchRequest2 = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource2 = MockResource::fetch(fetchRequest, fetcher2);
  Persistent<MockResourceClient> client2 = new MockResourceClient(newResource2);
  EXPECT_EQ(resource, newResource2);
  EXPECT_FALSE(resource->isLinkPreload());

  // DCL reached on first fetcher
  EXPECT_TRUE(resource->isPreloaded());
  fetcher->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);
  EXPECT_TRUE(memoryCache()->contains(resource));
  EXPECT_TRUE(resource->isPreloaded());

  // DCL reached on second fetcher
  fetcher2->clearPreloads(ResourceFetcher::ClearSpeculativeMarkupPreloads);
  EXPECT_TRUE(memoryCache()->contains(resource));
  EXPECT_FALSE(resource->isPreloaded());
}

TEST_F(ResourceFetcherTest, Revalidate304) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::create(url, Resource::Raw);
  memoryCache()->add(resource);
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(304);
  response.setHTTPHeaderField("etag", "1234567890");
  resource->responseReceived(response, nullptr);
  resource->finish();

  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextInternal);
  FetchRequest fetchRequest =
      FetchRequest(resourceRequest, FetchInitiatorInfo());
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WebURLResponse(), "");
  Resource* newResource = RawResource::fetch(fetchRequest, fetcher);
  fetcher->stopFetching();

  EXPECT_NE(resource, newResource);
}

TEST_F(ResourceFetcherTest, LinkPreloadResourceMultipleFetchersAndMove) {
  ResourceFetcher* fetcher = ResourceFetcher::create(context());
  ResourceFetcher* fetcher2 = ResourceFetcher::create(context());

  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.png");
  registerMockedURLLoad(url);

  FetchRequest fetchRequestOriginal = FetchRequest(url, FetchInitiatorInfo());
  fetchRequestOriginal.setLinkPreload(true);
  Resource* resource = MockResource::fetch(fetchRequestOriginal, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->isLinkPreload());
  EXPECT_FALSE(fetcher->isFetching());
  fetcher->preloadStarted(resource);

  // Resource created by parser on the second fetcher
  FetchRequest fetchRequest2 = FetchRequest(url, FetchInitiatorInfo());
  Resource* newResource2 = MockResource::fetch(fetchRequest2, fetcher2);
  Persistent<MockResourceClient> client2 = new MockResourceClient(newResource2);
  EXPECT_NE(resource, newResource2);
  EXPECT_FALSE(fetcher2->isFetching());
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
}

}  // namespace blink
