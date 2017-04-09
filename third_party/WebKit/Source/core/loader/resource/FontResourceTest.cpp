// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/FontResource.h"

#include "core/loader/resource/MockFontResourceClient.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/loader/testing/MockResourceClient.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FontResourceTest : public ::testing::Test {
  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }
};

// Tests if ResourceFetcher works fine with FontResource that requires defered
// loading supports.
TEST_F(FontResourceTest,
       ResourceFetcherRevalidateDeferedResourceFromTwoInitiators) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/font.woff");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  response.SetHTTPHeaderField(HTTPNames::ETag, "1234567890");
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response), "");

  MockFetchContext* context =
      MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
  ResourceFetcher* fetcher = ResourceFetcher::Create(context);

  // Fetch to cache a resource.
  ResourceRequest request1(url);
  FetchRequest fetch_request1 = FetchRequest(request1, FetchInitiatorInfo());
  Resource* resource1 = FontResource::Fetch(fetch_request1, fetcher);
  ASSERT_TRUE(resource1);
  fetcher->StartLoad(resource1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource1->IsLoaded());
  EXPECT_FALSE(resource1->ErrorOccurred());

  // Set the context as it is on reloads.
  context->SetLoadComplete(true);

  // Revalidate the resource.
  ResourceRequest request2(url);
  request2.SetCachePolicy(WebCachePolicy::kValidatingCacheData);
  FetchRequest fetch_request2 = FetchRequest(request2, FetchInitiatorInfo());
  Resource* resource2 = FontResource::Fetch(fetch_request2, fetcher);
  ASSERT_TRUE(resource2);
  EXPECT_EQ(resource1, resource2);
  EXPECT_TRUE(resource2->IsCacheValidator());
  EXPECT_TRUE(resource2->StillNeedsLoad());

  // Fetch the same resource again before actual load operation starts.
  ResourceRequest request3(url);
  request3.SetCachePolicy(WebCachePolicy::kValidatingCacheData);
  FetchRequest fetch_request3 = FetchRequest(request3, FetchInitiatorInfo());
  Resource* resource3 = FontResource::Fetch(fetch_request3, fetcher);
  ASSERT_TRUE(resource3);
  EXPECT_EQ(resource2, resource3);
  EXPECT_TRUE(resource3->IsCacheValidator());
  EXPECT_TRUE(resource3->StillNeedsLoad());

  // startLoad() can be called from any initiator. Here, call it from the
  // latter.
  fetcher->StartLoad(resource3);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_TRUE(resource3->IsLoaded());
  EXPECT_FALSE(resource3->ErrorOccurred());
  EXPECT_TRUE(resource2->IsLoaded());
  EXPECT_FALSE(resource2->ErrorOccurred());

  GetMemoryCache()->Remove(resource1);
}

// Tests if cache-aware font loading works correctly.
TEST_F(FontResourceTest, CacheAwareFontLoading) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/font.woff");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(
      url, WrappedResourceResponse(response), "");

  ResourceFetcher* fetcher = ResourceFetcher::Create(
      MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource));

  FetchRequest fetch_request = FetchRequest(url, FetchInitiatorInfo());
  fetch_request.SetCacheAwareLoadingEnabled(kIsCacheAwareLoadingEnabled);
  FontResource* resource = FontResource::Fetch(fetch_request, fetcher);
  ASSERT_TRUE(resource);

  Persistent<MockFontResourceClient> client =
      new MockFontResourceClient(resource);
  fetcher->StartLoad(resource);
  EXPECT_TRUE(resource->Loader()->IsCacheAwareLoadingActivated());
  resource->load_limit_state_ = FontResource::kUnderLimit;

  // FontResource callbacks should be blocked during cache-aware loading.
  resource->FontLoadShortLimitCallback(nullptr);
  EXPECT_FALSE(client->FontLoadShortLimitExceededCalled());

  // Fail first request as disk cache miss.
  resource->Loader()->HandleError(ResourceError::CacheMissError(url));

  // Once cache miss error returns, previously blocked callbacks should be
  // called immediately.
  EXPECT_FALSE(resource->Loader()->IsCacheAwareLoadingActivated());
  EXPECT_TRUE(client->FontLoadShortLimitExceededCalled());
  EXPECT_FALSE(client->FontLoadLongLimitExceededCalled());

  // Add client now, fontLoadShortLimitExceeded() should be called.
  Persistent<MockFontResourceClient> client2 =
      new MockFontResourceClient(resource);
  EXPECT_TRUE(client2->FontLoadShortLimitExceededCalled());
  EXPECT_FALSE(client2->FontLoadLongLimitExceededCalled());

  // FontResource callbacks are not blocked now.
  resource->FontLoadLongLimitCallback(nullptr);
  EXPECT_TRUE(client->FontLoadLongLimitExceededCalled());

  // Add client now, both callbacks should be called.
  Persistent<MockFontResourceClient> client3 =
      new MockFontResourceClient(resource);
  EXPECT_TRUE(client3->FontLoadShortLimitExceededCalled());
  EXPECT_TRUE(client3->FontLoadLongLimitExceededCalled());

  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  GetMemoryCache()->Remove(resource);
}

}  // namespace blink
