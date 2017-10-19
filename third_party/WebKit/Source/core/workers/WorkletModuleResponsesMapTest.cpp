// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/testing/DummyPageHolder.h"
#include "core/workers/WorkletModuleResponsesMap.h"
#include "platform/loader/testing/FetchTestingPlatformSupport.h"
#include "platform/loader/testing/MockFetchContext.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/Optional.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class ClientImpl final : public GarbageCollectedFinalized<ClientImpl>,
                         public WorkletModuleResponsesMap::Client {
  USING_GARBAGE_COLLECTED_MIXIN(ClientImpl);

 public:
  enum class Result { kInitial, kOK, kFailed };

  void OnRead(const ModuleScriptCreationParams& params) override {
    ASSERT_EQ(Result::kInitial, result_);
    result_ = Result::kOK;
    params_.emplace(params);
  }

  void OnFailed() override {
    ASSERT_EQ(Result::kInitial, result_);
    result_ = Result::kFailed;
  }

  Result GetResult() const { return result_; }
  WTF::Optional<ModuleScriptCreationParams> GetParams() const {
    return params_;
  }

 private:
  Result result_ = Result::kInitial;
  WTF::Optional<ModuleScriptCreationParams> params_;
};

}  // namespace

class WorkletModuleResponsesMapTest : public ::testing::Test {
 public:
  WorkletModuleResponsesMapTest() = default;

  void SetUp() override {
    platform_->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings
    dummy_page_holder_ = DummyPageHolder::Create();
    auto context =
        MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
    fetcher_ = ResourceFetcher::Create(context);
    map_ = new WorkletModuleResponsesMap(fetcher_);
  }

  void ReadEntry(const KURL& url, ClientImpl* client) {
    ResourceRequest resource_request(url);
    FetchParameters fetch_params(resource_request);
    map_->ReadEntry(fetch_params, client);
  }

 protected:
  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
  Persistent<ResourceFetcher> fetcher_;
  Persistent<WorkletModuleResponsesMap> map_;
};

TEST_F(WorkletModuleResponsesMapTest, Basic) {
  const KURL kUrl("https://example.com/module.js");
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl, testing::CoreTestDataPath("module.js"), "text/javascript");
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call initiates a fetch request.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());

  // The entry is now being fetched. Following read calls should wait for the
  // completion.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  clients.push_back(new ClientImpl);
  ReadEntry(kUrl, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());

  // Serve the fetch request. This should notify the waiting clients.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kOK, client->GetResult());
    EXPECT_TRUE(client->GetParams().has_value());
  }
}

TEST_F(WorkletModuleResponsesMapTest, Failure) {
  const KURL kUrl("https://example.com/module.js");
  URLTestHelpers::RegisterMockedErrorURLLoad(kUrl);
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call initiates a fetch request.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());

  // The entry is now being fetched. Following read calls should wait for the
  // completion.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  clients.push_back(new ClientImpl);
  ReadEntry(kUrl, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());

  // Serve the fetch request with 404. This should fail the waiting clients.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kFailed, client->GetResult());
    EXPECT_FALSE(client->GetParams().has_value());
  }
}

TEST_F(WorkletModuleResponsesMapTest, Isolation) {
  const KURL kUrl1("https://example.com/module?1.js");
  const KURL kUrl2("https://example.com/module?2.js");
  URLTestHelpers::RegisterMockedErrorURLLoad(kUrl1);
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl2, testing::CoreTestDataPath("module.js"), "text/javascript");
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call for |kUrl1| initiates a fetch request.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl1, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl1| should
  // wait for the completion.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl1, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  // An initial read call for |kUrl2| initiates a fetch request.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl2, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());
  EXPECT_FALSE(clients[2]->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl2| should
  // wait for the completion.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl2, clients[3]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[3]->GetResult());

  // The read call for |kUrl2| should not affect the other entry for |kUrl1|.
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());

  // Serve the fetch requests.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_EQ(ClientImpl::Result::kFailed, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());
  EXPECT_EQ(ClientImpl::Result::kFailed, clients[1]->GetResult());
  EXPECT_FALSE(clients[1]->GetParams().has_value());
  EXPECT_EQ(ClientImpl::Result::kOK, clients[2]->GetResult());
  EXPECT_TRUE(clients[2]->GetParams().has_value());
  EXPECT_EQ(ClientImpl::Result::kOK, clients[3]->GetResult());
  EXPECT_TRUE(clients[3]->GetParams().has_value());
}

TEST_F(WorkletModuleResponsesMapTest, InvalidURL) {
  const KURL kEmptyURL;
  ASSERT_TRUE(kEmptyURL.IsEmpty());
  ClientImpl* client1 = new ClientImpl;
  ReadEntry(kEmptyURL, client1);
  EXPECT_EQ(ClientImpl::Result::kFailed, client1->GetResult());
  EXPECT_FALSE(client1->GetParams().has_value());

  const KURL kNullURL = NullURL();
  ASSERT_TRUE(kNullURL.IsNull());
  ClientImpl* client2 = new ClientImpl;
  ReadEntry(kNullURL, client2);
  EXPECT_EQ(ClientImpl::Result::kFailed, client2->GetResult());
  EXPECT_FALSE(client2->GetParams().has_value());

  const KURL kInvalidURL;
  ASSERT_FALSE(kInvalidURL.IsValid());
  ClientImpl* client3 = new ClientImpl;
  ReadEntry(kInvalidURL, client3);
  EXPECT_EQ(ClientImpl::Result::kFailed, client3->GetResult());
  EXPECT_FALSE(client3->GetParams().has_value());
}

TEST_F(WorkletModuleResponsesMapTest, Dispose) {
  const KURL kUrl1("https://example.com/module?1.js");
  const KURL kUrl2("https://example.com/module?2.js");
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl1, testing::CoreTestDataPath("module.js"), "text/javascript");
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl2, testing::CoreTestDataPath("module.js"), "text/javascript");
  HeapVector<Member<ClientImpl>> clients;

  // An initial read call for |kUrl1| creates a placeholder entry and asks the
  // client to fetch a module script.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl1, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl1| should
  // wait for the completion.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl1, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  // An initial read call for |kUrl2| also creates a placeholder entry and asks
  // the client to fetch a module script.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl2, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());
  EXPECT_FALSE(clients[2]->GetParams().has_value());

  // The entry is now being fetched. Following read calls for |kUrl2| should
  // wait for the completion.
  clients.push_back(new ClientImpl);
  ReadEntry(kUrl2, clients[3]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[3]->GetResult());

  // Dispose() should notify to all waiting clients.
  map_->Dispose();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kFailed, client->GetResult());
    EXPECT_FALSE(client->GetParams().has_value());
  }
}

}  // namespace blink
