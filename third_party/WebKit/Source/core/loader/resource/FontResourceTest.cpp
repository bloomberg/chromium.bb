// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/FontResource.h"

#include "core/loader/resource/MockFontResourceClient.h"
#include "platform/exported/WrappedResourceResponse.h"
#include "platform/loader/fetch/FetchInitiatorInfo.h"
#include "platform/loader/fetch/FetchRequest.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceLoader.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/loader/testing/MockResourceClient.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FontResourceTest : public ::testing::Test {
  void TearDown() override {
    Platform::current()
        ->getURLLoaderMockFactory()
        ->unregisterAllURLsAndClearMemoryCache();
  }
};

// Tests if ResourceFetcher works fine with FontResource that requires defered
// loading supports.
TEST_F(FontResourceTest,
       ResourceFetcherRevalidateDeferedResourceFromTwoInitiators) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/font.woff");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  response.setHTTPHeaderField(HTTPNames::ETag, "1234567890");
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WrappedResourceResponse(response), "");

  MockFetchContext* context =
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource);
  ResourceFetcher* fetcher = ResourceFetcher::create(context);

  // Fetch to cache a resource.
  ResourceRequest request1(url);
  FetchRequest fetchRequest1 = FetchRequest(request1, FetchInitiatorInfo());
  Resource* resource1 = FontResource::fetch(fetchRequest1, fetcher);
  ASSERT_TRUE(resource1);
  fetcher->startLoad(resource1);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_TRUE(resource1->isLoaded());
  EXPECT_FALSE(resource1->errorOccurred());

  // Set the context as it is on reloads.
  context->setLoadComplete(true);

  // Revalidate the resource.
  ResourceRequest request2(url);
  request2.setCachePolicy(WebCachePolicy::ValidatingCacheData);
  FetchRequest fetchRequest2 = FetchRequest(request2, FetchInitiatorInfo());
  Resource* resource2 = FontResource::fetch(fetchRequest2, fetcher);
  ASSERT_TRUE(resource2);
  EXPECT_EQ(resource1, resource2);
  EXPECT_TRUE(resource2->isCacheValidator());
  EXPECT_TRUE(resource2->stillNeedsLoad());

  // Fetch the same resource again before actual load operation starts.
  ResourceRequest request3(url);
  request3.setCachePolicy(WebCachePolicy::ValidatingCacheData);
  FetchRequest fetchRequest3 = FetchRequest(request3, FetchInitiatorInfo());
  Resource* resource3 = FontResource::fetch(fetchRequest3, fetcher);
  ASSERT_TRUE(resource3);
  EXPECT_EQ(resource2, resource3);
  EXPECT_TRUE(resource3->isCacheValidator());
  EXPECT_TRUE(resource3->stillNeedsLoad());

  // startLoad() can be called from any initiator. Here, call it from the
  // latter.
  fetcher->startLoad(resource3);
  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  EXPECT_TRUE(resource3->isLoaded());
  EXPECT_FALSE(resource3->errorOccurred());
  EXPECT_TRUE(resource2->isLoaded());
  EXPECT_FALSE(resource2->errorOccurred());

  memoryCache()->remove(resource1);
}

// Tests if cache-aware font loading works correctly.
TEST_F(FontResourceTest, CacheAwareFontLoading) {
  KURL url(ParsedURLString, "http://127.0.0.1:8000/font.woff");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  Platform::current()->getURLLoaderMockFactory()->registerURL(
      url, WrappedResourceResponse(response), "");

  ResourceFetcher* fetcher = ResourceFetcher::create(
      MockFetchContext::create(MockFetchContext::kShouldLoadNewResource));

  FetchRequest fetchRequest = FetchRequest(url, FetchInitiatorInfo());
  fetchRequest.setCacheAwareLoadingEnabled(IsCacheAwareLoadingEnabled);
  FontResource* resource = FontResource::fetch(fetchRequest, fetcher);
  ASSERT_TRUE(resource);

  Persistent<MockFontResourceClient> client =
      new MockFontResourceClient(resource);
  fetcher->startLoad(resource);
  EXPECT_TRUE(resource->loader()->isCacheAwareLoadingActivated());
  resource->m_loadLimitState = FontResource::UnderLimit;

  // FontResource callbacks should be blocked during cache-aware loading.
  resource->fontLoadShortLimitCallback(nullptr);
  EXPECT_FALSE(client->fontLoadShortLimitExceededCalled());

  // Fail first request as disk cache miss.
  resource->loader()->handleError(ResourceError::cacheMissError(url));

  // Once cache miss error returns, previously blocked callbacks should be
  // called immediately.
  EXPECT_FALSE(resource->loader()->isCacheAwareLoadingActivated());
  EXPECT_TRUE(client->fontLoadShortLimitExceededCalled());
  EXPECT_FALSE(client->fontLoadLongLimitExceededCalled());

  // Add client now, fontLoadShortLimitExceeded() should be called.
  Persistent<MockFontResourceClient> client2 =
      new MockFontResourceClient(resource);
  EXPECT_TRUE(client2->fontLoadShortLimitExceededCalled());
  EXPECT_FALSE(client2->fontLoadLongLimitExceededCalled());

  // FontResource callbacks are not blocked now.
  resource->fontLoadLongLimitCallback(nullptr);
  EXPECT_TRUE(client->fontLoadLongLimitExceededCalled());

  // Add client now, both callbacks should be called.
  Persistent<MockFontResourceClient> client3 =
      new MockFontResourceClient(resource);
  EXPECT_TRUE(client3->fontLoadShortLimitExceededCalled());
  EXPECT_TRUE(client3->fontLoadLongLimitExceededCalled());

  Platform::current()->getURLLoaderMockFactory()->serveAsynchronousRequests();
  memoryCache()->remove(resource);
}

}  // namespace blink
