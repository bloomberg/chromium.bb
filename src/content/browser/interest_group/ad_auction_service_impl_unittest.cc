// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/ad_auction_service_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_fenced_frame_url_mapping_result_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/parakeet/ad_request.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr char kInterestGroupName[] = "interest-group-name";
constexpr char kOriginStringA[] = "https://a.test";
constexpr char kOriginStringB[] = "https://b.test";
constexpr char kOriginStringC[] = "https://c.test";
constexpr char kBiddingUrlPath[] = "/interest_group/bidding_logic.js";
constexpr char kNewBiddingUrlPath[] = "/interest_group/new_bidding_logic.js";
constexpr char kDecisionUrlPath[] = "/interest_group/decision_logic.js";
constexpr char kTrustedBiddingSignalsUrlPath[] =
    "/interest_group/trusted_bidding_signals.json";
constexpr char kDailyUpdateUrlPath[] =
    "/interest_group/daily_update_partial.json";

class AllowInterestGroupContentBrowserClient : public TestContentBrowserClient {
 public:
  explicit AllowInterestGroupContentBrowserClient() = default;
  ~AllowInterestGroupContentBrowserClient() override = default;

  AllowInterestGroupContentBrowserClient(
      const AllowInterestGroupContentBrowserClient&) = delete;
  AllowInterestGroupContentBrowserClient& operator=(
      const AllowInterestGroupContentBrowserClient&) = delete;

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(content::RenderFrameHost* render_frame_host,
                                 InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override {
    // Can join A interest groups on A top frames, B interest groups on B top
    // frames, C interest groups on C top frames, and C interest groups on A top
    // frames.
    return (top_frame_origin.host() == "a.test" &&
            api_origin.host() == "a.test") ||
           (top_frame_origin.host() == "b.test" &&
            api_origin.host() == "b.test") ||
           (top_frame_origin.host() == "c.test" &&
            api_origin.host() == "c.test") ||
           (top_frame_origin.host() == "a.test" &&
            api_origin.host() == "c.test");
  }
};

constexpr char kFledgeUpdateHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-type: Application/JSON\n"
    "X-Allow-FLEDGE: true\n";

constexpr char kFledgeScriptHeaders[] =
    "HTTP/1.1 200 OK\n"
    "Content-type: Application/Javascript\n"
    "X-Allow-FLEDGE: true\n";

constexpr char kFledgeReportHeaders[] =
    "HTTP/1.1 200 OK\n"
    "X-Allow-FLEDGE: true\n";

// Allows registering network responses to update and scoring / bidding script
// requests; *must* be destroyed before the task environment is shutdown (which
// happens in RenderViewHostTestHarness::TearDown()).
//
// Updates and script serving have different requirements, but unfortunately
// it's not possible to simultaneously instantiate 2 classes that both use their
// own URLLoaderInterceptor...so these are combined in this same class.
class NetworkResponder {
 public:
  // Register interest group update `response` to be served with JSON content
  // type when a request to `url_path` is made.
  void RegisterUpdateResponse(const std::string& url_path,
                              const std::string& response) {
    base::AutoLock auto_lock(lock_);
    json_update_map_[url_path] = response;
  }

  // Register script `response` to be served with Javascript content type when a
  // request to `url_path` is made.
  void RegisterScriptResponse(const std::string& url_path,
                              const std::string& response) {
    base::AutoLock auto_lock(lock_);
    script_map_[url_path] = response;
  }

  // Register ad auction reporting `response` to be served when a request to
  // `url_path` is made.
  void RegisterReportResponse(const std::string& url_path,
                              const std::string& response) {
    base::AutoLock auto_lock(lock_);
    report_map_[url_path] = response;
  }

  // Registers a URL to use a "deferred" update response. For a deferred
  // response, the request handler returns true without a write, and writes are
  // performed later in DoDeferredWrite() using a "stolen" Mojo pipe to the
  // URLLoaderClient.
  //
  // Only one request / response can be handled with this method at a time.
  void RegisterDeferredUpdateResponse(const std::string& url_path) {
    base::AutoLock auto_lock(lock_);
    deferred_response_url_path_ = url_path;
  }

  // Registers a URL that, when seen, will have its URLLoaderClient stored in
  // `stored_url_loader_client_` without sending a response.
  //
  // Only one request can be handled with this method at a time.
  void RegisterStoreUrlLoaderClient(const std::string& url_path) {
    base::AutoLock auto_lock(lock_);
    store_url_loader_client_url_path_ = url_path;
  }

  // Perform the deferred response -- the test fails if the client isn't waiting
  // on `url_path` registered with RegisterDeferredUpdateResponse().
  void DoDeferredUpdateResponse(const std::string& response) {
    base::AutoLock auto_lock(lock_);
    ASSERT_TRUE(deferred_response_url_loader_client_.is_bound());
    URLLoaderInterceptor::WriteResponse(
        kFledgeUpdateHeaders, response,
        deferred_response_url_loader_client_.get());
    deferred_response_url_loader_client_.reset();
    deferred_response_url_path_ = "";
  }

  // Make the next request fail with `error` -- subsequent requests will succeed
  // again unless another FailNextUpdateRequestWithError() call is made.
  void FailNextUpdateRequestWithError(net::Error error) {
    base::AutoLock auto_lock(lock_);
    next_error_ = error;
  }

  // Returns the number of updates that occurred -- does not include other
  // network requests.
  size_t UpdateCount() const {
    base::AutoLock auto_lock(lock_);
    return update_count_;
  }

  // Returns the number of reports that occurred -- does not include other
  // network requests.
  size_t ReportCount() const {
    base::AutoLock auto_lock(lock_);
    return report_count_;
  }

  // Indicates whether `stored_url_loader_client_` is connected to a receiver.
  bool RemoteIsConnected() {
    base::AutoLock auto_lock(lock_);
    return stored_url_loader_client_.is_connected();
  }

 private:
  bool RequestHandler(URLLoaderInterceptor::RequestParams* params) {
    base::AutoLock auto_lock(lock_);
    const auto script_it = script_map_.find(params->url_request.url.path());
    if (script_it != script_map_.end()) {
      URLLoaderInterceptor::WriteResponse(
          kFledgeScriptHeaders, script_it->second, params->client.get());
      return true;
    }

    // Not a script request, check if it's a reporting request.
    const auto report_it = report_map_.find(params->url_request.url.path());
    if (report_it != report_map_.end()) {
      report_count_++;
      URLLoaderInterceptor::WriteResponse(
          kFledgeReportHeaders, report_it->second, params->client.get());
      return true;
    }

    if ((params->url_request.url.path() == store_url_loader_client_url_path_)) {
      CHECK(!stored_url_loader_client_);
      stored_url_loader_client_ = std::move(params->client);
      report_count_++;
      return true;
    }

    // Not a script request or report request, so consider this an update
    // request.
    update_count_++;
    EXPECT_TRUE(params->url_request.trusted_params->isolation_info
                    .network_isolation_key()
                    .IsTransient());
    const auto update_it =
        json_update_map_.find(params->url_request.url.path());
    if (update_it != json_update_map_.end()) {
      URLLoaderInterceptor::WriteResponse(
          kFledgeUpdateHeaders, update_it->second, params->client.get());
      return true;
    }

    if (params->url_request.url.path() == deferred_response_url_path_) {
      CHECK(!deferred_response_url_loader_client_);
      deferred_response_url_loader_client_ = std::move(params->client);
      return true;
    }

    if (next_error_ != net::OK) {
      params->client->OnComplete(
          network::URLLoaderCompletionStatus(next_error_));
      next_error_ = net::OK;
      return true;
    }

    return false;
  }

  // Handles network requests for interest group updates and scripts.
  URLLoaderInterceptor network_interceptor_{
      base::BindRepeating(&NetworkResponder::RequestHandler,
                          base::Unretained(this))};

  mutable base::Lock lock_;

  // For each HTTPS request, we see if any path in the map matches the request
  // path. If so, the server returns the mapped value string as the response,
  // with JSON MIME type.
  base::flat_map<std::string, std::string> json_update_map_ GUARDED_BY(lock_);

  // Like `json_update_map_`, but for serving bidding / scoring scripts, with
  // the Javascript MIME type.
  base::flat_map<std::string, std::string> script_map_ GUARDED_BY(lock_);

  base::flat_map<std::string, std::string> report_map_ GUARDED_BY(lock_);

  // Stores the last URL path that was registered with
  // RegisterDeferredUpdateResponse(). Empty initially and after
  // DoDeferredUpdateResponse() -- when empty, no deferred response can occur.
  std::string deferred_response_url_path_ GUARDED_BY(lock_);

  // Stores the last URL path that was registered with
  // RegisterStoreUrlLoaderClient().
  std::string store_url_loader_client_url_path_ GUARDED_BY(lock_);

  // Stores the Mojo URLLoaderClient remote "stolen" from RequestHandler() for
  // use with deferred responses -- unbound if no remote has been "stolen" yet,
  // or if the last deferred response completed.
  mojo::Remote<network::mojom::URLLoaderClient>
      deferred_response_url_loader_client_ GUARDED_BY(lock_);

  // Stores the Mojo URLLoaderClient remote "stolen" from
  // RequestHandlerForUpdates() for use with no responses -- unbound if no
  // remote has been "stolen" yet, or if the last no response request timed out.
  mojo::Remote<network::mojom::URLLoaderClient> stored_url_loader_client_
      GUARDED_BY(lock_);

  net::Error next_error_ GUARDED_BY(lock_) = net::OK;

  size_t update_count_ GUARDED_BY(lock_) = 0;

  size_t report_count_ GUARDED_BY(lock_) = 0;
};

// AuctionProcessManager that allows running auctions in-proc.
class SameProcessAuctionProcessManager : public AuctionProcessManager {
 public:
  SameProcessAuctionProcessManager() = default;
  SameProcessAuctionProcessManager(const SameProcessAuctionProcessManager&) =
      delete;
  SameProcessAuctionProcessManager& operator=(
      const SameProcessAuctionProcessManager&) = delete;
  ~SameProcessAuctionProcessManager() override = default;

 private:
  void LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const std::string& display_name) override {
    // Create one AuctionWorkletServiceImpl per Mojo pipe, just like in
    // production code. Don't bother to delete the service on pipe close,
    // though; just keep it in a vector instead.
    auction_worklet_services_.push_back(
        std::make_unique<auction_worklet::AuctionWorkletServiceImpl>(
            std::move(auction_worklet_service_receiver)));
  }

  std::vector<std::unique_ptr<auction_worklet::AuctionWorkletServiceImpl>>
      auction_worklet_services_;
};

}  // namespace

// Tests the interest group management functionality of AdAuctionServiceImpl --
// this particular functionality used to be in a separate interface called
// RestrictedInterestStore. The interfaces were combined so so that they'd share
// a Mojo pipe (for message ordering consistency).
class AdAuctionServiceImplTest : public RenderViewHostTestHarness {
 public:
  AdAuctionServiceImplTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kInterestGroupStorage,
                              blink::features::kAdInterestGroupAPI,
                              blink::features::kFledge},
        /*disabled_features=*/{});
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

  ~AdAuctionServiceImplTest() override {
    SetBrowserClientForTesting(old_content_browser_client_);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(kUrlA);

    manager_ = (static_cast<StoragePartitionImpl*>(
                    browser_context()->GetDefaultStoragePartition()))
                   ->GetInterestGroupManager();
    // Process creation crashes in the Chrome zygote init in unit tests, so run
    // the auction "processes" in-process instead.
    manager_->set_auction_process_manager_for_testing(
        std::make_unique<SameProcessAuctionProcessManager>());
  }

  void TearDown() override {
    // `network_responder_` must be destructed while the task environment,
    // which gets destroyed by RenderViewHostTestHarness::TearDown(), is still
    // active.
    network_responder_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  std::vector<StorageInterestGroup> GetInterestGroupsForOwner(
      const url::Origin& owner) {
    std::vector<StorageInterestGroup> interest_groups;
    base::RunLoop run_loop;
    manager_->GetInterestGroupsForOwner(
        owner, base::BindLambdaForTesting(
                   [&run_loop, &interest_groups](
                       std::vector<StorageInterestGroup> groups) {
                     interest_groups = std::move(groups);
                     run_loop.Quit();
                   }));
    run_loop.Run();
    return interest_groups;
  }

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
      if (interest_group.interest_group.name == name) {
        return interest_group.bidding_browser_signals->join_count;
      }
    }
    return 0;
  }

  absl::optional<GURL> ConvertFencedFrameURNToURL(const GURL& urn_url) {
    TestFencedFrameURLMappingResultObserver observer;
    FencedFrameURLMapping& fenced_frame_urls_map =
        static_cast<RenderFrameHostImpl*>(main_rfh())
            ->GetPage()
            .fenced_frame_urls_map();
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap> ignored;
    fenced_frame_urls_map.ConvertFencedFrameURNToURL(urn_url, &observer);
    return observer.mapped_url();
  }

  // Create a new AdAuctionServiceImpl and use it to try and join
  // `interest_group`. Flushes the Mojo pipe to force the Mojo message to be
  // handled before returning.
  //
  // Creates a new AdAuctionServiceImpl with each call so the RFH
  // can be navigated between different sites. And
  // AdAuctionServiceImpl only handles one site (cross site navs use
  // different AdAuctionServices, and generally use different
  // RFHs as well).
  void JoinInterestGroupAndFlushForFrame(
      const blink::InterestGroup& interest_group,
      RenderFrameHost* rfh) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh, interest_service.BindNewPipeAndPassReceiver());

    interest_service->JoinInterestGroup(interest_group);
    interest_service.FlushForTesting();
  }

  // Like JoinInterestGroupAndFlushForFrame, but uses the render frame host of
  // the main frame.
  void JoinInterestGroupAndFlush(const blink::InterestGroup& interest_group) {
    JoinInterestGroupAndFlushForFrame(interest_group, main_rfh());
  }

  // Analogous to JoinInterestGroupAndFlushForFrame(), but leaves an interest
  // group instead of joining one.
  void LeaveInterestGroupAndFlushForFrame(const url::Origin& owner,
                                          const std::string& name,
                                          RenderFrameHost* rfh) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh, interest_service.BindNewPipeAndPassReceiver());

    interest_service->LeaveInterestGroup(owner, name);
    interest_service.FlushForTesting();
  }

  // Like LeaveInterestGroupAndFlushForFrame, but uses the render frame host of
  // the main frame.
  void LeaveInterestGroupAndFlush(const url::Origin& owner,
                                  const std::string& name) {
    LeaveInterestGroupAndFlushForFrame(owner, name, main_rfh());
  }

  // Updates registered interest groups according to their registered update
  // URL. Doesn't flush since the update operation requires a sequence of
  // asynchronous operations.
  void UpdateInterestGroupNoFlushForFrame(RenderFrameHost* rfh) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh, interest_service.BindNewPipeAndPassReceiver());

    interest_service->UpdateAdInterestGroups();
  }

  // Runs an ad auction using the config specified in `auction_config` in the
  // frame `rfh`. Returns the result of the auction, which is either a URL to
  // the winning ad, or absl::nullopt if no ad won the auction.
  absl::optional<GURL> RunAdAuctionAndFlushForFrame(
      blink::mojom::AuctionAdConfigPtr auction_config,
      RenderFrameHost* rfh) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        rfh, interest_service.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    absl::optional<GURL> maybe_url;
    interest_service->RunAdAuction(
        std::move(auction_config),
        base::BindLambdaForTesting(
            [&run_loop, &maybe_url](const absl::optional<GURL>& result) {
              maybe_url = result;
              run_loop.Quit();
            }));
    interest_service.FlushForTesting();
    run_loop.Run();
    return maybe_url;
  }

  // Like RunAdAuctionAndFlushForFrame(), but uses the render frame host of the
  // main frame.
  absl::optional<GURL> RunAdAuctionAndFlush(
      blink::mojom::AuctionAdConfigPtr auction_config) {
    return RunAdAuctionAndFlushForFrame(std::move(auction_config), main_rfh());
  }

  // Like UpdateInterestGroupNoFlushForFrame, but uses the render frame host of
  // the main frame.
  void UpdateInterestGroupNoFlush() {
    UpdateInterestGroupNoFlushForFrame(main_rfh());
  }

  // Helper to create a valid interest group with only an origin and name. All
  // URLs are nullopt.
  blink::InterestGroup CreateInterestGroup() {
    blink::InterestGroup interest_group;
    interest_group.expiry = base::Time::Now() + base::Seconds(300);
    interest_group.name = kInterestGroupName;
    interest_group.owner = kOriginA;
    return interest_group;
  }

  void CreateAdRequest(blink::mojom::AdRequestConfigPtr config,
                       AdAuctionServiceImpl::CreateAdRequestCallback callback) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    interest_service->CreateAdRequest(std::move(config), std::move(callback));
    interest_service.FlushForTesting();
  }

  void FinalizeAd(std::string guid,
                  blink::mojom::AuctionAdConfigPtr config,
                  AdAuctionServiceImpl::FinalizeAdCallback callback) {
    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        main_rfh(), interest_service.BindNewPipeAndPassReceiver());

    interest_service->FinalizeAd(guid, std::move(config), std::move(callback));
    interest_service.FlushForTesting();
  }

 protected:
  const GURL kUrlA = GURL(kOriginStringA);
  const url::Origin kOriginA = url::Origin::Create(kUrlA);
  const GURL kUrlB = GURL(kOriginStringB);
  const url::Origin kOriginB = url::Origin::Create(kUrlB);
  const GURL kUrlC = GURL(kOriginStringC);
  const url::Origin kOriginC = url::Origin::Create(kUrlC);
  const GURL kBiddingLogicUrlA = kUrlA.Resolve(kBiddingUrlPath);
  const GURL kNewBiddingLogicUrlA = kUrlA.Resolve(kNewBiddingUrlPath);
  const GURL kTrustedBiddingSignalsUrlA =
      kUrlA.Resolve(kTrustedBiddingSignalsUrlPath);
  const GURL kUpdateUrlA = kUrlA.Resolve(kDailyUpdateUrlPath);

  base::test::ScopedFeatureList feature_list_;

  AllowInterestGroupContentBrowserClient content_browser_client_;
  raw_ptr<ContentBrowserClient> old_content_browser_client_ = nullptr;
  raw_ptr<InterestGroupManager> manager_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;

  // Must be destroyed before RenderViewHostTestHarness::TearDown().
  std::unique_ptr<NetworkResponder> network_responder_{
      std::make_unique<NetworkResponder>()};
};

// Check basic success case.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupBasic) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Several tests assume interest group API are also allowed on kOriginB, so
  // make sure that's enabled correctly.
  NavigateAndCommit(kUrlB);
  interest_group.owner = kOriginB;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));
}

// Non-HTTPS interest groups should be rejected.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupOriginNotHttps) {
  // Note that the ContentBrowserClient allows URLs based on hosts, not origins,
  // so it should not block this URL. Instead, it should run into the HTTPS
  // check.
  const GURL kHttpUrlA = GURL("http://a.test/");
  const url::Origin kHttpOriginA = url::Origin::Create(kHttpUrlA);
  NavigateAndCommit(kHttpUrlA);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kHttpOriginA;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kHttpOriginA, kInterestGroupName));
}

// Test one origin trying to add an interest group for another.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupWrongOwnerOrigin) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginB;
  JoinInterestGroupAndFlush(interest_group);
  // Interest group should not be added for either origin.
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0, GetJoinCount(kOriginB, kInterestGroupName));
}

// Test joining an interest group with a cross-site owner.
TEST_F(AdAuctionServiceImplTest, JoinInterestFromCrossSiteIFrame) {
  // Create a subframe and use it to send the join request.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlC, subframe);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  JoinInterestGroupAndFlushForFrame(interest_group, subframe);
  JoinInterestGroupAndFlushForFrame(CreateInterestGroup(), subframe);

  // Subframes from origin C with a top frame of A should be able to join groups
  // with C as the owner, but the subframe from C should not be able to join
  // groups for A.
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlB, subframe);
  interest_group = CreateInterestGroup();
  interest_group.owner = kOriginB;
  JoinInterestGroupAndFlushForFrame(interest_group, subframe);

  // Subframes from origin B with a top frame of A should not (by policy) be
  // allowed to join groups with B as the owner.
  EXPECT_EQ(0, GetJoinCount(kOriginB, kInterestGroupName));
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Test joining an interest group with a disallowed cross-origin URL. Doesn't
// exhaustively test all cases, as the validation function has its own unit
// tests. This is just to make sure those are hooked up.
//
// TODO(mmenke): Once ReportBadMessage is called in these cases, make sure Mojo
// pipe is closed as well.
TEST_F(AdAuctionServiceImplTest, JoinInterestGroupCrossSiteUrls) {
  const GURL kBadUrl = GURL("https://user:pass@a.test/");

  // Test `bidding_url`.
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kBadUrl;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `update_url`.
  interest_group = CreateInterestGroup();
  interest_group.update_url = kBadUrl;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `trusted_bidding_signals_url`.
  interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_url = kBadUrl;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Attempt to join an interest group whose size is very large. No join should
// happen -- it should silently fail.
TEST_F(AdAuctionServiceImplTest, JoinMassiveInterestGroupFails) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  // 1 MiB of '5' characters is over the size limit.
  interest_group.user_bidding_signals = std::string(1024 * 1024, '5');
  JoinInterestGroupAndFlush(interest_group);

  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 0u);
}

// Check that cross-origin leave interest group operations don't work.
TEST_F(AdAuctionServiceImplTest, LeaveInterestGroupWrongOwnerOrigin) {
  // https://a.test/ joins an interest group.
  JoinInterestGroupAndFlush(CreateInterestGroup());
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // https://b.test/ cannot leave https://a.test/'s interest group.
  NavigateAndCommit(kUrlB);
  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // https://a.test/ can leave its own interest group.
  NavigateAndCommit(GURL("https://a.test/"));
  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Test leaving an interest group with a cross-site owner.
TEST_F(AdAuctionServiceImplTest, LeaveInterestFromCrossSiteIFrame) {
  // Join interest group from c.
  NavigateAndCommit(kUrlC);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  JoinInterestGroupAndFlush(interest_group);

  NavigateAndCommit(kUrlB);
  interest_group.owner = kOriginB;
  JoinInterestGroupAndFlush(interest_group);

  NavigateAndCommit(kUrlA);
  JoinInterestGroupAndFlush(CreateInterestGroup());

  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Create a subframe and use it to send the leave request.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlC, subframe);

  LeaveInterestGroupAndFlushForFrame(kOriginC, kInterestGroupName, subframe);
  LeaveInterestGroupAndFlushForFrame(kOriginA, kInterestGroupName, subframe);

  subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlB, subframe);

  LeaveInterestGroupAndFlushForFrame(kOriginB, kInterestGroupName, subframe);

  // Subframes from origin C with a top frame of A should be able to leave
  // groups with C as the owner, but the subframe from C should not be able to
  // leave groups for A. Pages with a top frame that is not B are not allowed
  // to leave B's interest groups (controlled by IsInterestGroupAPIAllowed)
  EXPECT_EQ(0, GetJoinCount(kOriginC, kInterestGroupName));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));
}

// These tests validate the `dailyUpdateUrl` and
// navigator.updateAdInterestGroups() functionality.

// The server JSON updates all fields that can be updated.
TEST_F(AdAuctionServiceImplTest, UpdateAllUpdatableFields) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath,
      base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                         kOriginStringA, kOriginStringA, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s/interest_group/new_bidding_logic.js",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf(
                "%s/interest_group/new_trusted_bidding_signals_url.json",
                kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "new_key");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Only set the ads field -- the other fields shouldn't be changed.
TEST_F(AdAuctionServiceImplTest, UpdatePartialPerformsMerge) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(
      group.bidding_url->spec(),
      base::StringPrintf("%s/interest_group/bidding_logic.js", kOriginStringA));
  ASSERT_TRUE(group.update_url.has_value());
  EXPECT_EQ(group.update_url->spec(),
            base::StringPrintf("%s/interest_group/daily_update_partial.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf("%s/interest_group/trusted_bidding_signals.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "key1");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// The update shouldn't change the expiration time of the interest group.
TEST_F(AdAuctionServiceImplTest, UpdateDoesntChangeExpiration) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Lookup expiry from the database before updating.
  const auto groups_before_update = GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups_before_update.size(), 1u);
  const base::Time kExpirationTime =
      groups_before_update[0].interest_group.expiry;

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The expiration time shouldn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  EXPECT_EQ(group.expiry, kExpirationTime);
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Only set the ads field -- the other fields shouldn't be changed.
TEST_F(AdAuctionServiceImplTest, UpdateSucceedsIfOptionalNameOwnerMatch) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath,
      base::StringPrintf(R"({
"name": "%s",
"owner": "%s",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                         kInterestGroupName, kOriginStringA, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(
      group.bidding_url->spec(),
      base::StringPrintf("%s/interest_group/bidding_logic.js", kOriginStringA));
  ASSERT_TRUE(group.update_url.has_value());
  EXPECT_EQ(group.update_url->spec(),
            base::StringPrintf("%s/interest_group/daily_update_partial.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_url.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_url->spec(),
            base::StringPrintf("%s/interest_group/trusted_bidding_signals.json",
                               kOriginStringA));
  ASSERT_TRUE(group.trusted_bidding_signals_keys.has_value());
  EXPECT_EQ(group.trusted_bidding_signals_keys->size(), 1u);
  EXPECT_EQ(group.trusted_bidding_signals_keys.value()[0], "key1");
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Try to set the name -- for security, name and owner shouldn't be
// allowed to change. If they don't match the interest group (update URLs are
// registered per interest group), fail the update and don't update anything.
TEST_F(AdAuctionServiceImplTest, NoUpdateIfOptionalNameDoesntMatch) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"name": "boats",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata":{"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Try to set the owner -- for security, name and owner shouldn't be
// allowed to change. If they don't match the interest group (update URLs are
// registered per interest group), fail the update and don't update anything.
TEST_F(AdAuctionServiceImplTest, NoUpdateIfOptionalOwnerDoesntMatch) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"owner": "%s",
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringB, kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Join 2 interest groups, each with the same owner, but with different update
// URLs. Both interest groups should be updated correctly.
TEST_F(AdAuctionServiceImplTest, UpdateMultipleInterestGroups) {
  constexpr char kGroupName1[] = "group1";
  constexpr char kGroupName2[] = "group2";
  constexpr char kDailyUpdateUrlPath1[] =
      "/interest_group/daily_update_partial1.json";
  constexpr char kDailyUpdateUrlPath2[] =
      "/interest_group/daily_update_partial2.json";
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath1, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url1",
         "metadata": {"new_a": "b1"}
        }]
})",
                                               kOriginStringA));
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath2, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url2",
         "metadata": {"new_a": "b2"}
        }]
})",
                                               kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.name = kGroupName1;
  interest_group.update_url = kUrlA.Resolve(kDailyUpdateUrlPath1);
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kGroupName1));

  // Now, join the second interest group, also belonging to `kOriginA`.
  blink::InterestGroup interest_group_2 = CreateInterestGroup();
  interest_group_2.name = kGroupName2;
  interest_group_2.update_url = kUrlA.Resolve(kDailyUpdateUrlPath2);
  interest_group_2.bidding_url = kBiddingLogicUrlA;
  interest_group_2.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group_2.trusted_bidding_signals_keys.emplace();
  interest_group_2.trusted_bidding_signals_keys->push_back("key1");
  interest_group_2.ads.emplace();
  ad = blink::InterestGroup::Ad();
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group_2.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_2);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kGroupName2));

  // Now, run the update. Both interest groups should update.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Both interest groups should update.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 2u);
  const auto& first_group = groups[0].interest_group.name == kGroupName1
                                ? groups[0].interest_group
                                : groups[1].interest_group;
  const auto& second_group = groups[0].interest_group.name == kGroupName2
                                 ? groups[0].interest_group
                                 : groups[1].interest_group;

  EXPECT_EQ(first_group.name, kGroupName1);
  ASSERT_TRUE(first_group.ads.has_value());
  ASSERT_EQ(first_group.ads->size(), 1u);
  EXPECT_EQ(first_group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url1", kOriginStringA));
  EXPECT_EQ(first_group.ads.value()[0].metadata, "{\"new_a\":\"b1\"}");

  EXPECT_EQ(second_group.name, kGroupName2);
  ASSERT_TRUE(second_group.ads.has_value());
  ASSERT_EQ(second_group.ads->size(), 1u);
  EXPECT_EQ(second_group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url2", kOriginStringA));
  EXPECT_EQ(second_group.ads.value()[0].metadata, "{\"new_a\":\"b2\"}");
}

// Join 2 interest groups, each with a different owner. When updating interest
// groups, only the 1 interest group owned by the origin of the frame that
// called navigator.updateAdInterestGroups() gets updated.
TEST_F(AdAuctionServiceImplTest, UpdateOnlyOwnOrigin) {
  // Both interest groups can share the same update logic and path (they just
  // use different origins).
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Now, join the second interest group, belonging to `kOriginB`.
  NavigateAndCommit(kUrlB);
  blink::InterestGroup interest_group_b = CreateInterestGroup();
  interest_group_b.owner = kOriginB;
  interest_group_b.update_url = kUrlB.Resolve(kDailyUpdateUrlPath);
  interest_group_b.bidding_url = kUrlB.Resolve(kBiddingUrlPath);
  interest_group_b.trusted_bidding_signals_url =
      kUrlB.Resolve(kTrustedBiddingSignalsUrlPath);
  interest_group_b.trusted_bidding_signals_keys.emplace();
  interest_group_b.trusted_bidding_signals_keys->push_back("key1");
  interest_group_b.ads.emplace();
  ad = blink::InterestGroup::Ad();
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group_b.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_b);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  // Now, run the update. Only the `kOriginB` group should get updated.
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The `kOriginB` interest group should update...
  std::vector<StorageInterestGroup> origin_b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(origin_b_groups.size(), 1u);
  const auto& origin_b_group = origin_b_groups[0].interest_group;
  EXPECT_EQ(origin_b_group.name, kInterestGroupName);
  ASSERT_TRUE(origin_b_group.ads.has_value());
  ASSERT_EQ(origin_b_group.ads->size(), 1u);
  EXPECT_EQ(origin_b_group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(origin_b_group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");

  // ...but the `kOriginA` interest group shouldn't change.
  std::vector<StorageInterestGroup> origin_a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(origin_a_groups.size(), 1u);
  const auto& origin_a_group = origin_a_groups[0].interest_group;
  ASSERT_TRUE(origin_a_group.ads.has_value());
  ASSERT_EQ(origin_a_group.ads->size(), 1u);
  EXPECT_EQ(origin_a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(origin_a_group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Test updating an interest group with a cross-site owner.
TEST_F(AdAuctionServiceImplTest, UpdateFromCrossSiteIFrame) {
  // All interest groups can share the same update logic and path (they just
  // use different origins).
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Now, join the second interest group, belonging to `kOriginB`.
  NavigateAndCommit(kUrlB);
  blink::InterestGroup interest_group_b = CreateInterestGroup();
  interest_group_b.owner = kOriginB;
  interest_group_b.update_url = kUrlB.Resolve(kDailyUpdateUrlPath);
  interest_group_b.bidding_url = kUrlB.Resolve(kBiddingUrlPath);
  interest_group_b.trusted_bidding_signals_url =
      kUrlB.Resolve(kTrustedBiddingSignalsUrlPath);
  interest_group_b.trusted_bidding_signals_keys.emplace();
  interest_group_b.trusted_bidding_signals_keys->push_back("key1");
  interest_group_b.ads.emplace();
  ad = blink::InterestGroup::Ad();
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group_b.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_b);
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));

  // Now, join the third interest group, belonging to `kOriginC`.
  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group_c = CreateInterestGroup();
  interest_group_c.owner = kOriginC;
  interest_group_c.update_url = kUrlC.Resolve(kDailyUpdateUrlPath);
  interest_group_c.bidding_url = kUrlC.Resolve(kBiddingUrlPath);
  interest_group_c.trusted_bidding_signals_url =
      kUrlC.Resolve(kTrustedBiddingSignalsUrlPath);
  interest_group_c.trusted_bidding_signals_keys.emplace();
  interest_group_c.trusted_bidding_signals_keys->push_back("key1");
  interest_group_c.ads.emplace();
  ad = blink::InterestGroup::Ad();
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group_c.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group_c);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  NavigateAndCommit(kUrlA);

  // Create a subframe and use it to send the join request.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlC, subframe);

  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  // Subframes from origin C with a top frame of A should update groups
  // with C as the owner, but the subframe from C should not be able to update
  // groups for A.
  // The `kOriginC` interest group should update...
  std::vector<StorageInterestGroup> origin_c_groups =
      GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(origin_c_groups.size(), 1u);
  const auto& origin_c_group = origin_c_groups[0].interest_group;
  EXPECT_EQ(origin_c_group.name, kInterestGroupName);
  ASSERT_TRUE(origin_c_group.ads.has_value());
  ASSERT_EQ(origin_c_group.ads->size(), 1u);
  EXPECT_EQ(origin_c_group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(origin_c_group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");

  // ...but the `kOriginA` interest group shouldn't change.
  std::vector<StorageInterestGroup> origin_a_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(origin_a_groups.size(), 1u);
  const auto& origin_a_group = origin_a_groups[0].interest_group;
  ASSERT_TRUE(origin_a_group.ads.has_value());
  ASSERT_EQ(origin_a_group.ads->size(), 1u);
  EXPECT_EQ(origin_a_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(origin_a_group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Now try on disallowed subframe from originB.
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlB, subframe);
  interest_group = CreateInterestGroup();
  interest_group.owner = kOriginB;
  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  // Subframes from origin B with a top frame of A should not (by policy) be
  // allowed to update groups with B as the owner.
  std::vector<StorageInterestGroup> origin_b_groups =
      GetInterestGroupsForOwner(kOriginB);
  ASSERT_EQ(origin_b_groups.size(), 1u);
  const auto& origin_b_group = origin_b_groups[0].interest_group;
  ASSERT_TRUE(origin_b_group.ads.has_value());
  ASSERT_EQ(origin_b_group.ads->size(), 1u);
  EXPECT_EQ(origin_b_group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(origin_b_group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// The `ads` field is valid, but the ad `renderUrl` field is an invalid
// URL. The entire update should get cancelled, since updates are atomic.
TEST_F(AdAuctionServiceImplTest, UpdateInvalidFieldCancelsAllUpdates) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"ads": [{"renderUrl": "https://invalid^&",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// The server response can't be parsed as valid JSON. The update is cancelled.
TEST_F(AdAuctionServiceImplTest, UpdateInvalidJSONIgnored) {
  network_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                             "This isn't JSON.");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// UpdateJSONParserCrash fails on Android because Android doesn't use a separate
// process to parse JSON -- instead, it validates JSON in-process in Java, then,
// if validation succeeded, uses the C++ JSON parser, also in-proc. On other
// platforms, the C++ parser runs out-of-proc for safety.
#if !BUILDFLAG(IS_ANDROID)

// The server response is valid, but we simulate the JSON parser (which may
// run in a separate process) crashing, so the update doesn't happen.
TEST_F(AdAuctionServiceImplTest, UpdateJSONParserCrash) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Simulate the JSON service crashing instead of returning a result.
  data_decoder::test::InProcessDataDecoder in_process_data_decoder;
  in_process_data_decoder.service().SimulateJsonParserCrashForTesting(
      /*drop=*/true);

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

#endif  // !BUILDFLAG(IS_ANDROID)

// The network request fails (not implemented), so the update is cancelled.
TEST_F(AdAuctionServiceImplTest, UpdateNetworkFailure) {
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUrlA.Resolve("no_handler.json");
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// The network request for updating interest groups times out, so the update
// fails.
TEST_F(AdAuctionServiceImplTest, UpdateTimeout) {
  network_responder_->RegisterDeferredUpdateResponse(kDailyUpdateUrlPath);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(base::Seconds(30) + base::Seconds(1));
  task_environment()->RunUntilIdle();

  // The request times out (ERR_TIMED_OUT), so the ads should not change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Start an update, and delay the server response so that the interest group
// expires before the interest group updates. Don't advance time enough for DB
// maintenance tasks to run -- that is the interest group will only exist on
// disk in an expired state, and not appear in queries.
TEST_F(AdAuctionServiceImplTest,
       UpdateDuringInterestGroupExpirationNoDbMaintenence) {
  const std::string kServerResponse = base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}}]
})",
                                                         kOriginStringA);
  network_responder_->RegisterDeferredUpdateResponse(kDailyUpdateUrlPath);

  // Make the interest group expire before the DB maintenance task should be
  // run, with a gap second where expiration has happened, but DB maintenance
  // has not. Time order:
  // (*NOW*, group expiration, db maintenance).
  const base::TimeDelta kExpiryDelta =
      InterestGroupStorage::kIdlePeriod - base::Seconds(2);
  ASSERT_GT(kExpiryDelta, base::Seconds(0));
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + kExpiryDelta;
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Start an interest group update and then advance time to ensure the interest
  // group expires before a response is returned.
  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(kExpiryDelta + base::Seconds(1));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Due to FastForwardBy(), we're at this time order:
  // (group expiration, *NOW*, db maintenance).
  // So, DB maintenance should not have been run.
  base::RunLoop run_loop;
  manager_->GetLastMaintenanceTimeForTesting(
      base::BindLambdaForTesting([&run_loop](base::Time time) {
        EXPECT_EQ(time, base::Time::Min());
        run_loop.Quit();
      }));
  run_loop.Run();

  // Now return the server response. The interest group shouldn't change as it's
  // expired.
  network_responder_->DoDeferredUpdateResponse(kServerResponse);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Updating again when the interest group has been deleted shouldn't somehow
  // bring it back -- also, advance past the rate limit window to ensure the
  // update actually happens.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod + base::Seconds(1));
  network_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                             kServerResponse);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // DB maintenance never occurs since we never FastForward() past db
  // maintenance. We still are at time order:
  // (group expiration, *NOW*, db maintenance).
}

// Start an update, and delay the server response so that the interest group
// expires before the interest group updates. Advance time enough for DB
// maintenance tasks to run -- that is the interest group will be deleted from
// the database.
TEST_F(AdAuctionServiceImplTest,
       UpdateDuringInterestGroupExpirationWithDbMaintenence) {
  const std::string kServerResponse = base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}}]
})",
                                                         kOriginStringA);
  network_responder_->RegisterDeferredUpdateResponse(kDailyUpdateUrlPath);

  // Make the interest group expire just before the DB maintenance task should
  // be run. Time order:
  // (*NOW*, group expiration, db maintenance).
  const base::Time now = base::Time::Now();
  const base::TimeDelta kExpiryDelta =
      InterestGroupStorage::kIdlePeriod - base::Seconds(1);
  ASSERT_GT(kExpiryDelta, base::Seconds(0));
  const base::Time next_maintenance_time =
      now + InterestGroupStorage::kIdlePeriod;
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = now + kExpiryDelta;
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Start an interest group update and then advance time to ensure the interest
  // group expires and then DB maintenance is performed, both before a response
  // is returned.
  UpdateInterestGroupNoFlush();
  task_environment()->FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                    base::Seconds(1));
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Due to FastForwardBy(), we're at this time order:
  // (group expiration, db maintenance, *NOW*).
  // So, DB maintenance should have been run.
  base::RunLoop run_loop;
  manager_->GetLastMaintenanceTimeForTesting(base::BindLambdaForTesting(
      [&run_loop, next_maintenance_time](base::Time time) {
        EXPECT_EQ(time, next_maintenance_time);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Now return the server response. The interest group shouldn't change as it's
  // expired.
  network_responder_->DoDeferredUpdateResponse(kServerResponse);
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());

  // Updating again when the interest group has been deleted shouldn't somehow
  // bring it back -- also, advance past the rate limit window to ensure the
  // update actually happens.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod + base::Seconds(1));
  network_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                             kServerResponse);
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0u, GetInterestGroupsForOwner(kOriginA).size());
}

// The update doesn't happen because the update URL isn't specified at
// Join() time.
TEST_F(AdAuctionServiceImplTest, DoesntChangeGroupsWithNoUpdateUrl) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // Check that the ads didn't change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");
}

// Register a bid and a win, then perform a successful update. The bid and win
// stats shouldn't change.
TEST_F(AdAuctionServiceImplTest, UpdateDoesntChangeBrowserSignals) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  interest_group.trusted_bidding_signals_url = kTrustedBiddingSignalsUrlA;
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->push_back("key1");
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Register 2 bids and a win.
  manager_->RecordInterestGroupBid(kOriginA, kInterestGroupName);
  manager_->RecordInterestGroupBid(kOriginA, kInterestGroupName);
  manager_->RecordInterestGroupWin(kOriginA, kInterestGroupName, "{}");

  std::vector<StorageInterestGroup> prev_groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(prev_groups.size(), 1u);
  const auto& prev_signals = prev_groups[0].bidding_browser_signals;
  EXPECT_EQ(prev_signals->join_count, 1);
  EXPECT_EQ(prev_signals->bid_count, 2);
  EXPECT_EQ(prev_signals->prev_wins.size(), 1u);

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The group updates, but the signals don't.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  const auto& signals = groups[0].bidding_browser_signals;

  EXPECT_EQ(signals->join_count, 1);
  EXPECT_EQ(signals->bid_count, 2);
  EXPECT_EQ(signals->prev_wins.size(), 1u);

  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Join an interest group.
// Update interest group successfully.
// Change update response to different value.
// Update attempt does nothing (rate limited).
// Advance to just before time limit drops, update does nothing (rate limited).
// Advance after time limit. Update should work.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedAfterSuccessfulUpdate) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update completes successfully.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/newer_ad_render_url",
         "metadata": {"newer_a": "b"}
        }]
})",
                                              kOriginStringA));
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group2.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group2.ads.value()[0].metadata, "{\"new_a\":\"b\"}");

  // Advance time to just before end of rate limit period. Update should still
  // do nothing due to rate limiting.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod - base::Seconds(1));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups3 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups3.size(), 1u);
  const auto& group3 = groups3[0].interest_group;
  ASSERT_TRUE(group3.ads.has_value());
  ASSERT_EQ(group3.ads->size(), 1u);
  EXPECT_EQ(group3.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group3.ads.value()[0].metadata, "{\"new_a\":\"b\"}");

  // Advance time to just after end of rate limit period. Update should now
  // succeed.
  task_environment()->FastForwardBy(base::Seconds(2));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents.
  std::vector<StorageInterestGroup> groups4 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups4.size(), 1u);
  const auto& group4 = groups4[0].interest_group;
  ASSERT_TRUE(group4.ads.has_value());
  ASSERT_EQ(group4.ads->size(), 1u);
  EXPECT_EQ(group4.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/newer_ad_render_url", kOriginStringA));
  EXPECT_EQ(group4.ads.value()[0].metadata, "{\"newer_a\":\"b\"}");
}

// Join an interest group.
// Set up update to fail (return invalid server response).
// Update interest group fails.
// Change update response to different value that will succeed.
// Update does nothing (rate limited).
// Advance to just before rate limit drops (which for bad response is the longer
// "successful" duration), update does nothing (rate limited).
// Advance after time limit. Update should work.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedAfterBadUpdateResponse) {
  network_responder_->RegisterUpdateResponse(kDailyUpdateUrlPath,
                                             "This isn't JSON.");

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update fails, nothing changes.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Advance time to just before end of rate limit period. Update should still
  // do nothing due to rate limiting. Invalid responses use the longer
  // "successful" backoff period.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateSucceededBackoffPeriod - base::Seconds(1));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups3 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups3.size(), 1u);
  const auto& group3 = groups3[0].interest_group;
  ASSERT_TRUE(group3.ads.has_value());
  ASSERT_EQ(group3.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Advance time to just after end of rate limit period. Update should now
  // succeed.
  task_environment()->FastForwardBy(base::Seconds(2));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents.
  std::vector<StorageInterestGroup> groups4 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups4.size(), 1u);
  const auto& group4 = groups4[0].interest_group;
  ASSERT_TRUE(group4.ads.has_value());
  ASSERT_EQ(group4.ads->size(), 1u);
  EXPECT_EQ(group4.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group4.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Join an interest group.
// Make interest group update fail with net::ERR_CONNECTION_RESET.
// Update interest group fails.
// Change update response to succeed.
// Update does nothing (rate limited).
// Advance to just before rate limit drops, update does nothing (rate limited).
// Advance after time limit. Update should work.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedAfterFailedUpdate) {
  network_responder_->FailNextUpdateRequestWithError(net::ERR_CONNECTION_RESET);

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update fails, nothing changes.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Advance time to just before end of rate limit period. Update should still
  // do nothing due to rate limiting.
  task_environment()->FastForwardBy(
      InterestGroupStorage::kUpdateFailedBackoffPeriod - base::Seconds(1));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update does nothing due to rate limiting, nothing changes.
  std::vector<StorageInterestGroup> groups3 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups3.size(), 1u);
  const auto& group3 = groups3[0].interest_group;
  ASSERT_TRUE(group3.ads.has_value());
  ASSERT_EQ(group3.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Advance time to just after end of rate limit period. Update should now
  // succeed.
  task_environment()->FastForwardBy(base::Seconds(2));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents.
  std::vector<StorageInterestGroup> groups4 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups4.size(), 1u);
  const auto& group4 = groups4[0].interest_group;
  ASSERT_TRUE(group4.ads.has_value());
  ASSERT_EQ(group4.ads->size(), 1u);
  EXPECT_EQ(group4.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group4.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// net::ERR_INTERNET_DISCONNECTED skips rate limiting, unlike other errors.
//
// Join an interest group.
// Make interest group update fail with net::ERR_INTERNET_DISCONNECTED.
// Update interest group fails.
// Change update response to different value that will succeed.
// Update succeeds (not rate limited).
TEST_F(AdAuctionServiceImplTest, UpdateNotRateLimitedIfDisconnected) {
  network_responder_->FailNextUpdateRequestWithError(
      net::ERR_INTERNET_DISCONNECTED);

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The first update fails (internet disconnected), nothing changes.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            "https://example.com/render");
  EXPECT_EQ(group.ads.value()[0].metadata,
            "{\"ad\":\"metadata\",\"here\":[1,2,3]}");

  // Change the update response and try updating again.
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));
  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  // The update changes the database contents -- no rate limiting occurs.
  std::vector<StorageInterestGroup> groups2 =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups2.size(), 1u);
  const auto& group2 = groups2[0].interest_group;
  ASSERT_TRUE(group2.ads.has_value());
  ASSERT_EQ(group2.ads->size(), 1u);
  EXPECT_EQ(group2.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group2.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Fire off many updates rapidly in a loop. Only one update should happen.
TEST_F(AdAuctionServiceImplTest, UpdateRateLimitedTightLoop) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              kOriginStringA));

  blink::InterestGroup interest_group = CreateInterestGroup();
  // Set a long expiration delta so that we can advance to the next rate limit
  // period without the interest group expiring.
  interest_group.expiry = base::Time::Now() + base::Days(30);
  interest_group.update_url = kUpdateUrlA;
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  ad.metadata = "{\"ad\":\"metadata\",\"here\":[1,2,3]}";
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  EXPECT_EQ(network_responder_->UpdateCount(), 0u);

  for (size_t i = 0; i < 1000u; i++) {
    UpdateInterestGroupNoFlush();
  }
  task_environment()->RunUntilIdle();

  EXPECT_EQ(network_responder_->UpdateCount(), 1u);

  // One of the updates completes successfully.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  ASSERT_TRUE(group.ads.has_value());
  ASSERT_EQ(group.ads->size(), 1u);
  EXPECT_EQ(group.ads.value()[0].render_url.spec(),
            base::StringPrintf("%s/new_ad_render_url", kOriginStringA));
  EXPECT_EQ(group.ads.value()[0].metadata, "{\"new_a\":\"b\"}");
}

// Add an interest group, and run an ad auction.
TEST_F(AdAuctionServiceImplTest, RunAdAuction) {
  constexpr char kBiddingScript[] = R"(
function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  auto auction_config = blink::mojom::AuctionAdConfig::New();
  auction_config->seller = kOriginA;
  auction_config->decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  auction_config->auction_ad_config_non_shared_params->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});
  absl::optional<GURL> auction_result =
      RunAdAuctionAndFlush(std::move(auction_config));
  ASSERT_NE(auction_result, absl::nullopt);
  EXPECT_EQ(ConvertFencedFrameURNToURL(*auction_result),
            GURL("https://example.com/render"));
}

TEST_F(AdAuctionServiceImplTest, FetchReport) {
  const std::string kBiddingScript = base::StringPrintf(R"(
function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin(
  auctionSignals, perBuyerSignals, sellerSignals, browserSignals) {
  sendReportTo('%s/report_bidder');
}
  )",
                                                        kOriginStringA);

  const std::string kDecisionScript =
      base::StringPrintf(R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult(auctionConfig, browserSignals) {
  sendReportTo('%s/report_seller');
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': '%s/report_seller',
  };
}
)",
                         kOriginStringA, kOriginStringA);

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterReportResponse("/report_bidder", "");
  network_responder_->RegisterStoreUrlLoaderClient("/report_seller");

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  auto auction_config = blink::mojom::AuctionAdConfig::New();
  auction_config->seller = kOriginA;
  auction_config->decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  auction_config->auction_ad_config_non_shared_params->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});
  absl::optional<GURL> auction_result =
      RunAdAuctionAndFlush(std::move(auction_config));
  EXPECT_NE(auction_result, absl::nullopt);

  task_environment()->FastForwardBy(base::Seconds(30) - base::Seconds(1));
  // There should be two reports, one for winning bidder and one for seller.
  EXPECT_EQ(network_responder_->ReportCount(), 2u);
  // The request to seller report url should hang before 30s.
  EXPECT_TRUE(network_responder_->RemoteIsConnected());
  task_environment()->FastForwardBy(base::Seconds(2));
  // The request to seller report url should be disconnected after 30s due to
  // timeout.
  EXPECT_FALSE(network_responder_->RemoteIsConnected());
}

// Run several auctions, some of which have a winner, and some of which do
// not. Verify that the auction result UMA is recorded correctly.
TEST_F(AdAuctionServiceImplTest,
       AddInterestGroupRunAuctionVerifyResultMetrics) {
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Run 7 auctions, with delays:
  //
  // succeed, (1s), fail, (3s), succeed, (1m), succeed, (10m) succeed, (30m)
  // fail, (1h), fail, which in bits (with an extra leading 1) is 0b1101110 --
  // the last failure isn't recorded in the bitfield, since only the first 6
  // auctions get recorded in the bitfield.

  // Expect*TimeSample() doesn't accept base::TimeDelta::Max(), but the max time
  // bucket size is 1 hour, so specifying kMaxTime will select the max bucket.
  constexpr base::TimeDelta kMaxTime{base::Days(1)};

  auto succeed_auction_config = blink::mojom::AuctionAdConfig::New();
  succeed_auction_config->seller = kOriginA;
  succeed_auction_config->decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  succeed_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  auto fail_auction_config = blink::mojom::AuctionAdConfig::New();
  fail_auction_config->seller = kOriginA;
  fail_auction_config->decision_logic_url =
      kUrlA.Resolve(kDecisionFailAllUrlPath);
  fail_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  fail_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  // 1st auction
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
            absl::nullopt);
  // Time metrics are published every auction.
  histogram_tester.ExpectUniqueTimeSample(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 1);

  // 2nd auction
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config->Clone()), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(1),
      1);

  // 3rd auction
  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
            absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(3),
      1);

  // 4th auction
  task_environment()->FastForwardBy(base::Minutes(1));
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
            absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Minutes(1),
      1);

  // 5th auction
  task_environment()->FastForwardBy(base::Minutes(10));
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
            absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage",
      base::Minutes(10), 1);

  // 6th auction
  task_environment()->FastForwardBy(base::Minutes(30));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config->Clone()), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage",
      base::Minutes(30), 1);

  // 7th auction
  task_environment()->FastForwardBy(base::Hours(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config->Clone()), absl::nullopt);
  // Since the 1st auction has no prior auction -- it gets put in the same
  // bucket with the 7th auction -- there are 2 auctions now in this bucket.
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 2);

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 7, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 4 * 100 / 7,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b1101110, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 0, 1);
}

// Like AddInterestGroupRunAuctionVerifyResultMetrics, but with a smaller number
// of auctions -- this verifies that metrics (especially the bit metrics) are
// reported correctly in this scenario.
TEST_F(AdAuctionServiceImplTest,
       AddInterestGroupRunAuctionVerifyResultMetricsFewAuctions) {
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Run 2 auctions, with delays:
  //
  // succeed, (1s), fail, which in bits (with an extra leading 1) is 0b110.

  // Expect*TimeSample() doesn't accept base::TimeDelta::Max(), but the max time
  // bucket size is 1 hour, so specifying kMaxTime will select the max bucket.
  constexpr base::TimeDelta kMaxTime{base::Days(1)};

  auto succeed_auction_config = blink::mojom::AuctionAdConfig::New();
  succeed_auction_config->seller = kOriginA;
  succeed_auction_config->decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  succeed_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  auto fail_auction_config = blink::mojom::AuctionAdConfig::New();
  fail_auction_config->seller = kOriginA;
  fail_auction_config->decision_logic_url =
      kUrlA.Resolve(kDecisionFailAllUrlPath);
  fail_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  fail_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  // 1st auction
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
            absl::nullopt);
  // Time metrics are published every auction.
  histogram_tester.ExpectUniqueTimeSample(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 1);

  // 2nd auction
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config->Clone()), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(1),
      1);

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 1 * 100 / 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b110, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 0, 1);
}

// Like AddInterestGroupRunAuctionVerifyResultMetricsFewAuctions, but with no
// auctions.
TEST_F(AdAuctionServiceImplTest,
       AddInterestGroupRunAuctionVerifyResultMetricsNoAuctions) {
  base::HistogramTester histogram_tester;

  // Don't run any auctions.

  // Navigate to "populate" remaining metrics.
  DeleteContents();

  // Nothing gets reported since there were no auctions.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(histogram_tester
                .GetAllSamples(
                    "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);
}

// The feature parameter that controls the interest group limit should default
// to off. We both check the parameter is off, and we run a number of auctions
// and make sure they all succeed.
TEST_F(AdAuctionServiceImplTest, NoInterestLimitByDefault) {
  EXPECT_FALSE(base::FeatureList::IsEnabled(features::kFledgeLimitNumAuctions));
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  constexpr int kNumAuctions = 10;
  // Run kNumAuctions auctions, all should succeed since there's no limit:
  auto succeed_auction_config = blink::mojom::AuctionAdConfig::New();
  succeed_auction_config->seller = kOriginA;
  succeed_auction_config->decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  succeed_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  for (int i = 0; i < kNumAuctions; i++) {
    EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
              absl::nullopt);
  }

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  // Every auction succeeds, none are skipped.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", kNumAuctions, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 100, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b1111111, 1);
  // However, we do record that the auction was skipped.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 0, 1);
}

class AdAuctionServiceImplNumAuctionLimitTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplNumAuctionLimitTest() {
    // Only 2 auctions are allowed per-page.
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kFledgeLimitNumAuctions, {{"max_auctions_per_page", "2"}});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Like AddInterestGroupRunAuctionVerifyResultMetrics, but with enforcement
// limiting the number of auctions.
TEST_F(AdAuctionServiceImplNumAuctionLimitTest,
       AddInterestGroupRunAuctionWithNumAuctionLimits) {
  base::HistogramTester histogram_tester;
  constexpr char kDecisionFailAllUrlPath[] =
      "/interest_group/decision_logic_fail_all.js";

  constexpr char kBiddingScript[] = R"(
function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  constexpr char kDecisionScriptFailAll[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return 0;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);
  network_responder_->RegisterScriptResponse(kDecisionFailAllUrlPath,
                                             kDecisionScriptFailAll);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Run 3 auctions, with delays:
  //
  // succeed, (1s), fail, (3s), succeed which in bits (with an extra leading 1)
  // is 0b110 -- the last success isn't recorded since the auction limit is
  // enforced.

  // Expect*TimeSample() doesn't accept base::TimeDelta::Max(), but the max time
  // bucket size is 1 hour, so specifying kMaxTime will select the max bucket.
  constexpr base::TimeDelta kMaxTime{base::Days(1)};

  auto succeed_auction_config = blink::mojom::AuctionAdConfig::New();
  succeed_auction_config->seller = kOriginA;
  succeed_auction_config->decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  succeed_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  auto fail_auction_config = blink::mojom::AuctionAdConfig::New();
  fail_auction_config->seller = kOriginA;
  fail_auction_config->decision_logic_url =
      kUrlA.Resolve(kDecisionFailAllUrlPath);
  fail_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  fail_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  // 1st auction
  EXPECT_NE(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
            absl::nullopt);
  // Time metrics are published every auction.
  histogram_tester.ExpectUniqueTimeSample(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", kMaxTime, 1);

  // 2nd auction
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(RunAdAuctionAndFlush(fail_auction_config->Clone()), absl::nullopt);
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(1),
      1);

  // 3rd auction -- fails even though decision_logic.js is used because the
  // auction limit is encountered.
  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_EQ(RunAdAuctionAndFlush(succeed_auction_config->Clone()),
            absl::nullopt);
  // The time metrics shouldn't get updated.
  histogram_tester.ExpectTimeBucketCount(
      "Ads.InterestGroup.Auction.TimeSinceLastAuctionPerPage", base::Seconds(3),
      0);

  // Some metrics only get reported until after navigation.
  EXPECT_EQ(histogram_tester
                .GetAllSamples("Ads.InterestGroup.Auction.NumAuctionsPerPage")
                .size(),
            0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples("Ads.InterestGroup.Auction.First6AuctionsBitsPerPage")
          .size(),
      0u);
  EXPECT_EQ(
      histogram_tester
          .GetAllSamples(
              "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit")
          .size(),
      0u);

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  // The last auction doesn't count towards these metrics since the auction
  // limit is enforced -- this is because that auction doesn't contribute any
  // knowledge about stored interest groups to the page.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 1 * 100 / 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b110, 1);
  // However, we do record that the auction was skipped.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit", 1, 1);
}

TEST_F(AdAuctionServiceImplNumAuctionLimitTest,
       AddInterestGroupRunAuctionStartManyAuctionsInParallel) {
  base::HistogramTester histogram_tester;

  constexpr char kBiddingScript[] = R"(
function generateBid(
  interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
  browserSignals) {
  return {'ad': 'example', 'bid': 1, 'render': 'https://example.com/render'};
}
function reportWin() {}
)";

  constexpr char kDecisionScript[] = R"(
function scoreAd(
  adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  return bid;
}
function reportResult() {}
)";

  network_responder_->RegisterScriptResponse(kBiddingUrlPath, kBiddingScript);
  network_responder_->RegisterScriptResponse(kDecisionUrlPath, kDecisionScript);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now() + base::Days(10);
  interest_group.bidding_url = kUrlA.Resolve(kBiddingUrlPath);
  interest_group.ads.emplace();
  blink::InterestGroup::Ad ad;
  ad.render_url = GURL("https://example.com/render");
  interest_group.ads->emplace_back(std::move(ad));
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  auto succeed_auction_config = blink::mojom::AuctionAdConfig::New();
  succeed_auction_config->seller = kOriginA;
  succeed_auction_config->decision_logic_url = kUrlA.Resolve(kDecisionUrlPath);
  succeed_auction_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  succeed_auction_config->auction_ad_config_non_shared_params
      ->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers({kOriginA});

  // Pick some large number, larger than the auction limit.
  constexpr int kNumAuctions = 10;
  base::RunLoop run_loop;
  mojo::Remote<blink::mojom::AdAuctionService> interest_service;
  AdAuctionServiceImpl::CreateMojoService(
      main_rfh(), interest_service.BindNewPipeAndPassReceiver());
  base::RepeatingClosure one_auction_complete =
      base::BarrierClosure(kNumAuctions, run_loop.QuitClosure());

  for (int i = 0; i < kNumAuctions; i++) {
    interest_service->RunAdAuction(
        succeed_auction_config->Clone(),
        base::BindLambdaForTesting(
            [&one_auction_complete](
                const absl::optional<GURL>& ignored_result) {
              one_auction_complete.Run();
            }));
  }
  run_loop.Run();

  // DeleteContents() to force-populate remaining metrics.
  DeleteContents();

  // Only the first 2 auctions should have succeeded -- the others should fail.
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsPerPage", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.PercentAuctionsSuccessfulPerPage", 2 * 100 / 2,
      1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.First6AuctionsBitsPerPage", 0b111, 1);
  histogram_tester.ExpectUniqueSample(
      "Ads.InterestGroup.Auction.NumAuctionsSkippedDueToAuctionLimit",
      kNumAuctions - 2, 1);
}

class AdAuctionServiceImplRestrictedPermissionsPolicyTest
    : public AdAuctionServiceImplTest {
 public:
  AdAuctionServiceImplRestrictedPermissionsPolicyTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault);
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Permissions policy feature join-ad-interest-group is enabled by default for
// top level frames under restricted permissions policy, so interest group
// APIs should succeed.
TEST_F(AdAuctionServiceImplRestrictedPermissionsPolicyTest,
       APICallsFromTopFrame) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath,
      base::StringPrintf(R"({"biddingLogicUrl": "%s%s"})", kOriginStringA,
                         kNewBiddingUrlPath));
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  JoinInterestGroupAndFlush(interest_group);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlush();
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s%s", kOriginStringA, kNewBiddingUrlPath));

  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Like APICallsFromTopFrame, but API calls happens in a same site iframe
// instead of a top frame.
TEST_F(AdAuctionServiceImplRestrictedPermissionsPolicyTest,
       APICallsFromSameSiteIframe) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath,
      base::StringPrintf(R"({"biddingLogicUrl": "%s%s"})", kOriginStringA,
                         kNewBiddingUrlPath));
  // Create a same site subframe and use it to send the interest group requests.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlA, subframe);

  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = kUpdateUrlA;
  interest_group.bidding_url = kBiddingLogicUrlA;
  JoinInterestGroupAndFlushForFrame(std::move(interest_group), subframe);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginA);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s%s", kOriginStringA, kNewBiddingUrlPath));

  LeaveInterestGroupAndFlushForFrame(kOriginA, kInterestGroupName, subframe);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Permissions policy feature join-ad-interest-group is disabled by default for
// cross site iframes under restricted permissions policy, so interest group
// APIs should not work.
TEST_F(AdAuctionServiceImplRestrictedPermissionsPolicyTest,
       APICallsFromCrossSiteIFrame) {
  network_responder_->RegisterUpdateResponse(
      kDailyUpdateUrlPath,
      base::StringPrintf(R"({"biddingLogicUrl": "%s%s"})", kOriginStringC,
                         kNewBiddingUrlPath));

  NavigateAndCommit(kUrlC);
  blink::InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = kOriginC;
  interest_group.bidding_url = kUrlC.Resolve(kBiddingUrlPath);
  JoinInterestGroupAndFlush(interest_group);

  NavigateAndCommit(kUrlA);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));

  // Create a cross site subframe and use it to send interest group requests.
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(main_rfh());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  subframe =
      NavigationSimulator::NavigateAndCommitFromDocument(kUrlC, subframe);
  blink::InterestGroup interest_group_2 = CreateInterestGroup();
  constexpr char kInterestGroupName2[] = "group2";
  interest_group.owner = kOriginC;
  interest_group.name = kInterestGroupName2;
  JoinInterestGroupAndFlushForFrame(std::move(interest_group_2), subframe);
  EXPECT_EQ(0, GetJoinCount(kOriginC, kInterestGroupName2));

  UpdateInterestGroupNoFlushForFrame(subframe);
  task_environment()->RunUntilIdle();

  // `bidding_url` should not change.
  std::vector<StorageInterestGroup> groups =
      GetInterestGroupsForOwner(kOriginC);
  ASSERT_EQ(groups.size(), 1u);
  const auto& group = groups[0].interest_group;
  EXPECT_EQ(group.name, kInterestGroupName);
  ASSERT_TRUE(group.bidding_url.has_value());
  EXPECT_EQ(group.bidding_url->spec(),
            base::StringPrintf("%s%s", kOriginStringC, kBiddingUrlPath));

  LeaveInterestGroupAndFlushForFrame(kOriginC, kInterestGroupName, subframe);
  EXPECT_EQ(1, GetJoinCount(kOriginC, kInterestGroupName));
}

// CreateAdRequest should reject if we have an empty config.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsEmptyConfigRequest) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// CreateAdRequest should reject if we have an otherwise okay request but our
// request URL is not using HTTPS.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsHttpUrls) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  mojo_config->ad_request_url = GURL("http://site.test/");
  auto mojo_ad_properties = blink::mojom::AdProperties::New();
  mojo_ad_properties->width = "48";
  mojo_ad_properties->height = "64";
  mojo_ad_properties->slot = "123";
  mojo_ad_properties->lang = "en";
  mojo_ad_properties->ad_type = "test";
  mojo_ad_properties->bid_floor = 1.0;
  mojo_config->ad_properties.push_back(std::move(mojo_ad_properties));

  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// CreateAdRequest should reject if we have an otherwise okay request but no ad
// properties.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsMissingAds) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  mojo_config->ad_request_url = GURL("https://site.test/");

  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// CreateAdRequest should reject if we have an otherwise okay request but
// include an HTTP fallback URL.
TEST_F(AdAuctionServiceImplTest, CreateAdRequestRejectsHttpFallback) {
  auto mojo_config = blink::mojom::AdRequestConfig::New();
  mojo_config->ad_request_url = GURL("https://site.test/");
  auto mojo_ad_properties = blink::mojom::AdProperties::New();
  mojo_ad_properties->width = "48";
  mojo_ad_properties->height = "64";
  mojo_ad_properties->slot = "123";
  mojo_ad_properties->lang = "en";
  mojo_ad_properties->ad_type = "test";
  mojo_ad_properties->bid_floor = 1.0;
  mojo_config->ad_properties.push_back(std::move(mojo_ad_properties));

  mojo_config->fallback_source = GURL("http://fallback_site.test/");

  bool callback_fired = false;
  CreateAdRequest(std::move(mojo_config),
                  base::BindLambdaForTesting(
                      [&](const absl::optional<std::string>& ads_guid) {
                        ASSERT_FALSE(ads_guid.has_value());
                        callback_fired = true;
                      }));
  ASSERT_TRUE(callback_fired);
}

// An empty config will cause FinalizeAd to fail and run the supplied callback.
TEST_F(AdAuctionServiceImplTest, FinalizeAdRejectsEmptyConfig) {
  auto mojo_config = blink::mojom::AuctionAdConfig::New();
  mojo_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();

  bool callback_fired = false;
  FinalizeAd(
      /*guid=*/std::string("1234"), std::move(mojo_config),
      base::BindLambdaForTesting([&](const absl::optional<GURL>& creative_url) {
        ASSERT_FALSE(creative_url.has_value());
        callback_fired = true;
      }));
  ASSERT_TRUE(callback_fired);
}

TEST_F(AdAuctionServiceImplTest, FinalizeAdRejectsHTTPDecisionUrl) {
  auto mojo_config = blink::mojom::AuctionAdConfig::New();
  mojo_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  mojo_config->seller = url::Origin::Create(GURL("https://site.test"));
  mojo_config->decision_logic_url = GURL("http://site.test/");

  bool callback_fired = false;
  FinalizeAd(
      /*guid=*/"1234", std::move(mojo_config),
      base::BindLambdaForTesting([&](const absl::optional<GURL>& creative_url) {
        ASSERT_FALSE(creative_url.has_value());
        callback_fired = true;
      }));
  ASSERT_TRUE(callback_fired);
}

// An empty GUID should trigger any FinalizeAd request to fail.
TEST_F(AdAuctionServiceImplTest, FinalizeAdRejectsMissingGuid) {
  auto mojo_config = blink::mojom::AuctionAdConfig::New();
  mojo_config->auction_ad_config_non_shared_params =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  mojo_config->seller = url::Origin::Create(GURL("https://site.test"));
  mojo_config->decision_logic_url = GURL("https://site.test/");

  bool callback_fired = false;
  FinalizeAd(
      /*guid=*/std::string(), std::move(mojo_config),
      base::BindLambdaForTesting([&](const absl::optional<GURL>& creative_url) {
        ASSERT_FALSE(creative_url.has_value());
        callback_fired = true;
      }));
  ASSERT_TRUE(callback_fired);
}

}  // namespace content
