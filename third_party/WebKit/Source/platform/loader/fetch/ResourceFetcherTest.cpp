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
#include "platform/loader/fetch/FetchParameters.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/loader/testing/FetchTestingPlatformSupport.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/loader/testing/MockResource.h"
#include "platform/loader/testing/MockResourceClient.h"
#include "platform/scheduler/test/fake_web_task_runner.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/testing/weburl_loader_mock.h"
#include "platform/testing/weburl_loader_mock_factory_impl.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLLoader.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

constexpr char kTestResourceFilename[] = "white-1x1.png";
constexpr char kTestResourceMimeType[] = "image/png";
constexpr int kTestResourceSize = 103;  // size of white-1x1.png

void RegisterMockedURLLoadWithCustomResponse(const KURL& url,
                                             const ResourceResponse& response) {
  URLTestHelpers::RegisterMockedURLLoadWithCustomResponse(
      url, testing::PlatformTestDataPath(kTestResourceFilename),
      WrappedResourceResponse(response));
}

void RegisterMockedURLLoad(const KURL& url) {
  URLTestHelpers::RegisterMockedURLLoad(
      url, testing::PlatformTestDataPath(kTestResourceFilename),
      kTestResourceMimeType);
}

}  // namespace

class ResourceFetcherTest : public ::testing::Test {
 public:
  ResourceFetcherTest() = default;
  ~ResourceFetcherTest() override { GetMemoryCache()->EvictResources(); }

 protected:
  MockFetchContext* Context() { return platform_->Context(); }

  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceFetcherTest);
};

TEST_F(ResourceFetcherTest, StartLoadAfterFrameDetach) {
  KURL secure_url(kParsedURLString, "https://secureorigin.test/image.png");
  // Try to request a url. The request should fail, and a resource in an error
  // state should be returned, and no resource should be present in the cache.
  ResourceFetcher* fetcher = ResourceFetcher::Create(
      &FetchContext::NullInstance(),
      Platform::Current()->CurrentThread()->GetWebTaskRunner());

  ResourceRequest resource_request(secure_url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  FetchParameters fetch_params(resource_request);
  Resource* resource = RawResource::Fetch(fetch_params, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->ErrorOccurred());
  EXPECT_TRUE(resource->GetResourceError().IsAccessCheck());
  EXPECT_FALSE(GetMemoryCache()->ResourceForURL(secure_url));

  // Start by calling startLoad() directly, rather than via requestResource().
  // This shouldn't crash.
  fetcher->StartLoad(RawResource::CreateForTest(secure_url, Resource::kRaw));
}

TEST_F(ResourceFetcherTest, UseExistingResource) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  response.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  RegisterMockedURLLoadWithCustomResponse(url, response);

  FetchParameters fetch_params{ResourceRequest(url)};
  Resource* resource = MockResource::Fetch(fetch_params, fetcher);
  ASSERT_TRUE(resource);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_TRUE(GetMemoryCache()->Contains(resource));

  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher);
  EXPECT_EQ(resource, new_resource);
}

TEST_F(ResourceFetcherTest, Vary) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::CreateForTest(url, Resource::kRaw);
  GetMemoryCache()->Add(resource);
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  response.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.SetHTTPHeaderField(HTTPNames::Vary, "*");
  resource->ResponseReceived(response, nullptr);
  resource->Finish();
  ASSERT_TRUE(resource->MustReloadDueToVaryHeader(ResourceRequest(url)));

  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  FetchParameters fetch_params(resource_request);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, WebURLResponse(), "");
  Resource* new_resource = RawResource::Fetch(fetch_params, fetcher);
  EXPECT_NE(resource, new_resource);
  new_resource->Loader()->Cancel();
}

TEST_F(ResourceFetcherTest, NavigationTimingInfo) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);

  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceRequest resource_request(url);
  resource_request.SetFrameType(WebURLRequest::kFrameTypeNested);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextForm);
  FetchParameters fetch_params(resource_request);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, WebURLResponse(), "");
  Resource* resource =
      RawResource::FetchMainResource(fetch_params, fetcher, SubstituteData());
  resource->ResponseReceived(response, nullptr);
  EXPECT_EQ(resource->GetType(), Resource::kMainResource);

  ResourceTimingInfo* navigation_timing_info =
      fetcher->GetNavigationTimingInfo();
  ASSERT_TRUE(navigation_timing_info);
  long long encoded_data_length = 123;
  resource->Loader()->DidFinishLoading(0.0, encoded_data_length, 0, 0);
  EXPECT_EQ(navigation_timing_info->TransferSize(), encoded_data_length);

  // When there are redirects.
  KURL redirect_url(kParsedURLString, "http://127.0.0.1:8000/redirect.html");
  ResourceResponse redirect_response;
  redirect_response.SetURL(redirect_url);
  redirect_response.SetHTTPStatusCode(200);
  long long redirect_encoded_data_length = 123;
  redirect_response.SetEncodedDataLength(redirect_encoded_data_length);
  ResourceRequest redirect_resource_request(url);
  fetcher->RecordResourceTimingOnRedirect(resource, redirect_response, false);
  EXPECT_EQ(navigation_timing_info->TransferSize(),
            encoded_data_length + redirect_encoded_data_length);
}

TEST_F(ResourceFetcherTest, VaryOnBack) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::CreateForTest(url, Resource::kRaw);
  GetMemoryCache()->Add(resource);
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  response.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.SetHTTPHeaderField(HTTPNames::Vary, "*");
  resource->ResponseReceived(response, nullptr);
  resource->Finish();
  ASSERT_TRUE(resource->MustReloadDueToVaryHeader(ResourceRequest(url)));

  ResourceRequest resource_request(url);
  resource_request.SetCachePolicy(WebCachePolicy::kReturnCacheDataElseLoad);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  FetchParameters fetch_params(resource_request);
  Resource* new_resource = RawResource::Fetch(fetch_params, fetcher);
  EXPECT_EQ(resource, new_resource);
}

TEST_F(ResourceFetcherTest, VaryResource) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  response.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.SetHTTPHeaderField(HTTPNames::Vary, "*");
  RegisterMockedURLLoadWithCustomResponse(url, response);

  FetchParameters fetch_params_original{ResourceRequest(url)};
  Resource* resource = MockResource::Fetch(fetch_params_original, fetcher);
  ASSERT_TRUE(resource);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  ASSERT_TRUE(resource->MustReloadDueToVaryHeader(ResourceRequest(url)));

  FetchParameters fetch_params{ResourceRequest(url)};
  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher);
  EXPECT_EQ(resource, new_resource);
}

class RequestSameResourceOnComplete
    : public GarbageCollectedFinalized<RequestSameResourceOnComplete>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(RequestSameResourceOnComplete);

 public:
  explicit RequestSameResourceOnComplete(Resource* resource)
      : resource_(resource), notify_finished_called_(false) {}

  void NotifyFinished(Resource* resource) override {
    EXPECT_EQ(resource_, resource);
    MockFetchContext* context =
        MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
    ResourceFetcher* fetcher2 =
        ResourceFetcher::Create(context, context->GetTaskRunner());
    ResourceRequest resource_request2(resource_->Url());
    resource_request2.SetCachePolicy(WebCachePolicy::kValidatingCacheData);
    FetchParameters fetch_params2(resource_request2);
    Resource* resource2 = MockResource::Fetch(fetch_params2, fetcher2);
    EXPECT_EQ(resource_, resource2);
    notify_finished_called_ = true;
  }
  bool NotifyFinishedCalled() const { return notify_finished_called_; }

  DEFINE_INLINE_TRACE() {
    visitor->Trace(resource_);
    RawResourceClient::Trace(visitor);
  }

  String DebugName() const override { return "RequestSameResourceOnComplete"; }

 private:
  Member<Resource> resource_;
  bool notify_finished_called_;
};

TEST_F(ResourceFetcherTest, RevalidateWhileFinishingLoading) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  response.SetHTTPHeaderField(HTTPNames::Cache_Control, "max-age=3600");
  response.SetHTTPHeaderField(HTTPNames::ETag, "1234567890");
  RegisterMockedURLLoadWithCustomResponse(url, response);
  ResourceFetcher* fetcher1 =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceRequest request1(url);
  request1.SetHTTPHeaderField(HTTPNames::Cache_Control, "no-cache");
  FetchParameters fetch_params1(request1);
  Resource* resource1 = MockResource::Fetch(fetch_params1, fetcher1);
  Persistent<RequestSameResourceOnComplete> client =
      new RequestSameResourceOnComplete(resource1);
  resource1->AddClient(client);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(client->NotifyFinishedCalled());
  resource1->RemoveClient(client);
}

TEST_F(ResourceFetcherTest, DontReuseMediaDataUrl) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceRequest request(KURL(kParsedURLString, "data:text/html,foo"));
  request.SetRequestContext(WebURLRequest::kRequestContextVideo);
  ResourceLoaderOptions options(kDoNotAllowStoredCredentials,
                                kClientDidNotRequestCredentials);
  options.data_buffering_policy = kDoNotBufferData;
  options.initiator_info.name = FetchInitiatorTypeNames::internal;
  FetchParameters fetch_params(request, options);
  Resource* resource1 = RawResource::FetchMedia(fetch_params, fetcher);
  Resource* resource2 = RawResource::FetchMedia(fetch_params, fetcher);
  EXPECT_NE(resource1, resource2);
}

class ServeRequestsOnCompleteClient final
    : public GarbageCollectedFinalized<ServeRequestsOnCompleteClient>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(ServeRequestsOnCompleteClient);

 public:
  void NotifyFinished(Resource*) override {
    Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  }

  // No callbacks should be received except for the notifyFinished() triggered
  // by ResourceLoader::cancel().
  void DataSent(Resource*, unsigned long long, unsigned long long) override {
    ASSERT_TRUE(false);
  }
  void ResponseReceived(Resource*,
                        const ResourceResponse&,
                        std::unique_ptr<WebDataConsumerHandle>) override {
    ASSERT_TRUE(false);
  }
  void SetSerializedCachedMetadata(Resource*, const char*, size_t) override {
    ASSERT_TRUE(false);
  }
  void DataReceived(Resource*, const char*, size_t) override {
    ASSERT_TRUE(false);
  }
  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override {
    ADD_FAILURE();
    return true;
  }
  void DataDownloaded(Resource*, int) override { ASSERT_TRUE(false); }
  void DidReceiveResourceTiming(Resource*, const ResourceTimingInfo&) override {
    ASSERT_TRUE(false);
  }

  DEFINE_INLINE_TRACE() { RawResourceClient::Trace(visitor); }

  String DebugName() const override { return "ServeRequestsOnCompleteClient"; }
};

// Regression test for http://crbug.com/594072.
// This emulates a modal dialog triggering a nested run loop inside
// ResourceLoader::cancel(). If the ResourceLoader doesn't promptly cancel its
// WebURLLoader before notifying its clients, a nested run loop  may send a
// network response, leading to an invalid state transition in ResourceLoader.
TEST_F(ResourceFetcherTest, ResponseOnCancel) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  FetchParameters fetch_params(resource_request);
  Resource* resource = RawResource::Fetch(fetch_params, fetcher);
  Persistent<ServeRequestsOnCompleteClient> client =
      new ServeRequestsOnCompleteClient();
  resource->AddClient(client);
  resource->Loader()->Cancel();
  resource->RemoveClient(client);
}

class ScopedMockRedirectRequester {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScopedMockRedirectRequester);

 public:
  explicit ScopedMockRedirectRequester(MockFetchContext* context)
      : context_(context) {}

  void RegisterRedirect(const WebString& from_url, const WebString& to_url) {
    KURL redirect_url(kParsedURLString, from_url);
    WebURLResponse redirect_response;
    redirect_response.SetURL(redirect_url);
    redirect_response.SetHTTPStatusCode(301);
    redirect_response.SetHTTPHeaderField(HTTPNames::Location, to_url);
    redirect_response.SetEncodedDataLength(kRedirectResponseOverheadBytes);
    Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
        redirect_url, redirect_response, "");
  }

  void RegisterFinalResource(const WebString& url) {
    KURL final_url(kParsedURLString, url);
    RegisterMockedURLLoad(final_url);
  }

  void Request(const WebString& url) {
    ResourceFetcher* fetcher =
        ResourceFetcher::Create(context_, context_->GetTaskRunner());
    ResourceRequest resource_request(url);
    resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
    FetchParameters fetch_params(resource_request);
    RawResource::Fetch(fetch_params, fetcher);
    Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  }

 private:
  Member<MockFetchContext> context_;
};

TEST_F(ResourceFetcherTest, SameOriginRedirect) {
  const char kRedirectURL[] = "http://127.0.0.1:8000/redirect.html";
  const char kFinalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester(Context());
  requester.RegisterRedirect(kRedirectURL, kFinalURL);
  requester.RegisterFinalResource(kFinalURL);
  requester.Request(kRedirectURL);

  EXPECT_EQ(kRedirectResponseOverheadBytes + kTestResourceSize,
            Context()->GetTransferSize());
}

TEST_F(ResourceFetcherTest, CrossOriginRedirect) {
  const char kRedirectURL[] = "http://otherorigin.test/redirect.html";
  const char kFinalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester(Context());
  requester.RegisterRedirect(kRedirectURL, kFinalURL);
  requester.RegisterFinalResource(kFinalURL);
  requester.Request(kRedirectURL);

  EXPECT_EQ(kTestResourceSize, Context()->GetTransferSize());
}

TEST_F(ResourceFetcherTest, ComplexCrossOriginRedirect) {
  const char kRedirectURL1[] = "http://127.0.0.1:8000/redirect1.html";
  const char kRedirectURL2[] = "http://otherorigin.test/redirect2.html";
  const char kRedirectURL3[] = "http://127.0.0.1:8000/redirect3.html";
  const char kFinalURL[] = "http://127.0.0.1:8000/final.html";
  ScopedMockRedirectRequester requester(Context());
  requester.RegisterRedirect(kRedirectURL1, kRedirectURL2);
  requester.RegisterRedirect(kRedirectURL2, kRedirectURL3);
  requester.RegisterRedirect(kRedirectURL3, kFinalURL);
  requester.RegisterFinalResource(kFinalURL);
  requester.Request(kRedirectURL1);

  EXPECT_EQ(kTestResourceSize, Context()->GetTransferSize());
}

TEST_F(ResourceFetcherTest, SynchronousRequest) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  FetchParameters fetch_params(resource_request);
  fetch_params.MakeSynchronous();
  Resource* resource = RawResource::Fetch(fetch_params, fetcher);
  EXPECT_TRUE(resource->IsLoaded());
  EXPECT_EQ(kResourceLoadPriorityHighest,
            resource->GetResourceRequest().Priority());
}

TEST_F(ResourceFetcherTest, PreloadResourceTwice) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original{ResourceRequest(url)};
  fetch_params_original.SetLinkPreload(true);
  Resource* resource = MockResource::Fetch(fetch_params_original, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  FetchParameters fetch_params{ResourceRequest(url)};
  fetch_params.SetLinkPreload(true);
  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher);
  EXPECT_EQ(resource, new_resource);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource));

  fetcher->ClearPreloads(ResourceFetcher::kClearAllPreloads);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource));
  EXPECT_FALSE(GetMemoryCache()->Contains(resource));
  EXPECT_FALSE(resource->IsPreloaded());
}

TEST_F(ResourceFetcherTest, LinkPreloadResourceAndUse) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  // Link preload preload scanner
  FetchParameters fetch_params_original{ResourceRequest(url)};
  fetch_params_original.SetLinkPreload(true);
  Resource* resource = MockResource::Fetch(fetch_params_original, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // Resource created by preload scanner
  FetchParameters fetch_params_preload_scanner{ResourceRequest(url)};
  Resource* preload_scanner_resource =
      MockResource::Fetch(fetch_params_preload_scanner, fetcher);
  EXPECT_EQ(resource, preload_scanner_resource);
  EXPECT_FALSE(resource->IsLinkPreload());

  // Resource created by parser
  FetchParameters fetch_params{ResourceRequest(url)};
  Resource* new_resource = MockResource::Fetch(fetch_params, fetcher);
  Persistent<MockResourceClient> client = new MockResourceClient(new_resource);
  EXPECT_EQ(resource, new_resource);
  EXPECT_FALSE(resource->IsLinkPreload());

  // DCL reached
  fetcher->ClearPreloads(ResourceFetcher::kClearSpeculativeMarkupPreloads);
  EXPECT_TRUE(GetMemoryCache()->Contains(resource));
  EXPECT_FALSE(resource->IsPreloaded());
}

TEST_F(ResourceFetcherTest, PreloadMatchWithBypassingCache) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original{ResourceRequest(url)};
  fetch_params_original.SetLinkPreload(true);
  Resource* resource = MockResource::Fetch(fetch_params_original, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  FetchParameters fetch_params_second{ResourceRequest(url)};
  fetch_params_second.MutableResourceRequest().SetCachePolicy(
      WebCachePolicy::kBypassingCache);
  Resource* second_resource = MockResource::Fetch(fetch_params_second, fetcher);
  EXPECT_EQ(resource, second_resource);
  EXPECT_FALSE(resource->IsLinkPreload());
}

TEST_F(ResourceFetcherTest, CrossFramePreloadMatchIsNotAllowed) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceFetcher* fetcher2 =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original{ResourceRequest(url)};
  fetch_params_original.SetLinkPreload(true);
  Resource* resource = MockResource::Fetch(fetch_params_original, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  FetchParameters fetch_params_second{ResourceRequest(url)};
  fetch_params_second.MutableResourceRequest().SetCachePolicy(
      WebCachePolicy::kBypassingCache);
  Resource* second_resource =
      MockResource::Fetch(fetch_params_second, fetcher2);

  EXPECT_NE(resource, second_resource);
  EXPECT_TRUE(resource->IsLinkPreload());
}

TEST_F(ResourceFetcherTest, RepetitiveLinkPreloadShouldBeMerged) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_for_request{ResourceRequest(url)};
  FetchParameters fetch_params_for_preload = fetch_params_for_request;
  fetch_params_for_preload.SetLinkPreload(true);

  Resource* resource1 = MockResource::Fetch(fetch_params_for_preload, fetcher);
  ASSERT_TRUE(resource1);
  EXPECT_TRUE(resource1->IsPreloaded());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // The second preload fetch returnes the first preload.
  Resource* resource2 = MockResource::Fetch(fetch_params_for_preload, fetcher);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_TRUE(resource1->IsPreloaded());
  EXPECT_EQ(resource1, resource2);

  // preload matching
  Resource* resource3 = MockResource::Fetch(fetch_params_for_request, fetcher);
  EXPECT_EQ(resource1, resource3);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource1));
  EXPECT_FALSE(resource1->IsPreloaded());
}

TEST_F(ResourceFetcherTest, RepetitiveSpeculativePreloadShouldBeMerged) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_for_request{ResourceRequest(url)};
  FetchParameters fetch_params_for_preload = fetch_params_for_request;
  fetch_params_for_preload.SetSpeculativePreloadType(
      FetchParameters::SpeculativePreloadType::kInDocument);

  Resource* resource1 = MockResource::Fetch(fetch_params_for_preload, fetcher);
  ASSERT_TRUE(resource1);
  EXPECT_TRUE(resource1->IsPreloaded());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // The second preload fetch returnes the first preload.
  Resource* resource2 = MockResource::Fetch(fetch_params_for_preload, fetcher);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_TRUE(resource1->IsPreloaded());
  EXPECT_EQ(resource1, resource2);

  // preload matching
  Resource* resource3 = MockResource::Fetch(fetch_params_for_request, fetcher);
  EXPECT_EQ(resource1, resource3);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource1));
  EXPECT_FALSE(resource1->IsPreloaded());
}

TEST_F(ResourceFetcherTest, SpeculativePreloadShouldBePromotedToLinkePreload) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_for_request{ResourceRequest(url)};
  FetchParameters fetch_params_for_speculative_preload =
      fetch_params_for_request;
  fetch_params_for_speculative_preload.SetSpeculativePreloadType(
      FetchParameters::SpeculativePreloadType::kInDocument);
  FetchParameters fetch_params_for_link_preload = fetch_params_for_request;
  fetch_params_for_link_preload.SetLinkPreload(true);

  Resource* resource1 =
      MockResource::Fetch(fetch_params_for_speculative_preload, fetcher);
  ASSERT_TRUE(resource1);
  EXPECT_TRUE(resource1->IsPreloaded());
  EXPECT_FALSE(resource1->IsLinkPreload());
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();

  // The second preload fetch returnes the first preload.
  Resource* resource2 =
      MockResource::Fetch(fetch_params_for_link_preload, fetcher);
  EXPECT_TRUE(fetcher->ContainsAsPreload(resource1));
  EXPECT_TRUE(resource1->IsPreloaded());
  EXPECT_TRUE(resource1->IsLinkPreload());
  EXPECT_EQ(resource1, resource2);

  // preload matching
  Resource* resource3 = MockResource::Fetch(fetch_params_for_request, fetcher);
  EXPECT_EQ(resource1, resource3);
  EXPECT_FALSE(fetcher->ContainsAsPreload(resource1));
  EXPECT_FALSE(resource1->IsPreloaded());
  EXPECT_FALSE(resource1->IsLinkPreload());
}

TEST_F(ResourceFetcherTest, Revalidate304) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  Resource* resource = RawResource::CreateForTest(url, Resource::kRaw);
  GetMemoryCache()->Add(resource);
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(304);
  response.SetHTTPHeaderField("etag", "1234567890");
  resource->ResponseReceived(response, nullptr);
  resource->Finish();

  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(WebURLRequest::kRequestContextInternal);
  FetchParameters fetch_params(resource_request);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, WebURLResponse(), "");
  Resource* new_resource = RawResource::Fetch(fetch_params, fetcher);
  fetcher->StopFetching();

  EXPECT_NE(resource, new_resource);
}

TEST_F(ResourceFetcherTest, LinkPreloadResourceMultipleFetchersAndMove) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  ResourceFetcher* fetcher2 =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());

  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.png");
  RegisterMockedURLLoad(url);

  FetchParameters fetch_params_original{ResourceRequest(url)};
  fetch_params_original.SetLinkPreload(true);
  Resource* resource = MockResource::Fetch(fetch_params_original, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_TRUE(resource->IsLinkPreload());
  EXPECT_EQ(0, fetcher->BlockingRequestCount());

  // Resource created by parser on the second fetcher
  FetchParameters fetch_params2{ResourceRequest(url)};
  Resource* new_resource2 = MockResource::Fetch(fetch_params2, fetcher2);
  Persistent<MockResourceClient> client2 =
      new MockResourceClient(new_resource2);
  EXPECT_NE(resource, new_resource2);
  EXPECT_EQ(0, fetcher2->BlockingRequestCount());
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
}

TEST_F(ResourceFetcherTest, ContentTypeDataURL) {
  ResourceFetcher* fetcher =
      ResourceFetcher::Create(Context(), Context()->GetTaskRunner());
  FetchParameters fetch_params{ResourceRequest("data:text/testmimetype,foo")};
  Resource* resource = MockResource::Fetch(fetch_params, fetcher);
  ASSERT_TRUE(resource);
  EXPECT_EQ(ResourceStatus::kCached, resource->GetStatus());
  EXPECT_EQ("text/testmimetype", resource->GetResponse().MimeType());
  EXPECT_EQ("text/testmimetype", resource->GetResponse().HttpContentType());
}

}  // namespace blink
