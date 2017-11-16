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
#include "platform/testing/TestingPlatformSupportWithMockScheduler.h"
#include "platform/testing/UnitTestHelpers.h"
#include "public/platform/Platform.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

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
  void Trace(blink::Visitor* visitor) override {
    RawResourceClient::Trace(visitor);
  }

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

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(dummy_client_);
    visitor->Trace(resource_);
    RawResourceClient::Trace(visitor);
  }

 private:
  Member<DummyClient> dummy_client_;
  Member<Resource> resource_;
};

TEST_F(RawResourceTest, AddClientDuringCallback) {
  Resource* raw = RawResource::CreateForTest("data:text/html,", Resource::kRaw);

  // Create a non-null response.
  ResourceResponse response = raw->GetResponse();
  response.SetURL(KURL("http://600.613/"));
  raw->SetResponse(response);
  raw->FinishForTest();
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
  void Trace(blink::Visitor* visitor) override {
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
  response.SetURL(KURL("http://600.613/"));
  raw->SetResponse(response);
  raw->FinishForTest();
  EXPECT_FALSE(raw->GetResponse().IsNull());

  Persistent<DummyClient> dummy_client = new DummyClient();
  Persistent<RemovingClient> removing_client =
      new RemovingClient(dummy_client.Get());
  raw->AddClient(dummy_client);
  raw->AddClient(removing_client);
  platform_->RunUntilIdle();
  EXPECT_FALSE(raw->IsAlive());
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
