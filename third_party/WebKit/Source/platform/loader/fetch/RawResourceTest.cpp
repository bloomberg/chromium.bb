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

#include "platform/loader/fetch/RawResource.h"

#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/loader/fetch/ResourceTimingInfo.h"
#include "platform/scheduler/child/web_scheduler.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

using ::testing::InSequence;
using ::testing::_;
using Checkpoint = ::testing::StrictMock<::testing::MockFunction<void(int)>>;

class RawResourceTest : public ::testing::Test {
 public:
  RawResourceTest() {}
  ~RawResourceTest() override {}

 protected:
  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RawResourceTest);
};

class MockRawResourceClient
    : public GarbageCollectedFinalized<MockRawResourceClient>,
      public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(MockRawResourceClient);

 public:
  static MockRawResourceClient* Create() {
    return new ::testing::StrictMock<MockRawResourceClient>;
  }

  MOCK_METHOD3(DataSent,
               void(Resource*, unsigned long long, unsigned long long));
  MOCK_METHOD3(ResponseReceivedInternal,
               void(Resource*,
                    const ResourceResponse&,
                    WebDataConsumerHandle*));
  MOCK_METHOD3(SetSerializedCachedMetadata,
               void(Resource*, const char*, size_t));
  MOCK_METHOD3(DataReceived, void(Resource*, const char*, size_t));
  MOCK_METHOD3(RedirectReceived,
               bool(Resource*,
                    const ResourceRequest&,
                    const ResourceResponse&));
  MOCK_METHOD0(RedirectBlocked, void());
  MOCK_METHOD2(DataDownloaded, void(Resource*, int));
  MOCK_METHOD2(DidReceiveResourceTiming,
               void(Resource*, const ResourceTimingInfo&));

  void ResponseReceived(
      Resource* resource,
      const ResourceResponse& response,
      std::unique_ptr<WebDataConsumerHandle> handle) override {
    ResponseReceivedInternal(resource, response, handle.get());
  }

  String DebugName() const override { return "MockRawResourceClient"; }

  DEFINE_INLINE_VIRTUAL_TRACE() { RawResourceClient::Trace(visitor); }

 protected:
  MockRawResourceClient() = default;
};

TEST_F(RawResourceTest, DontIgnoreAcceptForCacheReuse) {
  ResourceRequest jpeg_request;
  jpeg_request.SetHTTPAccept("image/jpeg");

  RawResource* jpeg_resource(
      RawResource::CreateForTest(jpeg_request, Resource::kRaw));

  ResourceRequest png_request;
  png_request.SetHTTPAccept("image/png");

  EXPECT_FALSE(jpeg_resource->CanReuse(FetchParameters(png_request)));
}

class DummyClient final : public GarbageCollectedFinalized<DummyClient>,
                          public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(DummyClient);

 public:
  DummyClient() : called_(false), number_of_redirects_received_(0) {}
  ~DummyClient() override {}

  // ResourceClient implementation.
  void NotifyFinished(Resource* resource) override { called_ = true; }
  String DebugName() const override { return "DummyClient"; }

  void DataReceived(Resource*, const char* data, size_t length) override {
    data_.Append(data, length);
  }

  bool RedirectReceived(Resource*,
                        const ResourceRequest&,
                        const ResourceResponse&) override {
    ++number_of_redirects_received_;
    return true;
  }

  bool Called() { return called_; }
  int NumberOfRedirectsReceived() const {
    return number_of_redirects_received_;
  }
  const Vector<char>& Data() { return data_; }
  DEFINE_INLINE_TRACE() { RawResourceClient::Trace(visitor); }

 private:
  bool called_;
  int number_of_redirects_received_;
  Vector<char> data_;
};

// This client adds another client when notified.
class AddingClient final : public GarbageCollectedFinalized<AddingClient>,
                           public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(AddingClient);

 public:
  AddingClient(DummyClient* client, Resource* resource)
      : dummy_client_(client), resource_(resource) {}

  ~AddingClient() override {}

  // ResourceClient implementation.
  void NotifyFinished(Resource* resource) override {
    // First schedule an asynchronous task to remove the client.
    // We do not expect a client to be called if the client is removed before
    // a callback invocation task queued inside addClient() is scheduled.
    Platform::Current()
        ->CurrentThread()
        ->Scheduler()
        ->LoadingTaskRunner()
        ->PostTask(BLINK_FROM_HERE, WTF::Bind(&AddingClient::RemoveClient,
                                              WrapPersistent(this)));
    resource->AddClient(dummy_client_);
  }
  String DebugName() const override { return "AddingClient"; }

  void RemoveClient() { resource_->RemoveClient(dummy_client_); }

  DEFINE_INLINE_VIRTUAL_TRACE() {
    visitor->Trace(dummy_client_);
    visitor->Trace(resource_);
    RawResourceClient::Trace(visitor);
  }

 private:
  Member<DummyClient> dummy_client_;
  Member<Resource> resource_;
};

TEST_F(RawResourceTest, RevalidationSucceeded) {
  Resource* resource =
      RawResource::CreateForTest("data:text/html,", Resource::kRaw);
  ResourceResponse response;
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  const char kData[5] = "abcd";
  resource->AppendData(kData, 4);
  resource->Finish();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest("data:text/html,"));

  Persistent<DummyClient> client = new DummyClient;
  resource->AddClient(client);

  ResourceResponse revalidating_response;
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_EQ(4u, resource->ResourceBuffer()->size());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(
                          KURL(kParsedURLString, "data:text/html,")));
  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->Called());
  EXPECT_EQ("abcd", String(client->Data().data(), client->Data().size()));
}

TEST_F(RawResourceTest, RevalidationSucceededForResourceWithoutBody) {
  Resource* resource =
      RawResource::CreateForTest("data:text/html,", Resource::kRaw);
  ResourceResponse response;
  response.SetHTTPStatusCode(200);
  resource->ResponseReceived(response, nullptr);
  resource->Finish();
  GetMemoryCache()->Add(resource);

  // Simulate a successful revalidation.
  resource->SetRevalidatingRequest(ResourceRequest("data:text/html,"));

  Persistent<DummyClient> client = new DummyClient;
  resource->AddClient(client);

  ResourceResponse revalidating_response;
  revalidating_response.SetHTTPStatusCode(304);
  resource->ResponseReceived(revalidating_response, nullptr);
  EXPECT_FALSE(resource->IsCacheValidator());
  EXPECT_EQ(200, resource->GetResponse().HttpStatusCode());
  EXPECT_FALSE(resource->ResourceBuffer());
  EXPECT_EQ(resource, GetMemoryCache()->ResourceForURL(
                          KURL(kParsedURLString, "data:text/html,")));
  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->Called());
  EXPECT_EQ(0u, client->Data().size());
}

TEST_F(RawResourceTest, RevalidationSucceededUpdateHeaders) {
  Resource* resource =
      RawResource::CreateForTest("data:text/html,", Resource::kRaw);
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

  Persistent<DummyClient> client = new DummyClient;
  resource->AddClient(client.Get());

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

  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  EXPECT_FALSE(resource->IsAlive());
  EXPECT_FALSE(client->Called());
  EXPECT_EQ(0u, client->Data().size());
}

TEST_F(RawResourceTest, RedirectDuringRevalidation) {
  Resource* resource =
      RawResource::CreateForTest("https://example.com/1", Resource::kRaw);
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

  Persistent<DummyClient> client = new DummyClient;
  resource->AddClient(client);

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

  EXPECT_TRUE(client->Called());
  EXPECT_EQ(1, client->NumberOfRedirectsReceived());
  EXPECT_EQ("xyz", String(client->Data().data(), client->Data().size()));

  // Test the case where a client is added after revalidation is completed.
  Persistent<DummyClient> client2 = new DummyClient;
  resource->AddClient(client2);

  // Because RawResourceClient is added asynchronously,
  // |runUntilIdle()| is called to make |client2| to be notified.
  platform_->RunUntilIdle();

  EXPECT_TRUE(client2->Called());
  EXPECT_EQ(1, client2->NumberOfRedirectsReceived());
  EXPECT_EQ("xyz", String(client2->Data().data(), client2->Data().size()));

  GetMemoryCache()->Remove(resource);

  resource->RemoveClient(client);
  resource->RemoveClient(client2);
  EXPECT_FALSE(resource->IsAlive());
}

TEST_F(RawResourceTest, AddClientDuringCallback) {
  Resource* raw = RawResource::CreateForTest("data:text/html,", Resource::kRaw);

  // Create a non-null response.
  ResourceResponse response = raw->GetResponse();
  response.SetURL(KURL(kParsedURLString, "http://600.613/"));
  raw->SetResponse(response);
  raw->Finish();
  EXPECT_FALSE(raw->GetResponse().IsNull());

  Persistent<DummyClient> dummy_client = new DummyClient();
  Persistent<AddingClient> adding_client =
      new AddingClient(dummy_client.Get(), raw);
  raw->AddClient(adding_client);
  platform_->RunUntilIdle();
  raw->RemoveClient(adding_client);
  EXPECT_FALSE(dummy_client->Called());
  EXPECT_FALSE(raw->IsAlive());
}

// This client removes another client when notified.
class RemovingClient : public GarbageCollectedFinalized<RemovingClient>,
                       public RawResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(RemovingClient);

 public:
  explicit RemovingClient(DummyClient* client) : dummy_client_(client) {}

  ~RemovingClient() override {}

  // ResourceClient implementation.
  void NotifyFinished(Resource* resource) override {
    resource->RemoveClient(dummy_client_);
    resource->RemoveClient(this);
  }
  String DebugName() const override { return "RemovingClient"; }
  DEFINE_INLINE_TRACE() {
    visitor->Trace(dummy_client_);
    RawResourceClient::Trace(visitor);
  }

 private:
  Member<DummyClient> dummy_client_;
};

TEST_F(RawResourceTest, RemoveClientDuringCallback) {
  Resource* raw = RawResource::CreateForTest("data:text/html,", Resource::kRaw);

  // Create a non-null response.
  ResourceResponse response = raw->GetResponse();
  response.SetURL(KURL(kParsedURLString, "http://600.613/"));
  raw->SetResponse(response);
  raw->Finish();
  EXPECT_FALSE(raw->GetResponse().IsNull());

  Persistent<DummyClient> dummy_client = new DummyClient();
  Persistent<RemovingClient> removing_client =
      new RemovingClient(dummy_client.Get());
  raw->AddClient(dummy_client);
  raw->AddClient(removing_client);
  platform_->RunUntilIdle();
  EXPECT_FALSE(raw->IsAlive());
}

// ResourceClient can be added to |m_clients| asynchronously via
// ResourceCallback. When revalidation is started after ResourceCallback is
// scheduled and before it is dispatched, ResourceClient's callbacks should be
// called appropriately.
TEST_F(RawResourceTest, StartFailedRevalidationWhileResourceCallback) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");

  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);

  ResourceResponse new_response;
  new_response.SetURL(url);
  new_response.SetHTTPStatusCode(201);

  Resource* resource =
      RawResource::CreateForTest("data:text/html,", Resource::kRaw);
  resource->ResponseReceived(response, nullptr);
  resource->AppendData("oldData", 8);
  resource->Finish();

  InSequence s;
  Checkpoint checkpoint;

  MockRawResourceClient* client = MockRawResourceClient::Create();

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, ResponseReceivedInternal(resource, new_response, _));
  EXPECT_CALL(*client, DataReceived(resource, ::testing::StrEq("newData"), 8));

  // Add a client. No callbacks are made here because ResourceCallback is
  // scheduled asynchronously.
  resource->AddClient(client);
  EXPECT_FALSE(resource->IsCacheValidator());

  // Start revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));
  EXPECT_TRUE(resource->IsCacheValidator());

  // Make the ResourceCallback to be dispatched.
  platform_->RunUntilIdle();

  checkpoint.Call(1);

  resource->ResponseReceived(new_response, nullptr);
  resource->AppendData("newData", 8);
}

TEST_F(RawResourceTest, StartSuccessfulRevalidationWhileResourceCallback) {
  KURL url(kParsedURLString, "http://127.0.0.1:8000/foo.html");

  ResourceResponse response;
  response.SetURL(url);
  response.SetHTTPStatusCode(200);

  ResourceResponse new_response;
  new_response.SetURL(url);
  new_response.SetHTTPStatusCode(304);

  Resource* resource =
      RawResource::CreateForTest("data:text/html,", Resource::kRaw);
  resource->ResponseReceived(response, nullptr);
  resource->AppendData("oldData", 8);
  resource->Finish();

  InSequence s;
  Checkpoint checkpoint;

  MockRawResourceClient* client = MockRawResourceClient::Create();

  EXPECT_CALL(checkpoint, Call(1));
  EXPECT_CALL(*client, ResponseReceivedInternal(resource, response, _));
  EXPECT_CALL(*client, DataReceived(resource, ::testing::StrEq("oldData"), 8));

  // Add a client. No callbacks are made here because ResourceCallback is
  // scheduled asynchronously.
  resource->AddClient(client);
  EXPECT_FALSE(resource->IsCacheValidator());

  // Start revalidation.
  resource->SetRevalidatingRequest(ResourceRequest(url));
  EXPECT_TRUE(resource->IsCacheValidator());

  // Make the ResourceCallback to be dispatched.
  platform_->RunUntilIdle();

  checkpoint.Call(1);

  resource->ResponseReceived(new_response, nullptr);
}

TEST_F(RawResourceTest,
       CanReuseDevToolsEmulateNetworkConditionsClientIdHeader) {
  ResourceRequest request("data:text/html,");
  request.SetHTTPHeaderField(
      HTTPNames::X_DevTools_Emulate_Network_Conditions_Client_Id, "Foo");
  Resource* raw = RawResource::CreateForTest(request, Resource::kRaw);
  EXPECT_TRUE(
      raw->CanReuse(FetchParameters(ResourceRequest("data:text/html,"))));
}

}  // namespace blink
