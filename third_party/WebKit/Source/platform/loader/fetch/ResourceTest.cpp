// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/Resource.h"

#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockPlatform final : public TestingPlatformSupportWithMockScheduler {
 public:
  MockPlatform() {}
  ~MockPlatform() override {}

  // From blink::Platform:
  void CacheMetadata(const WebURL& url, Time, const char*, size_t) override {
    cached_urls_.push_back(url);
  }

  const Vector<WebURL>& CachedURLs() const { return cached_urls_; }

 private:
  Vector<WebURL> cached_urls_;
};

ResourceResponse CreateTestResourceResponse() {
  ResourceResponse response;
  response.SetURL(URLTestHelpers::ToKURL("https://example.com/"));
  response.SetHTTPStatusCode(200);
  return response;
}

void CreateTestResourceAndSetCachedMetadata(const ResourceResponse& response) {
  const char kTestData[] = "test data";
  Resource* resource =
      RawResource::CreateForTest(response.Url(), Resource::kRaw);
  resource->SetResponse(response);
  resource->CacheHandler()->SetCachedMetadata(
      100, kTestData, sizeof(kTestData),
      CachedMetadataHandler::kSendToPlatform);
  return;
}

}  // anonymous namespace

TEST(ResourceTest, SetCachedMetadata_SendsMetadataToPlatform) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ResourceResponse response(CreateTestResourceResponse());
  CreateTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(1u, mock->CachedURLs().size());
}

TEST(
    ResourceTest,
    SetCachedMetadata_DoesNotSendMetadataToPlatformWhenFetchedViaServiceWorker) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  ResourceResponse response(CreateTestResourceResponse());
  response.SetWasFetchedViaServiceWorker(true);
  CreateTestResourceAndSetCachedMetadata(response);
  EXPECT_EQ(0u, mock->CachedURLs().size());
}

TEST(ResourceTest, RevalidateWithFragment) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);
  Resource* resource = RawResource::CreateForTest(url, Resource::kRaw);
  resource->ResponseReceived(response, nullptr);
  resource->Finish();

  // Revalidating with a url that differs by only the fragment
  // shouldn't trigger a securiy check.
  url.SetFragmentIdentifier("bar");
  resource->SetRevalidatingRequest(ResourceRequest(url));
  ResourceResponse revalidating_response;
  revalidating_response.SetURL(url);
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);
}

TEST(ResourceTest, Vary) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");
  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);

  Resource* resource = RawResource::CreateForTest(url, Resource::kRaw);
  resource->ResponseReceived(response, nullptr);
  resource->Finish();

  ResourceRequest new_request(url);
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  response.SetHTTPHeaderField(HTTPNames::Vary, "*");
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Irrelevant header
  response.SetHTTPHeaderField(HTTPNames::Vary, "definitelynotarealheader");
  resource->SetResponse(response);
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  // Header present on new but not old
  new_request.SetHTTPHeaderField(HTTPNames::User_Agent, "something");
  response.SetHTTPHeaderField(HTTPNames::Vary, HTTPNames::User_Agent);
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));
  new_request.ClearHTTPHeaderField(HTTPNames::User_Agent);

  ResourceRequest old_request(url);
  old_request.SetHTTPHeaderField(HTTPNames::User_Agent, "something");
  old_request.SetHTTPHeaderField(HTTPNames::Referer, "http://foo.com");
  resource = RawResource::CreateForTest(old_request, Resource::kRaw);
  resource->ResponseReceived(response, nullptr);
  resource->Finish();

  // Header present on old but not new
  new_request.ClearHTTPHeaderField(HTTPNames::User_Agent);
  response.SetHTTPHeaderField(HTTPNames::Vary, HTTPNames::User_Agent);
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Header present on both
  new_request.SetHTTPHeaderField(HTTPNames::User_Agent, "something");
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));

  // One matching, one mismatching
  response.SetHTTPHeaderField(HTTPNames::Vary, "User-Agent, Referer");
  resource->SetResponse(response);
  EXPECT_TRUE(resource->MustReloadDueToVaryHeader(new_request));

  // Two matching
  new_request.SetHTTPHeaderField(HTTPNames::Referer, "http://foo.com");
  EXPECT_FALSE(resource->MustReloadDueToVaryHeader(new_request));
}

}  // namespace blink
