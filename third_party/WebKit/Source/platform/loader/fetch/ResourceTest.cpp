// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/Resource.h"

#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/network/ResourceRequest.h"
#include "platform/network/ResourceResponse.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

class MockPlatform final : public TestingPlatformSupportWithMockScheduler {
 public:
  MockPlatform() {}
  ~MockPlatform() override {}

  // From blink::Platform:
  void cacheMetadata(const WebURL& url, int64_t, const char*, size_t) override {
    m_cachedURLs.push_back(url);
  }

  const Vector<WebURL>& cachedURLs() const { return m_cachedURLs; }

 private:
  Vector<WebURL> m_cachedURLs;
};

ResourceResponse createTestResourceResponse() {
  ResourceResponse response;
  response.setURL(URLTestHelpers::toKURL("https://example.com/"));
  response.setHTTPStatusCode(200);
  return response;
}

void createTestResourceAndSetCachedMetadata(const ResourceResponse& response) {
  const char testData[] = "test data";
  Resource* resource =
      RawResource::create(ResourceRequest(response.url()), Resource::Raw);
  resource->setResponse(response);
  resource->cacheHandler()->setCachedMetadata(
      100, testData, sizeof(testData), CachedMetadataHandler::SendToPlatform);
  return;
}

}  // anonymous namespace

TEST(ResourceTest, SetCachedMetadata_SendsMetadataToPlatform) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ResourceResponse response(createTestResourceResponse());
  createTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(1u, mock->cachedURLs().size());
}

TEST(
    ResourceTest,
    SetCachedMetadata_DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorker) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ResourceResponse response(createTestResourceResponse());
  response.setWasFetchedViaServiceWorker(true);
  createTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(0u, mock->cachedURLs().size());
}

TEST(ResourceTest, RevalidateWithFragment) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);
  Resource* resource = RawResource::create(url, Resource::Raw);
  resource->responseReceived(response, nullptr);
  resource->finish();

  // Revalidating with a url that differs by only the fragment
  // shouldn't trigger a securiy check.
  url.setFragmentIdentifier("bar");
  resource->setRevalidatingRequest(ResourceRequest(url));
  ResourceResponse revalidatingResponse;
  revalidatingResponse.setURL(url);
  revalidatingResponse.setHTTPStatusCode(304);
  resource->responseReceived(revalidatingResponse, nullptr);
}

TEST(ResourceTest, Vary) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  KURL url(ParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.setURL(url);
  response.setHTTPStatusCode(200);

  Resource* resource = RawResource::create(url, Resource::Raw);
  resource->responseReceived(response, nullptr);
  resource->finish();

  ResourceRequest newRequest(url);
  EXPECT_FALSE(resource->mustReloadDueToVaryHeader(newRequest));

  response.setHTTPHeaderField(HTTPNames::Vary, "*");
  resource->setResponse(response);
  EXPECT_TRUE(resource->mustReloadDueToVaryHeader(newRequest));

  // Irrelevant header
  response.setHTTPHeaderField(HTTPNames::Vary, "definitelynotarealheader");
  resource->setResponse(response);
  EXPECT_FALSE(resource->mustReloadDueToVaryHeader(newRequest));

  // Header present on new but not old
  newRequest.setHTTPHeaderField(HTTPNames::User_Agent, "something");
  response.setHTTPHeaderField(HTTPNames::Vary, HTTPNames::User_Agent);
  resource->setResponse(response);
  EXPECT_TRUE(resource->mustReloadDueToVaryHeader(newRequest));
  newRequest.clearHTTPHeaderField(HTTPNames::User_Agent);

  ResourceRequest oldRequest(url);
  oldRequest.setHTTPHeaderField(HTTPNames::User_Agent, "something");
  oldRequest.setHTTPHeaderField(HTTPNames::Referer, "http://foo.com");
  resource = RawResource::create(oldRequest, Resource::Raw);
  resource->responseReceived(response, nullptr);
  resource->finish();

  // Header present on old but not new
  newRequest.clearHTTPHeaderField(HTTPNames::User_Agent);
  response.setHTTPHeaderField(HTTPNames::Vary, HTTPNames::User_Agent);
  resource->setResponse(response);
  EXPECT_TRUE(resource->mustReloadDueToVaryHeader(newRequest));

  // Header present on both
  newRequest.setHTTPHeaderField(HTTPNames::User_Agent, "something");
  EXPECT_FALSE(resource->mustReloadDueToVaryHeader(newRequest));

  // One matching, one mismatching
  response.setHTTPHeaderField(HTTPNames::Vary, "User-Agent, Referer");
  resource->setResponse(response);
  EXPECT_TRUE(resource->mustReloadDueToVaryHeader(newRequest));

  // Two matching
  newRequest.setHTTPHeaderField(HTTPNames::Referer, "http://foo.com");
  EXPECT_FALSE(resource->mustReloadDueToVaryHeader(newRequest));
}

}  // namespace blink
