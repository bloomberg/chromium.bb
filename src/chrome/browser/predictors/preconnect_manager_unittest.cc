// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/preconnect_manager.h"

#include <map>
#include <utility>

#include "base/format_macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;
using testing::SaveArg;
using testing::StrictMock;

namespace predictors {

constexpr int kNormalLoadFlags = net::LOAD_NORMAL;
constexpr int kPrivateLoadFlags = net::LOAD_DO_NOT_SEND_COOKIES |
                                  net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DO_NOT_SEND_AUTH_DATA;

class MockPreconnectManagerDelegate
    : public PreconnectManager::Delegate,
      public base::SupportsWeakPtr<MockPreconnectManagerDelegate> {
 public:
  // Gmock doesn't support mocking methods with move-only argument types.
  void PreconnectFinished(std::unique_ptr<PreconnectStats> stats) override {
    PreconnectFinishedProxy(stats->url);
  }

  MOCK_METHOD1(PreconnectFinishedProxy, void(const GURL& url));
};

class MockNetworkContext : public network::TestNetworkContext {
 public:
  MockNetworkContext() = default;
  ~MockNetworkContext() override {
    EXPECT_TRUE(resolve_host_clients_.empty())
        << "Not all requests were satisfied";
  };

  void ResolveHost(
      const net::HostPortPair& host_port,
      network::mojom::ResolveHostParametersPtr control_handle,
      network::mojom::ResolveHostClientPtr response_client) override {
    const std::string& host = host_port.host();
    EXPECT_TRUE(
        resolve_host_clients_.emplace(host, std::move(response_client)).second);
    ResolveHostProxy(host);
  }

  void CompleteRequest(const std::string& host, int result) {
    auto it = resolve_host_clients_.find(host);
    if (it == resolve_host_clients_.end()) {
      ADD_FAILURE() << host << " wasn't found";
      return;
    }
    it->second->OnComplete(result, base::nullopt);
    resolve_host_clients_.erase(it);
    // Wait for OnComplete() to be executed on the UI thread.
    base::RunLoop().RunUntilIdle();
  }

  MOCK_METHOD1(ResolveHostProxy, void(const std::string& host));
  MOCK_METHOD4(PreconnectSockets,
               void(uint32_t num_streams,
                    const GURL& url,
                    int32_t load_flags,
                    bool privacy_mode_enabled));

 private:
  std::map<std::string, network::mojom::ResolveHostClientPtr>
      resolve_host_clients_;
};

class PreconnectManagerTest : public testing::Test {
 public:
  PreconnectManagerTest();
  ~PreconnectManagerTest() override;

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<StrictMock<MockNetworkContext>> mock_network_context_;
  std::unique_ptr<StrictMock<MockPreconnectManagerDelegate>> mock_delegate_;
  std::unique_ptr<PreconnectManager> preconnect_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PreconnectManagerTest);
};

PreconnectManagerTest::PreconnectManagerTest()
    : profile_(std::make_unique<TestingProfile>()),
      mock_network_context_(std::make_unique<StrictMock<MockNetworkContext>>()),
      mock_delegate_(
          std::make_unique<StrictMock<MockPreconnectManagerDelegate>>()),
      preconnect_manager_(
          std::make_unique<PreconnectManager>(mock_delegate_->AsWeakPtr(),
                                              profile_.get())) {
  preconnect_manager_->SetNetworkContextForTesting(mock_network_context_.get());
}

PreconnectManagerTest::~PreconnectManagerTest() = default;

TEST_F(PreconnectManagerTest, TestStartOneUrlPreresolve) {
  GURL main_frame_url("http://google.com");
  GURL url_to_preresolve("http://cdn.google.com");

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(url_to_preresolve.host()));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  preconnect_manager_->Start(main_frame_url,
                             {PreconnectRequest(url_to_preresolve, 0)});
  mock_network_context_->CompleteRequest(url_to_preresolve.host(), net::OK);
}

TEST_F(PreconnectManagerTest, TestStartOneUrlPreconnect) {
  GURL main_frame_url("http://google.com");
  GURL url_to_preconnect("http://cdn.google.com");

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(url_to_preconnect.host()));
  preconnect_manager_->Start(main_frame_url,
                             {PreconnectRequest(url_to_preconnect, 1)});
  EXPECT_CALL(*mock_network_context_,
              PreconnectSockets(1, url_to_preconnect, kNormalLoadFlags, false));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteRequest(url_to_preconnect.host(), net::OK);
}

TEST_F(PreconnectManagerTest, TestStopOneUrlBeforePreconnect) {
  GURL main_frame_url("http://google.com");
  GURL url_to_preconnect("http://cdn.google.com");

  // Preconnect job isn't started before preresolve is completed asynchronously.
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(url_to_preconnect.host()));
  preconnect_manager_->Start(main_frame_url,
                             {PreconnectRequest(url_to_preconnect, 1)});

  // Stop all jobs for |main_frame_url| before we get the callback.
  preconnect_manager_->Stop(main_frame_url);
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteRequest(url_to_preconnect.host(), net::OK);
}

TEST_F(PreconnectManagerTest, TestGetCallbackAfterDestruction) {
  GURL main_frame_url("http://google.com");
  GURL url_to_preconnect("http://cdn.google.com");
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(url_to_preconnect.host()));
  preconnect_manager_->Start(main_frame_url,
                             {PreconnectRequest(url_to_preconnect, 1)});

  // Callback may outlive PreconnectManager but it shouldn't cause a crash.
  preconnect_manager_ = nullptr;
  mock_network_context_->CompleteRequest(url_to_preconnect.host(), net::OK);
}

TEST_F(PreconnectManagerTest, TestUnqueuedPreresolvesCanceled) {
  GURL main_frame_url("http://google.com");
  size_t count = PreconnectManager::kMaxInflightPreresolves;
  std::vector<PreconnectRequest> requests;
  for (size_t i = 0; i < count; ++i) {
    // Exactly PreconnectManager::kMaxInflightPreresolves should be preresolved.
    requests.emplace_back(
        GURL(base::StringPrintf("http://cdn%" PRIuS ".google.com", i)), 1);
    EXPECT_CALL(*mock_network_context_,
                ResolveHostProxy(requests.back().origin.host()));
  }
  // This url shouldn't be preresolved.
  requests.emplace_back(GURL("http://no.preresolve.com"), 1);
  preconnect_manager_->Start(main_frame_url, requests);

  preconnect_manager_->Stop(main_frame_url);
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  for (size_t i = 0; i < count; ++i)
    mock_network_context_->CompleteRequest(requests[i].origin.host(), net::OK);
}

TEST_F(PreconnectManagerTest, TestTwoConcurrentMainFrameUrls) {
  GURL main_frame_url1("http://google.com");
  GURL url_to_preconnect1("http://cdn.google.com");
  GURL main_frame_url2("http://facebook.com");
  GURL url_to_preconnect2("http://cdn.facebook.com");

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(url_to_preconnect1.host()));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(url_to_preconnect2.host()));
  preconnect_manager_->Start(main_frame_url1,
                             {PreconnectRequest(url_to_preconnect1, 1)});
  preconnect_manager_->Start(main_frame_url2,
                             {PreconnectRequest(url_to_preconnect2, 1)});
  // Check that the first url didn't block the second one.
  Mock::VerifyAndClearExpectations(preconnect_manager_.get());

  preconnect_manager_->Stop(main_frame_url2);
  // Stopping the second url shouldn't stop the first one.
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, url_to_preconnect1, kNormalLoadFlags, false));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url1));
  mock_network_context_->CompleteRequest(url_to_preconnect1.host(), net::OK);
  // No preconnect for the second url.
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url2));
  mock_network_context_->CompleteRequest(url_to_preconnect2.host(), net::OK);
}

// Checks that the PreconnectManager handles no more than one URL per host
// simultaneously.
TEST_F(PreconnectManagerTest, TestTwoConcurrentSameHostMainFrameUrls) {
  GURL main_frame_url1("http://google.com/search?query=cats");
  GURL url_to_preconnect1("http://cats.google.com");
  GURL main_frame_url2("http://google.com/search?query=dogs");
  GURL url_to_preconnect2("http://dogs.google.com");

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(url_to_preconnect1.host()));
  preconnect_manager_->Start(main_frame_url1,
                             {PreconnectRequest(url_to_preconnect1, 1)});
  // This suggestion should be dropped because the PreconnectManager already has
  // a job for the "google.com" host.
  preconnect_manager_->Start(main_frame_url2,
                             {PreconnectRequest(url_to_preconnect2, 1)});

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, url_to_preconnect1, kNormalLoadFlags, false));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url1));
  mock_network_context_->CompleteRequest(url_to_preconnect1.host(), net::OK);
}

TEST_F(PreconnectManagerTest, TestStartPreresolveHost) {
  GURL url("http://cdn.google.com/script.js");
  GURL origin("http://cdn.google.com");

  // PreconnectFinished shouldn't be called.
  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(origin.host()));
  preconnect_manager_->StartPreresolveHost(url);
  mock_network_context_->CompleteRequest(origin.host(), net::OK);

  // Non http url shouldn't be preresovled.
  GURL non_http_url("file:///tmp/index.html");
  preconnect_manager_->StartPreresolveHost(non_http_url);
}

TEST_F(PreconnectManagerTest, TestStartPreresolveHosts) {
  GURL cdn("http://cdn.google.com");
  GURL fonts("http://fonts.google.com");

  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(cdn.host()));
  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(fonts.host()));
  preconnect_manager_->StartPreresolveHosts({cdn.host(), fonts.host()});
  mock_network_context_->CompleteRequest(cdn.host(), net::OK);
  mock_network_context_->CompleteRequest(fonts.host(), net::OK);
}

TEST_F(PreconnectManagerTest, TestStartPreconnectUrl) {
  GURL url("http://cdn.google.com/script.js");
  GURL origin("http://cdn.google.com");
  bool allow_credentials = false;

  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(origin.host()));
  preconnect_manager_->StartPreconnectUrl(url, allow_credentials);

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin, kPrivateLoadFlags, !allow_credentials));
  mock_network_context_->CompleteRequest(origin.host(), net::OK);

  // Non http url shouldn't be preconnected.
  GURL non_http_url("file:///tmp/index.html");
  preconnect_manager_->StartPreconnectUrl(non_http_url, allow_credentials);
}

TEST_F(PreconnectManagerTest, TestDetachedRequestHasHigherPriority) {
  GURL main_frame_url("http://google.com");
  size_t count = PreconnectManager::kMaxInflightPreresolves;
  std::vector<PreconnectRequest> requests;
  // Create enough asynchronous jobs to leave the last one in the queue.
  for (size_t i = 0; i < count; ++i) {
    requests.emplace_back(
        GURL(base::StringPrintf("http://cdn%" PRIuS ".google.com", i)), 0);
    EXPECT_CALL(*mock_network_context_,
                ResolveHostProxy(requests.back().origin.host()));
  }
  // This url will wait in the queue.
  GURL queued_url("http://fonts.google.com");
  requests.emplace_back(queued_url, 0);
  preconnect_manager_->Start(main_frame_url, requests);

  // This url should come to the front of the queue.
  GURL detached_preresolve("http://ads.google.com");
  preconnect_manager_->StartPreresolveHost(detached_preresolve);
  Mock::VerifyAndClearExpectations(preconnect_manager_.get());

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(detached_preresolve.host()));
  mock_network_context_->CompleteRequest(requests[0].origin.host(), net::OK);

  Mock::VerifyAndClearExpectations(preconnect_manager_.get());
  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(queued_url.host()));
  mock_network_context_->CompleteRequest(detached_preresolve.host(), net::OK);
  mock_network_context_->CompleteRequest(queued_url.host(), net::OK);

  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  for (size_t i = 1; i < count; ++i)
    mock_network_context_->CompleteRequest(requests[i].origin.host(), net::OK);
}

}  // namespace predictors
