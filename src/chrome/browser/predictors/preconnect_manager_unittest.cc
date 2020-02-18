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
#include "chrome/browser/predictors/proxy_lookup_client_impl.h"
#include "chrome/browser/predictors/resolve_host_client_impl.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::Mock;
using testing::SaveArg;
using testing::StrictMock;

namespace predictors {

namespace {

net::ProxyInfo GetIndirectProxyInfo() {
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("proxy.com");
  return proxy_info;
}

net::ProxyInfo GetDirectProxyInfo() {
  net::ProxyInfo proxy_info;
  proxy_info.UseDirect();
  return proxy_info;
}

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
        << "Not all resolve host requests were satisfied";
  }

  void ResolveHost(const net::HostPortPair& host_port,
                   const net::NetworkIsolationKey& network_isolation_key,
                   network::mojom::ResolveHostParametersPtr optional_parameters,
                   mojo::PendingRemote<network::mojom::ResolveHostClient>
                       response_client) override {
    const std::string& host = host_port.host();
    EXPECT_FALSE(IsHangingHost(GURL(host)))
        << " Hosts marked as hanging should not be resolved.";
    EXPECT_TRUE(resolve_host_clients_
                    .emplace(ResolveHostClientKey{host, network_isolation_key},
                             mojo::Remote<network::mojom::ResolveHostClient>(
                                 std::move(response_client)))
                    .second);
    ResolveHostProxy(host);
  }

  void LookUpProxyForURL(const GURL& url,
                         const net::NetworkIsolationKey& network_isolation_key,
                         mojo::PendingRemote<network::mojom::ProxyLookupClient>
                             proxy_lookup_client) override {
    EXPECT_TRUE(
        proxy_lookup_clients_.emplace(url, std::move(proxy_lookup_client))
            .second);
    if (!enabled_proxy_testing_) {
      // We don't want to test proxy, return that the proxy is disabled.
      CompleteProxyLookup(url, base::nullopt);
    }
  }

  void CompleteHostLookup(const std::string& host,
                          const net::NetworkIsolationKey& network_isolation_key,
                          int result) {
    DCHECK(result == net::OK || result == net::ERR_NAME_NOT_RESOLVED);
    auto it = resolve_host_clients_.find(
        ResolveHostClientKey{host, network_isolation_key});
    if (it == resolve_host_clients_.end()) {
      ADD_FAILURE() << host << " wasn't found";
      return;
    }
    it->second->OnComplete(result, net::ResolveErrorInfo(result),
                           base::nullopt);
    resolve_host_clients_.erase(it);
    // Wait for OnComplete() to be executed on the UI thread.
    base::RunLoop().RunUntilIdle();
  }

  void CompleteProxyLookup(const GURL& url,
                           const base::Optional<net::ProxyInfo>& result) {
    if (IsHangingHost(url))
      return;

    auto it = proxy_lookup_clients_.find(url);
    if (it == proxy_lookup_clients_.end()) {
      ADD_FAILURE() << url.spec() << " wasn't found";
      return;
    }
    it->second->OnProxyLookupComplete(net::ERR_FAILED, result);
    proxy_lookup_clients_.erase(it);
    // Wait for OnProxyLookupComplete() to be executed on the UI thread.
    base::RunLoop().RunUntilIdle();
  }

  // Preresolve/preconnect requests for all hosts in |hanging_url_requests| is
  // never completed.
  void SetHangingHostsFromPreconnectRequests(
      const std::vector<PreconnectRequest>& hanging_url_requests) {
    hanging_hosts_.clear();
    for (const auto& request : hanging_url_requests) {
      hanging_hosts_.push_back(request.origin.host());
    }
  }

  void EnableProxyTesting() { enabled_proxy_testing_ = true; }

  MOCK_METHOD1(ResolveHostProxy, void(const std::string& host));
  MOCK_METHOD4(PreconnectSockets,
               void(uint32_t num_streams,
                    const GURL& url,
                    bool allow_credentials,
                    const net::NetworkIsolationKey& network_isolation_key));

 private:
  bool IsHangingHost(const GURL& url) const {
    return std::find(hanging_hosts_.begin(), hanging_hosts_.end(),
                     url.host()) != hanging_hosts_.end();
  }

  using ResolveHostClientKey = std::pair<std::string, net::NetworkIsolationKey>;
  std::map<ResolveHostClientKey,
           mojo::Remote<network::mojom::ResolveHostClient>>
      resolve_host_clients_;
  std::map<GURL, mojo::Remote<network::mojom::ProxyLookupClient>>
      proxy_lookup_clients_;
  bool enabled_proxy_testing_ = false;
  std::vector<std::string> hanging_hosts_;
};

// Creates a NetworkIsolationKey for a main frame navigation to URL.
net::NetworkIsolationKey CreateNetworkIsolationKey(const GURL& main_frame_url) {
  url::Origin origin = url::Origin::Create(main_frame_url);
  return net::NetworkIsolationKey(origin, origin);
}

}  // namespace

class PreconnectManagerTest : public testing::Test {
 public:
  PreconnectManagerTest();
  ~PreconnectManagerTest() override;

  void VerifyAndClearExpectations() const {
    base::RunLoop().RunUntilIdle();
    Mock::VerifyAndClearExpectations(mock_network_context_.get());
    Mock::VerifyAndClearExpectations(mock_delegate_.get());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
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

PreconnectManagerTest::~PreconnectManagerTest() {
  VerifyAndClearExpectations();
}

TEST_F(PreconnectManagerTest, TestStartOneUrlPreresolve) {
  GURL main_frame_url("http://google.com");
  url::Origin origin_to_preresolve =
      url::Origin::Create(GURL("http://cdn.google.com"));

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preresolve.host()));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preresolve, 0,
                         CreateNetworkIsolationKey(main_frame_url))});
  mock_network_context_->CompleteHostLookup(
      origin_to_preresolve.host(), CreateNetworkIsolationKey(main_frame_url),
      net::OK);
}

TEST_F(PreconnectManagerTest, TestStartOneUrlPreconnect) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect =
      url::Origin::Create(GURL("http://cdn.google.com"));

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect.host()));
  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect, 1, network_isolation_key)});
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect.host(),
                                            network_isolation_key, net::OK);
}

TEST_F(PreconnectManagerTest,
       TestStartOneUrlPreconnectWithNetworkIsolationKey) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect =
      url::Origin::Create(GURL("http://cdn.google.com"));

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect.host()));
  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect, 1, network_isolation_key)});
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect.host(),
                                            network_isolation_key, net::OK);
}

// Sends preconnect request for a webpage, and stops the request before
// all pertaining preconnect requests finish. Next, preconnect request
// for the same webpage is sent again. Verifies that all the preconnects
// related to the second request are dispatched on the network.
TEST_F(PreconnectManagerTest, TestStartOneUrlPreconnect_MultipleTimes) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  size_t count = PreconnectManager::kMaxInflightPreresolves;
  std::vector<PreconnectRequest> requests;
  for (size_t i = 0; i < count + 1; ++i) {
    // Exactly PreconnectManager::kMaxInflightPreresolves should be preresolved.
    std::string url = base::StringPrintf("http://cdn%" PRIuS ".google.com", i);
    requests.emplace_back(url::Origin::Create(GURL(url)), 1,
                          network_isolation_key);
  }
  for (size_t i = 0; i < count; ++i) {
    // Exactly PreconnectManager::kMaxInflightPreresolves should be preresolved.
    EXPECT_CALL(*mock_network_context_,
                ResolveHostProxy(requests[i].origin.host()));
  }
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  preconnect_manager_->Start(main_frame_url, requests);
  preconnect_manager_->Stop(main_frame_url);
  for (size_t i = 0; i < count; ++i) {
    mock_network_context_->CompleteHostLookup(requests[i].origin.host(),
                                              network_isolation_key, net::OK);
  }
  VerifyAndClearExpectations();

  // Now, restart the preconnect request.
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, requests.back().origin.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(requests.back().origin.host()));
  for (size_t i = 0; i < count; ++i) {
    EXPECT_CALL(
        *mock_network_context_,
        PreconnectSockets(1, requests[i].origin.GetURL(),
                          true /* allow credentials */, network_isolation_key));
    EXPECT_CALL(*mock_network_context_,
                ResolveHostProxy(requests[i].origin.host()));
  }
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));

  preconnect_manager_->Start(main_frame_url, requests);

  for (size_t i = 0; i < count + 1; ++i) {
    mock_network_context_->CompleteHostLookup(requests[i].origin.host(),
                                              network_isolation_key, net::OK);
  }
}

// Sends preconnect request for two webpages, and stops one request before
// all pertaining preconnect requests finish. Next, preconnect request
// for the same webpage is sent again. Verifies that all the preconnects
// related to the second request are dispatched on the network.
TEST_F(PreconnectManagerTest, TestTwoConcurrentMainFrameUrls_MultipleTimes) {
  GURL main_frame_url_1("http://google.com");
  net::NetworkIsolationKey network_isolation_key_1 =
      CreateNetworkIsolationKey(main_frame_url_1);
  size_t count = PreconnectManager::kMaxInflightPreresolves;
  std::vector<PreconnectRequest> requests;
  for (size_t i = 0; i < count + 1; ++i) {
    std::string url = base::StringPrintf("http://cdn%" PRIuS ".google.com", i);
    requests.emplace_back(url::Origin::Create(GURL(url)), 1,
                          network_isolation_key_1);
  }

  GURL main_frame_url_2("http://google2.com");
  net::NetworkIsolationKey network_isolation_key_2 =
      CreateNetworkIsolationKey(main_frame_url_1);

  for (size_t i = 0; i < count; ++i) {
    EXPECT_CALL(*mock_network_context_,
                ResolveHostProxy(requests[i].origin.host()));
  }
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url_1));
  for (size_t i = 0; i < count - 1; ++i) {
    EXPECT_CALL(*mock_network_context_,
                PreconnectSockets(1, requests[i].origin.GetURL(),
                                  true /* allow credentials */,
                                  network_isolation_key_1));
  }

  preconnect_manager_->Start(
      main_frame_url_1, std::vector<PreconnectRequest>(
                            requests.begin(), requests.begin() + count - 1));
  preconnect_manager_->Start(main_frame_url_2,
                             std::vector<PreconnectRequest>(
                                 requests.begin() + count - 1, requests.end()));

  preconnect_manager_->Stop(main_frame_url_2);
  for (size_t i = 0; i < count - 1; ++i) {
    mock_network_context_->CompleteHostLookup(requests[i].origin.host(),
                                              network_isolation_key_1, net::OK);
  }
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url_2));
  // Preconnect to |requests[count-1].origin| finishes after |main_frame_url_2|
  // is stopped. Finishing of |requests[count-1].origin| should cause preconnect
  // manager to clear all internal state related to |main_frame_url_2|.
  mock_network_context_->CompleteHostLookup(requests[count - 1].origin.host(),
                                            network_isolation_key_1, net::OK);
  VerifyAndClearExpectations();

  // Now, restart the preconnect request.
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(requests[count - 1].origin.host()));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(requests[count].origin.host()));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url_2));
  // Since state related to |main_frame_url_2| has been cleared,
  // re-issuing a request for connect to |main_frame_url_2| should be
  // successful.
  preconnect_manager_->Start(main_frame_url_2,
                             std::vector<PreconnectRequest>(
                                 requests.begin() + count - 1, requests.end()));

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, requests[count - 1].origin.GetURL(),
                        true /* allow credentials */, network_isolation_key_2));
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, requests[count].origin.GetURL(),
                        true /* allow credentials */, network_isolation_key_2));

  mock_network_context_->CompleteHostLookup(requests[count - 1].origin.host(),
                                            network_isolation_key_2, net::OK);
  mock_network_context_->CompleteHostLookup(requests[count].origin.host(),
                                            network_isolation_key_2, net::OK);
}

// Starts preconnect request for two webpages. The preconnect request for the
// second webpage is cancelled after one of its associated preconnect request
// goes in-flight.
// Verifies that if (i) Preconnect for a webpage is cancelled then its state is
// cleared after its associated in-flight requests finish; and, (ii) If the
// preconnect for that webpage is requested again, then
// the pertaining requests are dispatched to the network.
TEST_F(PreconnectManagerTest,
       TestStartOneUrlPreconnect_MultipleTimes_CancelledAfterInFlight) {
  GURL main_frame_url_1("http://google1.com");
  net::NetworkIsolationKey network_isolation_key_1 =
      CreateNetworkIsolationKey(main_frame_url_1);
  size_t count = PreconnectManager::kMaxInflightPreresolves;
  std::vector<PreconnectRequest> requests;
  for (size_t i = 0; i < count - 1; ++i) {
    std::string url =
        base::StringPrintf("http://hanging.cdn%" PRIuS ".google.com", i);
    requests.emplace_back(url::Origin::Create(GURL(url)), 1,
                          network_isolation_key_1);
  }
  mock_network_context_->SetHangingHostsFromPreconnectRequests(requests);

  // Preconnect requests to |requests| would hang.
  preconnect_manager_->Start(main_frame_url_1, requests);

  GURL main_frame_url_2("http://google2.com");
  net::NetworkIsolationKey network_isolation_key_2 =
      CreateNetworkIsolationKey(main_frame_url_2);
  url::Origin origin_to_preconnect_1 =
      url::Origin::Create(GURL("http://cdn.google1.com"));
  url::Origin origin_to_preconnect_2 =
      url::Origin::Create(GURL("http://cdn.google2.com"));

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect_1.host()));
  // Starting and stopping preconnect request for |main_frame_url_2|
  // should still dispatch the request for |origin_to_preconnect_1| on the
  // network.
  preconnect_manager_->Start(
      main_frame_url_2,
      {PreconnectRequest(origin_to_preconnect_1, 1, network_isolation_key_2),
       PreconnectRequest(origin_to_preconnect_2, 1, network_isolation_key_2)});
  // preconnect request for |origin_to_preconnect_1| is still in-flight and
  // Stop() is called on the associated webpage.
  preconnect_manager_->Stop(main_frame_url_2);
  VerifyAndClearExpectations();

  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url_2));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect_1.host(),
                                            network_isolation_key_2, net::OK);
  VerifyAndClearExpectations();

  // Request preconnect for |main_frame_url_2| again.
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect_1.host()));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect_2.host()));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url_2));
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect_1.GetURL(),
                        true /* allow credentials */, network_isolation_key_2));
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect_2.GetURL(),
                        true /* allow credentials */, network_isolation_key_2));
  preconnect_manager_->Start(
      main_frame_url_2,
      {PreconnectRequest(origin_to_preconnect_1, 1, network_isolation_key_2),
       PreconnectRequest(origin_to_preconnect_2, 1, network_isolation_key_2)});

  mock_network_context_->CompleteHostLookup(origin_to_preconnect_1.host(),
                                            network_isolation_key_2, net::OK);
  mock_network_context_->CompleteHostLookup(origin_to_preconnect_2.host(),
                                            network_isolation_key_2, net::OK);
}

// Sends a preconnect request again after the first request finishes. Verifies
// that the second preconnect request is dispatched to the network.
TEST_F(PreconnectManagerTest,
       TestStartOneUrlPreconnect_MultipleTimes_LessThanThree) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect_1 =
      url::Origin::Create(GURL("http://cdn.google1.com"));
  url::Origin origin_to_preconnect_2 =
      url::Origin::Create(GURL("http://cdn.google2.com"));

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect_1.host()));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect_2.host()));

  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect_1, 1, network_isolation_key),
       PreconnectRequest(origin_to_preconnect_2, 1, network_isolation_key)});
  preconnect_manager_->Stop(main_frame_url);

  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect_1.host(),
                                            network_isolation_key, net::OK);
  mock_network_context_->CompleteHostLookup(origin_to_preconnect_2.host(),
                                            network_isolation_key, net::OK);
  VerifyAndClearExpectations();

  // Now, start the preconnect request again.
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect_1.host()));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect_2.host()));
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect_1.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect_2.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect_1, 1, network_isolation_key),
       PreconnectRequest(origin_to_preconnect_2, 1, network_isolation_key)});
  mock_network_context_->CompleteHostLookup(origin_to_preconnect_1.host(),
                                            network_isolation_key, net::OK);
  mock_network_context_->CompleteHostLookup(origin_to_preconnect_2.host(),
                                            network_isolation_key, net::OK);
}

TEST_F(PreconnectManagerTest, TestStopOneUrlBeforePreconnect) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect =
      url::Origin::Create(GURL("http://cdn.google.com"));

  // Preconnect job isn't started before preresolve is completed asynchronously.
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect.host()));
  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect, 1, network_isolation_key)});

  // Stop all jobs for |main_frame_url| before we get the callback.
  preconnect_manager_->Stop(main_frame_url);
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect.host(),
                                            network_isolation_key, net::OK);
}

TEST_F(PreconnectManagerTest, TestGetCallbackAfterDestruction) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect =
      url::Origin::Create(GURL("http://cdn.google.com"));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect.host()));
  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect, 1, network_isolation_key)});

  // Callback may outlive PreconnectManager but it shouldn't cause a crash.
  preconnect_manager_ = nullptr;
  mock_network_context_->CompleteHostLookup(origin_to_preconnect.host(),
                                            network_isolation_key, net::OK);
}

TEST_F(PreconnectManagerTest, TestUnqueuedPreresolvesCanceled) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  size_t count = PreconnectManager::kMaxInflightPreresolves;
  std::vector<PreconnectRequest> requests;
  for (size_t i = 0; i < count; ++i) {
    // Exactly PreconnectManager::kMaxInflightPreresolves should be preresolved.
    std::string url = base::StringPrintf("http://cdn%" PRIuS ".google.com", i);
    requests.emplace_back(url::Origin::Create(GURL(url)), 1,
                          network_isolation_key);
    EXPECT_CALL(*mock_network_context_,
                ResolveHostProxy(requests.back().origin.host()));
  }
  // This url shouldn't be preresolved.
  requests.emplace_back(url::Origin::Create(GURL("http://no.preresolve.com")),
                        1, network_isolation_key);
  preconnect_manager_->Start(main_frame_url, requests);

  preconnect_manager_->Stop(main_frame_url);
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  for (size_t i = 0; i < count; ++i) {
    mock_network_context_->CompleteHostLookup(requests[i].origin.host(),
                                              network_isolation_key, net::OK);
  }
}

TEST_F(PreconnectManagerTest, TestTwoConcurrentMainFrameUrls) {
  GURL main_frame_url1("http://google.com");
  net::NetworkIsolationKey network_isolation_key1 =
      CreateNetworkIsolationKey(main_frame_url1);
  url::Origin origin_to_preconnect1 =
      url::Origin::Create(GURL("http://cdn.google.com"));
  GURL main_frame_url2("http://facebook.com");
  net::NetworkIsolationKey network_isolation_key2 =
      CreateNetworkIsolationKey(main_frame_url2);
  url::Origin origin_to_preconnect2 =
      url::Origin::Create(GURL("http://cdn.facebook.com"));

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect1.host()));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect2.host()));
  preconnect_manager_->Start(
      main_frame_url1,
      {PreconnectRequest(origin_to_preconnect1, 1, network_isolation_key1)});
  preconnect_manager_->Start(
      main_frame_url2,
      {PreconnectRequest(origin_to_preconnect2, 1, network_isolation_key2)});
  // Check that the first url didn't block the second one.
  Mock::VerifyAndClearExpectations(preconnect_manager_.get());

  preconnect_manager_->Stop(main_frame_url2);
  // Stopping the second url shouldn't stop the first one.
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect1.GetURL(),
                        true /* allow credentials */, network_isolation_key1));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url1));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect1.host(),
                                            network_isolation_key1, net::OK);
  // No preconnect for the second url.
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url2));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect2.host(),
                                            network_isolation_key2, net::OK);
}

// Checks that the PreconnectManager handles no more than one URL per host
// simultaneously.
TEST_F(PreconnectManagerTest, TestTwoConcurrentSameHostMainFrameUrls) {
  GURL main_frame_url1("http://google.com/search?query=cats");
  net::NetworkIsolationKey network_isolation_key1 =
      CreateNetworkIsolationKey(main_frame_url1);
  url::Origin origin_to_preconnect1 =
      url::Origin::Create(GURL("http://cats.google.com"));
  GURL main_frame_url2("http://google.com/search?query=dogs");
  net::NetworkIsolationKey network_isolation_key2 =
      CreateNetworkIsolationKey(main_frame_url2);
  url::Origin origin_to_preconnect2 =
      url::Origin::Create(GURL("http://dogs.google.com"));

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect1.host()));
  preconnect_manager_->Start(
      main_frame_url1,
      {PreconnectRequest(origin_to_preconnect1, 1, network_isolation_key1)});
  // This suggestion should be dropped because the PreconnectManager already has
  // a job for the "google.com" host.
  preconnect_manager_->Start(
      main_frame_url2,
      {PreconnectRequest(origin_to_preconnect2, 1, network_isolation_key2)});

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect1.GetURL(),
                        true /* allow credentials */, network_isolation_key1));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url1));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect1.host(),
                                            network_isolation_key1, net::OK);
}

TEST_F(PreconnectManagerTest, TestStartPreresolveHost) {
  GURL url("http://cdn.google.com/script.js");
  GURL origin("http://cdn.google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(origin);

  // PreconnectFinished shouldn't be called.
  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(origin.host()));
  preconnect_manager_->StartPreresolveHost(url, network_isolation_key);
  mock_network_context_->CompleteHostLookup(origin.host(),
                                            network_isolation_key, net::OK);

  // Non http url shouldn't be preresovled.
  GURL non_http_url("file:///tmp/index.html");
  preconnect_manager_->StartPreresolveHost(non_http_url, network_isolation_key);
}

TEST_F(PreconnectManagerTest, TestStartPreresolveHosts) {
  GURL cdn("http://cdn.google.com");
  GURL fonts("http://fonts.google.com");

  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(cdn.host()));
  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(fonts.host()));
  preconnect_manager_->StartPreresolveHosts({cdn.host(), fonts.host()},
                                            net::NetworkIsolationKey());
  mock_network_context_->CompleteHostLookup(
      cdn.host(), net::NetworkIsolationKey(), net::OK);
  mock_network_context_->CompleteHostLookup(
      fonts.host(), net::NetworkIsolationKey(), net::OK);
}

TEST_F(PreconnectManagerTest, TestStartPreconnectUrl) {
  GURL url("http://cdn.google.com/script.js");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(url);
  GURL origin("http://cdn.google.com");
  bool allow_credentials = false;

  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(origin.host()));
  preconnect_manager_->StartPreconnectUrl(url, allow_credentials,
                                          network_isolation_key);

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin, allow_credentials, network_isolation_key));
  mock_network_context_->CompleteHostLookup(origin.host(),
                                            network_isolation_key, net::OK);

  // Non http url shouldn't be preconnected.
  GURL non_http_url("file:///tmp/index.html");
  preconnect_manager_->StartPreconnectUrl(non_http_url, allow_credentials,
                                          network_isolation_key);
}

TEST_F(PreconnectManagerTest, TestStartPreconnectUrlWithNetworkIsolationKey) {
  GURL url("http://cdn.google.com/script.js");
  GURL origin("http://cdn.google.com");
  bool allow_credentials = false;
  url::Origin requesting_origin = url::Origin::Create(GURL("http://foo.test"));
  net::NetworkIsolationKey network_isolation_key(requesting_origin,
                                                 requesting_origin);

  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(origin.host()));
  preconnect_manager_->StartPreconnectUrl(url, allow_credentials,
                                          network_isolation_key);

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin, allow_credentials, network_isolation_key));
  mock_network_context_->CompleteHostLookup(origin.host(),
                                            network_isolation_key, net::OK);
}

TEST_F(PreconnectManagerTest, TestDetachedRequestHasHigherPriority) {
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  size_t count = PreconnectManager::kMaxInflightPreresolves;
  std::vector<PreconnectRequest> requests;
  // Create enough asynchronous jobs to leave the last one in the queue.
  for (size_t i = 0; i < count; ++i) {
    std::string url = base::StringPrintf("http://cdn%" PRIuS ".google.com", i);
    requests.emplace_back(url::Origin::Create(GURL(url)), 0,
                          network_isolation_key);
    EXPECT_CALL(*mock_network_context_,
                ResolveHostProxy(requests.back().origin.host()));
  }
  // This url will wait in the queue.
  url::Origin queued_origin =
      url::Origin::Create(GURL("http://fonts.google.com"));
  requests.emplace_back(queued_origin, 0, network_isolation_key);
  preconnect_manager_->Start(main_frame_url, requests);

  // This url should come to the front of the queue.
  GURL detached_preresolve("http://ads.google.com");
  preconnect_manager_->StartPreresolveHost(detached_preresolve,
                                           network_isolation_key);
  Mock::VerifyAndClearExpectations(preconnect_manager_.get());

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(detached_preresolve.host()));
  mock_network_context_->CompleteHostLookup(requests[0].origin.host(),
                                            network_isolation_key, net::OK);

  Mock::VerifyAndClearExpectations(preconnect_manager_.get());
  EXPECT_CALL(*mock_network_context_, ResolveHostProxy(queued_origin.host()));
  mock_network_context_->CompleteHostLookup(detached_preresolve.host(),
                                            network_isolation_key, net::OK);
  mock_network_context_->CompleteHostLookup(queued_origin.host(),
                                            network_isolation_key, net::OK);

  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  for (size_t i = 1; i < count; ++i) {
    mock_network_context_->CompleteHostLookup(requests[i].origin.host(),
                                              network_isolation_key, net::OK);
  }
}

TEST_F(PreconnectManagerTest, TestSuccessfulProxyLookup) {
  mock_network_context_->EnableProxyTesting();
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect =
      url::Origin::Create(GURL("http://cdn.google.com"));

  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect, 1, network_isolation_key)});

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteProxyLookup(origin_to_preconnect.GetURL(),
                                             GetIndirectProxyInfo());
}

TEST_F(PreconnectManagerTest, TestSuccessfulHostLookupAfterProxyLookupFailure) {
  mock_network_context_->EnableProxyTesting();
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect =
      url::Origin::Create(GURL("http://cdn.google.com"));
  url::Origin origin_to_preconnect2 =
      url::Origin::Create(GURL("http://ads.google.com"));

  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect, 1, network_isolation_key),
       PreconnectRequest(origin_to_preconnect2, 1, network_isolation_key)});
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect.host()));
  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect2.host()));
  // First URL uses direct connection.
  mock_network_context_->CompleteProxyLookup(origin_to_preconnect.GetURL(),
                                             GetDirectProxyInfo());
  // Second URL proxy lookup failed.
  mock_network_context_->CompleteProxyLookup(origin_to_preconnect2.GetURL(),
                                             base::nullopt);
  Mock::VerifyAndClearExpectations(mock_network_context_.get());

  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(
      *mock_network_context_,
      PreconnectSockets(1, origin_to_preconnect2.GetURL(),
                        true /* allow credentials */, network_isolation_key));
  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect.host(),
                                            network_isolation_key, net::OK);
  mock_network_context_->CompleteHostLookup(origin_to_preconnect2.host(),
                                            network_isolation_key, net::OK);
}

TEST_F(PreconnectManagerTest, TestBothProxyAndHostLookupFailed) {
  mock_network_context_->EnableProxyTesting();
  GURL main_frame_url("http://google.com");
  net::NetworkIsolationKey network_isolation_key =
      CreateNetworkIsolationKey(main_frame_url);
  url::Origin origin_to_preconnect =
      url::Origin::Create(GURL("http://cdn.google.com"));

  preconnect_manager_->Start(
      main_frame_url,
      {PreconnectRequest(origin_to_preconnect, 1, network_isolation_key)});

  EXPECT_CALL(*mock_network_context_,
              ResolveHostProxy(origin_to_preconnect.host()));
  mock_network_context_->CompleteProxyLookup(origin_to_preconnect.GetURL(),
                                             base::nullopt);
  Mock::VerifyAndClearExpectations(mock_network_context_.get());

  EXPECT_CALL(*mock_delegate_, PreconnectFinishedProxy(main_frame_url));
  mock_network_context_->CompleteHostLookup(origin_to_preconnect.host(),
                                            network_isolation_key,
                                            net::ERR_NAME_NOT_RESOLVED);
}

}  // namespace predictors
