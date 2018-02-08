// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/Resource.h"

#include "platform/SharedBuffer.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/RawResource.h"
#include "platform/loader/fetch/ResourceRequest.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/loader/testing/MockResource.h"
#include "platform/loader/testing/MockResourceClient.h"
#include "platform/testing/TestingPlatformSupportWithMockScheduler.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/Vector.h"
#include "public/platform/Platform.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class MockPlatform final : public TestingPlatformSupportWithMockScheduler {
 public:
  MockPlatform() = default;
  ~MockPlatform() override = default;

  // From blink::Platform:
  void CacheMetadata(const WebURL& url, Time, const char*, size_t) override {
    cached_urls_.push_back(url);
  }

  const Vector<WebURL>& CachedURLs() const { return cached_urls_; }

 private:
  Vector<WebURL> cached_urls_;
};

ResourceResponse CreateTestResourceResponse() {
  ResourceResponse response(URLTestHelpers::ToKURL("https://example.com/"));
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
  KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHTTPStatusCode(200);
  Resource* resource = RawResource::CreateForTest(url, Resource::kRaw);
  resource->ResponseReceived(response, nullptr);
  resource->FinishForTest();

  // Revalidating with a url that differs by only the fragment
  // shouldn't trigger a securiy check.
  url.SetFragmentIdentifier("bar");
  resource->SetRevalidatingRequest(ResourceRequest(url));
  ResourceResponse revalidating_response(url);
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);
}

TEST(ResourceTest, Vary) {
  ScopedTestingPlatformSupport<MockPlatform> mock;
  const KURL url("http://127.0.0.1:8000/foo.html");
  ResourceResponse response(url);
  response.SetHTTPStatusCode(200);

  Resource* resource = RawResource::CreateForTest(url, Resource::kRaw);
  resource->ResponseReceived(response, nullptr);
  resource->FinishForTest();

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
  resource->FinishForTest();

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

TEST(ResourceTest, RevalidationFailed) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  Resource* resource = MockResource::Create(ResourceRequest(url));
  ResourceResponse response(url);
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  CachedMetadataHandler* original_cache_handler = resource->CacheHandler();
  EXPECT_TRUE(original_cache_handler);

  // Simulate revalidation start.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  EXPECT_EQ(original_cache_handler, resource->CacheHandler());

  Persistent<MockResourceClient> client = new MockResourceClient;
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHTTPStatusCode(200);
  resource->ResponseReceived(revalidating_response, nullptr);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_TRUE(resource->CacheHandler());
  EXPECT_NE(original_cache_handler, resource->CacheHandler());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));

  resource->AppendData(kData, 4);

  EXPECT_FALSE(client->NotifyFinishedCalled());

  resource->FinishForTest();

  EXPECT_TRUE(client->NotifyFinishedCalled());

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
}

TEST(ResourceTest, RevalidationSucceeded) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  Resource* resource = MockResource::Create(ResourceRequest(url));
  ResourceResponse response(url);
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  CachedMetadataHandler* original_cache_handler = resource->CacheHandler();
  EXPECT_TRUE(original_cache_handler);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  EXPECT_EQ(original_cache_handler, resource->CacheHandler());

  Persistent<MockResourceClient> client = new MockResourceClient;
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(4u, resource->ResourceBuffer()->size());
  EXPECT_EQ(original_cache_handler, resource->CacheHandler());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));

  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RevalidationSucceededForResourceWithoutBody) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  Resource* resource = MockResource::Create(ResourceRequest(url));
  ResourceResponse response(url);
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  Persistent<MockResourceClient> client = new MockResourceClient;
  resource->AddClient(client, nullptr);

  ResourceResponse revalidating_response(url);
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));
  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RevalidationSucceededUpdateHeaders) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/");
  Resource* resource = MockResource::Create(ResourceRequest(url));
  ResourceResponse response(url);
  response.SetHTTPStatusCode(200);
  response.AddHTTPHeaderField("keep-alive", "keep-alive value");
  response.AddHTTPHeaderField("expires", "expires value");
  response.AddHTTPHeaderField("last-modified", "last-modified value");
  response.AddHTTPHeaderField("proxy-authenticate", "proxy-authenticate value");
  response.AddHTTPHeaderField("proxy-connection", "proxy-connection value");
  response.AddHTTPHeaderField("x-custom", "custom value");
  resource->ResponseReceived(response, nullptr);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));

  // Validate that these headers pre-update.
  EXPECT_EQ("keep-alive value",
            resource->GetResponse().HttpHeaderField("keep-alive"));
  EXPECT_EQ("expires value",
            resource->GetResponse().HttpHeaderField("expires"));
  EXPECT_EQ("last-modified value",
            resource->GetResponse().HttpHeaderField("last-modified"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-connection value",
            resource->GetResponse().HttpHeaderField("proxy-connection"));
  EXPECT_EQ("custom value",
            resource->GetResponse().HttpHeaderField("x-custom"));

  Persistent<MockResourceClient> client = new MockResourceClient;
  resource->AddClient(client, nullptr);

  // Perform a revalidation step.
  ResourceResponse revalidating_response(url);
  revalidating_response.SetHTTPStatusCode(304);
  // Headers that aren't copied with an 304 code.
  revalidating_response.AddHTTPHeaderField("keep-alive", "garbage");
  revalidating_response.AddHTTPHeaderField("expires", "garbage");
  revalidating_response.AddHTTPHeaderField("last-modified", "garbage");
  revalidating_response.AddHTTPHeaderField("proxy-authenticate", "garbage");
  revalidating_response.AddHTTPHeaderField("proxy-connection", "garbage");
  // Header that is updated with 304 code.
  revalidating_response.AddHTTPHeaderField("x-custom", "updated");
  resource->ResponseReceived(revalidating_response, nullptr);

  // Validate the original response.
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());

  // Validate that these headers are not updated.
  EXPECT_EQ("keep-alive value",
            resource->GetResponse().HttpHeaderField("keep-alive"));
  EXPECT_EQ("expires value",
            resource->GetResponse().HttpHeaderField("expires"));
  EXPECT_EQ("last-modified value",
            resource->GetResponse().HttpHeaderField("last-modified"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-authenticate value",
            resource->GetResponse().HttpHeaderField("proxy-authenticate"));
  EXPECT_EQ("proxy-connection value",
            resource->GetResponse().HttpHeaderField("proxy-connection"));
  EXPECT_EQ("updated", resource->GetResponse().HttpHeaderField("x-custom"));

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RedirectDuringRevalidation) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  const KURL url("http://test.example.com/1");
  const KURL redirect_target_url("http://test.example.com/2");

  Resource* resource = MockResource::Create(ResourceRequest(url));
  ResourceResponse response(url);
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->FinishForTest();
  GetMemoryCache()->Add(resource);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(url, resource->LastResourceRequest().Url());

  CachedMetadataHandler* original_cache_handler = resource->CacheHandler();
  EXPECT_TRUE(original_cache_handler);

  // Simulate a revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));
  EXPECT_TRUE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(url, resource->LastResourceRequest().Url());
  EXPECT_EQ(original_cache_handler, resource->CacheHandler());

  Persistent<MockResourceClient> client = new MockResourceClient;
  resource->AddClient(client, nullptr);

  // The revalidating request is redirected.
  ResourceResponse redirect_response(url);
  redirect_response.SetHTTPHeaderField(
      "location", AtomicString(redirect_target_url.GetString()));
  redirect_response.SetHTTPStatusCode(308);
  ResourceRequest redirected_revalidating_request(redirect_target_url);
  resource->WillFollowRedirect(redirected_revalidating_request,
                               redirect_response);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(redirect_target_url, resource->LastResourceRequest().Url());
  EXPECT_FALSE(resource->CacheHandler());

  // The final response is received.
  ResourceResponse revalidating_response(redirect_target_url);
  revalidating_response.SetHTTPStatusCode(200);
  resource->ResponseReceived(revalidating_response, nullptr);

  EXPECT_TRUE(resource->CacheHandler());

  const char kData2[4] = "xyz";
  resource->AppendData(kData2, 3);
  resource->FinishForTest();
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(url, resource->GetResourceRequest().Url());
  EXPECT_EQ(redirect_target_url, resource->LastResourceRequest().Url());
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(3u, resource->ResourceBuffer()->size());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(url));

  EXPECT_TRUE(client->NotifyFinishedCalled());

  // Test the case where a client is added after revalidation is completed.
  Persistent<MockResourceClient> client2 = new MockResourceClient;
  resource->AddClient(
      client2, Platform::Current()->CurrentThread()->GetTaskRunner().get());

  // Because the client is added asynchronously,
  // |runUntilIdle()| is called to make |client2| to be notified.
  platform_->RunUntilIdle();

  EXPECT_TRUE(client2->NotifyFinishedCalled());

  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  resource->RemoveClient(client2);
  EXPECT_FALSE(resource->IsAlive());
}

}  // namespace blink
