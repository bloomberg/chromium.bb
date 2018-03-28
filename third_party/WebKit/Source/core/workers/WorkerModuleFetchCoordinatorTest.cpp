// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/modulescript/ModuleScriptCreationParams.h"
#include "core/workers/WorkerFetchTestHelper.h"
#include "core/workers/WorkerModuleFetchCoordinator.h"
#include "platform/loader/fetch/ResourceFetcher.h"
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

class WorkerModuleFetchCoordinatorTest : public ::testing::Test {
 public:
  WorkerModuleFetchCoordinatorTest() = default;

  void SetUp() override {
    platform_->AdvanceClockSeconds(1.);  // For non-zero DocumentParserTimings
    auto* context =
        MockFetchContext::Create(MockFetchContext::kShouldLoadNewResource);
    fetcher_ = ResourceFetcher::Create(context);
    coordinator_ = WorkerModuleFetchCoordinator::Create(fetcher_);
  }

  void Fetch(const KURL& url, ClientImpl* client) {
    ResourceRequest resource_request(url);
    FetchParameters fetch_params(resource_request);
    coordinator_->Fetch(fetch_params, client);
  }

 protected:
  ScopedTestingPlatformSupport<FetchTestingPlatformSupport> platform_;
  Persistent<ResourceFetcher> fetcher_;
  Persistent<WorkerModuleFetchCoordinator> coordinator_;
};

TEST_F(WorkerModuleFetchCoordinatorTest, Fetch_OneShot) {
  const KURL kUrl("https://example.com/module.js");
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl, test::CoreTestDataPath("module.js"), "text/javascript");

  ClientImpl* client = new ClientImpl;
  Fetch(kUrl, client);
  EXPECT_EQ(ClientImpl::Result::kInitial, client->GetResult());
  EXPECT_FALSE(client->GetParams().has_value());

  // Serve the fetch request. This should notify the waiting client.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_EQ(ClientImpl::Result::kOK, client->GetResult());
  EXPECT_TRUE(client->GetParams().has_value());
}

TEST_F(WorkerModuleFetchCoordinatorTest, Fetch_Repeat) {
  const KURL kUrl("https://example.com/module.js");
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl, test::CoreTestDataPath("module.js"), "text/javascript");
  HeapVector<Member<ClientImpl>> clients;

  clients.push_back(new ClientImpl);
  Fetch(kUrl, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());

  clients.push_back(new ClientImpl);
  Fetch(kUrl, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  clients.push_back(new ClientImpl);
  Fetch(kUrl, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());

  // Serve the fetch request. This should notify the waiting clients.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kOK, client->GetResult());
    EXPECT_TRUE(client->GetParams().has_value());
  }
}

TEST_F(WorkerModuleFetchCoordinatorTest, Failure) {
  const KURL kUrl("https://example.com/module.js");
  URLTestHelpers::RegisterMockedErrorURLLoad(kUrl);

  ClientImpl* client = new ClientImpl;
  Fetch(kUrl, client);
  EXPECT_EQ(ClientImpl::Result::kInitial, client->GetResult());
  EXPECT_FALSE(client->GetParams().has_value());

  // Serve the fetch request with 404. This should fail the waiting client.
  platform_->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  EXPECT_EQ(ClientImpl::Result::kFailed, client->GetResult());
  EXPECT_FALSE(client->GetParams().has_value());
}

TEST_F(WorkerModuleFetchCoordinatorTest, Isolation) {
  const KURL kUrl1("https://example.com/module.js?1");
  const KURL kUrl2("https://example.com/module.js?2");
  URLTestHelpers::RegisterMockedErrorURLLoad(kUrl1);
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl2, test::CoreTestDataPath("module.js"), "text/javascript");
  HeapVector<Member<ClientImpl>> clients;

  // Make fetch requests for |kUrl1| to be failed.
  clients.push_back(new ClientImpl);
  Fetch(kUrl1, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());
  clients.push_back(new ClientImpl);
  Fetch(kUrl1, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

  // Make fetch requests for |kUrl2| to be successfully fetched.
  clients.push_back(new ClientImpl);
  Fetch(kUrl2, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());
  EXPECT_FALSE(clients[2]->GetParams().has_value());
  clients.push_back(new ClientImpl);
  Fetch(kUrl2, clients[3]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[3]->GetResult());

  // The fetch requests for |kUrl2| should not affect the other requests for
  // |kUrl1|.
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());

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

TEST_F(WorkerModuleFetchCoordinatorTest, InvalidURL) {
  const KURL kInvalidURL;
  ClientImpl* client = new ClientImpl;
  Fetch(kInvalidURL, client);
  EXPECT_EQ(ClientImpl::Result::kFailed, client->GetResult());
  EXPECT_FALSE(client->GetParams().has_value());
}

TEST_F(WorkerModuleFetchCoordinatorTest, Dispose) {
  const KURL kUrl1("https://example.com/module.js?1");
  const KURL kUrl2("https://example.com/module.js?2");
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl1, test::CoreTestDataPath("module.js"), "text/javascript");
  URLTestHelpers::RegisterMockedURLLoad(
      kUrl2, test::CoreTestDataPath("module.js"), "text/javascript");
  HeapVector<Member<ClientImpl>> clients;

  // Make some fetch requests.
  clients.push_back(new ClientImpl);
  Fetch(kUrl1, clients[0]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[0]->GetResult());
  EXPECT_FALSE(clients[0]->GetParams().has_value());

  clients.push_back(new ClientImpl);
  Fetch(kUrl1, clients[1]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[1]->GetResult());
  EXPECT_FALSE(clients[1]->GetParams().has_value());

  clients.push_back(new ClientImpl);
  Fetch(kUrl2, clients[2]);
  EXPECT_EQ(ClientImpl::Result::kInitial, clients[2]->GetResult());
  EXPECT_FALSE(clients[2]->GetParams().has_value());

  // Dispose() should notify all waiting clients.
  coordinator_->Dispose();
  for (auto client : clients) {
    EXPECT_EQ(ClientImpl::Result::kFailed, client->GetResult());
    EXPECT_FALSE(client->GetParams().has_value());
  }
}

}  // namespace blink
