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

TEST(ResourceTest, RevalidationSucceeded) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  Resource* resource = MockResource::Create(ResourceRequest("data:text/html,"));
  ResourceResponse response;
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->Finish();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest("data:text/html,"));

  Persistent<MockResourceClient> client = new MockResourceClient(resource);

  ResourceResponse revalidating_response;
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(4u, resource->ResourceBuffer()->size());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(
                          KURL(kParsedURLString, "data:text/html,")));
  GetMemoryCache()->Remove(resource);

  client->RemoveAsClient();
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RevalidationSucceededForResourceWithoutBody) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  Resource* resource = MockResource::Create(ResourceRequest("data:text/html,"));
  ResourceResponse response;
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  resource->Finish();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest("data:text/html,"));

  Persistent<MockResourceClient> client = new MockResourceClient(resource);

  ResourceResponse revalidating_response;
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(
                          KURL(kParsedURLString, "data:text/html,")));
  GetMemoryCache()->Remove(resource);

  client->RemoveAsClient();
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RevalidationSucceededUpdateHeaders) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  Resource* resource = MockResource::Create(ResourceRequest("data:text/html,"));
  ResourceResponse response;
  response.SetHTTPStatusCode(200);
  response.AddHTTPHeaderField("keep-alive", "keep-alive value");
  response.AddHTTPHeaderField("expires", "expires value");
  response.AddHTTPHeaderField("last-modified", "last-modified value");
  response.AddHTTPHeaderField("proxy-authenticate", "proxy-authenticate value");
  response.AddHTTPHeaderField("proxy-connection", "proxy-connection value");
  response.AddHTTPHeaderField("x-custom", "custom value");
  resource->ResponseReceived(response, nullptr);
  resource->Finish();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest("data:text/html,"));

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

  Persistent<MockResourceClient> client = new MockResourceClient(resource);

  // Perform a revalidation step.
  ResourceResponse revalidating_response;
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

  client->RemoveAsClient();

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->NotifyFinishedCalled());
}

TEST(ResourceTest, RedirectDuringRevalidation) {
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
  Resource* resource =
      MockResource::Create(ResourceRequest("https://example.com/1"));
  ResourceResponse response;
  response.SetURL(KURL(kParsedURLString, "https://example.com/1"));
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->Finish();
  GetMemoryCache()->Add(resource);

  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ("https://example.com/1",
            resource->GetResourceRequest().Url().GetString());
  EXPECT_EQ("https://example.com/1",
            resource->LastResourceRequest().Url().GetString());

  // Simulate a revalidation.
  resource->SetRevalidatingRequest(ResourceRequest("https://example.com/1"));
  EXPECT_TRUE(resource->IsCacheValidator());
  EXPECT_EQ("https://example.com/1",
            resource->GetResourceRequest().Url().GetString());
  EXPECT_EQ("https://example.com/1",
            resource->LastResourceRequest().Url().GetString());

  Persistent<MockResourceClient> client = new MockResourceClient(resource);

  // The revalidating request is redirected.
  ResourceResponse redirect_response;
  redirect_response.SetURL(KURL(kParsedURLString, "https://example.com/1"));
  redirect_response.SetHTTPHeaderField("location", "https://example.com/2");
  redirect_response.SetHTTPStatusCode(308);
  ResourceRequest redirected_revalidating_request("https://example.com/2");
  resource->WillFollowRedirect(redirected_revalidating_request,
                               redirect_response);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ("https://example.com/1",
            resource->GetResourceRequest().Url().GetString());
  EXPECT_EQ("https://example.com/2",
            resource->LastResourceRequest().Url().GetString());

  // The final response is received.
  ResourceResponse revalidating_response;
  revalidating_response.SetURL(KURL(kParsedURLString, "https://example.com/2"));
  revalidating_response.SetHTTPStatusCode(200);
  resource->ResponseReceived(revalidating_response, nullptr);
  const char kData2[4] = "xyz";
  resource->AppendData(kData2, 3);
  resource->Finish();
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ("https://example.com/1",
            resource->GetResourceRequest().Url().GetString());
  EXPECT_EQ("https://example.com/2",
            resource->LastResourceRequest().Url().GetString());
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(3u, resource->ResourceBuffer()->size());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(
                          KURL(kParsedURLString, "https://example.com/1")));

  EXPECT_TRUE(client->NotifyFinishedCalled());

  // Test the case where a client is added after revalidation is completed.
  Persistent<MockResourceClient> client2 = new MockResourceClient(resource);

  // Because the client is added asynchronously,
  // |runUntilIdle()| is called to make |client2| to be notified.
  platform_->RunUntilIdle();

  EXPECT_TRUE(client2->NotifyFinishedCalled());

  GetMemoryCache()->Remove(resource);

  client->RemoveAsClient();
  client2->RemoveAsClient();
  EXPECT_FALSE(resource->IsAlive());
}

}  // namespace blink
