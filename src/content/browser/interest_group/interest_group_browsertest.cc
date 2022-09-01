// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/renderer_host/page_impl.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/shell/browser/shell.h"
#include "content/test/fenced_frame_test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {

using ::testing::Eq;
using ::testing::Optional;

// Returned by test Javascript code when join or leave promises complete without
// throwing an exception.
const char kSuccess[] = "success";

// Creates string representations of ads and adComponents arrays from the
// provided InterestGroup::Ads.
std::string MakeAdsArg(const std::vector<blink::InterestGroup::Ad>& ads) {
  std::string out = "";
  for (const auto& ad : ads) {
    if (!out.empty())
      out += ",";
    if (ad.metadata) {
      // Since ad.metadata is JSON, it shouldn't be wrapped in quotes, so can't
      // use JsReplace.
      out += base::StringPrintf("{renderUrl : '%s', metadata: %s}",
                                ad.render_url.spec().c_str(),
                                ad.metadata->c_str());
    } else {
      out += JsReplace("{renderUrl : $1}", ad.render_url);
    }
  }
  return "[" + out + "]";
}

class AllowlistedOriginContentBrowserClient : public TestContentBrowserClient {
 public:
  explicit AllowlistedOriginContentBrowserClient() = default;

  AllowlistedOriginContentBrowserClient(
      const AllowlistedOriginContentBrowserClient&) = delete;
  AllowlistedOriginContentBrowserClient& operator=(
      const AllowlistedOriginContentBrowserClient&) = delete;

  void SetAllowList(base::flat_set<url::Origin>&& allow_list) {
    allow_list_ = allow_list;
  }

  void AddToAllowList(const std::vector<url::Origin>& add_to_allow_list) {
    allow_list_.insert(add_to_allow_list.begin(), add_to_allow_list.end());
  }

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(
      content::RenderFrameHost* render_frame_host,
      ContentBrowserClient::InterestGroupApiOperation operation,
      const url::Origin& top_frame_origin,
      const url::Origin& api_origin) override {
    return allow_list_.contains(top_frame_origin) &&
           allow_list_.contains(api_origin);
  }

 private:
  base::flat_set<url::Origin> allow_list_;
};

// A special path for updates that allows deferring the server response. Only
// update requests to this path can be deferred, because the path must be
// registered before the EmbeddedTestServer starts.
constexpr char kDeferredUpdateResponsePath[] =
    "/interest_group/daily_update_deferred.json";

constexpr char kFledgeHeader[] = "X-Allow-FLEDGE";

// Allows registering responses to network requests.
class NetworkResponder {
 public:
  explicit NetworkResponder(net::EmbeddedTestServer& server)
      : controllable_response_(&server, kDeferredUpdateResponsePath) {
    server.RegisterRequestHandler(base::BindRepeating(
        &NetworkResponder::RequestHandler, base::Unretained(this)));
  }

  NetworkResponder(const NetworkResponder&) = delete;
  NetworkResponder& operator=(const NetworkResponder&) = delete;

  void RegisterNetworkResponse(
      const std::string& url_path,
      const std::string& body,
      const std::string& mime_type = "application/json") {
    base::AutoLock auto_lock(response_map_lock_);
    Response response;
    response.body = body;
    response.mime_type = mime_type;
    response_map_[url_path] = response;
  }

  // Register a response that's a bidder script. Takes the body of the
  // generateBid() method.
  void RegisterBidderScript(const std::string& url_path,
                            const std::string& generate_bid_body) {
    std::string script = base::StringPrintf(R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
  %s
})",
                                            generate_bid_body.c_str());
    RegisterNetworkResponse(url_path, script, "application/javascript");
  }

  // Perform the deferred response -- the test hangs if the client isn't waiting
  // on a response to kDeferredUpdateResponsePath.
  void DoDeferredUpdateResponse(
      const std::string& response,
      const std::string& content_type = "application/json") {
    controllable_response_.WaitForRequest();
    controllable_response_.Send(net::HTTP_OK, content_type, response,
                                /*cookies=*/{},
                                /*extra_headers=*/{std::string(kFledgeHeader)});
    controllable_response_.Done();
  }

 private:
  struct Response {
    std::string body;
    std::string mime_type;
  };

  std::unique_ptr<net::test_server::HttpResponse> RequestHandler(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(response_map_lock_);
    const auto it = response_map_.find(request.GetURL().path());
    if (it == response_map_.end())
      return nullptr;
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader(kFledgeHeader, "true");
    response->set_code(net::HTTP_OK);
    response->set_content(it->second.body);
    response->set_content_type(it->second.mime_type);
    return std::move(response);
  }

  // EmbeddedTestServer RequestHandlers can't be added after the server has
  // started, but tests may want to specify network responses after the server
  // starts in the fixture. A handler is therefore registered that uses
  // `response_map_` to serve network responses.
  base::Lock response_map_lock_;

  // For each HTTPS request, we see if any path in the map matches the request
  // path. If so, the server returns the mapped value string as the response.
  base::flat_map<std::string, Response> response_map_
      GUARDED_BY(response_map_lock_);

  net::test_server::ControllableHttpResponse controllable_response_;
};

// Handle well-known requests. Frame origins are expected to be of the form
// "allow-join...", "allow-leave...", or "no-cors...".
std::unique_ptr<net::test_server::HttpResponse> HandleWellKnownRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url,
                        "/.well-known/interest-group/permissions/?origin=")) {
    return nullptr;
  }

  // .well-known requests should advertise they accept JSON responses.
  const auto accept_header =
      request.headers.find(net::HttpRequestHeaders::kAccept);
  DCHECK(accept_header != request.headers.end());
  EXPECT_EQ(accept_header->second, "application/json");

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("application/json");
  response->set_content("{}");

  const auto host_header = request.headers.find(net::HttpRequestHeaders::kHost);
  DCHECK(host_header != request.headers.end());
  if (base::StartsWith(host_header->second, "allow-join.")) {
    response->set_content(R"({"joinAdInterestGroup" : true})");
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  } else if (base::StartsWith(host_header->second, "allow-leave.")) {
    response->set_content(R"({"leaveAdInterestGroup" : true})");
    response->AddCustomHeader("Access-Control-Allow-Origin", "*");
  } else if (base::StartsWith(host_header->second, "no-cors.")) {
    response->set_content(
        R"({"joinAdInterestGroup" : true, "leaveAdInterestGroup" : true})");
  } else {
    NOTREACHED();
  }
  return response;
}

class InterestGroupTestObserver
    : public InterestGroupManagerImpl::InterestGroupObserverInterface {
 public:
  using Entry = std::tuple<
      InterestGroupManagerImpl::InterestGroupObserverInterface::AccessType,
      std::string,
      std::string>;
  void OnInterestGroupAccessed(
      const base::Time& access_time,
      InterestGroupManagerImpl::InterestGroupObserverInterface::AccessType type,
      const std::string& owner_origin,
      const std::string& name) override {
    accesses.emplace_back(Entry{type, owner_origin, name});

    if (run_loop_.running() && accesses.size() >= expected_.size()) {
      run_loop_.Quit();
    }
  }
  void WaitForAccesses(const std::vector<Entry>& expected) {
    if (accesses.size() < expected.size()) {
      expected_ = expected;
      run_loop_.Run();
    }
    EXPECT_EQ(accesses, expected);
  }
  std::vector<Entry> accesses;

 private:
  std::vector<Entry> expected_;
  base::RunLoop run_loop_;
};

class InterestGroupBrowserTest : public ContentBrowserTest {
 public:
  InterestGroupBrowserTest() {
    feature_list_.InitWithFeatures(
        /*`enabled_features`=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kParakeet,
         blink::features::kFledge, blink::features::kAllowURNsInIframes,
         blink::features::kBiddingAndScoringDebugReportingAPI},
        /*disabled_features=*/
        {blink::features::kFencedFrames});
  }

  ~InterestGroupBrowserTest() override {
    if (old_content_browser_client_)
      SetBrowserClientForTesting(old_content_browser_client_);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::test_server::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->RegisterRequestHandler(
        base::BindRepeating(&HandleWellKnownRequest));
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &InterestGroupBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    network_responder_ = std::make_unique<NetworkResponder>(*https_server_);
    ASSERT_TRUE(https_server_->Start());
    manager_ = static_cast<InterestGroupManagerImpl*>(
        shell()
            ->web_contents()
            ->GetBrowserContext()
            ->GetDefaultStoragePartition()
            ->GetInterestGroupManager());
    observer_ = std::make_unique<InterestGroupTestObserver>();
    content_browser_client_.SetAllowList(
        {https_server_->GetOrigin("a.test"), https_server_->GetOrigin("b.test"),
         https_server_->GetOrigin("c.test"),
         // Magic interest group origins used in cross-site join/leave tests.
         https_server_->GetOrigin("allow-join.a.test"),
         https_server_->GetOrigin("allow-leave.a.test"),
         https_server_->GetOrigin("no-cors.a.test"),
         // HTTP origins like those below aren't supported for FLEDGE -- some
         // tests verify that HTTP origins are rejected, even if somehow they
         // are allowed by the allowlist.
         https_server_->GetOrigin("a.test"), https_server_->GetOrigin("b.test"),
         https_server_->GetOrigin("c.test")});
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

  // Attempts to join the specified interest group. Returns kSuccess if the
  // operation claims to have succeeded, and the exception message on failure.
  //
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string JoinInterestGroup(
      url::Origin owner,
      std::string name,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  JsReplace(R"(
    (async function() {
      try {
        await navigator.joinAdInterestGroup(
          {name: $1, owner: $2}, /*joinDurationSec=*/ 300);
        return 'success';
      } catch (e) {
        return e.toString();
      }
    })())",
                            name, owner))
        .ExtractString();
  }

  // Just like JoinInterestGroup() above, but also verifies that the interest
  // group was joined or not, depending on the return value.
  [[nodiscard]] std::string JoinInterestGroupAndVerify(
      const url::Origin& owner,
      const std::string& name,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    int initial_count = GetJoinCount(owner, name);
    std::string result = JoinInterestGroup(owner, name, execution_target);
    int final_count = GetJoinCount(owner, name);
    if (result == kSuccess) {
      // On success, the user should have joined the interest group.
      EXPECT_EQ(initial_count + 1, final_count);
    } else {
      // On failure, nothing should have changed.
      EXPECT_EQ(initial_count, final_count);
    }

    return result;
  }

  // The `trusted_bidding_signals_keys` and `ads` fields of `group` will be
  // ignored in favor of the passed in values.
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string JoinInterestGroup(
      const blink::InterestGroup& group,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    // TODO(qingxin): Use base::Value to replace ostringstream.
    std::ostringstream buf;
    buf << "{"
        << "name: '" << group.name << "', "
        << "owner: '" << group.owner << "', "
        << "priority: '" << group.priority.value() << "'";
    if (group.bidding_url) {
      buf << ", biddingLogicUrl: '" << *group.bidding_url << "'";
    }
    if (group.bidding_wasm_helper_url) {
      buf << ", biddingWasmHelperUrl: '" << *group.bidding_wasm_helper_url
          << "'";
    }
    if (group.daily_update_url) {
      buf << ", dailyUpdateUrl: '" << *group.daily_update_url << "'";
    }
    if (group.trusted_bidding_signals_url) {
      buf << ", trustedBiddingSignalsUrl: '"
          << *group.trusted_bidding_signals_url << "'";
    }
    if (group.user_bidding_signals) {
      buf << ", userBiddingSignals: " << group.user_bidding_signals.value();
    }
    if (group.trusted_bidding_signals_keys) {
      buf << ", trustedBiddingSignalsKeys: [";
      for (size_t i = 0; i < group.trusted_bidding_signals_keys->size(); ++i) {
        if (i > 0)
          buf << ",";
        buf << "'" << (*group.trusted_bidding_signals_keys)[i] << "'";
      }
      buf << "]";
    }
    if (group.ads) {
      buf << ", ads: " << MakeAdsArg(*group.ads);
    }
    if (group.ad_components) {
      buf << ", adComponents: " << MakeAdsArg(*group.ad_components);
    }

    buf << "}";

    return EvalJs(execution_target ? *execution_target : shell(),
                  base::StringPrintf(R"(
    (async function() {
      try {
        await navigator.joinAdInterestGroup(
          %s, /*join_duration_sec=*/ 300);
        return 'success';
      } catch (e) {
        return e.toString();
      }
    })())",
                                     buf.str().c_str()))
        .ExtractString();
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  EvalJsResult UpdateInterestGroupsInJS(
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(), R"(
(function() {
  try {
    navigator.updateAdInterestGroups();
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())");
  }

  // Attempts to leave the specified interest group. Returns kSuccess if the
  // operation claims to have succeeded, and the exception message on failure.
  //
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string LeaveInterestGroup(
      url::Origin owner,
      std::string name,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  JsReplace(R"(
    (async function() {
      try {
        await navigator.leaveAdInterestGroup({name: $1, owner: $2});
        return 'success';
      } catch (e) {
        return e.toString();
      }
    })())",
                            name, owner))
        .ExtractString();
  }

  // Just like LeaveInterestGroupInJS(), but also verifies that the interest
  // group was left or not, depending on the return value.
  [[nodiscard]] std::string LeaveInterestGroupAndVerify(
      const url::Origin& owner,
      const std::string& name,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    int initial_count = GetJoinCount(owner, name);
    std::string result = LeaveInterestGroup(owner, name, execution_target);
    int final_count = GetJoinCount(owner, name);
    if (result == kSuccess) {
      // On success, the user should no longer be in the interest group.
      EXPECT_EQ(0, final_count);
    } else {
      // On failure, nothing should have changed.
      EXPECT_EQ(initial_count, final_count);
    }

    return result;
  }

  std::vector<url::Origin> GetAllInterestGroupsOwners() {
    std::vector<url::Origin> interest_group_owners;
    base::RunLoop run_loop;
    manager_->GetAllInterestGroupOwners(base::BindLambdaForTesting(
        [&run_loop, &interest_group_owners](std::vector<url::Origin> owners) {
          interest_group_owners = std::move(owners);
          run_loop.Quit();
        }));
    run_loop.Run();
    return interest_group_owners;
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

  std::vector<std::pair<url::Origin, std::string>> GetAllInterestGroups() {
    std::vector<std::pair<url::Origin, std::string>> interest_groups;
    for (const auto& owner : GetAllInterestGroupsOwners()) {
      for (const auto& storage_group : GetInterestGroupsForOwner(owner)) {
        interest_groups.emplace_back(storage_group.interest_group.owner,
                                     storage_group.interest_group.name);
      }
    }
    return interest_groups;
  }

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    for (const auto& storage_group : GetInterestGroupsForOwner(owner)) {
      if (storage_group.interest_group.name == name) {
        return storage_group.bidding_browser_signals->join_count;
      }
    }
    return 0;
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] std::string JoinInterestGroupAndVerify(
      const blink::InterestGroup& group,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    int initial_count = GetJoinCount(group.owner, group.name);
    std::string result = JoinInterestGroup(group, execution_target);
    if (result != kSuccess) {
      EXPECT_EQ(initial_count, GetJoinCount(group.owner, group.name));
    } else {
      EXPECT_EQ(initial_count + 1, GetJoinCount(group.owner, group.name));
    }

    return result;
  }

  // Simplified method to join an interest group for tests that only care about
  // a few fields.
  [[nodiscard]] std::string JoinInterestGroupAndVerify(
      const url::Origin& owner,
      const std::string& name,
      double priority,
      absl::optional<GURL> bidding_url = absl::nullopt,
      absl::optional<std::vector<blink::InterestGroup::Ad>> ads = absl::nullopt,
      absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components =
          absl::nullopt,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    return JoinInterestGroupAndVerify(
        blink::InterestGroup(
            /*expiry=*/base::Time(), owner, name, priority,
            std::move(bidding_url),
            /*bidding_wasm_helper_url=*/absl::nullopt,
            /*daily_update_url=*/absl::nullopt,
            /*trusted_bidding_signals_url=*/absl::nullopt,
            /*trusted_bidding_signals_keys=*/absl::nullopt,
            /*user_bidding_signals=*/absl::nullopt, std::move(ads),
            std::move(ad_components)),
        execution_target);
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] content::EvalJsResult RunAuctionAndWait(
      const std::string& auction_config_json,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  base::StringPrintf(
                      R"(
(async function() {
  try {
    return await navigator.runAdAuction(%s);
  } catch (e) {
    return e.toString();
  }
})())",
                      auction_config_json.c_str()));
  }

  // Wrapper around RunAuctionAndWait that assumes the result is a URN URL and
  // returns the mapped URL.
  [[nodiscard]] std::string RunAuctionAndWaitForURL(
      const std::string& auction_config_json,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    auto result = RunAuctionAndWait(auction_config_json, execution_target);
    GURL urn_url = GURL(result.ExtractString());
    EXPECT_TRUE(urn_url.is_valid());
    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_TRUE(observer.mapped_url()) << urn_url;
    return observer.mapped_url()->spec();
  }

  // Navigates an iframe with the id="test_iframe" to the provided URL and
  // checks that the last committed url is the expected url. There must only be
  // one iframe in the main document.
  void NavigateIframeAndCheckURL(WebContents* web_contents,
                                 const GURL& url,
                                 const GURL& expected_commit_url) {
    FrameTreeNode* parent = FrameTreeNode::From(web_contents->GetMainFrame());
    CHECK(parent->child_count() > 0u);
    FrameTreeNode* iframe = parent->child_at(0);
    TestFrameNavigationObserver nav_observer(iframe->current_frame_host());
    const std::string kIframeId = "test_iframe";
    EXPECT_TRUE(BeginNavigateIframeToURL(web_contents, kIframeId, url));
    nav_observer.Wait();
    EXPECT_EQ(expected_commit_url, nav_observer.last_committed_url());
    EXPECT_TRUE(nav_observer.last_navigation_succeeded());
  }

  // Wrapper around RunAuctionAndWait that assumes the result is a URN URL and
  // tries to navigate to it. Returns the mapped URL.
  void RunAuctionAndWaitForURLAndNavigateIframe(
      const std::string& auction_config_json,
      GURL expected_url) {
    auto result = RunAuctionAndWait(auction_config_json,
                                    /*execution_target=*/absl::nullopt);
    GURL urn_url = GURL(result.ExtractString());
    EXPECT_TRUE(urn_url.is_valid());
    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_TRUE(observer.mapped_url()) << urn_url;

    NavigateIframeAndCheckURL(web_contents(), urn_url, expected_url);
    EXPECT_EQ(expected_url, observer.mapped_url());
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] content::EvalJsResult CreateAdRequestAndWait(
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  R"(
(async function() {
  try {
    return await navigator.createAdRequest({
      adRequestUrl: "https://example.site",
      adProperties: [
        { width: "24", height: "48", slot: "first",
          lang: "en-us", adType: "test-ad2", bidFloor: 42.0 }],
      publisherCode: "pubCode123",
      targeting: { interests: ["interest1", "interest2"] },
      anonymizedProxiedSignals: [],
      fallbackSource: "https://fallback.site"
    });
  } catch (e) {
    return e.toString();
  }
})())");
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  [[nodiscard]] content::EvalJsResult FinalizeAdAndWait(
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return EvalJs(execution_target ? *execution_target : shell(),
                  R"(
(async function() {
  try {
    return await navigator.createAdRequest({
      adRequestUrl: "https://example.site",
      adProperties: [
        { width: "24", height: "48", slot: "first",
          lang: "en-us", adType: "test-ad2", bidFloor: 42.0 }],
      publisherCode: "pubCode123",
      targeting: { interests: ["interest1", "interest2"] },
      anonymizedProxiedSignals: [],
      fallbackSource: "https://fallback.site"
    }).then(ads => {
      return navigator.finalizeAd(ads, {
        seller: "https://example.site",
        decisionLogicUrl: "https://example.site/script.js",
        perBuyerSignals: {"example.site": { randomParam: "value1" }},
        auctionSignals: "pubCode123",
        sellerSignals: { someKey: "sellerValue" }
      });
    });
  } catch (e) {
    return e.toString();
  }
})())");
  }

  // Waits until the `condition` callback over the interest groups returns true.
  void WaitForInterestGroupsSatisfying(
      const url::Origin& owner,
      base::RepeatingCallback<bool(const std::vector<StorageInterestGroup>&)>
          condition) {
    while (true) {
      if (condition.Run(GetInterestGroupsForOwner(owner)))
        break;
    }
  }

  // Waits for `url` to be requested by `https_server_`, or any other server
  // that OnHttpsTestServerRequestMonitor() has been configured to monitor.
  // `url`'s hostname is replaced with "127.0.0.1", since the embedded test
  // server always claims requests were for 127.0.0.1, rather than revealing the
  // hostname that was actually associated with a request.
  void WaitForURL(const GURL& url) {
    GURL::Replacements replacements;
    replacements.SetHostStr("127.0.0.1");
    GURL wait_for_url = url.ReplaceComponents(replacements);

    {
      base::AutoLock auto_lock(requests_lock_);
      if (received_https_test_server_requests_.count(wait_for_url) > 0u)
        return;
      wait_for_url_ = wait_for_url;
      request_run_loop_ = std::make_unique<base::RunLoop>();
    }

    request_run_loop_->Run();
    request_run_loop_.reset();
  }

  void OnHttpsTestServerRequestMonitor(
      const net::test_server::HttpRequest& request) {
    base::AutoLock auto_lock(requests_lock_);
    received_https_test_server_requests_.insert(request.GetURL());
    if (wait_for_url_ == request.GetURL()) {
      wait_for_url_ = GURL();
      request_run_loop_->Quit();
    }
  }

  void ClearReceivedRequests() {
    base::AutoLock auto_lock(requests_lock_);
    received_https_test_server_requests_.clear();
  }

  bool HasServerSeenUrl(const GURL& url) {
    base::AutoLock auto_lock(requests_lock_);
    return received_https_test_server_requests_.find(url) !=
           received_https_test_server_requests_.end();
  }

  void ExpectNotAllowedToJoinOrUpdateInterestGroup(
      const url::Origin& origin, RenderFrameHost* execution_target) {
    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'joinAdInterestGroup' on "
        "'Navigator': Feature join-ad-interest-group is not enabled by "
        "Permissions Policy",
        EvalJs(execution_target, JsReplace(
                                     R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                     origin)));

    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'updateAdInterestGroups' on "
        "'Navigator': Feature join-ad-interest-group is not enabled by "
        "Permissions Policy",
        UpdateInterestGroupsInJS(execution_target));
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  void ExpectNotAllowedToLeaveInterestGroup(const url::Origin& origin,
                                            std::string name,
                                            RenderFrameHost* execution_target) {
    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'leaveAdInterestGroup' on "
        "'Navigator': Feature join-ad-interest-group is not enabled by "
        "Permissions Policy",
        EvalJs(execution_target,
               base::StringPrintf(R"(
(async function() {
  try {
    await navigator.leaveAdInterestGroup({name: '%s', owner: '%s'});
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                  name.c_str(), origin.Serialize().c_str())));
  }

  void ExpectNotAllowedToRunAdAuction(const url::Origin& origin,
                                      const GURL& url,
                                      RenderFrameHost* execution_target) {
    EXPECT_EQ(
        "NotAllowedError: Failed to execute 'runAdAuction' on 'Navigator': "
        "Feature run-ad-auction is not enabled by Permissions Policy",
        RunAuctionAndWait(JsReplace(
                              R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                              )",
                              origin, url),
                          execution_target));
  }

  std::string WarningPermissionsPolicy(std::string feature, std::string api) {
    return base::StringPrintf(
        "In the future, Permissions Policy feature %s will not be enabled by "
        "default in cross-origin iframes or same-origin iframes nested in "
        "cross-origin iframes. Calling %s will be rejected with "
        "NotAllowedError if it is not explicitly enabled",
        feature.c_str(), api.c_str());
  }

  void ConvertFencedFrameURNToURL(
      const GURL& urn_url,
      TestFencedFrameURLMappingResultObserver* observer,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    ToRenderFrameHost adapter(execution_target ? *execution_target : shell());
    FencedFrameURLMapping& fenced_frame_urls_map =
        static_cast<RenderFrameHostImpl*>(adapter.render_frame_host())
            ->GetPage()
            .fenced_frame_urls_map();
    fenced_frame_urls_map.ConvertFencedFrameURNToURL(urn_url, observer);
  }

  absl::optional<GURL> ConvertFencedFrameURNToURLInJS(
      const GURL& urn_url,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    ToRenderFrameHost adapter(execution_target ? *execution_target : shell());
    EvalJsResult result = EvalJs(adapter, JsReplace(R"(
      navigator.deprecatedURNToURL($1)
    )",
                                                    urn_url));
    if (!result.error.empty() || result.value.is_none())
      return absl::nullopt;
    return GURL(result.ExtractString());
  }

  bool ReplaceInURNInJS(
      const GURL& urn_url,
      const base::flat_map<std::string, std::string> replacements,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    base::Value::Dict replacement_value;
    for (const auto& replacement : replacements)
      replacement_value.Set(replacement.first, replacement.second);
    EvalJsResult result =
        EvalJs(execution_target ? *execution_target : shell(),
               JsReplace(R"(
    (async function() {
      await navigator.deprecatedReplaceInURN($1, $2);
      return 'done';
    })())",
                         urn_url, base::Value(std::move(replacement_value))));
    EXPECT_EQ("", result.error);
    return "done" == result;
  }

  void AttachInterestGroupObserver() {
    manager_->AddInterestGroupObserver(observer_.get());
  }

  void WaitForAccessObserved(
      const std::vector<InterestGroupTestObserver::Entry>& expected) {
    observer_->WaitForAccesses(expected);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  AllowlistedOriginContentBrowserClient content_browser_client_;
  raw_ptr<ContentBrowserClient> old_content_browser_client_;
  std::unique_ptr<InterestGroupTestObserver> observer_;
  raw_ptr<InterestGroupManagerImpl> manager_;
  base::Lock requests_lock_;
  std::set<GURL> received_https_test_server_requests_
      GUARDED_BY(requests_lock_);
  std::unique_ptr<base::RunLoop> request_run_loop_;
  GURL wait_for_url_ GUARDED_BY(requests_lock_);
  std::unique_ptr<NetworkResponder> network_responder_;
};

// At the moment, InterestGroups use URN urls when fenced frames are enabled,
// and normal URLs when not. This means they require ads be loaded in fenced
// frames when Chrome is running with the option enabled.
class InterestGroupFencedFrameBrowserTest
    : public InterestGroupBrowserTest,
      public ::testing::WithParamInterface<
          blink::features::FencedFramesImplementationType> {
 public:
  InterestGroupFencedFrameBrowserTest() {
    // Tests are run with both the ShadowDOM and MPArch ("Multi-Page
    // Architecture") fenced frames implementations.
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames,
          {{"implementation_type", GetFencedFrameFeatureParam()}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        {/* disabled_features */});
  }

  const char* GetFencedFrameFeatureParam() const {
    switch (GetParam()) {
      case blink::features::FencedFramesImplementationType::kShadowDOM:
        return "shadow_dom";
      case blink::features::FencedFramesImplementationType::kMPArch:
        return "mparch";
    }
  }

  ~InterestGroupFencedFrameBrowserTest() override = default;

  // Runs the specified auction using RunAuctionAndWait(), expecting a success
  // resulting in a URN URL. Then navigates a pre-existing fenced frame to that
  // URL, expecting `expected_ad_url` to be loaded in the fenced frame.
  //
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  //
  // The target must already contain a single fenced frame.
  void RunAuctionAndNavigateFencedFrame(
      const GURL& expected_ad_url,
      const std::string& auction_config_json,
      absl::optional<ToRenderFrameHost> execution_target = absl::nullopt) {
    if (!execution_target)
      execution_target = shell();
    content::EvalJsResult urn_url_string =
        RunAuctionAndWait(auction_config_json, execution_target);
    ASSERT_TRUE(urn_url_string.value.is_string())
        << "Expected string, but got " << urn_url_string.value;

    GURL urn_url(urn_url_string.ExtractString());
    ASSERT_TRUE(urn_url.is_valid())
        << "URL is not valid: " << urn_url_string.ExtractString();
    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

    NavigateFencedFrameAndWait(urn_url, expected_ad_url, *execution_target);
  }

  // Navigates the only fenced frame in `execution_target` to `url` and waits
  // for the navigation to complete, expecting the frame to navigate to
  // `expected_url`. Also checks that the URL is actually requested from the
  // test server if `expected_url` is an HTTPS URL.
  void NavigateFencedFrameAndWait(const GURL& url,
                                  const GURL& expected_url,
                                  const ToRenderFrameHost& execution_target) {
    // Use to wait for navigation completion in the ShadowDOM case only.
    // Harmlessly created but not used in the MPArch case.
    TestFrameNavigationObserver observer(
        GetFencedFrameRenderFrameHost(execution_target));

    EXPECT_TRUE(ExecJs(
        execution_target,
        JsReplace("document.querySelector('fencedframe').src = $1;", url)));

    // If the URL is HTTPS, wait for the URL to be requested, to make sure the
    // fenced frame actually made the request and, in the MPArch case, to make
    // sure the load actually started. On regression, this is likely to hang.
    if (expected_url.SchemeIs(url::kHttpsScheme)) {
      WaitForURL(expected_url);
    } else {
      // The only other URLs this should be used with are about:blank URLs.
      ASSERT_EQ(GURL(url::kAboutBlankURL), expected_url);
    }

    // Wait for the load to complete.
    observer.Wait();

    RenderFrameHost* fenced_frame_host =
        GetFencedFrameRenderFrameHost(execution_target);
    // Verify that the URN was resolved to the correct URL.
    EXPECT_EQ(expected_url, fenced_frame_host->GetLastCommittedURL());

    // Make sure the URL was successfully committed. If the page failed to load
    // the URL will be `expected_url`, but IsErrorDocument() will be true, and
    // the last committed origin will be opaque.
    EXPECT_FALSE(fenced_frame_host->IsErrorDocument());
    // If scheme is HTTP or HTTPS, check the last committed origin here. If
    // scheme is about:blank, don't do so, since url::Origin::Create() will
    // return an opaque origin in that case.
    if (expected_url.SchemeIsHTTPOrHTTPS()) {
      EXPECT_EQ(url::Origin::Create(expected_url),
                fenced_frame_host->GetLastCommittedOrigin());
    }
  }

  // Returns the RenderFrameHostImpl for a fenced frame in `execution_target`,
  // which is assumed to contain only one fenced frame and no iframes.
  RenderFrameHostImpl* GetFencedFrameRenderFrameHost(
      const ToRenderFrameHost& execution_target) {
    switch (GetParam()) {
      case blink::features::FencedFramesImplementationType::kShadowDOM: {
        // Make sure there's only one child frame.
        CHECK(!ChildFrameAt(execution_target, 1));
        CHECK(ChildFrameAt(execution_target, 0));

        return static_cast<RenderFrameHostImpl*>(
            ChildFrameAt(execution_target, 0));
      }
      case blink::features::FencedFramesImplementationType::kMPArch: {
        return GetFencedFrame(execution_target)->GetInnerRoot();
      }
    }
  }

  // Returns FencedFrame in `execution_target` frame. Requires that
  // `execution_target` have one and only one FencedFrame. MPArch only, as the
  // ShadowDOM implementation doesn't use the FencedFrame class.
  FencedFrame* GetFencedFrame(const ToRenderFrameHost& execution_target) {
    CHECK_EQ(GetParam(),
             blink::features::FencedFramesImplementationType::kMPArch);

    std::vector<FencedFrame*> fenced_frames =
        static_cast<RenderFrameHostImpl*>(execution_target.render_frame_host())
            ->GetFencedFrames();
    CHECK_EQ(1u, fenced_frames.size());
    return fenced_frames[0];
  }

  // Navigates the main frame, adds an interest group with a single component
  // URL, and runs an auction where an ad with that component URL wins.
  // Navigates a fenced frame to the winning render URL (which contains a nested
  // fenced frame), and navigates that fenced frame to the component ad URL.
  // Provides a common starting state for testing behavior of component ads and
  // fenced frames.
  //
  // Writes URN for the component ad to `component_ad_urn`, if non-null.
  void RunBasicAuctionWithAdComponents(const GURL& ad_component_url,
                                       GURL* component_ad_urn = nullptr) {
    GURL test_url =
        https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html");
    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GURL ad_url =
        https_server_->GetURL("c.test", "/fenced_frames/opaque_ads.html");
    EXPECT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(
            /*owner=*/url::Origin::Create(test_url),
            /*name=*/"cars",
            /*priority=*/0.0,
            /*bidding_url=*/
            https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
            /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
            /*ad_components=*/
            {{{ad_component_url, /*metadata=*/absl::nullopt}}}));

    ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
        ad_url, JsReplace(
                    R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1]
                    })",
                    url::Origin::Create(test_url),
                    https_server_->GetURL(
                        "a.test", "/interest_group/decision_logic.js"))));

    // Get first component URL from the fenced frame.
    RenderFrameHost* ad_frame = GetFencedFrameRenderFrameHost(shell());
    absl::optional<std::vector<GURL>> components =
        GetAdAuctionComponentsInJS(ad_frame, 1);
    ASSERT_TRUE(components);
    ASSERT_EQ(1u, components->size());
    EXPECT_EQ(url::kUrnScheme, (*components)[0].scheme_piece());
    if (component_ad_urn)
      *component_ad_urn = (*components)[0];

    // Load the ad component in the nested fenced frame. The load should
    // succeed.
    NavigateFencedFrameAndWait((*components)[0], ad_component_url, ad_frame);
  }

  absl::optional<std::vector<GURL>> GetAdAuctionComponentsInJS(
      const ToRenderFrameHost& execution_target,
      int num_params) {
    auto result = EvalJs(
        execution_target,
        base::StringPrintf("navigator.adAuctionComponents(%i)", num_params));
    // Return nullopt if an exception was thrown, as should be the case for
    // loading pages that are not the result of an auction.
    if (!result.error.empty())
      return absl::nullopt;

    // Otherwise, adAuctionComponents should always return a list, since it
    // forces its input to be a number, and clamps it to the expected range.
    EXPECT_TRUE(result.value.is_list());
    if (!result.value.is_list())
      return absl::nullopt;

    std::vector<GURL> out;
    for (const auto& value : result.value.GetListDeprecated()) {
      if (!value.is_string()) {
        ADD_FAILURE() << "Expected string: " << value;
        return std::vector<GURL>();
      }
      GURL url(value.GetString());
      if (!url.is_valid() || !url.SchemeIs(url::kUrnScheme)) {
        ADD_FAILURE() << "Expected valid URN URL: " << value;
        return std::vector<GURL>();
      }
      out.emplace_back(std::move(url));
    }
    return out;
  }

  // Validates that navigator.adAuctionComponents() returns URNs that map to
  // `expected_ad_component_urls`. `expected_ad_component_urls` is padded with
  // about:blank URLs up to blink::kMaxAdAuctionAdComponents. Calls
  // adAuctionComponents() with a number of different input parameters to get a
  // list of URNs and checks them against FencedFrameURLMapping to make sure
  // they're mapped to `expected_ad_component_urls`, and in the same order.
  void CheckAdComponents(std::vector<GURL> expected_ad_component_urls,
                         RenderFrameHostImpl* render_frame_host) {
    while (expected_ad_component_urls.size() <
           blink::kMaxAdAuctionAdComponents) {
      expected_ad_component_urls.emplace_back(GURL(url::kAboutBlankURL));
    }

    absl::optional<std::vector<GURL>> all_component_urls =
        GetAdAuctionComponentsInJS(render_frame_host,
                                   blink::kMaxAdAuctionAdComponents);
    ASSERT_TRUE(all_component_urls);
    ASSERT_EQ(blink::kMaxAdAuctionAdComponents, all_component_urls->size());
    for (size_t i = 0; i < all_component_urls->size(); ++i) {
      // All ad component URLs should use the URN scheme.
      EXPECT_EQ(url::kUrnScheme, (*all_component_urls)[i].scheme_piece());

      // All ad component URLs should be unique.
      for (size_t j = 0; j < i; ++j)
        EXPECT_NE((*all_component_urls)[i], (*all_component_urls)[j]);

      // Check URNs are mapped to the values in `expected_ad_component_urls`.
      TestFencedFrameURLMappingResultObserver observer;
      ConvertFencedFrameURNToURL((*all_component_urls)[i], &observer,
                                 render_frame_host);
      EXPECT_TRUE(observer.mapped_url());
      EXPECT_EQ(expected_ad_component_urls[i], observer.mapped_url());
    }

    // Make sure smaller values passed to GetAdAuctionComponentsInJS() return
    // the first elements of the full kMaxAdAuctionAdComponents element list
    // retrieved above.
    for (size_t i = 0; i < blink::kMaxAdAuctionAdComponents; ++i) {
      absl::optional<std::vector<GURL>> component_urls =
          GetAdAuctionComponentsInJS(render_frame_host, i);
      ASSERT_TRUE(component_urls);
      EXPECT_THAT(*component_urls,
                  testing::ElementsAreArray(all_component_urls->begin(),
                                            all_component_urls->begin() + i));
    }

    // Test clamping behavior.
    EXPECT_EQ(std::vector<GURL>(),
              GetAdAuctionComponentsInJS(render_frame_host, -32769));
    EXPECT_EQ(std::vector<GURL>(),
              GetAdAuctionComponentsInJS(render_frame_host, -2));
    EXPECT_EQ(std::vector<GURL>(),
              GetAdAuctionComponentsInJS(render_frame_host, -1));
    EXPECT_EQ(all_component_urls,
              GetAdAuctionComponentsInJS(render_frame_host,
                                         blink::kMaxAdAuctionAdComponents + 1));
    EXPECT_EQ(all_component_urls,
              GetAdAuctionComponentsInJS(render_frame_host,
                                         blink::kMaxAdAuctionAdComponents + 2));
    EXPECT_EQ(all_component_urls,
              GetAdAuctionComponentsInJS(render_frame_host, 32768));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// Make sure that FLEDGE has protections against making local network requests..
class InterestGroupPrivateNetworkBrowserTest : public InterestGroupBrowserTest {
 protected:
  InterestGroupPrivateNetworkBrowserTest()
      : remote_test_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitAndEnableFeature(
        features::kPrivateNetworkAccessRespectPreflightResults);

    remote_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    remote_test_server_.AddDefaultHandlers(GetTestDataFilePath());
    remote_test_server_.RegisterRequestMonitor(base::BindRepeating(
        &InterestGroupBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    EXPECT_TRUE(remote_test_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        network::switches::kIpAddressSpaceOverrides,
        base::StringPrintf(
            "%s=public",
            remote_test_server_.host_port_pair().ToString().c_str()));
  }

  void SetUpOnMainThread() override {
    InterestGroupBrowserTest::SetUpOnMainThread();

    // Extend allow list to include the remote server.
    content_browser_client_.AddToAllowList(
        {url::Origin::Create(remote_test_server_.GetURL("a.test", "/")),
         url::Origin::Create(remote_test_server_.GetURL("b.test", "/")),
         url::Origin::Create(remote_test_server_.GetURL("c.test", "/"))});
  }

 protected:
  // Test server which is treated as remote, due to command line options. Can't
  // use "Content-Security-Policy: treat-as-public-address", because that would
  // block all local requests, including loading the seller script, even if the
  // seller script had the same header.
  net::test_server::EmbeddedTestServer remote_test_server_;

  base::test::ScopedFeatureList feature_list_;
};

// More restricted Permissions Policy is set for features join-ad-interest-group
// and run-ad-auction (EnableForSelf instead of EnableForAll) when runtime flag
// kAdInterestGroupAPIRestrictedPolicyByDefault is enabled.
class InterestGroupRestrictedPermissionsPolicyBrowserTest
    : public InterestGroupBrowserTest {
 public:
  InterestGroupRestrictedPermissionsPolicyBrowserTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    InterestGroupFencedFrameBrowserTest,
    ::testing::Values(
        blink::features::FencedFramesImplementationType::kShadowDOM,
        blink::features::FencedFramesImplementationType::kMPArch));

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SameOriginJoinLeaveInterestGroup) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  AttachInterestGroupObserver();

  // This join should succeed.
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(test_origin_a, "cars"));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the bidding_url, bid.a.test.
  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "biddingUrl 'https://bid.a.test/' for AuctionAdInterestGroup with "
          "owner '%s' and name 'bicycles' biddingUrl must have the same origin "
          "as the InterestGroup owner and have no fragment identifier or "
          "embedded credentials.",
          test_origin_a.Serialize().c_str()),
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin_a,
          /*name=*/"bicycles",
          /*priority=*/0.0,
          /*bidding_url=*/GURL("https://bid.a.test"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt)));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the update_url, update.a.test.
  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "updateUrl 'https://update.a.test/' for AuctionAdInterestGroup with "
          "owner '%s' and name 'tricycles' updateUrl must have the same origin "
          "as the InterestGroup owner and have no fragment identifier or "
          "embedded credentials.",
          test_origin_a.Serialize().c_str()),
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin_a,
          /*name=*/"tricycles",
          /*priority=*/0.0,
          /*bidding_url=*/absl::nullopt,
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/GURL("https://update.a.test"),
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt)));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the trusted_bidding_signals_url, signals.a.test.
  EXPECT_EQ(base::StringPrintf(
                "TypeError: Failed to execute 'joinAdInterestGroup' on "
                "'Navigator': trustedBiddingSignalsUrl "
                "'https://signals.a.test/' for AuctionAdInterestGroup with "
                "owner '%s' and name 'four-wheelers' trustedBiddingSignalsUrl "
                "must have the same origin as the InterestGroup owner and have "
                "no query string, fragment identifier or embedded credentials.",
                test_origin_a.Serialize().c_str()),
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin_a,
                /*name=*/"four-wheelers",
                /*priority=*/0.0,
                /*bidding_url=*/absl::nullopt,
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/GURL("https://signals.a.test"),
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/absl::nullopt,
                /*ad_components=*/absl::nullopt)));

  // This join should silently fail since d.test is not allowlisted for the API,
  // and allowlist checks only happen in the browser process, so don't throw an
  // exception. Can't use JoinInterestGroupAndVerify() because of the silent
  // failure.
  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  url::Origin test_origin_d = url::Origin::Create(test_url_d);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  EXPECT_EQ(kSuccess, JoinInterestGroup(test_origin_d, "toys"));

  // Another successful join.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(test_origin_b, "trucks"));

  // Check that only the a.test and b.test interest groups were added to
  // the database.
  std::vector<std::pair<url::Origin, std::string>> expected_groups = {
      {test_origin_a, "cars"}, {test_origin_b, "trucks"}};
  std::vector<std::pair<url::Origin, std::string>> received_groups;
  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));

  // Now test leaving
  // Test that we can't leave an interest group from a site not allowedlisted
  // for the API.
  // Inject an interest group into the DB for that for that site so we can try
  // to remove it.
  manager_->JoinInterestGroup(
      blink::InterestGroup(
          /*expiry=*/base::Time::Now() + base::Seconds(300),
          /*owner=*/test_origin_d,
          /*name=*/"candy",
          /*priority=*/0.0,
          /*bidding_url=*/absl::nullopt,
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      test_origin_d.GetURL());

  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  // This leave should do nothing because `origin_d` is not allowed by privacy
  // sandbox. Can't use LeaveInterestGroupAndVerify() because it returns "true"
  // but doesn't actually leave the interest group.
  EXPECT_EQ(kSuccess, LeaveInterestGroup(test_origin_d, "candy"));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  // This leave should do nothing because there is not interest group of that
  // name.
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(test_origin_b, "cars"));

  // This leave should succeed.
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(test_origin_a, "cars"));

  // We expect that `test_origin_b` and the (injected) `test_origin_d` interest
  // groups remain.
  expected_groups = {{test_origin_b, "trucks"}, {test_origin_d, "candy"}};
  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));
  WaitForAccessObserved(
      {{InterestGroupTestObserver::kJoin, test_origin_a.Serialize(), "cars"},
       {InterestGroupTestObserver::kJoin, test_origin_b.Serialize(), "trucks"},
       {InterestGroupTestObserver::kJoin, test_origin_d.Serialize(), "candy"},
       {InterestGroupTestObserver::kLeave, test_origin_b.Serialize(), "cars"},
       {InterestGroupTestObserver::kLeave, test_origin_a.Serialize(), "cars"}});
}

// Test the case of a cross-origin iframe joining and leaving same-origin
// interest groups. This should succeed without any .well-known fetches needed.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SameOriginIframeJoinLeaveInterestGroup) {
  const char kGroup[] = "goats";

  // b.test iframes a.test. The iframe should be able to successfully join and
  // leave a.test interest group without needing any .well-known fetches.
  GURL main_url =
      https_server_->GetURL("b.test",
                            "/cross_site_iframe_factory.html?b.test("
                            "a.test{allow-join-ad-interest-group}"
                            ")");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  url::Origin group_origin = https_server_->GetOrigin("a.test");

  FrameTreeNode* parent = FrameTreeNode::From(web_contents()->GetMainFrame());
  ASSERT_GT(parent->child_count(), 0u);
  RenderFrameHost* iframe = parent->child_at(0)->current_frame_host();

  // Both joining and leaving should work.
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          group_origin, kGroup,
                          /*priority=*/0, /*bidding_url=*/absl::nullopt,
                          /*ads=*/absl::nullopt,
                          /*ad_components=*/absl::nullopt, iframe));
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(group_origin, kGroup, iframe));
}

// Test cross-origin joining/leaving of interest groups, in the case an IG owner
// only allows cross-origin joining. Only allow joining to make sure join and
// leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginJoinAllowedByWellKnownFetch) {
  const char kGroup[] = "aardvarks";

  url::Origin allow_join_origin = url::Origin::Create(
      GURL(https_server_->GetURL("allow-join.a.test", "/")));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Joining a group cross-origin should succeed.
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(allow_join_origin, kGroup));

  // Leaving the group should fail.
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroupAndVerify(allow_join_origin, kGroup));
}

// Test cross-origin joining/leaving of interest groups, in the case an IG owner
// only allows cross-origin leaving. Only allow leaving to make sure leave and
// leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginLeaveAllowedByWellKnownFetch) {
  // Group that's joined with a same-origin request.
  const char kJoinSucceedsGroup[] = "join-succeeds-group";
  // Group where joining fails due to the request being cross-origin, and
  // cross-origin joins being blocked.
  const char kJoinFailsGroup[] = "join-fails-group";

  GURL allow_leave_url =
      GURL(https_server_->GetURL("allow-leave.a.test", "/echo"));
  url::Origin allow_leave_origin = url::Origin::Create(allow_leave_url);

  // Navigate to the origin that allows leaving only, and join one of its
  // groups, which should succeed.
  ASSERT_TRUE(NavigateToURL(shell(), allow_leave_url));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(allow_leave_origin, kJoinSucceedsGroup));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Try to join an `allow_leave_origin` group, which should fail.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroupAndVerify(allow_leave_origin, kJoinFailsGroup));

  // Leaving the group that was successfully joined earlier should succeed.
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(allow_leave_origin,
                                                  kJoinSucceedsGroup));
}

// Test cross-origin joining/leaving of interest groups from an iframe, in the
// case an IG owner only allows cross-origin joining. Only allow joining to make
// sure join and leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginIframeJoinAllowedByWellKnownFetch) {
  const char kGroup[] = "aardvarks";

  url::Origin allow_join_origin = url::Origin::Create(
      GURL(https_server_->GetURL("allow-join.a.test", "/")));

  // allow-join.a.test iframes b.test. The iframe should require preflights to
  // be able to join or leave allow-join.a.test's interest groups. In this test,
  // the preflights only allow joins.
  GURL main_url =
      https_server_->GetURL("allow-join.a.test",
                            "/cross_site_iframe_factory.html?allow-join.a.test("
                            "b.test{allow-join-ad-interest-group}"
                            ")");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* parent = FrameTreeNode::From(web_contents()->GetMainFrame());
  ASSERT_GT(parent->child_count(), 0u);
  RenderFrameHost* iframe = parent->child_at(0)->current_frame_host();

  // Joining a group cross-origin should succeed.
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          allow_join_origin, kGroup,
                          /*priority=*/0, /*bidding_url=*/absl::nullopt,
                          /*ads=*/absl::nullopt,
                          /*ad_components=*/absl::nullopt, iframe));

  // Leaving the group should fail.
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroupAndVerify(allow_join_origin, kGroup, iframe));
}

// Test cross-origin joining/leaving of interest groups from an iframe, in the
// case an IG owner only allows cross-origin leaving. Only allow leaving to make
// sure join and leave permissions are tracked separately.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginIframeLeaveAllowedByWellKnownFetch) {
  // Group that's joined with a same-origin request.
  const char kJoinSucceedsGroup[] = "join-succeeds-group";
  // Group where joining fails due to the request being cross-origin, and
  // cross-origin joins being blocked.
  const char kJoinFailsGroup[] = "join-fails-group";

  url::Origin allow_leave_origin =
      https_server_->GetOrigin("allow-leave.a.test");

  // allow-leave.a.test iframes b.test. The iframe should require preflights to
  // be able to join or leave allow-leave.a.test's interest groups. In this
  // test, the preflights only allow leaves.
  GURL main_url = https_server_->GetURL(
      "allow-leave.a.test",
      "/cross_site_iframe_factory.html?allow-leave.a.test("
      "b.test{allow-join-ad-interest-group}"
      ")");
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // The main frame joins kJoinSucceedsGroup, which should succeed.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(allow_leave_origin, kJoinSucceedsGroup));

  FrameTreeNode* parent = FrameTreeNode::From(web_contents()->GetMainFrame());
  ASSERT_GT(parent->child_count(), 0u);
  RenderFrameHost* iframe = parent->child_at(0)->current_frame_host();

  // Try to join an `allow_leave_origin` group from the iframe, which should
  // fail.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroupAndVerify(allow_leave_origin, kJoinFailsGroup,
                                       /*priority=*/0.0,
                                       /*bidding_url=*/absl::nullopt,
                                       /*ads=*/absl::nullopt,
                                       /*ad_components=*/
                                       absl::nullopt,
                                       /*execution_target=*/iframe));

  // Leaving the group from the iframe that was successfully joined earlier
  // should succeed.
  EXPECT_EQ(kSuccess, LeaveInterestGroupAndVerify(allow_leave_origin,
                                                  kJoinSucceedsGroup, iframe));
}

// Test the case cross-origin joining/leaving of interest groups is blocked by
// the ContentBrowserClient, but allowed by the .well-known URL. In this case,
// the .well-known URL should be fetched, and the return value should be derived
// from that fetch returned, but the database should not updated, regardless of
// whether the .well-known URL allows it. This can happen if, for example,
// cookies blocking is enabled for a site.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginJoinLeaveBlockedByContentBrowserClient) {
  const char kGroup1[] = "aardvarks";
  const char kGroup2[] = "wombats";

  // Interest groups operations are not allowed on "*.d.test" by the
  // ContentBrowserClient. One allows only joins, one only leaves, which should
  // affect return values, but not whether the page can actually join or leave
  // cross-origin interest groups.
  url::Origin allow_join_origin = https_server_->GetOrigin("allow-join.d.test");
  url::Origin allow_leave_origin =
      https_server_->GetOrigin("allow-leave.d.test");

  // Join kGroup2 directly for both origins, so can check leave calls have no
  // effect.
  blink::InterestGroup interest_group;
  interest_group.owner = allow_join_origin;
  interest_group.name = kGroup2;
  interest_group.expiry = base::Time::Now() + base::Days(1);
  // The joining URL doesn't actually matter.
  manager_->JoinInterestGroup(
      interest_group,
      /*joining_url=*/https_server_->GetURL("allow-join.d.test", "/"));
  interest_group.owner = allow_leave_origin;
  manager_->JoinInterestGroup(
      interest_group,
      /*joining_url=*/https_server_->GetURL("allow-leave.d.test", "/"));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Join/leave calls for `allow_join_origin` should claim joining succeeded,
  // and leaving failed, but neither call should actually affect what interest
  // groups the user is in.
  EXPECT_EQ(kSuccess, JoinInterestGroup(allow_join_origin, kGroup1));
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroup(allow_join_origin, kGroup2));

  // Join/leave calls for `allow_leave_origin` should claim joining failed, and
  // leaving succeeded, but neither call should actually affect what interest
  // groups the user is in.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroup(allow_leave_origin, kGroup1));
  EXPECT_EQ(kSuccess, LeaveInterestGroup(allow_leave_origin, kGroup2));

  // The user should still be in kGroup2, but not kGroup1, for both origins.
  std::vector<std::pair<url::Origin, std::string>> expected_groups = {
      {allow_join_origin, kGroup2}, {allow_leave_origin, kGroup2}};
  EXPECT_THAT(GetAllInterestGroups(),
              testing::UnorderedElementsAreArray(expected_groups));
}

// Test cross-origin joining of interest groups requires CORS.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginJoinNoCors) {
  const char kGroup[] = "aardvarks";

  url::Origin no_cors_origin =
      url::Origin::Create(GURL(https_server_->GetURL("no-cors.a.test", "/")));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Joining a group should fail.
  EXPECT_EQ("NotAllowedError: Permission to join interest group denied.",
            JoinInterestGroupAndVerify(no_cors_origin, kGroup));
}

// Test cross-origin leaving of interest groups requires CORS.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginLeaveNoCors) {
  const char kGroup[] = "aardvarks";

  GURL no_cors_url = GURL(https_server_->GetURL("no-cors.a.test", "/echo"));
  url::Origin no_cors_origin = url::Origin::Create(no_cors_url);

  // Navigate to `no_cors_url` and join an IG, which should succeed, since it's
  // a same-origin join.
  ASSERT_TRUE(NavigateToURL(shell(), no_cors_url));
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(no_cors_origin, kGroup));

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));

  // Leaving the group should fail.
  EXPECT_EQ("NotAllowedError: Permission to leave interest group denied.",
            LeaveInterestGroupAndVerify(no_cors_origin, kGroup));
}

// Test the renderer restricting the number of active cross-origin joins per
// frame. One page tries to join kMaxActiveCrossSiteJoins+1 cross-origin
// interest groups from 3 different origins, the last two requests are to two
// distinct origins. All but the last are sent to the browser process. The
// results in two .well-known permissions requests. While those two requests are
// pending, the last request is held back in the renderer.
//
// Then the site joins and leaves a same-origin interest group, which should
// bypass the queue. Then one of the hung .well-known requests completes, which
// should allow the final cross-origin join to send out its .well-known request.
//
// Then a cross-origin leave request is issued for the group just joined, which
// should not wait before sending the request to the browser process, since
// leaves and joins are throttled separately. The browser process then leaves
// the group immediately, using the cached result of the previous .well-known
// fetch.
//
// The remaining two .well-known requests for the joins are then completed,
// which should result in all pending joins completing successfully.
//
// The title of the page is updated when each promise completes successfully, to
// allow waiting on promises that were created earlier in the test run.
//
// Only 3 cross-site origins are used to limit the test to 3 simultaneous
// .well-known requests. Using too many would run into the network stack's
// request throttling code.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginJoinQueue) {
  // This matches the value in navigator_auction.cc in blink/.
  const int kMaxActiveCrossSiteJoins = 20;

  // Since this is using another port from `cross_origin_server` below, the
  // hostname doesn't matter, but use a different one, just in case.
  GURL main_url = https_server_->GetURL("a.test", "/echo");
  url::Origin main_origin = url::Origin::Create(main_url);

  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      permissions_responses;
  net::EmbeddedTestServer cross_origin_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  cross_origin_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  // There should be 3 .well-known requests for the cross-origin joins. The
  // cross-origin leave should use a cached result.
  for (int i = 0; i < 3; ++i) {
    permissions_responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &cross_origin_server,
            "/.well-known/interest-group/permissions/?origin=" +
                base::EscapeQueryParamValue(main_origin.Serialize(),
                                            /*use_plus=*/false)));
  }
  ASSERT_TRUE(cross_origin_server.Start());

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  for (int i = 0; i < kMaxActiveCrossSiteJoins + 1; ++i) {
    const char* other_origin_host = "0.b.test";
    if (i == kMaxActiveCrossSiteJoins - 1) {
      other_origin_host = "1.b.test";
    } else if (i == kMaxActiveCrossSiteJoins) {
      other_origin_host = "2.b.test";
    }
    url::Origin other_origin = cross_origin_server.GetOrigin(other_origin_host);
    content_browser_client_.AddToAllowList({other_origin});

    ExecuteScriptAsync(shell(),
                       JsReplace(R"(
navigator.joinAdInterestGroup(
    {name: $1, owner: $2}, /*joinDurationSec=*/ 300)
    .then(() => {
      // Append the first character of the owner's host to the title.
      document.title += (new URL($2)).host[0];
    });)",
                                 base::NumberToString(i), other_origin));

    // Wait for .well-known requests to be made for "0.b.test" and "1.b.test".
    // Need to wait for them immediately after the Javascript calls that should
    // trigger the requests to prevent their order from being racily reversed at
    // the network layer.
    if (i == 0) {
      permissions_responses[0]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[0]->http_request()->headers.at("Host"),
          "0.b.test"));
    } else if (i == kMaxActiveCrossSiteJoins - 1) {
      permissions_responses[1]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[1]->http_request()->headers.at("Host"),
          "1.b.test"));
    }
  }

  // Clear title, as each successful join modifies the title, so need a basic
  // title to start with. Can't set an empty title, so use "_" instead.
  ExecuteScriptAsync(shell(), "document.title='_'");

  // Joining and leaving a same-origin interest group should not be throttled.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(main_origin, "helmets for unicorns"));
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(main_origin, "helmets for unicorns"));

  // The "2.b.test" cross-origin join should still be waiting for one of the
  // other cross-site joins to complete.
  EXPECT_FALSE(permissions_responses[2]->has_received_request());

  // Complete the "1.b.test" .well-known request, which should cause the
  // "2.b.test" join request to be sent to the browser, which should issue
  // another .well-known request.
  TitleWatcher title_watcher1(web_contents(), u"_1");
  permissions_responses[1]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/
      R"({"joinAdInterestGroup" : true, "leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[1]->Done();
  EXPECT_EQ(u"_1", title_watcher1.WaitAndGetTitle());

  // The "2.b.test" cross-origin join should advance out of the queue and send a
  // .well-known request.
  permissions_responses[2]->WaitForRequest();
  EXPECT_TRUE(base::StartsWith(
      permissions_responses[2]->http_request()->headers.at("Host"),
      "2.b.test"));

  // A new cross-origin leave should bypass the join queue, and start
  // immediately, retrieving the previous .well-known response from the cache.
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(
                /*owner=*/cross_origin_server.GetOrigin("1.b.test"),
                /*name=*/base::NumberToString(kMaxActiveCrossSiteJoins)));

  // Complete the "2.b.test" join's .well-known request.
  TitleWatcher title_watcher2(web_contents(), u"_12");
  permissions_responses[2]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[2]->Done();
  EXPECT_EQ(u"_12", title_watcher2.WaitAndGetTitle());

  // Complete the "0.b.test" joins' .well-known request.
  std::u16string final_title =
      u"_12" + std::u16string(kMaxActiveCrossSiteJoins - 1, u'0');
  TitleWatcher title_watcher3(web_contents(), final_title);
  permissions_responses[0]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[0]->Done();
  EXPECT_EQ(final_title, title_watcher3.WaitAndGetTitle());
}

// The inverse of CrossOriginJoinQueue. Unlike most leave tests, leaves interest
// groups the user isn't actually in.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOriginLeaveQueue) {
  // This matches the value in navigator_auction.cc in blink/.
  const int kMaxActiveCrossSiteLeaves = 20;

  // Since this is using another port from `cross_origin_server` below, the
  // hostname doesn't matter, but use a different one, just in case.
  GURL main_url = https_server_->GetURL("a.test", "/echo");
  url::Origin main_origin = url::Origin::Create(main_url);

  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      permissions_responses;
  net::EmbeddedTestServer cross_origin_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  cross_origin_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  // There should be 3 .well-known requests for the cross-origin leaves. The
  // cross-origin join should use a cached result.
  for (int i = 0; i < 3; ++i) {
    permissions_responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &cross_origin_server,
            "/.well-known/interest-group/permissions/?origin=" +
                base::EscapeQueryParamValue(main_origin.Serialize(),
                                            /*use_plus=*/false)));
  }
  ASSERT_TRUE(cross_origin_server.Start());

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  for (int i = 0; i < kMaxActiveCrossSiteLeaves + 1; ++i) {
    const char* other_origin_host = "0.b.test";
    if (i == kMaxActiveCrossSiteLeaves - 1) {
      other_origin_host = "1.b.test";
    } else if (i == kMaxActiveCrossSiteLeaves) {
      other_origin_host = "2.b.test";
    }
    url::Origin other_origin = cross_origin_server.GetOrigin(other_origin_host);
    content_browser_client_.AddToAllowList({other_origin});

    ExecuteScriptAsync(shell(),
                       JsReplace(R"(
navigator.leaveAdInterestGroup({name: $1, owner: $2})
    .then(() => {
      // Append the first character of the owner's host to the title.
      document.title += (new URL($2)).host[0];
    });)",
                                 base::NumberToString(i), other_origin));

    // Wait for .well-known requests to be made for "0.b.test" and "1.b.test".
    // Need to wait for them immediately after the Javascript calls that should
    // trigger the requests to prevent their order from being racily reversed at
    // the network layer.
    if (i == 0) {
      permissions_responses[0]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[0]->http_request()->headers.at("Host"),
          "0.b.test"));
    } else if (i == kMaxActiveCrossSiteLeaves - 1) {
      permissions_responses[1]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[1]->http_request()->headers.at("Host"),
          "1.b.test"));
    }
  }

  // Clear title, as each successful leave modifies the title, so need a basic
  // title to start with. Can't set an empty title, so use "_" instead.
  ExecuteScriptAsync(shell(), "document.title='_'");

  // Joining and leaving a same-origin interest group should not be throttled.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(main_origin, "helmets for unicorns"));
  EXPECT_EQ(kSuccess,
            LeaveInterestGroupAndVerify(main_origin, "helmets for unicorns"));

  // The "2.b.test" cross-origin leave should still be waiting for one of the
  // other cross-site leaves to complete.
  EXPECT_FALSE(permissions_responses[2]->has_received_request());

  // Complete the "1.b.test" .well-known request, which should cause the
  // "2.b.test" leave request to be sent to the browser, which should issue
  // another .well-known request.
  TitleWatcher title_watcher1(web_contents(), u"_1");
  permissions_responses[1]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/
      R"({"joinAdInterestGroup" : true, "leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[1]->Done();
  EXPECT_EQ(u"_1", title_watcher1.WaitAndGetTitle());

  // The "2.b.test" cross-origin leave should advance out of the queue and send
  // a .well-known request.
  permissions_responses[2]->WaitForRequest();
  EXPECT_TRUE(base::StartsWith(
      permissions_responses[2]->http_request()->headers.at("Host"),
      "2.b.test"));

  // A new cross-origin join should bypass the leave queue, and start
  // immediately, retrieving the previous .well-known response from the cache.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/cross_origin_server.GetOrigin("1.b.test"),
                /*name=*/base::NumberToString(kMaxActiveCrossSiteLeaves),
                /*priority=*/0.0));

  // Complete the "2.b.test" leave's .well-known request.
  TitleWatcher title_watcher2(web_contents(), u"_12");
  permissions_responses[2]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[2]->Done();
  EXPECT_EQ(u"_12", title_watcher2.WaitAndGetTitle());

  // Complete the "0.b.test" leaves' .well-known request.
  std::u16string final_title =
      u"_12" + std::u16string(kMaxActiveCrossSiteLeaves - 1, u'0');
  TitleWatcher title_watcher3(web_contents(), final_title);
  permissions_responses[0]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"leaveAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[0]->Done();
  EXPECT_EQ(final_title, title_watcher3.WaitAndGetTitle());
}

// Much like CrossOriginJoinQueue, but navigates the page when the queue is
// full. Makes sure started joins complete successfully, and a join that was
// still queued when the frame was navigated away is dropped.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       CrossOriginJoinAndNavigateAway) {
  // This matches the value in navigator_auction.cc in blink/.
  const int kMaxActiveCrossSiteJoins = 20;

  // Since this is using another port from `cross_origin_server` below, the
  // hostname doesn't matter, but use a different one, just in case.
  GURL main_url = https_server_->GetURL("a.test", "/echo");
  url::Origin main_origin = url::Origin::Create(main_url);

  // URL with same origin as `main_url` to navigate to afterwards. Same origin
  // so that the renderer process will be shared.
  GURL same_origin_url = https_server_->GetURL("a.test", "/echo?2");

  net::EmbeddedTestServer cross_origin_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);
  cross_origin_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      permissions_responses;
  // While there should only be 2 .well-known requests in this test, create an
  // extra ControllableHttpResponse so can make sure it never sees a request.
  for (int i = 0; i < 3; ++i) {
    permissions_responses.emplace_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &cross_origin_server,
            "/.well-known/interest-group/permissions/?origin=" +
                base::EscapeQueryParamValue(main_origin.Serialize(),
                                            /*use_plus=*/false)));
  }
  ASSERT_TRUE(cross_origin_server.Start());

  // Navigate to a cross-origin URL.
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  for (int i = 0; i < kMaxActiveCrossSiteJoins + 1; ++i) {
    const char* other_origin_host = "0.b.test";
    if (i == kMaxActiveCrossSiteJoins - 1) {
      other_origin_host = "1.b.test";
    } else if (i == kMaxActiveCrossSiteJoins) {
      other_origin_host = "2.b.test";
    }
    url::Origin other_origin = cross_origin_server.GetOrigin(other_origin_host);
    content_browser_client_.AddToAllowList({other_origin});

    ExecuteScriptAsync(shell(),
                       JsReplace(R"(
navigator.joinAdInterestGroup(
    {name: $1, owner: $2}, /*joinDurationSec=*/ 300);)",
                                 base::NumberToString(i), other_origin));

    // Wait for .well-known requests to be made for "0.b.test" and "1.b.test".
    // Need to wait for them immediately after the Javascript calls that should
    // trigger the requests to prevent their order from being racily reversed at
    // the network layer.
    //
    // Also need to wait for them to make sure the requests reach the browser
    // process before the frame is navigated.
    if (i == 0) {
      permissions_responses[0]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[0]->http_request()->headers.at("Host"),
          "0.b.test"));
    } else if (i == kMaxActiveCrossSiteJoins - 1) {
      permissions_responses[1]->WaitForRequest();
      EXPECT_TRUE(base::StartsWith(
          permissions_responses[1]->http_request()->headers.at("Host"),
          "1.b.test"));
    }
  }

  // Navigate the frame.
  ASSERT_TRUE(NavigateToURL(shell(), same_origin_url));

  // Complete the "1.b.test" .well-known request.
  permissions_responses[1]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[1]->Done();

  // Wait for the "1.b.test" group to be joined successfully.
  while (GetJoinCount(cross_origin_server.GetOrigin("1.b.test"),
                      base::NumberToString(kMaxActiveCrossSiteJoins - 1)) !=
         1) {
    continue;
  }

  // Complete the "0.b.test" .well-known request.
  permissions_responses[0]->Send(
      net::HttpStatusCode::HTTP_OK,
      /*content_type=*/"application/json",
      /*content=*/R"({"joinAdInterestGroup" : true})",
      /*cookies=*/{},
      /*extra_headers=*/{"Access-Control-Allow-Origin: *"});
  permissions_responses[0]->Done();
  // Wait for two of the "0.b.test" groups to be joined successfully.
  while (GetJoinCount(cross_origin_server.GetOrigin("0.b.test"),
                      base::NumberToString(0)) != 1) {
    continue;
  }
  while (GetJoinCount(cross_origin_server.GetOrigin("0.b.test"),
                      base::NumberToString(kMaxActiveCrossSiteJoins - 2)) !=
         1) {
    continue;
  }

  // The "2.b.test" cross-origin join should never have made it to the browser
  // process, let alone to the test server.
  EXPECT_FALSE(permissions_responses[2]->has_received_request());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidBiddingLogicUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "biddingLogicUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingLogicUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidBiddingWasmHelperUrl) {
  const char kScriptTemplate[] = R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          biddingWasmHelperUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())";

  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "biddingWasmHelperUrl 'https://invalid^&' for AuctionAdInterestGroup "
          "with owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(kScriptTemplate, origin_string.c_str())));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidDailyUpdateUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "dailyUpdateUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          dailyUpdateUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidTrustedBiddingSignalsUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(base::StringPrintf(
                "TypeError: Failed to execute 'joinAdInterestGroup' on "
                "'Navigator': trustedBiddingSignalsUrl 'https://invalid^&' for "
                "AuctionAdInterestGroup with owner '%s' and name 'cars' cannot "
                "be resolved to a valid URL.",
                origin_string.c_str()),
            EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          trustedBiddingSignalsUrl: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                      origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidUserBiddingSignals) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "userBiddingSignals for AuctionAdInterestGroup with owner '%s' and "
          "name 'cars' must be a JSON-serializable object.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          userBiddingSignals: function() {},
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "ad renderUrl 'https://invalid^&' for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' cannot be resolved to a valid URL.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl:"https://invalid^&"}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidAdMetadata) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  std::string origin_string = url::Origin::Create(url).Serialize();
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on "
          "'Navigator': ad metadata for AuctionAdInterestGroup with "
          "owner '%s' and name 'cars' must be a JSON-serializable object.",
          origin_string.c_str()),
      EvalJs(shell(), JsReplace(R"(
(async function() {
  let x = {};
  let y = {};
  x.a = y;
  y.a = x;
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          ads: [{renderUrl:"https://test.com", metadata:x}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                                origin_string.c_str())));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       LeaveInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'leaveAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(async function() {
  try {
    await navigator.leaveAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://invalid^&',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionInvalidSeller) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'https://invalid^&' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://invalid^&',
      decisionLogicUrl: 'https://test.com/decision_logic'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionHttpSeller) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'http://test.com' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'http://test.com',
      decisionLogicUrl: 'https://test.com/decision_logic'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidDecisionLogicUrl) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "decisionLogicUrl 'https://invalid^&' for AuctionAdConfig with seller "
      "'https://test.com' cannot be resolved to a valid URL.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://invalid^&'
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidTrustedScoringSignalsUrl) {
  GURL url = https_server_->GetURL("a.test", "/echo");
  url::Origin origin = url::Origin::Create(url);
  ASSERT_TRUE(NavigateToURL(shell(), url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
          "trustedScoringSignalsUrl 'https://invalid^&' for AuctionAdConfig "
          "with seller '%s' cannot be resolved to a valid URL.",
          origin.Serialize().c_str()),
      RunAuctionAndWait(JsReplace(R"({
      seller: $1,
      decisionLogicUrl: $2,
      trustedScoringSignalsUrl: 'https://invalid^&'
  })",
                                  origin, url)));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDecisionLogicUrlDifferentFromSeller) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "decisionLogicUrl 'https://b.test/foo' for AuctionAdConfig with seller "
      "'https://a.test/' must match seller origin.",
      RunAuctionAndWait(R"({
    seller: "https://a.test/",
    decisionLogicUrl: "https://b.test/foo",
    interestGroupBuyers: ["https://c.test/"],
                        })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionTrustedScoringSignalsUrlDifferentFromSeller) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  AttachInterestGroupObserver();

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "trustedScoringSignalsUrl 'https://b.test/foo' for AuctionAdConfig with "
      "seller 'https://a.test/' must match seller origin.",
      RunAuctionAndWait(R"({
    seller: "https://a.test/",
    decisionLogicUrl: "https://a.test/foo",
    trustedScoringSignalsUrl: "https://b.test/foo",
    interestGroupBuyers: ["https://c.test/"],
                        })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "interestGroupBuyers buyer 'https://invalid^&' for AuctionAdConfig "
      "with seller 'https://test.com' must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: ['https://invalid^&'],
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyersStr) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'interestGroupBuyers' property from 'AuctionAdConfig': The "
      "provided value cannot be converted to a sequence.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: 'not an array',
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionNoInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionEmptyInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: [],
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidAuctionSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "auctionSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      auctionSignals: alert
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidSellerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "sellerSignals for AuctionAdConfig with seller 'https://test.com' must "
      "be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      sellerSignals: function() {}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignalsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://invalid^&': {a:1}}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerTimeoutsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerTimeouts buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be \"*\" (wildcard) or a valid https "
      "origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerTimeouts: {'https://invalid^&': 100}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerGroupLimitsValue) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerGroupLimits value '0' for AuctionAdConfig with "
      "seller 'https://test.com' must be greater than 0.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerGroupLimits: {'https://test.com': 0}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerGroupLimitsOrigin) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerGroupLimits buyer 'https://invalid^&' for AuctionAdConfig with "
      "seller 'https://test.com' must be \"*\" (wildcard) or a valid https "
      "origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerGroupLimits: {'https://invalid^&': 100}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionsArray) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'componentAuctions' property from 'AuctionAdConfig': The "
      "provided value cannot be converted to a sequence.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: ''
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionsElementType) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Failed to "
      "read the 'componentAuctions' property from 'AuctionAdConfig': "
      "The provided value is not of type 'AuctionAdConfig'.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: ['test']
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionsAuctionConfig) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': seller "
      "'http://test.com' for AuctionAdConfig must be a valid https origin.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: [{
        seller: 'http://test.com',
        decisionLogicUrl: 'http://test.com'
      }]
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidComponentAuctionDepth) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': Auctions "
      "listed in componentAuctions may not have their own nested "
      "componentAuctions.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      componentAuctions: [{
        seller: 'https://test2.com',
        decisionLogicUrl: 'https://test2.com',
        componentAuctions: [{
          seller: 'https://test3.com',
          decisionLogicUrl: 'https://test3.com',
        }]
      }]
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidPerBuyerSignals) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "perBuyerSignals for AuctionAdConfig with seller 'https://test.com' "
      "must be a JSON-serializable object.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      perBuyerSignals: {'https://test.com': function() {}}
  })"));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionBuyersNoInterestGroup) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  AttachInterestGroupObserver();

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                         })",
                         url::Origin::Create(test_url),
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
  WaitForAccessObserved({});
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionPrivacySandboxDisabled) {
  // Successful join at a.test
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  AttachInterestGroupObserver();

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin_a,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));

  // Auction should not be run since d.test has the API disabled.
  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$3: {even: 'more', x: 4.5}}
          })",
          url::Origin::Create(test_url_d),
          https_server_->GetURL("d.test", "/interest_group/decision_logic.js"),
          test_origin_a)));

  // No requests should have been made for the interest group or auction URLs.
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/bidding_logic.js")));
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/trusted_bidding_signals.json")));
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL("/interest_group/decision_logic.js")));
  WaitForAccessObserved({
      {InterestGroupTestObserver::kJoin, test_origin_a.Serialize(), "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDisabledInterestGroup) {
  // Inject an interest group into the DB for that for a disabled site so we can
  // try to remove it.
  GURL disabled_domain = https_server_->GetURL("d.test", "/");
  url::Origin disabled_origin = url::Origin::Create(disabled_domain);
  AttachInterestGroupObserver();

  blink::InterestGroup disabled_group;
  disabled_group.expiry = base::Time::Now() + base::Seconds(300);
  disabled_group.owner = disabled_origin;
  disabled_group.name = "candy";
  disabled_group.bidding_url = https_server_->GetURL(
      disabled_domain.host(),
      "/interest_group/bidding_logic_stop_bidding_after_win.js");
  disabled_group.ads.emplace();
  disabled_group.ads->emplace_back(blink::InterestGroup::Ad(
      GURL("https://stop_bidding_after_win.com/render"), absl::nullopt));
  manager_->JoinInterestGroup(std::move(disabled_group), disabled_domain);
  ASSERT_EQ(1, GetJoinCount(disabled_origin, "candy"));

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(test_url.host(),
                                "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(test_url.host(),
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL(test_url.host(),
                            "/interest_group/decision_logic.js"),
      disabled_origin);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);
  // No requests should have been made for the disabled interest group's URLs.
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(
      received_https_test_server_requests_,
      https_server_->GetURL(
          "/interest_group/bidding_logic_stop_bidding_after_win.js")));
  WaitForAccessObserved({
      {InterestGroupTestObserver::kJoin, disabled_origin.Serialize(), "candy"},
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    sellerTimeout: 200,
    perBuyerSignals: {$1: {even: 'more', x: 4.5}},
    perBuyerTimeouts: {$1: 100, '*': 150}
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  // InterestGroupAccessObserver never was activated, so nothing was observed.
  WaitForAccessObserved({});

  // Check ResourceRequest structs of requests issued by the worklet process.
  const struct ExpectedRequest {
    GURL url;
    const char* accept_header;
    bool expect_trusted_params;
  } kExpectedRequests[] = {
      {https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
       "application/javascript", /*expect_trusted_params=*/true},
      {https_server_->GetURL(
           "a.test",
           "/interest_group/"
           "trusted_bidding_signals.json?hostname=a.test&keys=key1"),
       "application/json", /*expect_trusted_params=*/true},
      {https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
       "application/javascript", /*expect_trusted_params=*/false},
  };
  for (const auto& expected_request : kExpectedRequests) {
    SCOPED_TRACE(expected_request.url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_request.url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_EQ(1u, request->headers.GetHeaderVector().size());
    std::string accept_value;
    ASSERT_TRUE(request->headers.GetHeader(net::HttpRequestHeaders::kAccept,
                                           &accept_value));
    EXPECT_EQ(expected_request.accept_header, accept_value);

    EXPECT_EQ(expected_request.expect_trusted_params,
              request->trusted_params.has_value());
    EXPECT_EQ(network::mojom::RequestMode::kNoCors, request->mode);
    if (request->trusted_params) {
      // Requests for interest-group provided URLs are cross-origin to the
      // publisher page, and set trusted params to use the right cache shard,
      // using a trusted URLLoaderFactory.
      const net::IsolationInfo& isolation_info =
          request->trusted_params->isolation_info;
      EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
                isolation_info.request_type());
      url::Origin expected_origin = url::Origin::Create(expected_request.url);
      EXPECT_EQ(expected_origin, isolation_info.top_frame_origin());
      EXPECT_EQ(expected_origin, isolation_info.frame_origin());
      EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
    }
  }

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      https_server_->GetURL("a.test", "/echoall?report_bidder"),
  };
  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Wait for the report URL to be fetched, which only happens after the
    // auction has completed.
    WaitForURL(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_report_url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }

  // The two reporting requests should use different NIKs to prevent the
  // requests from being correlated.
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[0])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[1])
                ->trusted_params->isolation_info.network_isolation_key());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithWinnerReplacedURN) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url =
      GURL(https_server_->GetURL("c.test", "/%%echo%%?${INTEREST_GROUP_NAME}")
               .spec());
  GURL expected_ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/test_origin,
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, "{ad:'metadata', here:[1,2]}"}}}));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  auto result = RunAuctionAndWait(auction_config,
                                  /*execution_target=*/absl::nullopt);
  GURL urn_url = GURL(result.ExtractString());
  EXPECT_TRUE(urn_url.is_valid());
  EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

  {
    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_TRUE(observer.mapped_url()) << urn_url;
    EXPECT_EQ(ad_url, observer.mapped_url());
  }

  EXPECT_TRUE(ReplaceInURNInJS(
      urn_url,
      {{"${INTEREST_GROUP_NAME}", "render_cars"}, {"%%echo%%", "echo"}}));

  {
    TestFencedFrameURLMappingResultObserver observer;
    ConvertFencedFrameURNToURL(urn_url, &observer);
    EXPECT_EQ(expected_ad_url, observer.mapped_url());
  }
  NavigateIframeAndCheckURL(web_contents(), urn_url, expected_ad_url);
}

IN_PROC_BROWSER_TEST_F(
    InterestGroupBrowserTest,
    RunAdAuctionPerBuyerSignalsAndPerBuyerTimeoutsOriginNotInBuyers) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  AttachInterestGroupObserver();

  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{ad_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  RunAuctionAndWaitForURLAndNavigateIframe(
      JsReplace(
          R"({
            seller: $1,
            decisionLogicUrl: $2,
            interestGroupBuyers: [$1],
            perBuyerSignals: {$1: {a:1}, 'https://not_in_buyers.com': {a:1}},
            perBuyerTimeouts: {'https://not_in_buyers.com': 100}
          })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      ad_url);
  WaitForAccessObserved(
      {{InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"}});
}

// Runs an auction where the bidding function uses a WASM helper.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithBidderWasm) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/bidding_logic_use_wasm.js"),
          /*bidding_wasm_helper_url=*/
          https_server_->GetURL("a.test", "/interest_group/multiply.wasm"),
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/{},
          /*user_bidding_signals=*/"{}",
          /*ads=*/{{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));
  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        interestGroupBuyers: [$1],
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithDebugReporting) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_winner");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"winner",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_with_debugging_report.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"bikes",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_with_debugging_report.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"shoes",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_with_debugging_report.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/{{{ad3_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL(
          "a.test", "/interest_group/decision_logic_with_debugging_report.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      // Return value from seller's ReportResult() method.
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      // Return value from winning bidder's ReportWin() method.
      https_server_->GetURL("a.test", "/echoall?report_bidder/winner"),
      // Debugging report URL from seller for win report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_win/winner"),
      // Debugging report URL from winning bidder for win report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_win/winner"),
      // Debugging report URL from seller for loss report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_loss/bikes"),
      https_server_->GetURL("a.test", "/echo?seller_debug_report_loss/shoes"),
      // Debugging report URL from losing bidders for loss report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_loss/bikes"),
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_loss/shoes")};

  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Wait for the report URL to be fetched, which only happens after the
    // auction has completed.
    WaitForURL(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_report_url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }

  // The reporting requests should use different NIKs to prevent the requests
  // from being correlated.
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[0])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[2])
                ->trusted_params->isolation_info.network_isolation_key());
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[2])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[3])
                ->trusted_params->isolation_info.network_isolation_key());
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[2])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[4])
                ->trusted_params->isolation_info.network_isolation_key());
}

// All bidders' genereteBid() failed so no bid was made, thus no render url.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithDebugReportingNoBid) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"shoes",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test", "/interest_group/bidding_logic_loop_forever.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"bikes",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test", "/interest_group/bidding_logic_throws.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));

  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
                })",
          test_origin,
          https_server_->GetURL(
              "a.test",
              "/interest_group/decision_logic_with_debugging_report.js"))));

  // Debugging loss reports which are made before generateBid()'s timeout and
  // error-throwing statements should be sent. Those made after that should not
  // be sent.
  const GURL kExpectedReportUrls[] = {
      // Debugging loss report URL (before the timeout) from bidder whose
      // generateBid() timed out.
      https_server_->GetURL(
          "a.test", "/echo?bidder_debug_report_loss/shoes/before_timeout"),
      // Debugging loss report URL (before the error) from bidder whose
      // generateBid() throws an error.
      https_server_->GetURL(
          "a.test", "/echo?bidder_debug_report_loss/bikes/before_error")};

  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Wait for the report URL to be fetched, which only happens after the
    // auction has completed.
    WaitForURL(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_report_url);
    ASSERT_TRUE(request);
  }
}

// Runs auction just like test InterestGroupBrowserTest.RunAdAuctionWithWinner,
// but runs with fenced frames enabled and expects to receive a URN URL to be
// used. After the auction, loads the URL in a fenced frame, and expects the
// correct URL is loaded.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url =
      https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          {{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
                  test_origin,
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // InterestGroupAccessObserver never was activated, so nothing was observed.
  WaitForAccessObserved({});

  // Check ResourceRequest structs of requests issued by the worklet process.
  const struct ExpectedRequest {
    GURL url;
    const char* accept_header;
    bool expect_trusted_params;
  } kExpectedRequests[] = {
      {https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
       "application/javascript", /*expect_trusted_params=*/true},
      {https_server_->GetURL(
           "a.test",
           "/interest_group/"
           "trusted_bidding_signals.json?hostname=a.test&keys=key1"),
       "application/json", /*expect_trusted_params=*/true},
      {https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
       "application/javascript", /*expect_trusted_params=*/false},
  };
  for (const auto& expected_request : kExpectedRequests) {
    SCOPED_TRACE(expected_request.url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_request.url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_EQ(1u, request->headers.GetHeaderVector().size());
    std::string accept_value;
    ASSERT_TRUE(request->headers.GetHeader(net::HttpRequestHeaders::kAccept,
                                           &accept_value));
    EXPECT_EQ(expected_request.accept_header, accept_value);

    EXPECT_EQ(expected_request.expect_trusted_params,
              request->trusted_params.has_value());
    EXPECT_EQ(network::mojom::RequestMode::kNoCors, request->mode);
    if (request->trusted_params) {
      // Requests for interest-group provided URLs are cross-origin to the
      // publisher page, and set trusted params to use the right cache shard,
      // using a trusted URLLoaderFactory.
      const net::IsolationInfo& isolation_info =
          request->trusted_params->isolation_info;
      EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
                isolation_info.request_type());
      url::Origin expected_origin = url::Origin::Create(expected_request.url);
      EXPECT_EQ(expected_origin, isolation_info.top_frame_origin());
      EXPECT_EQ(expected_origin, isolation_info.frame_origin());
      EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
    }
  }

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      https_server_->GetURL("a.test", "/echoall?report_bidder"),
  };
  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Wait for the report URL to be fetched, which only happens after the
    // auction has completed.
    WaitForURL(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_report_url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(test_origin, request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }

  // The two reporting requests should use different NIKs to prevent the
  // requests from being correlated.
  EXPECT_NE(url_loader_monitor.GetRequestInfo(kExpectedReportUrls[0])
                ->trusted_params->isolation_info.network_isolation_key(),
            url_loader_monitor.GetRequestInfo(kExpectedReportUrls[1])
                ->trusted_params->isolation_info.network_isolation_key());
}

IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinnerReplacedURN) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url =
      https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: %%LOADING_MODE%%");
  GURL expected_ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          {{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  content::EvalJsResult urn_url_string = RunAuctionAndWait(
      JsReplace(
          R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      shell());
  ASSERT_TRUE(urn_url_string.value.is_string())
      << "Expected string, but got " << urn_url_string.value;

  GURL urn_url(urn_url_string.ExtractString());
  ASSERT_TRUE(urn_url.is_valid())
      << "URL is not valid: " << urn_url_string.ExtractString();
  EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

  ReplaceInURNInJS(urn_url, {{"%%LOADING_MODE%%", "fenced-frame"}});

  NavigateFencedFrameAndWait(urn_url, expected_ad_url, shell());
}

// Runs two ad auctions with fenced frames enabled. Both auctions should
// succeed and are then loaded in separate fenced frames. Both auctions try to
// leave the interest group, but only the one whose ad matches the joining
// origin should succeed.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       RunTwoAdAuctionWithWinnerLeaveGroup) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL(
      "a.test",
      base::StringPrintf(
          "/cross_site_iframe_factory.html?a(%s,%s)",
          base::EscapeUrlEncodedData(
              https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html")
                  .spec(),
              /*use_plus=*/false)
              .c_str(),
          base::EscapeUrlEncodedData(
              https_server_->GetURL("b.test", "/fenced_frames/opaque_ads.html")
                  .spec(),
              /*use_plus=*/false)
              .c_str()));
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  RenderFrameHost* rfh1 = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  ASSERT_TRUE(rfh1);
  RenderFrameHost* rfh2 = ChildFrameAt(web_contents()->GetMainFrame(), 1);
  ASSERT_TRUE(rfh2);
  url::Origin test_origin = url::Origin::Create(test_url);

  AttachInterestGroupObserver();
  GURL ad_url = https_server_->GetURL(
      "a.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::InterestGroup(
              /*expiry=*/base::Time(),
              /*owner=*/test_origin,
              /*name=*/"cars",
              /*priority=*/0.0,
              /*bidding_url=*/
              https_server_->GetURL(
                  "a.test",
                  // Using bidding_logic_stop_bidding_after_win.js ensures the
                  // "cars" interest group wins the first auction (whose
                  // leaveAdInterestGroup call succeeds).
                  "/interest_group/bidding_logic_stop_bidding_after_win.js"),
              /*bidding_wasm_helper_url=*/absl::nullopt,
              /*daily_update_url=*/absl::nullopt,
              /*trusted_bidding_signals_url=*/
              https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"),
              /*trusted_bidding_signals_keys=*/{{"key1"}},
              /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
              {{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
              /*ad_components=*/absl::nullopt),
          rfh1));

  GURL ad_url2 = https_server_->GetURL(
      "b.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          blink::InterestGroup(
              /*expiry=*/base::Time(),
              /*owner=*/test_origin,
              /*name=*/"trucks",
              /*priority=*/0.0,
              /*bidding_url=*/
              https_server_->GetURL("a.test",
                                    "/interest_group/bidding_logic.js"),
              /*bidding_wasm_helper_url=*/absl::nullopt,
              /*daily_update_url=*/absl::nullopt,
              /*trusted_bidding_signals_url=*/
              https_server_->GetURL(
                  "a.test", "/interest_group/trusted_bidding_signals.json"),
              /*trusted_bidding_signals_keys=*/{{"key1"}},
              /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
              {{{ad_url2, "{ad:'metadata', here:[1,2]}"}}},
              /*ad_components=*/absl::nullopt),
          rfh1));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url,
      JsReplace(
          R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      rfh1));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url2,
      JsReplace(
          R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
          test_origin,
          https_server_->GetURL("a.test", "/interest_group/decision_logic.js")),
      rfh2));

  // InterestGroupAccessObserver should see the join, auction, and implicit
  // leave. Note that the implicit leave for "trucks" does not succeed because
  // leaveAdInterestGroup is not called from the Interest Group owner's frame.
  WaitForAccessObserved(
      {{InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "trucks"},
       {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kBid, test_origin.Serialize(), "trucks"},
       {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kLeave, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kBid, test_origin.Serialize(), "trucks"},
       {InterestGroupTestObserver::kWin, test_origin.Serialize(), "trucks"}});

  // The ad should have left the "cars" interest group when the page was shown.
  auto groups = GetAllInterestGroups();
  ASSERT_EQ(1u, groups.size());
  EXPECT_EQ("trucks", groups[0].second);
}

// Runs ad auction with fenced frames enabled. The auction should succeed and
// be loaded in a fenced frames. The displayed ad leaves the interest group
// from a nested iframe.
//
// TODO(crbug.com/1320438): Re-enable the test.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinnerNestedLeaveGroup) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url =
      https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  AttachInterestGroupObserver();
  GURL inner_url = https_server_->GetURL(
      "a.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  GURL ad_url = https_server_->GetURL(
      "b.test", "/fenced_frames/outer_inner_frame_as_param.html");
  GURL::Replacements rep;
  std::string query = "innerFrame=" + base::EscapeUrlEncodedData(
                                          inner_url.spec(), /*use_plus=*/false);
  rep.SetQueryStr(query);
  ad_url = ad_url.ReplaceComponents(rep);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          {{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1],
auctionSignals: {x: 1},
sellerSignals: {yet: 'more', info: 1},
perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                  })",
                  test_origin,
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // InterestGroupAccessObserver should see the join, auction, and implicit
  // leave.
  WaitForAccessObserved(
      {{InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kLeave, test_origin.Serialize(), "cars"}});

  // The ad should have left the interest group when the page was shown.
  EXPECT_EQ(0u, GetAllInterestGroups().size());
}

// Creates a Fenced Frame and then tries to use the leaveAdInterestGroup API.
// Leaving the interest group should silently fail.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       LeaveAdInterestGroupNoAuction) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "a.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  AttachInterestGroupObserver();
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          {{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  // Navigate fenced frame with no ad.
  ASSERT_NO_FATAL_FAILURE(NavigateFencedFrameAndWait(ad_url, ad_url, shell()));

  // InterestGroupAccessObserver should see the join.
  WaitForAccessObserved(
      {{InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"}});

  // The ad should not have left the interest group when the page was shown.
  EXPECT_EQ(1u, GetAllInterestGroups().size());
}

// Use different origins for publisher, bidder, and seller, and make sure
// everything works as expected.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOrigin) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kSeller[] = "c.test";

  AttachInterestGroupObserver();

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(kBidder, "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidder,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  // Navigate to publisher.
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kPublisher, "/echo")));

  GURL seller_logic_url = https_server_->GetURL(
      kSeller, "/interest_group/decision_logic_need_signals.js");
  // Register a seller script that only bids if the `trustedScoringSignals` are
  // successfully fetched.
  network_responder_->RegisterNetworkResponse(seller_logic_url.path(), R"(
function scoreAd(
    adMetadata, bid, auctionConfig, trustedScoringSignals, browserSignals) {
  // Reject bits if trustedScoringSignals is not received.
  if (trustedScoringSignals.renderUrl["https://example.com/render"] === "foo")
    return bid;
  return 0;
}

function reportResult(
  auctionConfig, browserSignals) {
  sendReportTo(auctionConfig.seller + '/echoall?report_seller');
  return {
    'success': true,
    'signalsForWinner': {'signalForWinner': 1},
    'reportUrl': auctionConfig.seller + '/report_seller',
  };
}
)",
                                              "application/javascript");

  // Run an auction with the scoring script. It should succeed.
  ASSERT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(JsReplace(
                R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  trustedScoringSignalsUrl: $3,
  interestGroupBuyers: [$4],
}
                )",
                url::Origin::Create(seller_logic_url), seller_logic_url,
                https_server_->GetURL(
                    kSeller, "/interest_group/trusted_scoring_signals.json"),
                bidder_origin)));

  WaitForAccessObserved({
      {InterestGroupTestObserver::kJoin, bidder_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kBid, bidder_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kWin, bidder_origin.Serialize(), "cars"},
  });

  // Reporting urls should be fetched after an auction succeeded.
  WaitForURL(https_server_->GetURL("/echoall?report_seller"));
  WaitForURL(https_server_->GetURL("/echoall?report_bidder"));
  // Double-check that the trusted scoring signals URL was requested as well.
  WaitForURL(
      https_server_->GetURL("/interest_group/trusted_scoring_signals.json"
                            "?hostname=a.test"
                            "&renderUrls=https%3A%2F%2Fexample.com%2Frender"));
}

// Test that ad_components in an iframe ad are requested.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWinnerWithComponents) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url =
      https_server_->GetURL("c.test", "/fenced_frames/ad_with_components.html");
  GURL component_url = https_server_->GetURL("c.test", "/echo?component");
  AttachInterestGroupObserver();

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/{{{component_url, "{ad:'component metadata'}"}}})));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  WaitForAccessObserved({
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"},
  });

  // Wait for the component to load.
  WaitForURL(component_url);
}

// Make sure correct topFrameHostname is passed in. Check auctions from top
// frames, and iframes of various depth.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, TopFrameHostname) {
  // Buyer, seller, and iframe all use the same host.
  const char kOtherHost[] = "b.test";
  // Top frame host is unique.
  const char kTopFrameHost[] = "a.test";

  // Navigate to bidder site, and add an interest group.
  GURL other_url = https_server_->GetURL(kOtherHost, "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  ASSERT_TRUE(NavigateToURL(shell(), other_url));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/other_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(
              kOtherHost,
              "/interest_group/bidding_logic_expect_top_frame_a_test.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  const struct {
    int depth;
    std::string top_frame_path;
    const char* seller_path;
  } kTestCases[] = {
      {0, "/echo", "/interest_group/decision_logic_expect_top_frame_a_test.js"},
      {1,
       base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s)",
                          kOtherHost),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
      {2,
       base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s(%s))",
                          kOtherHost, kOtherHost),
       "/interest_group/decision_logic_expect_top_frame_a_test.js"},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.depth);

    // Navigate to publisher, with the cross-site iframe..
    ASSERT_TRUE(NavigateToURL(
        shell(),
        https_server_->GetURL(kTopFrameHost, test_case.top_frame_path)));

    RenderFrameHost* frame = web_contents()->GetMainFrame();
    EXPECT_EQ(https_server_->GetOrigin(kTopFrameHost),
              frame->GetLastCommittedOrigin());
    for (int i = 0; i < test_case.depth; ++i) {
      frame = ChildFrameAt(frame, 0);
      ASSERT_TRUE(frame);
      EXPECT_EQ(other_origin, frame->GetLastCommittedOrigin());
    }

    // Run auction with a seller script with an "Access-Control-Allow-Origin"
    // header. The auction should succeed.
    GURL seller_logic_url =
        https_server_->GetURL(kOtherHost, test_case.seller_path);
    ASSERT_EQ("https://example.com/render",
              RunAuctionAndWaitForURL(JsReplace(
                                          R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3],
  auctionSignals: {x: 1},
  sellerSignals: {yet: 'more', info: 1},
  perBuyerSignals: {$3: {even: 'more', x: 4.5}}
}
                                    )",
                                          url::Origin::Create(seller_logic_url),
                                          seller_logic_url, other_origin),
                                      frame));

    // Reporting urls should be fetched after an auction succeeded.
    WaitForURL(https_server_->GetURL("/echoall?report_seller"));
    WaitForURL(https_server_->GetURL("/echoall?report_bidder"));
    ClearReceivedRequests();
  }
}

// Test running auctions in cross-site iframes, and loading the winner into a
// nested fenced frame.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest, Iframe) {
  // Use different hostnames for each participant.
  const char kTopFrameHost[] = "a.test";
  const char kBidderHost[] = "b.test";
  const char kSellerHost[] = "c.test";
  const char kIframeHost[] = "d.test";
  const char kAdHost[] = "ad.d.test";
  content_browser_client_.AddToAllowList(
      {url::Origin::Create(https_server_->GetURL(kIframeHost, "/"))});

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));

  GURL ad_url = https_server_->GetURL(
      kAdHost, "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/bidder_origin,
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              kBidderHost, "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  GURL main_frame_url = https_server_->GetURL(
      kTopFrameHost,
      base::StringPrintf(
          "/cross_site_iframe_factory.html?%s(%s)", kTopFrameHost,
          https_server_->GetURL(kIframeHost, "/fenced_frames/opaque_ads.html")
              .spec()
              .c_str()));
  ASSERT_TRUE(NavigateToURL(shell(), main_frame_url));

  RenderFrameHost* iframe = ChildFrameAt(web_contents()->GetMainFrame(), 0);
  ASSERT_TRUE(iframe);
  EXPECT_EQ(kIframeHost, iframe->GetLastCommittedOrigin().host());

  GURL seller_logic_url =
      https_server_->GetURL(kSellerHost, "/interest_group/decision_logic.js");
  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url,
      JsReplace(
          R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3]
}
          )",
          url::Origin::Create(seller_logic_url), seller_logic_url,
          bidder_origin),
      iframe));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithWinnerManyInterestGroups) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_stop_bidding_after_win.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"bikes",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"shoes",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad3_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  // Seller and winning bidder should get reports, and other bidders shouldn't
  // get reports.
  WaitForURL(https_server_->GetURL("/echoall?report_seller"));
  WaitForURL(
      https_server_->GetURL("/echoall?report_bidder_stop_bidding_after_win"));
  base::AutoLock auto_lock(requests_lock_);
  EXPECT_FALSE(base::Contains(received_https_test_server_requests_,
                              https_server_->GetURL("/echoall?report_bidder")));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionAllGroupsLimited) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  AttachInterestGroupObserver();

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/2.3,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"bikes",
          /*priority=*/2.2,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"shoes",
          /*priority=*/2.1,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad3_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    perBuyerGroupLimits: {'*': 1},
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  WaitForAccessObserved({
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "bikes"},
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "shoes"},
      {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionOneGroupLimited) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  GURL test_url2 = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  url::Origin test_origin2 = url::Origin::Create(test_url2);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  AttachInterestGroupObserver();

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"cars",
                /*priority=*/3,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_stop_bidding_after_win.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"bikes",
          /*priority=*/2,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"shoes",
          /*priority=*/1,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad3_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin2,
          /*name=*/"cars",
          /*priority=*/3,
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin2,
          /*name=*/"bikes",
          /*priority=*/2,
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("b.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin2,
          /*name=*/"shoes",
          /*priority=*/1,
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad3_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  std::string auction_config = JsReplace(
      R"({
    seller: $3,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
    perBuyerGroupLimits: {$1: 1, '*': 2},
                })",
      test_origin,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      test_origin2);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  WaitForAccessObserved({
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "bikes"},
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "shoes"},
      {InterestGroupTestObserver::kJoin, test_origin2.Serialize(), "cars"},
      {InterestGroupTestObserver::kJoin, test_origin2.Serialize(), "bikes"},
      {InterestGroupTestObserver::kJoin, test_origin2.Serialize(), "shoes"},
      {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kBid, test_origin2.Serialize(), "bikes"},
      {InterestGroupTestObserver::kBid, test_origin2.Serialize(), "cars"},
      {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionOneGroupHighLimit) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  GURL test_url2 = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  url::Origin test_origin2 = url::Origin::Create(test_url2);
  GURL ad1_url = https_server_->GetURL("c.test", "/echo?render_cars");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");
  GURL ad3_url = https_server_->GetURL("c.test", "/echo?render_shoes");
  AttachInterestGroupObserver();

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"cars",
                /*priority=*/3,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_stop_bidding_after_win.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"bikes",
          /*priority=*/2,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"shoes",
          /*priority=*/1,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad3_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin2,
          /*name=*/"cars",
          /*priority=*/3,
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad1_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin2,
          /*name=*/"bikes",
          /*priority=*/2,
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("b.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin2,
          /*name=*/"shoes",
          /*priority=*/1,
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad3_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));
  std::string auction_config = JsReplace(
      R"({
    seller: $3,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
    perBuyerGroupLimits: {$3: 3, '*': 1},
                })",
      test_origin,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      test_origin2);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  WaitForAccessObserved({
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "bikes"},
      {InterestGroupTestObserver::kJoin, test_origin.Serialize(), "shoes"},
      {InterestGroupTestObserver::kJoin, test_origin2.Serialize(), "cars"},
      {InterestGroupTestObserver::kJoin, test_origin2.Serialize(), "bikes"},
      {InterestGroupTestObserver::kJoin, test_origin2.Serialize(), "shoes"},
      {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
      {InterestGroupTestObserver::kBid, test_origin2.Serialize(), "bikes"},
      {InterestGroupTestObserver::kBid, test_origin2.Serialize(), "cars"},
      {InterestGroupTestObserver::kBid, test_origin2.Serialize(), "shoes"},
      {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionMultipleAuctions) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  const url::Origin origin = url::Origin::Create(test_url);

  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  // This group will win if it has never won an auction.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_stop_bidding_after_win.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/{{{ad1_url, "{ad:'metadata', here:[1,2]}"}}},
                /*ad_components=*/absl::nullopt)));

  GURL test_url2 = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  const url::Origin origin2 = url::Origin::Create(test_url2);
  // This group will win if the other interest group has won an auction.
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/origin2,
          /*name=*/"shoes",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  // Both owners have one interest group in storage, and both interest groups
  // have no `prev_wins`.
  std::vector<StorageInterestGroup> storage_interest_groups =
      GetInterestGroupsForOwner(origin);
  EXPECT_EQ(storage_interest_groups.size(), 1u);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      0u);
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            0);
  std::vector<StorageInterestGroup> storage_interest_groups2 =
      GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(storage_interest_groups2.size(), 1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            0u);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            0);

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
      origin2,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      origin);
  // Run an ad auction. Interest group cars of owner `test_url` wins.
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  // `prev_wins` of `test_url`'s interest group cars is updated in storage.
  storage_interest_groups = GetInterestGroupsForOwner(origin);
  storage_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            0u);
  EXPECT_EQ(
      storage_interest_groups.front()
          .bidding_browser_signals->prev_wins.front()
          ->ad_json,
      JsReplace(
          R"({"render_url":$1,"metadata":{"ad":"metadata","here":[1,2]}})",
          ad1_url));
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            1);

  // Start observer in the middle.
  AttachInterestGroupObserver();

  // Run auction again. Interest group shoes of owner `test_url2` wins.
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);
  // `test_url2`'s interest group shoes has one `prev_wins` in storage.
  storage_interest_groups = GetInterestGroupsForOwner(origin);
  storage_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.front()
                ->ad_json,
            JsReplace(R"({"render_url":$1})", ad2_url));
  // First interest group didn't bid this time.
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            2);

  // Run auction third time, and only interest group "shoes" bids this time.
  auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
      origin2,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);
  // `test_url2`'s interest group shoes has two `prev_wins` in storage.
  storage_interest_groups = GetInterestGroupsForOwner(origin);
  storage_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_browser_signals->prev_wins.size(),
      1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.size(),
            2u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_browser_signals->prev_wins.back()
                ->ad_json,
            JsReplace(R"({"render_url":$1})", ad2_url));
  // First interest group didn't bid this time.
  EXPECT_EQ(storage_interest_groups.front().bidding_browser_signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_browser_signals->bid_count,
            3);
  // Observer was not active for joins and first auction.
  WaitForAccessObserved({
      {InterestGroupTestObserver::kBid, origin2.Serialize(), "shoes"},
      {InterestGroupTestObserver::kWin, origin2.Serialize(), "shoes"},
      {InterestGroupTestObserver::kBid, origin2.Serialize(), "shoes"},
      {InterestGroupTestObserver::kWin, origin2.Serialize(), "shoes"},
  });
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ReportingMultipleAuctions) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  const url::Origin origin_a = url::Origin::Create(test_url_a);

  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  // This group will win if it has never won an auction.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/origin_a,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_stop_bidding_after_win.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/{{{ad1_url, "{ad:'metadata', here:[1,2]}"}}},
                /*ad_components=*/absl::nullopt)));

  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  const url::Origin origin_b = url::Origin::Create(test_url_b);
  // This group will win if the other interest group has won an auction.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/origin_b,
                /*name=*/"shoes",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "b.test",
                    "/interest_group/bidding_logic_with_debugging_report.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
      origin_b,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      origin_a);
  // Setting a small reporting interval to run the test faster.
  manager_->set_reporting_interval_for_testing(base::Milliseconds(1));

  // Run an ad auction. Interest group cars of owner `test_url_a` wins.
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad1_url);

  // Run auction again on the same page. Interest group shoes of owner
  // `test_url2` wins.
  auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1, $3],
  })",
      origin_b,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      origin_a);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);

  // Run the third auction on another page c.test, and only interest group
  // "shoes" of c.test bids this time.
  GURL test_url_c = https_server_->GetURL("c.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_c));
  const url::Origin origin_c = url::Origin::Create(test_url_c);

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/origin_c,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "c.test",
                    "/interest_group/bidding_logic_with_debugging_report.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));

  auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    sellerSignals: {reportTo: $3},
                })",
      origin_c,
      https_server_->GetURL(
          "c.test",
          "/interest_group/decision_logic_report_to_seller_signals.js"),
      https_server_->GetURL("c.test", "/echoall?report_seller/cars"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);

  // Check ResourceRequest structs of report requests.
  // The URLs must not have the same path with different hostnames, because
  // WaitForURL() always replaces hostnames with "127.0.0.1", thus only waits
  // for the first URL among URLs with the same path.
  const struct ExpectedReportRequest {
    GURL url;
    url::Origin request_initiator;
  } kExpectedReportRequests[] = {
      // First auction's seller's ReportResult() URL.
      {https_server_->GetURL("b.test", "/echoall?report_seller"), origin_b},
      // First auction's winning bidder's ReportWin() URL.
      {https_server_->GetURL("a.test",
                             "/echoall?report_bidder_stop_bidding_after_win"),
       origin_b},
      // First auction's debugging loss report URL from bidder.
      {https_server_->GetURL("b.test", "/echo?bidder_debug_report_loss/shoes"),
       origin_b},

      // Second auction's seller's ReportResult() URL. Although this URL is the
      // second time requesting this URL, this test does not confirm that we
      // requested the URL twice unfortunately.
      // TODO(qingxinwu): Update the test fixture's use of RequestMonitor
      // instead of URLLoaderMonitor to handle duplicate URLs.
      {https_server_->GetURL("b.test", "/echoall?report_seller"), origin_b},
      // Second auction's winning bidder's ReportWin() URL.
      {https_server_->GetURL("b.test", "/echoall?report_bidder/shoes"),
       origin_b},
      // Second auction's debugging win report URL from bidder.
      {https_server_->GetURL("b.test", "/echo?bidder_debug_report_win/shoes"),
       origin_b},

      // Third auction's seller's ReportResult() URL.
      {https_server_->GetURL("c.test", "/echoall?report_seller/cars"),
       origin_c},
      // Third auction's winning bidder's ReportWin() URL.
      {https_server_->GetURL("c.test", "/echoall?report_bidder/cars"),
       origin_c},
      // Third auction's debugging win report URL from seller.
      {https_server_->GetURL("c.test",
                             "/echoall?report_seller/cars_debug_win_report"),
       origin_c},
      // Third auction's debugging win report URL from bidder.
      {https_server_->GetURL("c.test", "/echo?bidder_debug_report_win/cars"),
       origin_c}};

  for (const auto& expected_report_request : kExpectedReportRequests) {
    SCOPED_TRACE(expected_report_request.url);

    // Wait for the report URL to be fetched.
    WaitForURL(expected_report_request.url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_report_request.url);
    ASSERT_TRUE(request);
    EXPECT_EQ(network::mojom::CredentialsMode::kOmit,
              request->credentials_mode);
    EXPECT_EQ(network::mojom::RedirectMode::kError, request->redirect_mode);
    EXPECT_EQ(expected_report_request.request_initiator,
              request->request_initiator);

    EXPECT_TRUE(request->headers.IsEmpty());

    ASSERT_TRUE(request->trusted_params);
    const net::IsolationInfo& isolation_info =
        request->trusted_params->isolation_info;
    EXPECT_EQ(net::IsolationInfo::RequestType::kOther,
              isolation_info.request_type());
    EXPECT_TRUE(isolation_info.network_isolation_key().IsTransient());
    EXPECT_TRUE(isolation_info.site_for_cookies().IsNull());
  }
}

// Adding an interest group and then immediately running the ad acution, without
// waiting in between, should always work because although adding the interest
// group is async (and intentionally without completion notification), it should
// complete before the auction runs.
//
// On regression, this test will likely only fail with very low frequency.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       AddInterestGroupRunAuctionWithWinnerWithoutWaiting) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");
  const char kName[] = "cars";

  // All joinAdInterestGroup wrapper calls wait for the returned promise to
  // complete. Inline the call to avoid waiting.
  EXPECT_EQ(
      "done",
      EvalJs(shell(),
             base::StringPrintf(
                 R"(
(function() {
  navigator.joinAdInterestGroup(
    {
      name: '%s',
      owner: '%s',
      biddingLogicUrl: '%s',
      ads: %s
    },
    /*joinDurationSec=*/ 300);
  return 'done';
})())",
                 kName, test_origin.Serialize().c_str(),
                 https_server_
                     ->GetURL("a.test", "/interest_group/bidding_logic.js")
                     .spec()
                     .c_str(),
                 MakeAdsArg({{{ad_url, "{ad:'metadata', here : [1,2] }"}}})
                     .c_str())));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));

  // All leaveAdInterestGroup wrapper calls wait for the returned promise to
  // complete. Inline the call to avoid waiting.
  EXPECT_EQ("done", EvalJs(shell(), JsReplace(R"(
(function() {
  navigator.leaveAdInterestGroup({name: $1, owner: $2});
  return 'done';
})())",
                                              kName, test_origin)));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                         })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
}

// The winning ad's render url is invalid (invalid url or has http scheme).
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithInvalidAdUrl) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(
              "a.test", "/interest_group/bidding_logic_invalid_ad_url.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/
          {{{GURL("https://shoes.com/render"),
             "{ad:'metadata', here : [1,2] }"}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                         })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
}

// Test that when there are no ad components, an array of ad components is still
// available, and they're all mapped to about:blank.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest, NoAdComponents) {
  GURL test_url =
      https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Trying to retrieve the adAuctionComponents of the main frame should throw
  // an exception.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(shell(), 1));

  GURL ad_url =
      https_server_->GetURL("c.test", "/fenced_frames/opaque_ads.html");
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/url::Origin::Create(test_url),
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
                          /*ad_components=*/absl::nullopt));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1]
}
                  )",
                  url::Origin::Create(test_url),
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // Check that adAuctionComponents() returns an array of URNs that all map to
  // about:blank.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  CheckAdComponents(/*expected_ad_component_urls=*/std::vector<GURL>{},
                    ad_frame);

  absl::optional<std::vector<GURL>> all_component_urls =
      GetAdAuctionComponentsInJS(ad_frame, blink::kMaxAdAuctionAdComponents);
  ASSERT_TRUE(all_component_urls);
  NavigateFencedFrameAndWait((*all_component_urls)[0],
                             GURL(url::kAboutBlankURL),
                             GetFencedFrameRenderFrameHost(shell()));
  NavigateFencedFrameAndWait(
      (*all_component_urls)[blink::kMaxAdAuctionAdComponents - 1],
      GURL(url::kAboutBlankURL), GetFencedFrameRenderFrameHost(shell()));
}

// Test with an ad component. Run an auction with an ad component, load the ad
// in a fenced frame, and the ad component in a nested fenced frame. Fully
// exercise navigator.adAuctionComponents() on the main ad's fenced frame.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest, AdComponents) {
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  ASSERT_NO_FATAL_FAILURE(RunBasicAuctionWithAdComponents(ad_component_url));

  // Trying to retrieve the adAuctionComponents of the main frame should throw
  // an exception.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(shell(), 1));

  // Check that adAuctionComponents() returns an array of URNs, the first of
  // which maps to `ad_component_url`, and the rest of which map to about:blank.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  CheckAdComponents(
      /*expected_ad_component_urls=*/std::vector<GURL>{ad_component_url},
      ad_frame);

  absl::optional<std::vector<GURL>> all_component_urls =
      GetAdAuctionComponentsInJS(ad_frame, blink::kMaxAdAuctionAdComponents);
  ASSERT_TRUE(all_component_urls);
  NavigateFencedFrameAndWait((*all_component_urls)[1],
                             GURL(url::kAboutBlankURL),
                             GetFencedFrameRenderFrameHost(shell()));
  NavigateFencedFrameAndWait(
      (*all_component_urls)[blink::kMaxAdAuctionAdComponents - 1],
      GURL(url::kAboutBlankURL), GetFencedFrameRenderFrameHost(shell()));
}

// Checked that navigator.adAuctionComponents() from an ad auction with
// components aren't leaked to other frames. In particular, check that they
// aren't provided to:
// * The main frame. It will throw an exception.
// * The fenced frame the ad component is loaded in, though it will have a list
//   of URNs that map to about:blank.
// * The ad fenced frame itself, after a renderer-initiated navigation.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       AdComponentsNotLeaked) {
  GURL ad_component_url =
      https_server_->GetURL("d.test", "/fenced_frames/opaque_ads.html");
  ASSERT_NO_FATAL_FAILURE(RunBasicAuctionWithAdComponents(ad_component_url));

  // The top frame should have no ad components.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(shell(), 1));

  // Check that adAuctionComponents(), when invoked in the ad component's frame,
  // returns an array of URNs that all map to about:blank.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  RenderFrameHostImpl* ad_component_frame =
      GetFencedFrameRenderFrameHost(ad_frame);
  CheckAdComponents(/*expected_ad_component_urls=*/std::vector<GURL>{},
                    ad_component_frame);

  // Navigate the ad component's nested fenced frame (3 fenced frames deep) to
  // some of the URNs, which should navigate it to about:blank.
  absl::optional<std::vector<GURL>> all_component_urls =
      GetAdAuctionComponentsInJS(ad_component_frame,
                                 blink::kMaxAdAuctionAdComponents);
  ASSERT_TRUE(all_component_urls);
  NavigateFencedFrameAndWait((*all_component_urls)[0],
                             GURL(url::kAboutBlankURL), ad_component_frame);
  NavigateFencedFrameAndWait(
      (*all_component_urls)[blink::kMaxAdAuctionAdComponents - 1],
      GURL(url::kAboutBlankURL), ad_component_frame);

  // Load a new URL in the top-level fenced frame, which should cause future
  // navigator.adComponents() calls to fail. Use a new URL, so can wait for the
  // server to see it. Same origin navigation so that the RenderFrameHost will
  // be reused.
  GURL new_url = https_server_->GetURL(
      ad_frame->GetLastCommittedOrigin().host(), "/echoall");

  // Used to wait for navigation completion in the ShadowDOM case only.
  // Harmlessly created but not used in the MPArch case.
  TestFrameNavigationObserver observer(ad_frame);

  EXPECT_TRUE(ExecJs(ad_frame, JsReplace("document.location = $1;", new_url)));

  // Wait for the URL to be requested, to make sure the fenced frame actually
  // made the request and, in the MPArch case, to make sure the load actually
  // started.
  WaitForURL(new_url);

  // Wait for the load to complete.
  observer.Wait();

  // Navigating the ad fenced frame may result in it using a new
  // RenderFrameHost, invalidating the old `ad_frame`.
  ad_frame = GetFencedFrameRenderFrameHost(shell());

  // Make sure the expected page has loaded in the ad frame.
  EXPECT_EQ(new_url, ad_frame->GetLastCommittedURL());

  // Calling navigator.adAuctionComponents on the new frame should fail.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(ad_frame, 1));
}

// Test with an ad component that tries to leave the group. Verify that leaving
// the group from within an ad component has no effect
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest, AdComponentsLeave) {
  url::Origin test_origin =
      url::Origin::Create(https_server_->GetURL("a.test", "/"));
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/fenced_frames/ad_that_leaves_interest_group.html");
  AttachInterestGroupObserver();

  ASSERT_NO_FATAL_FAILURE(RunBasicAuctionWithAdComponents(ad_component_url));

  // InterestGroupAccessObserver should see the join and auction, but not the
  // implicit leave since it was blocked.
  WaitForAccessObserved(
      {{InterestGroupTestObserver::kJoin, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kBid, test_origin.Serialize(), "cars"},
       {InterestGroupTestObserver::kWin, test_origin.Serialize(), "cars"}});

  // The ad shouldn't have left the interest group when the component ad was
  // shown.
  EXPECT_EQ(1u, GetAllInterestGroups().size());
}

// Test navigating multiple fenced frames to the same render URL from a single
// auction, when the winning bid included ad components. All fenced frames
// navigated to the URL should get ad component URLs from the winning bid.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       AdComponentsMainAdLoadedInMultipleFrames) {
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  GURL test_url =
      https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL ad_url =
      https_server_->GetURL("c.test", "/fenced_frames/opaque_ads.html");
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/url::Origin::Create(test_url),
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
                          /*ad_components=*/
                          {{{ad_component_url, /*metadata=*/absl::nullopt}}}));

  content::EvalJsResult urn_url_string = RunAuctionAndWait(JsReplace(
      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1]
}
      )",
      url::Origin::Create(test_url),
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js")));
  ASSERT_TRUE(urn_url_string.value.is_string())
      << "Expected string, but got " << urn_url_string.value;

  GURL urn_url(urn_url_string.ExtractString());
  ASSERT_TRUE(urn_url.is_valid())
      << "URL is not valid: " << urn_url_string.ExtractString();
  EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());

  // Repeatedly load the URN in fenced frames.  The first two iterations use the
  // original fenced frame, the next two use a new one that replaces the first.
  for (int i = 0; i < 4; ++i) {
    if (i == 2) {
      EXPECT_TRUE(ExecJs(shell(),
                         "document.querySelector('fencedframe').remove();"
                         "const ff = document.createElement('fencedframe');"
                         "ff.mode = 'opaque-ads';"
                         "document.body.appendChild(ff);"));
    }
    ClearReceivedRequests();
    NavigateFencedFrameAndWait(urn_url, ad_url, shell());

    RenderFrameHost* ad_frame = GetFencedFrameRenderFrameHost(shell());
    absl::optional<std::vector<GURL>> components =
        GetAdAuctionComponentsInJS(ad_frame, 1);
    ASSERT_TRUE(components);
    ASSERT_EQ(1u, components->size());
    EXPECT_EQ(url::kUrnScheme, (*components)[0].scheme_piece());
    NavigateFencedFrameAndWait((*components)[0], ad_component_url, ad_frame);
  }
}

// Test with multiple ad components. Also checks that ad component metadata is
// passed in correctly.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       MultipleAdComponents) {
  // Note that the extra "&1" and the like are added to make the URLs unique.
  // They have no impact on the returned result, since they aren't a
  // header/value pair.
  std::vector<blink::InterestGroup::Ad> ad_components{
      {https_server_->GetURL(
           "d.test", "/set-header?Supports-Loading-Mode: fenced-frame&1"),
       absl::nullopt},
      {https_server_->GetURL(
           "d.test", "/set-header?Supports-Loading-Mode: fenced-frame&2"),
       "2"},
      {https_server_->GetURL(
           "d.test", "/set-header?Supports-Loading-Mode: fenced-frame&3"),
       "[3, {'4': 'five'}]"},
  };

  GURL test_url =
      https_server_->GetURL("a.test", "/fenced_frames/opaque_ads.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Register bidding script that validates interestGroup.adComponents and
  // returns the first and third components in the offered bid.
  std::string bidding_script = R"(
let adComponents = interestGroup.adComponents;
if (adComponents.length !== 3)
  throw 'Incorrect length';
if (adComponents[0].metadata !== undefined)
  throw 'adComponents[0] has incorrect metadata: ' + adComponents[0].metadata;
if (adComponents[1].metadata !== 2)
  throw 'adComponents[1] has incorrect metadata: ' + adComponents[1].metadata;
if (JSON.stringify(adComponents[2].metadata) !== '[3,{"4":"five"}]') {
  throw 'adComponents[2] has incorrect metadata: ' + adComponents[2].metadata;
}

return {
  ad: 'ad',
  bid: 1,
  render: interestGroup.ads[0].renderUrl,
  adComponents: [interestGroup.adComponents[0].renderUrl,
                 interestGroup.adComponents[2].renderUrl]
};
  )";
  GURL bidding_url =
      https_server_->GetURL("a.test", "/generated_bidding_logic.js");
  network_responder_->RegisterBidderScript(bidding_url.path(), bidding_script);

  GURL ad_url =
      https_server_->GetURL("c.test", "/fenced_frames/opaque_ads.html");
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/url::Origin::Create(test_url),
                          /*name=*/"cars", /*priority=*/0.0, bidding_url,
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
                          ad_components));

  ASSERT_NO_FATAL_FAILURE(RunAuctionAndNavigateFencedFrame(
      ad_url, JsReplace(
                  R"({
seller: $1,
decisionLogicUrl: $2,
interestGroupBuyers: [$1]
                  })",
                  url::Origin::Create(test_url),
                  https_server_->GetURL("a.test",
                                        "/interest_group/decision_logic.js"))));

  // Validate ad components array. The bidder script should return only the
  // first and last ad component URLs, skpping the second.
  RenderFrameHostImpl* ad_frame = GetFencedFrameRenderFrameHost(shell());
  CheckAdComponents(
      /*expected_ad_component_urls=*/{ad_components[0].render_url,
                                      ad_components[2].render_url},
      ad_frame);

  // Get first three URLs from the fenced frame.
  absl::optional<std::vector<GURL>> components =
      GetAdAuctionComponentsInJS(ad_frame, 3);
  ASSERT_TRUE(components);
  ASSERT_EQ(3u, components->size());

  // Load each of the ad components in the nested fenced frame, validating the
  // URLs they're mapped to.
  NavigateFencedFrameAndWait((*components)[0], ad_components[0].render_url,
                             ad_frame);
  NavigateFencedFrameAndWait((*components)[1], ad_components[2].render_url,
                             ad_frame);
  NavigateFencedFrameAndWait((*components)[2], GURL(url::kAboutBlankURL),
                             ad_frame);
}

// These end-to-end tests validate that information from navigator-exposed APIs
// is correctly passed to worklets.

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       BuyerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/bidding_logic_throws.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2,3]}"}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ(
      nullptr,
      EvalJs(shell(), JsReplace(
                          R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  });
})())",
                          test_origin,
                          https_server_->GetURL(
                              "a.test", "/interest_group/decision_logic.js"))));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ComponentAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/test_origin,
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        // Signal to the top-level seller to allow participation in a component
        // auction.
        auctionSignals: "sellerAllowsComponentAuction",
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the bidder and component seller to allow participation in
          // a component auction.
          auctionSignals: "bidderAllowsComponentAuction,"+
                          "sellerAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);
}

// Test the case of a component argument in the case a bidder refuses to
// participate in component auctions.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionBidderRefuses) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/test_origin,
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        // Signal to the top-level seller to allow participation in a component
        // auction.
        auctionSignals: "sellerAllowsComponentAuction",
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the component seller to allow participation in a
          // component auction.
          auctionSignals: "sellerAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  EXPECT_EQ(nullptr, RunAuctionAndWait(auction_config));
}

// Test the case of a component argument in the case the top-level seller
// refuses to participate in component auctions.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionTopLevelSellerRefuses) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/test_origin,
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the bidder and component seller to allow participation in
          // a component auction.
          auctionSignals: "bidderAllowsComponentAuction,"+
                          "sellerAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  EXPECT_EQ(nullptr, RunAuctionAndWait(auction_config));
}

// Test the case of a component argument in the case a component seller refuses
// to participate in component auctions.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionComponentSellerRefuses) {
  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/test_origin,
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  std::string auction_config = JsReplace(
      R"({
        seller: $1,
        decisionLogicUrl: $2,
        // Signal to the top-level seller to allow participation in a component
        // auction.
        auctionSignals: "sellerAllowsComponentAuction",
        componentAuctions: [{
          seller: $1,
          decisionLogicUrl: $2,
          interestGroupBuyers: [$1],
          // Signal to the bidder to allow participation in a component auction.
          auctionSignals: "bidderAllowsComponentAuction"
        }]
      })",
      test_origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  EXPECT_EQ(nullptr, RunAuctionAndWait(auction_config));
}

// Use bidder and seller worklet files that validate their arguments all have
// the expected values.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, ValidateWorkletParameters) {
  // Use different hostnames for each participant, since
  // `trusted_bidding_signals` only checks the hostname of certain parameters.
  constexpr char kBidderHost[] = "a.test";
  constexpr char kSellerHost[] = "b.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kSecondBidderHost[] = "d.test";
  content_browser_client_.AddToAllowList(
      {url::Origin::Create(https_server_->GetURL(kSecondBidderHost, "/"))});

  // Start by adding a placeholder bidder in domain d.test, used for
  // perBuyerSignals validation.
  GURL second_bidder_url = https_server_->GetURL(kSecondBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), second_bidder_url));
  url::Origin second_bidder_origin = url::Origin::Create(second_bidder_url);

  ASSERT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/second_bidder_origin,
                /*name=*/"boats",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(kSecondBidderHost,
                                      "/interest_group/bidding_logic.js"),
                /*ads=*/
                {{{GURL("https://should-not-be-returned/"),
                   /*metadata=*/absl::nullopt}}}));

  // This is the primary interest group that wins the auction because
  // bidding_argument_validator.js bids 2, whereas bidding_logic.js bids 1, and
  // decision_logic.js just returns the bid as the rank -- highest rank wins.
  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(
              kBidderHost, "/interest_group/bidding_argument_validator.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/
          https_server_->GetURL(kBidderHost,
                                "/not_found_daily_update_url.json"),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidderHost,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2,3]}"}}},
          /*ad_components=*/
          {{{GURL("https://example.com/render-component"),
             /*metadata=*/absl::nullopt}}})));

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, "/echo")));
  GURL seller_script_url = https_server_->GetURL(
      kSellerHost, "/interest_group/decision_argument_validator.js");

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    trustedScoringSignalsUrl: $3,
    interestGroupBuyers: [$4, $5],
    auctionSignals: {so: 'I', hear: ['you', 'like', 'json']},
    sellerSignals: {signals: 'from', the: ['seller']},
    sellerTimeout: 200,
    perBuyerSignals: {$4: {signalsForBuyer: 1}, $5: {signalsForBuyer: 2}},
    perBuyerTimeouts: {$4: 110, $5: 120, '*': 150}
  });
})())",
                      url::Origin::Create(seller_script_url), seller_script_url,
                      https_server_->GetURL(
                          kSellerHost,
                          "/interest_group/trusted_scoring_signals.json"),
                      bidder_origin, second_bidder_origin))
               .ExtractString()),
      &observer);
  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Use bidder and seller worklet files that validate their arguments all have
// the expected values, in the case of an auction with one component auction.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       ComponentAuctionValidateWorkletParameters) {
  // Use different hostnames for each participant.
  //
  // Match assignments in above test as closely as possible, to make scripts
  // similar.
  constexpr char kBidderHost[] = "a.test";
  constexpr char kTopLevelSellerHost[] = "b.test";
  constexpr char kTopFrameHost[] = "c.test";
  constexpr char kComponentSellerHost[] = "d.test";

  content_browser_client_.AddToAllowList(
      {url::Origin::Create(https_server_->GetURL(kComponentSellerHost, "/"))});

  GURL bidder_url = https_server_->GetURL(kBidderHost, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(
              kBidderHost,
              "/interest_group/"
              "component_auction_bidding_argument_validator.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/
          https_server_->GetURL(kBidderHost,
                                "/not_found_daily_update_url.json"),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidderHost,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2,3]}"}}},
          /*ad_components=*/
          {{{GURL("https://example.com/render-component"),
             /*metadata=*/absl::nullopt}}})));

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, "/echo")));
  GURL top_level_seller_script_url = https_server_->GetURL(
      kTopLevelSellerHost,
      "/interest_group/"
      "component_auction_top_level_decision_argument_validator.js");
  GURL component_seller_script_url = https_server_->GetURL(
      kComponentSellerHost,
      "/interest_group/"
      "component_auction_component_decision_argument_validator.js");

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    trustedScoringSignalsUrl: $3,
    auctionSignals: ["top-level auction signals"],
    sellerSignals: ["top-level seller signals"],
    sellerTimeout: 300,
    perBuyerSignals: {$7: ["top-level buyer signals"]},
    perBuyerTimeouts: {$7: 110, '*': 150},
    componentAuctions: [{
      seller: $4,
      decisionLogicUrl: $5,
      trustedScoringSignalsUrl: $6,
      interestGroupBuyers: [$7],
      auctionSignals: ["component auction signals"],
      sellerSignals: ["component seller signals"],
      sellerTimeout: 200,
      perBuyerSignals: {$7: ["component buyer signals"]},
      perBuyerTimeouts: {$7: 200},
    }],
  });
})())",
                      url::Origin::Create(top_level_seller_script_url),
                      top_level_seller_script_url,
                      https_server_->GetURL(
                          kTopLevelSellerHost,
                          "/interest_group/trusted_scoring_signals.json"),
                      url::Origin::Create(component_seller_script_url),
                      component_seller_script_url,
                      https_server_->GetURL(
                          kComponentSellerHost,
                          "/interest_group/trusted_scoring_signals2.json"),
                      bidder_origin))
               .ExtractString()),
      &observer);
  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
  WaitForURL(https_server_->GetURL(kTopLevelSellerHost,
                                   "/echo?report_top_level_seller"));
  WaitForURL(https_server_->GetURL(kComponentSellerHost,
                                   "/echo?report_component_seller"));
  WaitForURL(https_server_->GetURL(kBidderHost, "/echo?report_bidder"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SellerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2,3]}"}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ(
      nullptr,
      EvalJs(shell(),
             JsReplace(
                 R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
  });
})())",
                 test_origin,
                 https_server_->GetURL(
                     "a.test", "/interest_group/decision_logic_throws.js"))));
}

// JSON fields of joinAdInterestGroup() and runAdAuction() should support
// non-object types, like numbers.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupNonObjectJSONFields) {
  // These scripts are generated by this test.
  constexpr char kBiddingLogicPath[] =
      "/interest_group/non_object_bidding_argument_validator.js";
  constexpr char kDecisionLogicPath[] =
      "/interest_group/non_object_decision_argument_validator.js";
  constexpr char kTrustedBiddingSignalsPath[] =
      "/interest_group/non_object_bidding_signals.json";
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  // In the below JavaScript, if fields are incorrectly passed in as a string
  // ("2") instead of a number (2), JSON.stringify() will wrap it in another
  // layer of quotes, causing the test to fail. The order of properties produced
  // by stringify() isn't guaranteed by the ECMAScript standard, but some sites
  // depend on the V8 behavior of serializing in declaration order.

  constexpr char kBiddingLogicScript[] = R"(
function generateBid(
    interestGroup, auctionSignals, perBuyerSignals, trustedBiddingSignals,
    unusedBrowserSignals) {
  validateInterestGroup(interestGroup);
  validateAuctionSignals(auctionSignals);
  validatePerBuyerSignals(perBuyerSignals);
  validateTrustedBiddingSignals(trustedBiddingSignals);
  const ad = interestGroup.ads[0];
  return {'ad': ad, 'bid': 1, 'render': ad.renderUrl};
}

function validateInterestGroup(interestGroup) {
  const userBiddingSignalsJSON =
      JSON.stringify(interestGroup.userBiddingSignals);
  if (userBiddingSignalsJSON !== '1')
    throw 'Wrong userBiddingSignals ' + userBiddingSignalsJSON;
  if (interestGroup.ads.length !== 1)
    throw 'Wrong ads.length ' + ads.length;
  const adMetadataJSON = JSON.stringify(interestGroup.ads[0].metadata);
  if (adMetadataJSON !== '2')
    throw 'Wrong ads[0].metadata ' + adMetadataJSON;
}

function validateAuctionSignals(auctionSignals) {
  const auctionSignalsJSON = JSON.stringify(auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionSignalsJSON;
}

function validatePerBuyerSignals(perBuyerSignals) {
  const perBuyerSignalsJson = JSON.stringify(perBuyerSignals);
  if (perBuyerSignalsJson !== '5')
    throw 'Wrong perBuyerSignas ' + perBuyerSignalsJson;
}

function validateTrustedBiddingSignals(trustedBiddingSignals) {
  const trustedBiddingSignalsJSON = JSON.stringify(trustedBiddingSignals);
  if (trustedBiddingSignalsJSON !== '{"key1":0}')
    throw 'Wrong trustedBiddingSignals ' + trustedBiddingSignalsJSON;
}
)";

  constexpr char kDecisionLogicScript[] = R"(
function scoreAd(
    adMetadata, bid, auctionConfig, unusedTrustedScoringSignals,
    unusedBrowserSignals) {
  validateAdMetadata(adMetadata);
  validateAuctionConfig(auctionConfig);
  return bid;
}

function validateAdMetadata(adMetadata) {
  const adMetadataJSON = JSON.stringify(adMetadata);
  if (adMetadataJSON !==
      '{"renderUrl":"https://example.com/render","metadata":2}')
    throw 'Wrong adMetadata ' + adMetadataJSON;
}

function validateAuctionConfig(auctionConfig) {
  const auctionSignalsJSON = JSON.stringify(auctionConfig.auctionSignals);
  if (auctionSignalsJSON !== '3')
    throw 'Wrong auctionSignals ' + auctionConfig.auctionSignalsJSON;
  const sellerSignalsJSON = JSON.stringify(auctionConfig.sellerSignals);
  if (sellerSignalsJSON !== '4')
    throw 'Wrong sellerSignals ' + auctionConfig.sellerSignalsJSON;
  const perBuyerSignalsJson = JSON.stringify(auctionConfig.perBuyerSignals);
  if (!perBuyerSignalsJson.includes('a.test') ||
      !perBuyerSignalsJson.includes('5')) {
    throw 'Wrong perBuyerSignals ' + perBuyerSignalsJson;
  }
}
)";

  network_responder_->RegisterNetworkResponse(
      kBiddingLogicPath, kBiddingLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kDecisionLogicPath, kDecisionLogicScript, "application/javascript");
  network_responder_->RegisterNetworkResponse(
      kTrustedBiddingSignalsPath, R"({"key1":0})", "application/json");

  EXPECT_EQ(
      "done",
      EvalJs(shell(),
             JsReplace(
                 R"(
(async function() {
  try {
    await navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: $1,
          trustedBiddingSignalsUrl: $2,
          trustedBiddingSignalsKeys: ['key1'],
          biddingLogicUrl: $3,
          userBiddingSignals: 1,
          ads: [{renderUrl:"https://example.com/render", metadata:2}],
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())",
                 test_origin,
                 https_server_->GetURL("a.test", kTrustedBiddingSignalsPath),
                 https_server_->GetURL("a.test", kBiddingLogicPath))));

  TestFencedFrameURLMappingResultObserver observer;
  ConvertFencedFrameURNToURL(
      GURL(EvalJs(shell(),
                  JsReplace(
                      R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: 3,
    sellerSignals: 4,
    perBuyerSignals: {$1: 5}
  });
})())",
                      test_origin,
                      https_server_->GetURL("a.test", kDecisionLogicPath)))
               .ExtractString()),
      &observer);

  EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
}

// Make sure that qutting with a live auction doesn't crash.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, QuitWithRunningAuction) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL hanging_url = https_server_->GetURL("a.test", "/hung");
  url::Origin hanging_origin = url::Origin::Create(hanging_url);

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/hanging_origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/hanging_url,
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   "{ad:'metadata', here:[1,2]}"}}},
                /*ad_components=*/absl::nullopt)));

  ExecuteScriptAsync(shell(), JsReplace(R"(
navigator.runAdAuction({
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1]
});
                                        )",
                                        hanging_origin, hanging_url));

  WaitForURL(https_server_->GetURL("/hung"));
}

// These tests validate the `dailyUpdateUrl` and
// navigator.updateAdInterestGroups() functionality.

// The server JSON updates all fields that can be updated.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, UpdateAllUpdatableFields) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // The server JSON updates all fields that can be updated.
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"biddingLogicUrl": "%s/interest_group/new_bidding_logic.js",
"trustedBiddingSignalsUrl":
  "%s/interest_group/new_trusted_bidding_signals_url.json",
"trustedBiddingSignalsKeys": ["new_key"],
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              test_origin.Serialize().c_str(),
                                              test_origin.Serialize().c_str(),
                                              test_origin.Serialize().c_str()));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/
          https_server_->GetURL("a.test", kDailyUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2,3]}"}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 1)
              return false;
            const auto& group = groups[0].interest_group;
            return group.name == "cars" && group.bidding_url.has_value() &&
                   group.bidding_url->path() ==
                       "/interest_group/new_bidding_logic.js" &&
                   group.trusted_bidding_signals_url.has_value() &&
                   group.trusted_bidding_signals_url->path() ==
                       "/interest_group/new_trusted_bidding_signals_url.json" &&
                   group.trusted_bidding_signals_keys.has_value() &&
                   group.trusted_bidding_signals_keys->size() == 1 &&
                   group.trusted_bidding_signals_keys.value()[0] == "new_key" &&
                   group.ads.has_value() && group.ads->size() == 1 &&
                   group.ads.value()[0].render_url.path() ==
                       "/new_ad_render_url" &&
                   group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
          }));
}

// Updates can proceed even if the page that started the update isn't running
// anymore.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       UpdateAndNavigateAwayStillCompletes) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Start an update, then navigate to a different page. The update completes
  // even though the page that started the update is gone.
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  network_responder_->RegisterNetworkResponse(
      kDailyUpdateUrlPath, base::StringPrintf(R"({
"ads": [{"renderUrl": "%s/new_ad_render_url",
         "metadata": {"new_a": "b"}
        }]
})",
                                              test_origin.Serialize().c_str()));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/
          https_server_->GetURL("a.test", kDailyUpdateUrlPath),
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL("a.test",
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2,3]}"}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Navigate away -- the update should still continue.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 1)
              return false;
            const auto& group = groups[0].interest_group;
            return group.name == "cars" && group.bidding_url.has_value() &&
                   group.bidding_url->path() ==
                       "/interest_group/bidding_logic.js" &&
                   group.daily_update_url.has_value() &&
                   group.daily_update_url->path() ==
                       "/interest_group/daily_update_partial.json" &&
                   group.trusted_bidding_signals_url.has_value() &&
                   group.trusted_bidding_signals_url->path() ==
                       "/interest_group/trusted_bidding_signals.json" &&
                   group.trusted_bidding_signals_keys.has_value() &&
                   group.trusted_bidding_signals_keys->size() == 1 &&
                   group.trusted_bidding_signals_keys.value()[0] == "key1" &&
                   group.ads.has_value() && group.ads->size() == 1 &&
                   group.ads.value()[0].render_url.path() ==
                       "/new_ad_render_url" &&
                   group.ads.value()[0].metadata == "{\"new_a\":\"b\"}";
          }));
}

// Bidders' generateBid() scripts that run forever should timeout. They will not
// affect other bidders or fail the auction.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithCustomPerBuyerTimeouts) {
  const char kHostA[] = "a.test";
  const char kHostB[] = "b.test";
  // Navigate to other bidder site, and add an interest group.
  GURL bidder_b_url = https_server_->GetURL(kHostB, "/echo");
  url::Origin bidder_b_origin = url::Origin::Create(bidder_b_url);
  ASSERT_TRUE(NavigateToURL(shell(), bidder_b_url));

  GURL ad_url_b = https_server_->GetURL(kHostB, "/echo?render_shoes");
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/bidder_b_origin,
                          /*name=*/"shoes",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              kHostB, "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url_b, /*metadata=*/absl::nullopt}}}));

  GURL bidder_a_url = https_server_->GetURL(kHostA, "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_a_url));
  url::Origin bidder_a_origin = url::Origin::Create(bidder_a_url);
  GURL ad1_url_a = https_server_->GetURL(kHostA, "/echo?render_cars");
  GURL ad2_url_a = https_server_->GetURL(kHostA, "/echo?render_bikes");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_a_origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    kHostA, "/interest_group/bidding_logic_loop_forever.js"),
                /*ads=*/{{{ad1_url_a, /*metadata=*/absl::nullopt}}}));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                /*owner=*/bidder_a_origin,
                /*name=*/"bikes",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    kHostA, "/interest_group/bidding_logic_loop_forever.js"),
                /*ads=*/{{{ad2_url_a, /*metadata=*/absl::nullopt}}}));

  // Set per buyer timeout of bidder a to 1 ms, so that its generateBid()
  // scripts which has an endless loop times out fast.
  const std::string kTestPerBuyerTimeouts[] = {
      JsReplace("{$1: 1}", bidder_a_origin),
      JsReplace("{$1: 1, '*': 100}", bidder_a_origin),
      JsReplace("{$1: 100, '*': 1}", bidder_b_origin),
  };

  for (const auto& test_per_buyer_timeouts : kTestPerBuyerTimeouts) {
    std::string auction_config = JsReplace(
        R"({
      seller: $1,
      decisionLogicUrl: $2,
      interestGroupBuyers: [$1, $3],
                  )",
        bidder_a_origin,
        https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
        bidder_b_origin);
    // Since test_per_buyer_timeout is JSON, it shouldn't be wrapped in quotes,
    // so can't use JsReplace.
    auction_config += base::StringPrintf("perBuyerTimeouts: %s}",
                                         test_per_buyer_timeouts.c_str());
    // Bidder b won the auction.
    RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url_b);
  }
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithCustomSellerTimeout) {
  const char kHostA[] = "a.test";
  GURL test_url = https_server_->GetURL(kHostA, "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL(kHostA, "/echo?render_cars");

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/test_origin,
                          /*name=*/"cars",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          https_server_->GetURL(
                              kHostA, "/interest_group/bidding_logic.js"),
                          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  // The auction fails, since seller's scoreAd() script times out after 1 ms.
  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
          R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    sellerTimeout: 1,
                })",
          test_origin,
          https_server_->GetURL(
              "a.test", "/interest_group/decision_logic_loop_forever.js"))));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithExperimentGroupId) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kSeller[] = "c.test";

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(kBidder, "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidder,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  // Navigate to publisher.
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kPublisher, "/echo")));
  GURL seller_logic_url =
      https_server_->GetURL(kSeller, "/interest_group/decision_logic.js");

  const char kAuctionConfigTemplate[] = R"({
    seller: $1,
    decisionLogicUrl: $2,
    trustedScoringSignalsUrl: $3,
    interestGroupBuyers: [$4],
    sellerExperimentGroupId: 8349,
    perBuyerExperimentGroupIds: {'*': 3498},
  })";

  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(JsReplace(
                kAuctionConfigTemplate, url::Origin::Create(seller_logic_url),
                seller_logic_url,
                https_server_->GetURL(
                    kSeller, "/interest_group/trusted_scoring_signals.json"),
                bidder_origin)));

  // Make sure that the right trusted signals URLs got fetched, incorporating
  // the experiment group ID.
  WaitForURL(https_server_->GetURL(
      "/interest_group/trusted_bidding_signals.json?hostname=a.test&keys=key1"
      "&experimentGroupId=3498"));
  WaitForURL(https_server_->GetURL(
      "/interest_group/trusted_scoring_signals.json?hostname=a.test"
      "&renderUrls=https%3A%2F%2Fexample.com%2Frender&experimentGroupId=8349"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionWithPerBuyerExperimentGroupId) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kBidder2[] = "d.test";
  const char kSeller[] = "c.test";

  // Navigate to bidder site, and add an interest group, then same for bidder 2.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(kBidder, "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidder,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  GURL bidder2_url = https_server_->GetURL(kBidder2, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder2_url));
  url::Origin bidder2_origin = url::Origin::Create(bidder2_url);
  content_browser_client_.AddToAllowList({bidder2_origin});
  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/bidder2_origin,
          /*name=*/"cars_and_trucks",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL(kBidder2, "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/
          https_server_->GetURL(kBidder2,
                                "/interest_group/trusted_bidding_signals.json"),
          /*trusted_bidding_signals_keys=*/{{"key2"}},
          /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here:[1,2]}"}}},
          /*ad_components=*/absl::nullopt)));

  // Navigate to publisher.
  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kPublisher, "/echo")));
  GURL seller_logic_url =
      https_server_->GetURL(kSeller, "/interest_group/decision_logic.js");

  const char kAuctionConfigTemplate[] = R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$3, $4],
    perBuyerExperimentGroupIds: {'*': 3498,
                                 $4: 1203},
  })";

  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(JsReplace(
                kAuctionConfigTemplate, url::Origin::Create(seller_logic_url),
                seller_logic_url, bidder_origin, bidder2_origin)));

  // Make sure that the right trusted signals URLs got fetched, incorporating
  // the experiment group IDs.
  WaitForURL(https_server_->GetURL(
      "/interest_group/trusted_bidding_signals.json?hostname=a.test&keys=key1"
      "&experimentGroupId=3498"));
  WaitForURL(https_server_->GetURL(
      "/interest_group/trusted_bidding_signals.json?hostname=a.test&keys=key2"
      "&experimentGroupId=1203"));
}

// Validate that createAdRequest is available and be successfully called as part
// of PARAKEET.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CreateAdRequestWorks) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  EXPECT_EQ("NotSupportedError: createAdRequest API not yet implemented",
            CreateAdRequestAndWait());
}

// Validate that finalizeAd is available and be successfully called as part of
// PARAKEET.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, FinalizeAdWorks) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  // The finalize API relies on createAdRequest, until it is fully implemented
  // we expect a createAdRequest failure initially.
  EXPECT_EQ("NotSupportedError: createAdRequest API not yet implemented",
            FinalizeAdAndWait());
}

// The bidder worklet is served from a private network, everything else from a
// public network. The auction should fail.
IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       BidderOnPrivateNetwork) {
  URLLoaderMonitor url_loader_monitor;

  // Learn the bidder IG, served from the local server.
  GURL bidder_url =
      https_server_->GetURL("b.test", "/interest_group/bidding_logic.js");
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/bidder_origin,
                          /*name=*/"Cthulhu", /*priority=*/0.0, bidder_url,
                          /*ads=*/
                          {{{GURL("https://example.com/render"),
                             /*metadata=*/absl::nullopt}}}));

  // Use `remote_test_server_` for all other URLs.
  GURL test_url = remote_test_server_.GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3]
}
                         )",
                         url::Origin::Create(test_url),
                         remote_test_server_.GetURL(
                             "a.test", "/interest_group/decision_logic.js"),
                         bidder_origin)));

  // The URLLoaderMonitor should have seen a request for the bidder URL, which
  // should have been made from a public address space.
  absl::optional<network::ResourceRequest> bidder_request =
      url_loader_monitor.GetRequestInfo(bidder_url);
  ASSERT_TRUE(bidder_request);
  EXPECT_EQ(
      network::mojom::IPAddressSpace::kPublic,
      bidder_request->trusted_params->client_security_state->ip_address_space);

  const network::URLLoaderCompletionStatus& bidder_status =
      url_loader_monitor.WaitForRequestCompletion(bidder_url);
  EXPECT_EQ(net::ERR_FAILED, bidder_status.error_code);
  EXPECT_THAT(bidder_status.cors_error_status,
              Optional(network::CorsErrorStatus(
                  network::mojom::CorsError::kPreflightMissingAllowOriginHeader,
                  network::mojom::IPAddressSpace::kLocal,
                  network::mojom::IPAddressSpace::kUnknown)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       SellerOnPrivateNetwork) {
  GURL seller_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");

  // Use `remote_test_server_` for all URLs except the seller worklet.
  GURL test_url = remote_test_server_.GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  // Need to set this up before the join, since joining instantiates the
  // AdAuctionServiceImpl's URLLoaderFactory.
  URLLoaderMonitor url_loader_monitor;

  EXPECT_EQ(kSuccess, JoinInterestGroupAndVerify(
                          /*owner=*/test_origin,
                          /*name=*/"Cthulhu",
                          /*priority=*/0.0,
                          /*bidding_url=*/
                          remote_test_server_.GetURL(
                              "a.test", "/interest_group/bidding_logic.js"),
                          /*ads=*/
                          {{{GURL("https://example.com/render"),
                             /*metadata=*/absl::nullopt}}}));

  EXPECT_EQ(nullptr,
            RunAuctionAndWait(JsReplace(
                R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$3]
}
                )",
                url::Origin::Create(seller_url), seller_url, test_origin)));

  // The URLLoaderMonitor should have seen a request for the seller URL. The
  // request should have gone through the renderer's URLLoader, and inherited
  // its IPAddressSpace, instead of passing its own.
  absl::optional<network::ResourceRequest> seller_request =
      url_loader_monitor.GetRequestInfo(seller_url);
  ASSERT_TRUE(seller_request);
  EXPECT_FALSE(seller_request->trusted_params);

  const network::URLLoaderCompletionStatus& seller_status =
      url_loader_monitor.WaitForRequestCompletion(seller_url);
  EXPECT_EQ(net::ERR_FAILED, seller_status.error_code);
  EXPECT_THAT(seller_status.cors_error_status,
              Optional(network::CorsErrorStatus(
                  network::mojom::CorsError::kPreflightMissingAllowOriginHeader,
                  network::mojom::IPAddressSpace::kLocal,
                  network::mojom::IPAddressSpace::kUnknown)));
}

// Have the auction and worklets server from public IPs, but send reports to a
// private network. The reports should be blocked.
IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       ReportToPrivateNetwork) {
  // Use `remote_test_server_` exclusively with hostname "a.test" for root page
  // and script URLs.
  GURL test_url =
      remote_test_server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  // Use `https_server_` exclusively with hostname "b.test" for reports.
  GURL bidder_report_to_url = https_server_->GetURL("b.test", "/bidder_report");
  GURL seller_report_to_url = https_server_->GetURL("b.test", "/seller_report");
  GURL bidder_debug_win_report_url =
      https_server_->GetURL("b.test", "/bidder_report_debug_win_report");
  GURL seller_debug_win_report_url =
      https_server_->GetURL("b.test", "/seller_report_debug_win_report");
  URLLoaderMonitor url_loader_monitor;

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(
          /*owner=*/test_origin,
          /*name=*/bidder_report_to_url.spec(),
          /*priority=*/0.0,
          /*bidding_url=*/
          remote_test_server_.GetURL(
              "a.test", "/interest_group/bidding_logic_report_to_name.js"),
          /*ads=*/
          {{{GURL("https://example.com/render"),
             /*metadata=*/absl::nullopt}}}));

  EXPECT_EQ(
      "https://example.com/render",
      RunAuctionAndWaitForURL(JsReplace(
          R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
  sellerSignals: {reportTo: $3},
}
          )",
          test_origin,
          remote_test_server_.GetURL(
              "a.test",
              "/interest_group/decision_logic_report_to_seller_signals.js"),
          seller_report_to_url)));

  // Wait for both requests to be completed, and check their IPAddressSpace and
  // make sure that they failed.
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(bidder_report_to_url)
                .trusted_params->client_security_state->ip_address_space);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(seller_report_to_url)
                .trusted_params->client_security_state->ip_address_space);

  for (const GURL& report_url :
       {bidder_report_to_url, seller_report_to_url, bidder_debug_win_report_url,
        seller_debug_win_report_url}) {
    SCOPED_TRACE(report_url.spec());
    const network::URLLoaderCompletionStatus& report_status =
        url_loader_monitor.WaitForRequestCompletion(report_url);
    EXPECT_EQ(net::ERR_FAILED, report_status.error_code);
    EXPECT_THAT(
        report_status.cors_error_status,
        Optional(network::CorsErrorStatus(
            network::mojom::CorsError::kPreflightMissingAllowOriginHeader,
            network::mojom::IPAddressSpace::kLocal,
            network::mojom::IPAddressSpace::kUnknown)));
  }
}

// Have all requests for an auction served from a public network, and all
// reports send there as well. The auction should succeed, and all reports
// should be sent.
IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       ReportToPublicNetwork) {
  // Use `remote_test_server_` exclusively with hostname "a.test" for root page
  // and script URLs.
  GURL test_url =
      remote_test_server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL bidder_url = remote_test_server_.GetURL(
      "a.test", "/interest_group/bidding_logic_report_to_name.js");
  GURL trusted_bidding_signals_url = remote_test_server_.GetURL(
      "a.test", "/interest_group/trusted_bidding_signals.json");
  GURL trusted_bidding_signals_url_with_query = remote_test_server_.GetURL(
      "a.test",
      "/interest_group/trusted_bidding_signals.json?hostname=a.test&keys=key1");

  GURL seller_url = remote_test_server_.GetURL(
      "a.test", "/interest_group/decision_logic_report_to_seller_signals.js");
  GURL ad_url = https_server_->GetURL("c.test", "/echo");

  // While reports should be made to these URLs in this test, their results
  // don't matter, so there's no need for a test server to respond to these URLs
  // with anything other than errors.
  GURL bidder_report_to_url =
      remote_test_server_.GetURL("a.test", "/bidder_report");
  GURL seller_report_to_url =
      remote_test_server_.GetURL("a.test", "/seller_report");
  GURL bidder_debug_win_report_url =
      remote_test_server_.GetURL("a.test", "/bidder_report_debug_win_report");
  GURL seller_debug_win_report_url =
      remote_test_server_.GetURL("a.test", "/seller_report_debug_win_report");
  URLLoaderMonitor url_loader_monitor;

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/bidder_report_to_url.spec(), /*priority=*/0.0, bidder_url,
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt, trusted_bidding_signals_url,
          /*trusted_bidding_signals_keys=*/{{"key1"}},
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
  sellerSignals: {reportTo: $3},
}
          )",
      test_origin,
      remote_test_server_.GetURL(
          "a.test",
          "/interest_group/decision_logic_report_to_seller_signals.js"),
      seller_report_to_url);
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(bidder_url)
                .trusted_params->client_security_state->ip_address_space);
  EXPECT_EQ(
      network::mojom::IPAddressSpace::kPublic,
      url_loader_monitor.WaitForUrl(trusted_bidding_signals_url_with_query)
          .trusted_params->client_security_state->ip_address_space);
  // Unlike the others, the request for the seller URL has an empty
  // `trusted_params`, since it uses the renderer's untrusted URLLoader.
  EXPECT_FALSE(url_loader_monitor.WaitForUrl(seller_url).trusted_params);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(seller_report_to_url)
                .trusted_params->client_security_state->ip_address_space);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.WaitForUrl(seller_debug_win_report_url)
                .trusted_params->client_security_state->ip_address_space);

  for (const GURL& report_url :
       {bidder_report_to_url, seller_report_to_url, bidder_debug_win_report_url,
        seller_debug_win_report_url}) {
    SCOPED_TRACE(report_url.spec());
    EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
              url_loader_monitor.GetRequestInfo(report_url)
                  ->trusted_params->client_security_state->ip_address_space);
  }

  // Check that all reports reached the server.
  WaitForURL(bidder_report_to_url);
  WaitForURL(seller_report_to_url);
  WaitForURL(bidder_debug_win_report_url);
  WaitForURL(seller_debug_win_report_url);
}

// Make sure that the IPAddressSpace of the frame that triggers the update is
// respected for the update request. Does this by adding an interest group,
// trying to update it from a public page, and expecting the request to be
// blocked, and then adding another interest group and updating it from a
// private page, which should succeed. Have to use two interest groups to avoid
// the delay between updates.
IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       UpdatePublicVsPrivateNetwork) {
  const char kPubliclyUpdateGroupName[] = "Publicly updated group";
  const char kLocallyUpdateGroupName[] = "Locally updated group";

  GURL update_url = https_server_->GetURL(
      "a.test", "/interest_group/daily_update_partial.json");
  GURL initial_bidding_url = https_server_->GetURL(
      "a.test", "/interest_group/initial_bidding_logic.js");
  GURL new_bidding_url =
      https_server_->GetURL("a.test", "/interest_group/new_bidding_logic.js");

  // The server JSON updates biddingLogicUrl only.
  network_responder_->RegisterNetworkResponse(update_url.path(),
                                              JsReplace(R"(
{
  "biddingLogicUrl": $1
}
                                                        )",
                                                        new_bidding_url));

  URLLoaderMonitor url_loader_monitor;
  for (bool public_address_space : {true, false}) {
    SCOPED_TRACE(public_address_space);

    GURL test_url;
    std::string group_name;
    if (public_address_space) {
      // This header treats a response from a server on a private IP as if the
      // server were on public address space.
      test_url = https_server_->GetURL(
          "a.test",
          "/set-header?Content-Security-Policy: treat-as-public-address");
      group_name = kPubliclyUpdateGroupName;
    } else {
      test_url = https_server_->GetURL("a.test", "/echo");
      group_name = kLocallyUpdateGroupName;
    }
    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    ASSERT_EQ(kSuccess,
              JoinInterestGroupAndVerify(blink::InterestGroup(
                  /*expiry=*/base::Time(),
                  /*owner=*/url::Origin::Create(test_url), group_name,
                  /*priority=*/0.0, initial_bidding_url,
                  /*bidding_wasm_helper_url=*/absl::nullopt, update_url,
                  /*trusted_bidding_signals_url=*/absl::nullopt,
                  /*trusted_bidding_signals_keys=*/absl::nullopt,
                  /*user_bidding_signals=*/absl::nullopt,
                  /*ads=*/
                  {{{GURL("https://example.com/render"),
                     /*metadata=*/absl::nullopt}}},
                  /*ad_components=*/absl::nullopt)));

    EXPECT_EQ("done", UpdateInterestGroupsInJS());

    // Wait for the update request to be made, and check its IPAddressSpace.
    url_loader_monitor.WaitForUrls();
    const network::ResourceRequest& request =
        url_loader_monitor.WaitForUrl(update_url);
    ASSERT_TRUE(request.trusted_params->client_security_state);
    if (public_address_space) {
      EXPECT_EQ(
          network::mojom::IPAddressSpace::kPublic,
          request.trusted_params->client_security_state->ip_address_space);
    } else {
      EXPECT_EQ(
          network::mojom::IPAddressSpace::kLocal,
          request.trusted_params->client_security_state->ip_address_space);
    }
    // Not the main purpose of this test, but it should be using a transient
    // NetworkIsolationKey as well.
    ASSERT_TRUE(request.trusted_params->isolation_info.network_isolation_key()
                    .IsTransient());

    // The request should be blocked in the public address space case.
    if (public_address_space) {
      EXPECT_EQ(
          net::ERR_FAILED,
          url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
    } else {
      EXPECT_EQ(
          net::OK,
          url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
    }

    url_loader_monitor.ClearRequests();
  }

  // Wait for the kLocallyUpdateGroupName interest group to have an updated
  // bidding URL, while expecting the kPubliclyUpdateGroupName to continue to
  // have the original bidding URL. Have to wait because just because
  // URLLoaderMonitor has seen the request completed successfully doesn't mean
  // that the InterestGroup has been updated yet.
  WaitForInterestGroupsSatisfying(
      url::Origin::Create(initial_bidding_url),
      base::BindLambdaForTesting(
          [&](const std::vector<StorageInterestGroup>& storage_groups) {
            bool found_updated_group = false;
            for (const auto& storage_group : storage_groups) {
              const blink::InterestGroup& group = storage_group.interest_group;
              if (group.name == kPubliclyUpdateGroupName) {
                EXPECT_EQ(initial_bidding_url, group.bidding_url);
              } else {
                EXPECT_EQ(group.name, kLocallyUpdateGroupName);
                found_updated_group = (new_bidding_url == group.bidding_url);
              }
            }
            return found_updated_group;
          }));
}

// Create three interest groups, each belonging to different origins. Update one
// on a private network, but delay its server response. Update the second on a
// public network (thus expecting the request to be blocked). Update the final
// interest group on a private interest group -- it should be updated after the
// first two. After the server responds to the first update request, all updates
// should proceed -- the first should succeed, and the second should be blocked
// since the page is on a public network, and the third should succeed.
IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       PrivateNetProtectionsApplyToSubsequentUpdates) {
  constexpr char kLocallyUpdateGroupName[] = "Locally updated group";
  constexpr char kPubliclyUpdateGroupName[] = "Publicly updated group";

  // The update for a.test happens locally and gets deferred, whereas the update
  // for b.test and c.test are allowed to proceed immediately.
  const GURL update_url_a =
      https_server_->GetURL("a.test", kDeferredUpdateResponsePath);
  const GURL update_url_b = https_server_->GetURL(
      "b.test", "/interest_group/daily_update_partial_b.json");
  const GURL update_url_c = https_server_->GetURL(
      "c.test", "/interest_group/daily_update_partial_c.json");

  constexpr char kInitialBiddingPath[] =
      "/interest_group/initial_bidding_logic.js";
  const GURL initial_bidding_url_a =
      https_server_->GetURL("a.test", kInitialBiddingPath);
  const GURL initial_bidding_url_b =
      https_server_->GetURL("b.test", kInitialBiddingPath);
  const GURL initial_bidding_url_c =
      https_server_->GetURL("c.test", kInitialBiddingPath);

  constexpr char kNewBiddingPath[] = "/interest_group/new_bidding_logic.js";
  const GURL new_bidding_url_a =
      https_server_->GetURL("a.test", kNewBiddingPath);
  const GURL new_bidding_url_b =
      https_server_->GetURL("b.test", kNewBiddingPath);
  const GURL new_bidding_url_c =
      https_server_->GetURL("c.test", kNewBiddingPath);

  // The server JSON updates biddingLogicUrl only.
  constexpr char kUpdateContentTemplate[] = R"(
{
  "biddingLogicUrl": $1
}
)";
  // a.test's response is delayed until later.
  network_responder_->RegisterNetworkResponse(
      update_url_b.path(),
      JsReplace(kUpdateContentTemplate, new_bidding_url_b));
  network_responder_->RegisterNetworkResponse(
      update_url_c.path(),
      JsReplace(kUpdateContentTemplate, new_bidding_url_c));

  // First, create an interest group in a.test and start updating it from a
  // private site. The update doesn't finish yet because the network response
  // is delayed.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(initial_bidding_url_a),
          kLocallyUpdateGroupName, /*priority=*/0.0, initial_bidding_url_a,
          /*bidding_wasm_helper_url=*/absl::nullopt, update_url_a,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Now, create an interest group in b.test and start updating it from a
  // public site. The update will be delayed because the first interest group
  // hasn't finished updating, and it should get blocked because we are on a
  // public page.
  ASSERT_TRUE(NavigateToURL(
      shell(),
      https_server_->GetURL(
          "b.test",
          "/set-header?Content-Security-Policy: treat-as-public-address")));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(initial_bidding_url_b),
          kPubliclyUpdateGroupName, /*priority=*/0.0, initial_bidding_url_b,
          /*bidding_wasm_helper_url=*/absl::nullopt, update_url_b,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Finally, create and update the last interest group on a private network --
  // this update shouldn't be blocked.
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("c.test", "/echo")));

  ASSERT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/url::Origin::Create(initial_bidding_url_c),
          kLocallyUpdateGroupName, /*priority=*/0.0, initial_bidding_url_c,
          /*bidding_wasm_helper_url=*/absl::nullopt, update_url_c,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}},
          /*ad_components=*/absl::nullopt)));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  // Now, finish the first interest group update by responding to its update
  // network request. All interest groups should be able to update now.
  network_responder_->DoDeferredUpdateResponse(
      JsReplace(kUpdateContentTemplate, new_bidding_url_a));

  // Wait for the c.test to update -- after it updates, all the other interest
  // groups should have updated too.
  WaitForInterestGroupsSatisfying(
      url::Origin::Create(initial_bidding_url_c),
      base::BindLambdaForTesting(
          [&](const std::vector<StorageInterestGroup>& storage_groups) {
            return storage_groups.size() == 1 &&
                   storage_groups[0].interest_group.bidding_url ==
                       new_bidding_url_c;
          }));

  // By this point, all the interest group updates should have completed.
  std::vector<StorageInterestGroup> a_groups =
      GetInterestGroupsForOwner(url::Origin::Create(initial_bidding_url_a));
  ASSERT_EQ(a_groups.size(), 1u);
  EXPECT_EQ(a_groups[0].interest_group.bidding_url, new_bidding_url_a);

  std::vector<StorageInterestGroup> b_groups =
      GetInterestGroupsForOwner(url::Origin::Create(initial_bidding_url_b));
  ASSERT_EQ(b_groups.size(), 1u);

  // Because it was updated on a public address, the update for b.test didn't
  // happen.
  EXPECT_EQ(b_groups[0].interest_group.bidding_url, initial_bidding_url_b);
}

// Do the following, both where the created interest groups are updated from
// public addresses, and where they are updated from private addresses. (The
// private should come second so we can block on its updates completing).
//
// Create 2 interest groups of different owners, and then run a component
// auction where the first group's owner is a buyer on the outer auction, and
// the second group's owner is a buyer on the inner auction. (The auction is
// expected to fail when run from a public address, but an attempt should still
// be made to update interest groups).
//
// After this, check the results of all 4 interest groups. Those updated from
// private IPs should have updated, whereas those updated from public IPs
// should not have updated.
IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       PrivateNetProtectionsApplyToPostAuctionUpdates) {
  const char kPubliclyUpdateGroupName[] = "Publicly updated group";
  const char kLocallyUpdateGroupName[] = "Locally updated group";

  // The update URLs are always local, regardless of whether we're on a local or
  // private page.
  const GURL update_url_a = https_server_->GetURL(
      "a.test", "/interest_group/daily_update_partial_a.json");
  const GURL update_url_b = https_server_->GetURL(
      "b.test", "/interest_group/daily_update_partial_b.json");

  constexpr char kUpdateResponse[] = R"(
{
"ads": [{"renderUrl": "https://example.com/render2"
        }]
})";

  // The server JSON updates the ads only.
  network_responder_->RegisterNetworkResponse(update_url_a.path(),
                                              kUpdateResponse);
  network_responder_->RegisterNetworkResponse(update_url_b.path(),
                                              kUpdateResponse);

  const url::Origin test_origin_a =
      url::Origin::Create(https_server_->GetURL("a.test", "/echo"));
  const url::Origin test_origin_b =
      url::Origin::Create(https_server_->GetURL("b.test", "/echo"));

  URLLoaderMonitor url_loader_monitor;
  for (bool public_address_space : {true, false}) {
    SCOPED_TRACE(public_address_space);

    GURL test_url_a;
    GURL test_url_b;
    std::string group_name;
    if (public_address_space) {
      // This header treats a response from a server on a private IP as if the
      // server were on public address space.
      test_url_a = https_server_->GetURL(
          "a.test",
          "/set-header?Content-Security-Policy: treat-as-public-address");
      test_url_b = https_server_->GetURL(
          "b.test",
          "/set-header?Content-Security-Policy: treat-as-public-address");
      group_name = kPubliclyUpdateGroupName;
    } else {
      test_url_a = https_server_->GetURL("a.test", "/echo");
      test_url_b = https_server_->GetURL("b.test", "/echo");
      group_name = kLocallyUpdateGroupName;
    }

    ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
    EXPECT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(blink::InterestGroup(
            /*expiry=*/base::Time(),
            /*owner=*/test_origin_a,
            /*name=*/group_name,
            /*priority=*/0.0,
            /*bidding_url=*/
            https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
            /*bidding_wasm_helper_url=*/absl::nullopt,
            /*daily_update_url=*/update_url_a,
            /*trusted_bidding_signals_url=*/absl::nullopt,
            /*trusted_bidding_signals_keys=*/absl::nullopt,
            /*user_bidding_signals=*/absl::nullopt,
            /*ads=*/
            {{{GURL("https://example.com/render"),
               /*metadata=*/absl::nullopt}}},
            /*ad_components=*/absl::nullopt)));

    ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
    EXPECT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(blink::InterestGroup(
            /*expiry=*/base::Time(),
            /*owner=*/test_origin_b,
            /*name=*/group_name,
            /*priority=*/0.0,
            /*bidding_url=*/
            https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
            /*bidding_wasm_helper_url=*/absl::nullopt,
            /*daily_update_url=*/update_url_b,
            /*trusted_bidding_signals_url=*/absl::nullopt,
            /*trusted_bidding_signals_keys=*/absl::nullopt,
            /*user_bidding_signals=*/absl::nullopt,
            /*ads=*/
            {{{GURL("https://example.com/render"),
               /*metadata=*/absl::nullopt}}},
            /*ad_components=*/absl::nullopt)));

    ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
    EvalJsResult auction_result = EvalJs(
        shell(), JsReplace(
                     R"(
(async function() {
  return await navigator.runAdAuction({
    seller: $1,
    decisionLogicUrl: $3,
    interestGroupBuyers: [$1],
    auctionSignals: "bidderAllowsComponentAuction,"+
                    "sellerAllowsComponentAuction",
    componentAuctions: [{
      seller: $1,
      decisionLogicUrl: $3,
      interestGroupBuyers: [$2],
      auctionSignals: "bidderAllowsComponentAuction,"+
                      "sellerAllowsComponentAuction"

    }],
  });
})())",
                     test_origin_a, test_origin_b,
                     https_server_->GetURL(
                         "a.test", "/interest_group/decision_logic.js")));
    if (public_address_space) {
      // The auction fails because the scripts get blocked; the update request
      // should still happen though.
      EXPECT_EQ(nullptr, auction_result);
    } else {
      TestFencedFrameURLMappingResultObserver observer;
      ConvertFencedFrameURNToURL(GURL(auction_result.ExtractString()),
                                 &observer);
      EXPECT_EQ(GURL("https://example.com/render"), observer.mapped_url());
    }

    // Wait for the update request to be made, and check its IPAddressSpace.
    url_loader_monitor.WaitForUrls();
    for (GURL update_url : {update_url_a, update_url_b}) {
      const network::ResourceRequest& request =
          url_loader_monitor.WaitForUrl(update_url);
      ASSERT_TRUE(request.trusted_params->client_security_state);
      if (public_address_space) {
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kPublic,
            request.trusted_params->client_security_state->ip_address_space);
      } else {
        EXPECT_EQ(
            network::mojom::IPAddressSpace::kLocal,
            request.trusted_params->client_security_state->ip_address_space);
      }
      // Not the main purpose of this test, but it should be using a transient
      // NetworkIsolationKey as well.
      ASSERT_TRUE(request.trusted_params->isolation_info.network_isolation_key()
                      .IsTransient());

      // The request should be blocked in the public address space case.
      if (public_address_space) {
        EXPECT_EQ(
            net::ERR_FAILED,
            url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
      } else {
        EXPECT_EQ(
            net::OK,
            url_loader_monitor.WaitForRequestCompletion(update_url).error_code);
      }
    }
    url_loader_monitor.ClearRequests();
  }

  // Wait for the local origin A and origin B interest groups to update. Then
  // check that the public origin A and origin B interest groups didn't update.
  const GURL initial_ad_url = GURL("https://example.com/render");
  const GURL new_ad_url = GURL("https://example.com/render2");

  auto update_condition = base::BindLambdaForTesting(
      [&](const std::vector<StorageInterestGroup>& storage_groups) {
        EXPECT_EQ(storage_groups.size(), 2u);
        bool found_updated_group = false;
        bool found_non_updated_group = false;
        for (const auto& storage_group : storage_groups) {
          const blink::InterestGroup& group = storage_group.interest_group;
          EXPECT_TRUE(group.ads.has_value());
          EXPECT_EQ(group.ads->size(), 1u);
          if (group.name == kPubliclyUpdateGroupName) {
            EXPECT_EQ(initial_ad_url, group.ads.value()[0].render_url);
            found_non_updated_group = true;
          } else {
            EXPECT_EQ(group.name, kLocallyUpdateGroupName);
            found_updated_group =
                (new_ad_url == group.ads.value()[0].render_url);
          }
        }
        return found_updated_group && found_non_updated_group;
      });

  WaitForInterestGroupsSatisfying(test_origin_a, update_condition);
  WaitForInterestGroupsSatisfying(test_origin_b, update_condition);
}

// Interest group APIs succeeded (i.e., feature join-ad-interest-group is
// enabled by Permissions Policy), and runAdAuction succeeded (i.e., feature
// run-ad-auction is enabled by Permissions Policy) in all contexts, because
// the kAdInterestGroupAPIRestrictedPolicyByDefault runtime flag is disabled by
// default and in that case the default value for those features are
// EnableForAll.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       FeaturesEnabledForAllByPermissionsPolicy) {
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "a.test,"
          "b.test("
              "c.test{allow-join-ad-interest-group;run-ad-auction},"
              "a.test{allow-join-ad-interest-group;run-ad-auction},"
              "a.test{allow-join-ad-interest-group;run-ad-auction}"
          ")"
       ")");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  RenderFrameHost* same_origin_iframe = ChildFrameAt(main_frame, 0);
  RenderFrameHost* cross_origin_iframe = ChildFrameAt(main_frame, 1);
  RenderFrameHost* inner_cross_origin_iframe =
      ChildFrameAt(cross_origin_iframe, 0);
  RenderFrameHost* same_origin_iframe_in_cross_origin_iframe =
      ChildFrameAt(cross_origin_iframe, 1);
  RenderFrameHost* same_origin_iframe_in_cross_origin_iframe2 =
      ChildFrameAt(cross_origin_iframe, 2);

  // The server JSON updates all fields that can be updated.
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  network_responder_->RegisterNetworkResponse(kDailyUpdateUrlPath,
                                              base::StringPrintf(
                                                  R"(
  {
    "trustedBiddingSignalsKeys": ["new_key"],
  }
                                                  )"));

  GURL url;
  url::Origin origin;
  std::string host;
  RenderFrameHost* execution_targets[] = {
      main_frame,
      same_origin_iframe,
      cross_origin_iframe,
      inner_cross_origin_iframe,
      same_origin_iframe_in_cross_origin_iframe,
      same_origin_iframe_in_cross_origin_iframe2};

  for (auto* execution_target : execution_targets) {
    url = execution_target->GetLastCommittedURL();
    origin = url::Origin::Create(url);
    host = url.host();
    WebContentsConsoleObserver console_observer(shell()->web_contents());
    console_observer.SetPattern(WarningPermissionsPolicy("*", "*"));

    EXPECT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(
            blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(host, "/interest_group/bidding_logic.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/
                https_server_->GetURL(
                    host, "/interest_group/daily_update_partial.json"),
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt),
            execution_target));

    EXPECT_EQ("https://example.com/render",
              RunAuctionAndWaitForURL(
                  JsReplace(
                      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                              )",
                      origin,
                      https_server_->GetURL(
                          host, "/interest_group/decision_logic.js")),
                  execution_target));

    EXPECT_EQ("done", UpdateInterestGroupsInJS(execution_target));
    // The second UpdateInterestGroupsInJS will not add a warning message, since
    // the same message has already been added and redundant messages will be
    // discarded.
    EXPECT_EQ("done", UpdateInterestGroupsInJS(execution_target));
    EXPECT_EQ(kSuccess, LeaveInterestGroup(origin, "cars", execution_target));

    // It seems discard_duplicates of AddConsoleMessage works differently on
    // Android and other platforms. On Android, a message will be discarded if
    // it's not unique across all frames in a page. On other platforms, a
    // message will be discarded if it's not unique per origin (e.g., iframe
    // a.test and iframe b.test can have the same message, while the same
    // message from another a.test will be discarded).

#if BUILDFLAG(IS_ANDROID)
    RenderFrameHost* execution_targets_with_message[] = {cross_origin_iframe};
#else
    RenderFrameHost* execution_targets_with_message[] = {
        cross_origin_iframe, inner_cross_origin_iframe,
        same_origin_iframe_in_cross_origin_iframe};
#endif  // BUILDFLAG(IS_ANDROID)

    if (std::find(std::begin(execution_targets_with_message),
                  std::end(execution_targets_with_message), execution_target) !=
        std::end(execution_targets_with_message)) {
      EXPECT_EQ(WarningPermissionsPolicy("join-ad-interest-group",
                                         "joinAdInterestGroup"),
                console_observer.GetMessageAt(0));
      EXPECT_EQ(WarningPermissionsPolicy("run-ad-auction", "runAdAuction"),
                console_observer.GetMessageAt(1));
      EXPECT_EQ(WarningPermissionsPolicy("join-ad-interest-group",
                                         "updateAdInterestGroups"),
                console_observer.GetMessageAt(2));
      EXPECT_EQ(WarningPermissionsPolicy("join-ad-interest-group",
                                         "leaveAdInterestGroup"),
                console_observer.GetMessageAt(3));
    } else {
      EXPECT_TRUE(console_observer.messages().empty());
    }
  }
}

// Features join-ad-interest-group and run-ad-auction can be disabled by HTTP
// headers, and they cannot be enabled again by container policy in that case.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       FeaturesDisabledByHttpHeader) {
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/page-with-fledge-permissions-policy-disabled.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);

  for (auto* execution_target : {main_frame, iframe}) {
    ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, execution_target);
    ExpectNotAllowedToRunAdAuction(
        origin,
        https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
        execution_target);
    ExpectNotAllowedToLeaveInterestGroup(origin, "cars", execution_target);
  }
}

// Features join-ad-interest-group and run-ad-auction can be disabled by
// container policy.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       FeaturesDisabledByContainerPolicy) {
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/"
      "page-with-fledge-permissions-policy-disabled-in-iframe.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* same_origin_iframe =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);
  ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, same_origin_iframe);
  ExpectNotAllowedToRunAdAuction(
      origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
      same_origin_iframe);
  ExpectNotAllowedToLeaveInterestGroup(origin, "cars", same_origin_iframe);
}

// Interest group APIs succeeded (i.e., feature join-ad-interest-group is
// enabled by Permissions Policy), and runAdAuction succeeded (i.e., feature
// run-ad-auction is enabled by Permissions Policy) in
// (1) same-origin frames by default,
// (2) cross-origin iframes that enable those features in container policy
//     (iframe "allow" attribute).
// (3) cross-origin iframes that enables those features inside parent
//     cross-origin iframes that also enables those features.
IN_PROC_BROWSER_TEST_F(InterestGroupRestrictedPermissionsPolicyBrowserTest,
                       EnabledByPermissionsPolicy) {
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "a.test,"
          "b.test{allow-join-ad-interest-group;run-ad-auction}("
              "c.test{allow-join-ad-interest-group;run-ad-auction}"
          ")"
       ")");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  RenderFrameHost* same_origin_iframe = ChildFrameAt(main_frame, 0);
  RenderFrameHost* cross_origin_iframe = ChildFrameAt(main_frame, 1);
  RenderFrameHost* inner_cross_origin_iframe =
      ChildFrameAt(cross_origin_iframe, 0);

  // The server JSON updates all fields that can be updated.
  constexpr char kDailyUpdateUrlPath[] =
      "/interest_group/daily_update_partial.json";
  network_responder_->RegisterNetworkResponse(kDailyUpdateUrlPath,
                                              base::StringPrintf(
                                                  R"(
  {
    "trustedBiddingSignalsKeys": ["new_key"],
  }
                                                  )"));

  GURL url;
  url::Origin origin;
  std::string host;
  RenderFrameHost* execution_targets[] = {main_frame, same_origin_iframe,
                                          cross_origin_iframe,
                                          inner_cross_origin_iframe};

  for (auto* execution_target : execution_targets) {
    url = execution_target->GetLastCommittedURL();
    origin = url::Origin::Create(url);
    host = url.host();
    WebContentsConsoleObserver console_observer(shell()->web_contents());
    console_observer.SetPattern(WarningPermissionsPolicy("*", "*"));

    EXPECT_EQ(
        kSuccess,
        JoinInterestGroupAndVerify(
            blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/origin,
                /*name=*/"cars",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(host, "/interest_group/bidding_logic.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/
                https_server_->GetURL(
                    host, "/interest_group/daily_update_partial.json"),
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/
                {{{GURL("https://example.com/render"),
                   /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt),
            execution_target));

    EXPECT_EQ("https://example.com/render",
              RunAuctionAndWaitForURL(
                  JsReplace(
                      R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                              )",
                      origin,
                      https_server_->GetURL(
                          host, "/interest_group/decision_logic.js")),
                  execution_target));

    EXPECT_EQ("done", UpdateInterestGroupsInJS(execution_target));
    EXPECT_EQ(kSuccess, LeaveInterestGroup(origin, "cars", execution_target));
    EXPECT_TRUE(console_observer.messages().empty());
  }
}

// Interest group APIs throw NotAllowedError (i.e., feature
// join-ad-interest-group is disabled by Permissions Policy), and runAdAuction
// throws NotAllowedError (i.e, feature run-ad-auction is disabled by
// Permissions Policy) in
// (1) same-origin iframes that disabled the features using allow attribute,
// (2) cross-origin iframes that don't enable those features in container policy
//     (iframe "allow" attribute).
// (3) iframes that enables those features inside parent cross-origin iframes
//     that don't enable those features.
IN_PROC_BROWSER_TEST_F(InterestGroupRestrictedPermissionsPolicyBrowserTest,
                       DisabledByContainerPolicy) {
  GURL other_url = https_server_->GetURL("b.test", "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "b.test("
              "b.test{allow-join-ad-interest-group;run-ad-auction}"
          ")"
       ")");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* outter_iframe =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);
  RenderFrameHost* inner_iframe = ChildFrameAt(outter_iframe, 0);

  for (auto* execution_target : {outter_iframe, inner_iframe}) {
    ExpectNotAllowedToJoinOrUpdateInterestGroup(other_origin, execution_target);
    ExpectNotAllowedToRunAdAuction(
        other_origin,
        https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
        execution_target);
    ExpectNotAllowedToLeaveInterestGroup(other_origin, "cars",
                                         execution_target);
  }

  test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/"
      "page-with-fledge-permissions-policy-disabled-in-iframe.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* same_origin_iframe =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);
  ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, same_origin_iframe);
  ExpectNotAllowedToRunAdAuction(
      origin,
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
      same_origin_iframe);
  ExpectNotAllowedToLeaveInterestGroup(origin, "cars", same_origin_iframe);
}

// Features join-ad-interest-group and run-ad-auction can be enabled/disabled
// separately.
IN_PROC_BROWSER_TEST_F(
    InterestGroupRestrictedPermissionsPolicyBrowserTest,
    EnableOneOfInterestGroupAPIsAndAuctionAPIForIframe) {
  GURL other_url = https_server_->GetURL("b.test", "/echo");
  url::Origin other_origin = url::Origin::Create(other_url);
  // clang-format off
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/cross_site_iframe_factory.html?a.test("
          "b.test{allow-join-ad-interest-group},"
          "b.test{allow-run-ad-auction})");
  // clang-format on
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* iframe_interest_group =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);
  RenderFrameHost* iframe_ad_auction =
      ChildFrameAt(web_contents()->GetMainFrame(), 1);

  // Interest group APIs succeed and run ad auction fails for
  // iframe_interest_group.
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(
                blink::InterestGroup(
                    /*expiry=*/base::Time(),
                    /*owner=*/other_origin,
                    /*name=*/"cars",
                    /*priority=*/0.0,
                    /*bidding_url=*/
                    https_server_->GetURL("b.test",
                                          "/interest_group/bidding_logic.js"),
                    /*bidding_wasm_helper_url=*/absl::nullopt,
                    /*daily_update_url=*/
                    https_server_->GetURL(
                        "b.test", "/interest_group/daily_update_partial.json"),
                    /*trusted_bidding_signals_url=*/absl::nullopt,
                    /*trusted_bidding_signals_keys=*/absl::nullopt,
                    /*user_bidding_signals=*/absl::nullopt,
                    /*ads=*/
                    {{{GURL("https://example.com/render"),
                       /*metadata=*/absl::nullopt}}},
                    /*ad_components=*/absl::nullopt),
                iframe_interest_group));

  EXPECT_EQ("done", UpdateInterestGroupsInJS(iframe_interest_group));
  ExpectNotAllowedToRunAdAuction(
      other_origin,
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js"),
      iframe_interest_group);

  // Interest group APIs fail and run ad auction succeeds for iframe_ad_auction.
  ExpectNotAllowedToJoinOrUpdateInterestGroup(other_origin, iframe_ad_auction);
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(
                JsReplace(
                    R"(
{
  seller: $1,
  decisionLogicUrl: $2,
  interestGroupBuyers: [$1],
}
                            )",
                    other_origin,
                    https_server_->GetURL("b.test",
                                          "/interest_group/decision_logic.js")),
                iframe_ad_auction));
  ExpectNotAllowedToLeaveInterestGroup(other_origin, "cars", iframe_ad_auction);

  EXPECT_EQ(kSuccess,
            LeaveInterestGroup(other_origin, "cars", iframe_interest_group));
}

// Features join-ad-interest-group and run-ad-auction can be disabled by HTTP
// headers, and they cannot be enabled again by container policy in that case.
IN_PROC_BROWSER_TEST_F(InterestGroupRestrictedPermissionsPolicyBrowserTest,
                       DisabledByHttpHeader) {
  GURL test_url = https_server_->GetURL(
      "a.test",
      "/interest_group/page-with-fledge-permissions-policy-disabled.html");
  url::Origin origin = url::Origin::Create(test_url);
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  RenderFrameHost* iframe = ChildFrameAt(main_frame, 0);

  for (auto* execution_target : {main_frame, iframe}) {
    ExpectNotAllowedToJoinOrUpdateInterestGroup(origin, execution_target);
    ExpectNotAllowedToRunAdAuction(
        origin,
        https_server_->GetURL("a.test", "/interest_group/decision_logic.js"),
        execution_target);
    ExpectNotAllowedToLeaveInterestGroup(origin, "cars", execution_target);
  }
}

// navigator.deprecatedURNToURL returns null for an invalid URN.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, InvalidURN) {
  GURL invalid_urn("urn:uuid:c36973b5-e5d9-de59-e4c4-364f137b3c7a");
  EXPECT_EQ(absl::nullopt, ConvertFencedFrameURNToURLInJS(invalid_urn));
}

class InterestGroupAuctionLimitBrowserTest : public InterestGroupBrowserTest {
 public:
  InterestGroupAuctionLimitBrowserTest() {
    // Only 2 auctions are allowed per-page.
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kFledgeLimitNumAuctions, {{"max_auctions_per_page", "2"}}},
         {blink::features::kAdInterestGroupAPIRestrictedPolicyByDefault, {}}},
        /*disabled_features=*/{});
    // TODO(crbug.com/1186444): When
    // kAdInterestGroupAPIRestrictedPolicyByDefault is the default, we won't
    // need to set it here.
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/1289207): Investigate why this is failing on
// android-pie-x86-rel.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_NavigatingWithBfcachePreservesAuctionLimits \
  DISABLED_NavigatingWithBfcachePreservesAuctionLimits
#else
#define MAYBE_NavigatingWithBfcachePreservesAuctionLimits \
  NavigatingWithBfcachePreservesAuctionLimits
#endif  // BUILDFLAG(IS_ANDROID)

// Perform an auction, navigate the top-level frame, then navigate it back.
// Perform 2 more auctions. The second of those two should fail, because 2
// auctions have already been performed on the page -- one before the top level
// bfcached navigations, and one after.
//
// That is, the auction limit count is preserved due to bfcache.
IN_PROC_BROWSER_TEST_F(InterestGroupAuctionLimitBrowserTest,
                       MAYBE_NavigatingWithBfcachePreservesAuctionLimits) {
  const GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  const url::Origin test_origin = url::Origin::Create(test_url);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here : [1,2] }"}}},
          /*ad_components=*/absl::nullopt)));

  // 1st auction -- before navigations
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                test_origin,
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));

  // Navigate, then navigate back. The auction limits shouldn't be reset since
  // the original page goes into the bfcache.
  const GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  TestNavigationObserver back_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_load_observer.Wait();

  // 2nd auction -- after navigations
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                test_origin,
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));

  // 3rd auction -- after navigations; should fail due to hitting the auction
  // limit.
  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
}

// Create a page with a cross-origin iframe. Run an auction in the main frame,
// then run 2 auctions in the cross-origin iframe. The last auction should fail
// due to encontering the auction limit, since the limit is stored per-page (top
// level frame), not per frame.
IN_PROC_BROWSER_TEST_F(InterestGroupAuctionLimitBrowserTest,
                       AuctionLimitSharedWithCrossOriginFrameOnPage) {
  // Give the cross-origin iframe permission to run auctions.
  const GURL test_url =
      https_server_->GetURL("a.test",
                            "/cross_site_iframe_factory.html?a.test(b.test{"
                            "allow-run-ad-auction})");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  const url::Origin test_origin = url::Origin::Create(test_url);
  RenderFrameHost* const b_iframe =
      ChildFrameAt(web_contents()->GetMainFrame(), 0);

  EXPECT_EQ(
      kSuccess,
      JoinInterestGroupAndVerify(blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/test_origin,
          /*name=*/"cars",
          /*priority=*/0.0,
          /*bidding_url=*/
          https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
          /*bidding_wasm_helper_url=*/absl::nullopt,
          /*daily_update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{GURL("https://example.com/render"),
             "{ad:'metadata', here : [1,2] }"}}},
          /*ad_components=*/absl::nullopt)));

  // 1st auction -- in main frame
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(JsReplace(
                R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                test_origin,
                https_server_->GetURL("a.test",
                                      "/interest_group/decision_logic.js"))));

  // 2nd auction -- in cross-origin iframe
  EXPECT_EQ("https://example.com/render",
            RunAuctionAndWaitForURL(
                JsReplace(
                    R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                    test_origin,
                    https_server_->GetURL("a.test",
                                          "/interest_group/decision_logic.js")),
                b_iframe));

  // 3rd auction -- in cross-origin iframe; should fail due to hitting the
  // auction limit.
  EXPECT_EQ(
      nullptr,
      RunAuctionAndWait(JsReplace(
                            R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                })",
                            test_origin,
                            https_server_->GetURL(
                                "a.test", "/interest_group/decision_logic.js")),
                        b_iframe));
}

// forDebuggingOnly.reportAdAuctionLoss() and
// forDebuggingOnly.reportAdAuctionWin() APIs will be disabled (available but do
// nothing) when feature kBiddingAndScoringDebugReportingAPI is disabled.
class InterestGroupBiddingAndScoringDebugReportingAPIDisabledBrowserTest
    : public InterestGroupBrowserTest {
 public:
  InterestGroupBiddingAndScoringDebugReportingAPIDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    InterestGroupBiddingAndScoringDebugReportingAPIDisabledBrowserTest,
    RunAdAuctionWithDebugReporting) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_winner");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_bikes");

  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"winner",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_with_debugging_report.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/{{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
                /*ad_components=*/absl::nullopt)));
  EXPECT_EQ(kSuccess,
            JoinInterestGroupAndVerify(blink::InterestGroup(
                /*expiry=*/base::Time(),
                /*owner=*/test_origin,
                /*name=*/"bikes",
                /*priority=*/0.0,
                /*bidding_url=*/
                https_server_->GetURL(
                    "a.test",
                    "/interest_group/bidding_logic_with_debugging_report.js"),
                /*bidding_wasm_helper_url=*/absl::nullopt,
                /*daily_update_url=*/absl::nullopt,
                /*trusted_bidding_signals_url=*/absl::nullopt,
                /*trusted_bidding_signals_keys=*/absl::nullopt,
                /*user_bidding_signals=*/absl::nullopt,
                /*ads=*/{{{ad2_url, /*metadata=*/absl::nullopt}}},
                /*ad_components=*/absl::nullopt)));

  std::string auction_config = JsReplace(
      R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    auctionSignals: {x: 1},
    sellerSignals: {yet: 'more', info: 1},
    perBuyerSignals: {$1: {even: 'more', x: 4.5}}
                })",
      test_origin,
      https_server_->GetURL(
          "a.test", "/interest_group/decision_logic_with_debugging_report.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  // Check ResourceRequest structs of report requests.
  const GURL kExpectedReportUrls[] = {
      https_server_->GetURL("a.test", "/echoall?report_seller"),
      https_server_->GetURL("a.test", "/echoall?report_bidder/winner")};

  for (const auto& expected_report_url : kExpectedReportUrls) {
    SCOPED_TRACE(expected_report_url);

    // Wait for the report URL to be fetched, which only happens after the
    // auction has completed.
    WaitForURL(expected_report_url);

    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(expected_report_url);
    ASSERT_TRUE(request);
  }

  // No requests should be sent to forDebuggingOnly reporting URLs when
  // feature kBiddingAndScoringDebugReportingAPI is disabled.
  const GURL kDebuggingReportUrls[] = {
      // Debugging report URL from winner for win report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_win/winner"),
      // Debugging report URL from losing bidder for loss report.
      https_server_->GetURL("a.test", "/echo?bidder_debug_report_loss/bikes"),
      // Debugging report URL from seller for loss report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_loss/bikes"),
      // Debugging report URL from seller for win report.
      https_server_->GetURL("a.test", "/echo?seller_debug_report_win/winner")};
  for (const auto& debugging_report_url : kDebuggingReportUrls) {
    absl::optional<network::ResourceRequest> request =
        url_loader_monitor.GetRequestInfo(debugging_report_url);
    ASSERT_FALSE(request);
  }
}

}  // namespace

}  // namespace content
