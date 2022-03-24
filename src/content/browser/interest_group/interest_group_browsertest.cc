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
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/fenced_frame/fenced_frame.h"
#include "content/browser/fenced_frame/fenced_frame_url_mapping.h"
#include "content/browser/interest_group/ad_auction_service_impl.h"
#include "content/browser/interest_group/interest_group_manager.h"
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
#include "content/shell/browser/shell.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/base/network_isolation_key.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
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
  bool IsInterestGroupAPIAllowed(content::BrowserContext* browser_context,
                                 const url::Origin& top_frame_origin,
                                 const GURL& api_url) override {
    return allow_list_.contains(top_frame_origin) &&
           allow_list_.contains(url::Origin::Create(api_url));
  }

 private:
  base::flat_set<url::Origin> allow_list_;
};

// Allows registering responses to network requests.
class NetworkResponder {
 public:
  explicit NetworkResponder(net::EmbeddedTestServer& server) {
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
    response->AddCustomHeader("X-Allow-FLEDGE", "true");
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
};

class InterestGroupBrowserTest : public ContentBrowserTest {
 public:
  InterestGroupBrowserTest() {
    feature_list_.InitWithFeatures(
        /*`enabled_features`=*/
        {blink::features::kInterestGroupStorage,
         blink::features::kAdInterestGroupAPI, blink::features::kParakeet,
         blink::features::kFledge, blink::features::kAllowURNsInIframes},
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
    https_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_server_->RegisterRequestMonitor(base::BindRepeating(
        &InterestGroupBrowserTest::OnHttpsTestServerRequestMonitor,
        base::Unretained(this)));
    network_responder_ = std::make_unique<NetworkResponder>(*https_server_);
    ASSERT_TRUE(https_server_->Start());
    manager_ =
        static_cast<StoragePartitionImpl*>(shell()
                                               ->web_contents()
                                               ->GetBrowserContext()
                                               ->GetDefaultStoragePartition())
            ->GetInterestGroupManager();
    content_browser_client_.SetAllowList(
        {url::Origin::Create(https_server_->GetURL("a.test", "/")),
         url::Origin::Create(https_server_->GetURL("b.test", "/")),
         url::Origin::Create(https_server_->GetURL("c.test", "/")),
         // HTTP origins like those below aren't supported for FLEDGE -- some
         // tests verify that HTTP origins are rejected, even if somehow they
         // are allowed by the allowlist.
         url::Origin::Create(embedded_test_server()->GetURL("a.test", "/")),
         url::Origin::Create(embedded_test_server()->GetURL("b.test", "/")),
         url::Origin::Create(embedded_test_server()->GetURL("c.test", "/"))});
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

  bool JoinInterestGroupInJS(url::Origin owner,
                             std::string name) WARN_UNUSED_RESULT {
    return "done" ==
           EvalJs(shell(),
                  base::StringPrintf(R"(
    (function() {
      navigator.joinAdInterestGroup(
        {name: '%s', owner: '%s'}, /*joinDurationSec=*/ 300);
      return 'done';
    })())",
                                     name.c_str(), owner.Serialize().c_str()));
  }

  // The `trusted_bidding_signals_keys` and `ads` fields of `group` will be
  // ignored in favor of the passed in values.
  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  bool JoinInterestGroupInJS(
      const blink::InterestGroup& group,
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
    // TODO(qingxin): Use base::Value to replace ostringstream.
    std::ostringstream buf;
    buf << "{"
        << "name: '" << group.name << "', "
        << "owner: '" << group.owner << "'";
    if (group.bidding_url) {
      buf << ", biddingLogicUrl: '" << *group.bidding_url << "'";
    }
    if (group.update_url) {
      buf << ", dailyUpdateUrl: '" << *group.update_url << "'";
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

    return "done" == EvalJs(execution_target ? *execution_target : shell(),
                            base::StringPrintf(R"(
    (function() {
      navigator.joinAdInterestGroup(
        %s, /*join_duration_sec=*/ 300);
      return 'done';
    })())",
                                               buf.str().c_str()));
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

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  bool LeaveInterestGroupInJS(
      url::Origin owner, std::string name,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    return "done" ==
           EvalJs(execution_target ? *execution_target : shell(),
                  base::StringPrintf(R"(
    (function() {
      navigator.leaveAdInterestGroup({name: '%s', owner: '%s'});
      return 'done';
    })())",
                                     name.c_str(), owner.Serialize().c_str()));
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
      for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
        interest_groups.emplace_back(interest_group.bidding_group->group.owner,
                                     interest_group.bidding_group->group.name);
      }
    }
    return interest_groups;
  }

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
      if (interest_group.bidding_group->group.name == name) {
        return interest_group.bidding_group->signals->join_count;
      }
    }
    return 0;
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  bool JoinInterestGroupAndWaitInJs(
      const blink::InterestGroup& group,
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
    int initial_count = GetJoinCount(group.owner, group.name);
    if (!JoinInterestGroupInJS(group, execution_target)) {
      return false;
    }
    while (GetJoinCount(group.owner, group.name) != initial_count + 1) {
    }

    return true;
  }

  // Simplified method to join an interest group for tests that only care about
  // a few fields.
  bool JoinInterestGroupAndWaitInJs(
      const url::Origin& owner,
      const std::string& name,
      absl::optional<GURL> bidding_url = absl::nullopt,
      absl::optional<std::vector<blink::InterestGroup::Ad>> ads = absl::nullopt,
      absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components =
          absl::nullopt) {
    return JoinInterestGroupAndWaitInJs(blink::InterestGroup(
        /*expiry=*/base::Time(), owner, name, std::move(bidding_url),
        /*update_url=*/absl::nullopt,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/absl::nullopt,
        /*user_bidding_signals=*/absl::nullopt, std::move(ads),
        std::move(ad_components)));
  }

  bool LeaveInterestGroupAndWait(const url::Origin& owner,
                                 const std::string& name) {
    if (!LeaveInterestGroupInJS(owner, name)) {
      return false;
    }
    while (GetJoinCount(owner, name) != 0) {
    }
    return true;
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  content::EvalJsResult RunAuctionAndWait(
      const std::string& auction_config_json,
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
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
  std::string RunAuctionAndWaitForURL(
      const std::string& auction_config_json,
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
    auto result = RunAuctionAndWait(auction_config_json, execution_target);
    GURL urn_url = GURL(result.ExtractString());
    EXPECT_TRUE(urn_url.is_valid());
    EXPECT_EQ(url::kUrnScheme, urn_url.scheme_piece());
    absl::optional<GURL> maybe_url = ConvertFencedFrameURNToURL(urn_url);
    EXPECT_TRUE(maybe_url) << urn_url;
    return maybe_url->spec();
  }

  // Navigates an iframe with the id="test_iframe" to the provided URL and
  // checks that the last navigated url is the expected url.
  void NavigateIframeAndCheckURL(WebContents* web_contents,
                                 const GURL& url,
                                 const GURL& expected_commit_url) {
    const std::string kIframeId = "test_iframe";
    TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(BeginNavigateIframeToURL(web_contents, kIframeId, url));
    nav_observer.Wait();
    EXPECT_EQ(expected_commit_url, nav_observer.last_navigation_url());
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
    absl::optional<GURL> maybe_url = ConvertFencedFrameURNToURL(urn_url);
    EXPECT_TRUE(maybe_url) << urn_url;
    NavigateIframeAndCheckURL(web_contents(), urn_url, expected_url);
    EXPECT_EQ(expected_url, maybe_url);
  }

  // If `execution_target` is non-null, uses it as the target. Otherwise, uses
  // shell().
  content::EvalJsResult CreateAdRequestAndWait(
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
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
  content::EvalJsResult FinalizeAdAndWait(
      const absl::optional<ToRenderFrameHost> execution_target = absl::nullopt)
      WARN_UNUSED_RESULT {
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
(function() {
  try {
    navigator.joinAdInterestGroup(
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
(function() {
  try {
    navigator.leaveAdInterestGroup({name: '%s', owner: '%s'});
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
        "In the future, feature %s will not be enabled by default by "
        "Permissions Policy (thus calling %s will be rejected with "
        "NotAllowedError) in cross-origin iframes or same-origin iframes nested"
        " in cross-origin iframes",
        feature.c_str(), api.c_str());
  }

  absl::optional<GURL> ConvertFencedFrameURNToURL(
      const GURL& urn_url,
      const absl::optional<ToRenderFrameHost> execution_target =
          absl::nullopt) {
    ToRenderFrameHost adapter(execution_target ? *execution_target : shell());
    const FencedFrameURLMapping& fenced_frame_urls_map =
        static_cast<RenderFrameHostImpl*>(adapter.render_frame_host())
            ->GetPage()
            .fenced_frame_urls_map();
    absl::optional<FencedFrameURLMapping::PendingAdComponentsMap> ignored;
    return fenced_frame_urls_map.ConvertFencedFrameURNToURL(
        const_cast<GURL&>(urn_url), ignored);
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  base::test::ScopedFeatureList feature_list_;
  AllowlistedOriginContentBrowserClient content_browser_client_;
  raw_ptr<ContentBrowserClient> old_content_browser_client_;
  raw_ptr<InterestGroupManager> manager_;
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
    feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames,
        {{"implementation_type", GetFencedFrameFeatureParam()}});
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

    switch (GetParam()) {
      case blink::features::FencedFramesImplementationType::kShadowDOM: {
        observer.Wait();
        break;
      }
      case blink::features::FencedFramesImplementationType::kMPArch: {
        // Wait for the load to complete.
        FencedFrame* fenced_frame = GetFencedFrame(execution_target);
        fenced_frame->WaitForDidStopLoadingForTesting();
      }
    }

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
        https_server_->GetURL("a.test", "/fenced_frames/basic.html");
    ASSERT_TRUE(NavigateToURL(shell(), test_url));

    GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/nested.html");
    EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
        /*owner=*/url::Origin::Create(test_url),
        /*name=*/"cars",
        /*bidding_url=*/
        https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
        /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
        /*ad_components=*/{{{ad_component_url, /*metadata=*/absl::nullopt}}}));

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
    for (const auto& value : result.value.GetList()) {
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
      absl::optional<GURL> mapped_url = ConvertFencedFrameURNToURL(
          (*all_component_urls)[i], render_frame_host);
      EXPECT_EQ(expected_ad_component_urls[i], mapped_url);
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

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, JoinLeaveInterestGroup) {
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

  // This join should succeed and be added to the database.
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(test_origin_a, "cars"));

  // This join should fail and throw an exception since a.test is not the same
  // origin as foo.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(
      url::Origin::Create(GURL("https://foo.a.test")), "cars"));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the bidding_url, bid.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin_a,
      /*name=*/"bicycles",
      /*bidding_url=*/GURL("https://bid.a.test"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/absl::nullopt,
      /*ads=*/absl::nullopt,
      /*ad_components=*/absl::nullopt)));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the update_url, update.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin_a,
      /*name=*/"tricycles",
      /*bidding_url=*/absl::nullopt,
      /*update_url=*/GURL("https://update.a.test"),
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/absl::nullopt,
      /*ads=*/absl::nullopt,
      /*ad_components=*/absl::nullopt)));

  // This join should fail and throw an exception since a.test is not the same
  // origin as the trusted_bidding_signals_url, signals.a.test.
  EXPECT_FALSE(JoinInterestGroupInJS(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin_a,
      /*name=*/"four-wheelers",
      /*bidding_url=*/absl::nullopt,
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/GURL("https://signals.a.test"),
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/absl::nullopt,
      /*ads=*/absl::nullopt,
      /*ad_components=*/absl::nullopt)));

  // This join should silently fail since d.test is not allowlisted for the API,
  // and allowlist checks only happen in the browser process, so don't throw an
  // exception.
  GURL test_url_d = https_server_->GetURL("d.test", "/echo");
  url::Origin test_origin_d = url::Origin::Create(test_url_d);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  EXPECT_TRUE(JoinInterestGroupInJS(test_origin_d, "toys"));

  // Another successful join.
  GURL test_url_b = https_server_->GetURL("b.test", "/echo");
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(test_origin_b, "trucks"));

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
          /*bidding_url=*/absl::nullopt,
          /*update_url=*/absl::nullopt,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/absl::nullopt,
          /*ad_components=*/absl::nullopt),
      test_origin_d.GetURL());

  ASSERT_TRUE(NavigateToURL(shell(), test_url_d));
  // This leave should do nothing because origin_d is not allowed by privacy
  // sandbox.
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_d, "candy"));

  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));
  // This leave should do nothing because there is not interest group of that
  // name.
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_b, "cars"));

  // This leave should silently fail because it is cross-origin.
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  EXPECT_TRUE(LeaveInterestGroupInJS(test_origin_b, "trucks"));

  // This leave should succeed.
  EXPECT_TRUE(LeaveInterestGroupAndWait(test_origin_a, "cars"));

  // We expect that test_origin_b and the (injected) test_origin_d's interest
  // groups remain.
  expected_groups = {{test_origin_b, "trucks"}, {test_origin_d, "candy"}};
  received_groups = GetAllInterestGroups();
  EXPECT_THAT(received_groups,
              testing::UnorderedElementsAreArray(expected_groups));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionStarInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
    seller: 'https://test.com',
    decisionLogicUrl: 'https://test.com/decision_logic',
    interestGroupBuyers: '*',
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       JoinInterestGroupOwnerDoesntMatchFrame) {
  const GURL page_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), page_url));

  EXPECT_EQ(
      base::StringPrintf(
          "TypeError: Failed to execute 'joinAdInterestGroup' on 'Navigator': "
          "owner 'https://test.com' for AuctionAdInterestGroup with name "
          "'cars' match frame origin '%s'.",
          url::Origin::Create(page_url).Serialize().c_str()),
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.joinAdInterestGroup(
        {
          name: 'cars',
          owner: 'https://test.com',
        },
        /*joinDurationSec=*/1);
  } catch (e) {
    return e.toString();
  }
  return 'done';
})())"));
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
(function() {
  try {
    navigator.joinAdInterestGroup(
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
(function() {
  try {
    navigator.joinAdInterestGroup(
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
(function() {
  try {
    navigator.joinAdInterestGroup(
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
(function() {
  try {
    navigator.joinAdInterestGroup(
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
(function() {
  try {
    navigator.joinAdInterestGroup(
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
(function() {
  let x = {};
  let y = {};
  x.a = y;
  y.a = x;
  try {
    navigator.joinAdInterestGroup(
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       LeaveInterestGroupInvalidOwner) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'leaveAdInterestGroup' on 'Navigator': "
      "owner 'https://invalid^&' for AuctionAdInterestGroup with name 'cars' "
      "must be a valid https origin.",
      EvalJs(shell(), R"(
(function() {
  try {
    navigator.leaveAdInterestGroup(
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDecisionLogicUrlDifferentFromSeller) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2]}"}}},
      /*ad_components=*/absl::nullopt)));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                         })",
                         test_origin,
                         https_server_->GetURL(
                             "b.test", "/interest_group/decision_logic.js"))));
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionInvalidInterestGroupBuyersStr) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(
      "TypeError: Failed to execute 'runAdAuction' on 'Navigator': "
      "interestGroupBuyers 'not star' for AuctionAdConfig with seller "
      "'https://test.com' must be \"*\" (wildcard) or a list of buyer "
      "https origin strings.",
      RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: 'not star',
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionNoInterestGroupBuyersField) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
  })"));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionNoInterestGroupBuyers) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  EXPECT_EQ(nullptr, RunAuctionAndWait(R"({
      seller: 'https://test.com',
      decisionLogicUrl: 'https://test.com',
      interestGroupBuyers: [],
  })"));
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionPerBuyerSignalsOriginNotInBuyers) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2]}"}}},
      /*ad_components=*/absl::nullopt)));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
    perBuyerSignals: {$1: {a:1}, 'https://not_in_buyers.com': {a:1}}
                         })",
                         test_origin,
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionBuyersNoInterestGroup) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  EXPECT_EQ(nullptr, RunAuctionAndWait(JsReplace(
                         R"({
    seller: $1,
    decisionLogicUrl: $2,
    interestGroupBuyers: [$1],
                         })",
                         url::Origin::Create(test_url),
                         https_server_->GetURL(
                             "a.test", "/interest_group/decision_logic.js"))));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionPrivacySandboxDisabled) {
  // Successful join at a.test
  GURL test_url_a = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url_a));
  url::Origin test_origin_a = url::Origin::Create(test_url_a);
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin_a,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2]}"}}},
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       RunAdAuctionDisabledInterestGroup) {
  // Inject an interest group into the DB for that for a disabled site so we can
  // try to remove it.
  GURL disabled_domain = https_server_->GetURL("d.test", "/");
  url::Origin disabled_origin = url::Origin::Create(disabled_domain);
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

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL(test_url.host(),
                            "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL(test_url.host(),
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
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
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);
  GURL ad_url = https_server_->GetURL("c.test", "/echo?render_cars");

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad_url, "{ad:'metadata', here:[1,2]}"}}},
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
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

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

// Runs auction just like the above test, but runs with fenced frames enabled
// and expects to receive a URN URL to be used. After the auction, loads the URL
// in a fenced frame, and expects the correct URL is loaded.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       RunAdAuctionWithWinner) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  GURL ad_url = https_server_->GetURL(
      "c.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
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

// Use different origins for publisher, bidder, and seller, and make sure
// everything works as expected.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, CrossOrigin) {
  const char kPublisher[] = "a.test";
  const char kBidder[] = "b.test";
  const char kSeller[] = "c.test";

  // Navigate to bidder site, and add an interest group.
  GURL bidder_url = https_server_->GetURL(kBidder, "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), bidder_url));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/bidder_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL(kBidder, "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL(kBidder,
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2]}"}}},
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

  // Reporting urls should be fetched after an auction succeeded.
  WaitForURL(https_server_->GetURL("/echoall?report_seller"));
  WaitForURL(https_server_->GetURL("/echoall?report_bidder"));
  // Double-check that the trusted scoring signals URL was requested as well.
  WaitForURL(
      https_server_->GetURL("/interest_group/trusted_scoring_signals.json"
                            "?hostname=a.test"
                            "&renderUrls=https%3A%2F%2Fexample.com%2Frender"));
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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/other_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL(
          kOtherHost,
          "/interest_group/bidding_logic_expect_top_frame_a_test.js"),
      /*update_url=*/absl::nullopt,
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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/bidder_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL(kBidderHost, "/interest_group/bidding_logic.js"),
      /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}));

  GURL main_frame_url = https_server_->GetURL(
      kTopFrameHost,
      base::StringPrintf(
          "/cross_site_iframe_factory.html?%s(%s)", kTopFrameHost,
          https_server_->GetURL(kIframeHost, "/fenced_frames/basic.html")
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

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL(
          "a.test", "/interest_group/bidding_logic_stop_bidding_after_win.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad1_url,
         /*metadata=*/absl::nullopt}}},
      /*ad_components=*/absl::nullopt)));
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"bikes",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad2_url, /*metadata=*/absl::nullopt}}},
      /*ad_components=*/absl::nullopt)));
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"shoes",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad3_url, /*metadata=*/absl::nullopt}}},
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

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionMultipleAuctions) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  const url::Origin origin = url::Origin::Create(test_url);

  GURL ad1_url =
      https_server_->GetURL("c.test", "/echo?stop_bidding_after_win");
  GURL ad2_url = https_server_->GetURL("c.test", "/echo?render_shoes");

  // This group will win if it has never won an auction.
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL(
          "a.test", "/interest_group/bidding_logic_stop_bidding_after_win.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad1_url, "{ad:'metadata', here:[1,2]}"}}},
      /*ad_components=*/absl::nullopt)));

  GURL test_url2 = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url2));
  const url::Origin origin2 = url::Origin::Create(test_url2);
  // This group will win if the other interest group has won an auction.
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/origin2,
      /*name=*/"shoes",
      /*bidding_url=*/
      https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad2_url, /*metadata=*/absl::nullopt}}},
      /*ad_components=*/absl::nullopt)));

  // Both owners have one interest group in storage, and both interest groups
  // have no `prev_wins`.
  std::vector<StorageInterestGroup> storage_interest_groups =
      GetInterestGroupsForOwner(origin);
  EXPECT_EQ(storage_interest_groups.size(), 1u);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_group->signals->prev_wins.size(),
      0u);
  EXPECT_EQ(storage_interest_groups.front().bidding_group->signals->bid_count,
            0);
  std::vector<StorageInterestGroup> storage_interest_groups2 =
      GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(storage_interest_groups2.size(), 1u);
  EXPECT_EQ(
      storage_interest_groups2.front().bidding_group->signals->prev_wins.size(),
      0u);
  EXPECT_EQ(storage_interest_groups2.front().bidding_group->signals->bid_count,
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
      storage_interest_groups.front().bidding_group->signals->prev_wins.size(),
      1u);
  EXPECT_EQ(
      storage_interest_groups2.front().bidding_group->signals->prev_wins.size(),
      0u);
  EXPECT_EQ(
      storage_interest_groups.front()
          .bidding_group->signals->prev_wins.front()
          ->ad_json,
      JsReplace(
          R"({"render_url":$1,"metadata":{"ad":"metadata","here":[1,2]}})",
          ad1_url));
  EXPECT_EQ(storage_interest_groups.front().bidding_group->signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_group->signals->bid_count,
            1);

  // Run auction again. Interest group shoes of owner `test_url2` wins.
  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad2_url);
  // `test_url2`'s interest group shoes has one `prev_wins` in storage.
  storage_interest_groups = GetInterestGroupsForOwner(origin);
  storage_interest_groups2 = GetInterestGroupsForOwner(origin2);
  EXPECT_EQ(
      storage_interest_groups.front().bidding_group->signals->prev_wins.size(),
      1u);
  EXPECT_EQ(
      storage_interest_groups2.front().bidding_group->signals->prev_wins.size(),
      1u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_group->signals->prev_wins.front()
                ->ad_json,
            JsReplace(R"({"render_url":$1})", ad2_url));
  // First interest group didn't bid this time.
  EXPECT_EQ(storage_interest_groups.front().bidding_group->signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_group->signals->bid_count,
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
      storage_interest_groups.front().bidding_group->signals->prev_wins.size(),
      1u);
  EXPECT_EQ(
      storage_interest_groups2.front().bidding_group->signals->prev_wins.size(),
      2u);
  EXPECT_EQ(storage_interest_groups2.front()
                .bidding_group->signals->prev_wins.back()
                ->ad_json,
            JsReplace(R"({"render_url":$1})", ad2_url));
  // First interest group didn't bid this time.
  EXPECT_EQ(storage_interest_groups.front().bidding_group->signals->bid_count,
            1);
  EXPECT_EQ(storage_interest_groups2.front().bidding_group->signals->bid_count,
            3);
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

  // Use JoinInterestGroupInJS() instead of JoinInterestGroupAndWaitInJs().

  EXPECT_TRUE(JoinInterestGroupInJS(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{ad_url, "{ad:'metadata', here : [1,2] }"}}},
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
      https_server_->GetURL("a.test", "/interest_group/decision_logic.js"));

  RunAuctionAndWaitForURLAndNavigateIframe(auction_config, ad_url);

  // Leave the interest group, then re-run the auction. We shouldn't get a
  // result.
  LeaveInterestGroupInJS(/*owner=*/test_origin, /*name=*/"cars");
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

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/bidding_logic_invalid_ad_url.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{GURL("https://shoes.com/render"), "{ad:'metadata', here : [1,2] }"}}},
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
  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  // Trying to retrieve the adAuctionComponents of the main frame should throw
  // an exception.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(shell(), 1));

  GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/nested.html");
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/url::Origin::Create(test_url),
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
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

  // Navigate the ad component fenced frame to some of the URNs, which
  // should navigate it to about:blank. MPArch mode currently crashes on
  // navigations to about:blank, so skip in that case.
  //
  // TODO(https://crbug.com/1268238): Always do this once MPArch can handle
  // about:blank navigations.
  if (GetParam() != blink::features::FencedFramesImplementationType::kMPArch) {
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

  // Navigate the ad component fenced frame to some of the about:blank URNs.
  // MPArch mode currently crashes on navigations to about:blank, so skip in
  // that case.
  //
  // TODO(https://crbug.com/1268238): Always do this once MPArch can handle
  // about:blank navigations.
  if (GetParam() != blink::features::FencedFramesImplementationType::kMPArch) {
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
      https_server_->GetURL("d.test", "/fenced_frames/nested.html");
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
  // some of the URNs, which should navigate it to about:blank. MPArch mode
  // currently crashes on navigations to about:blank, so skip in that case.
  //
  // TODO(https://crbug.com/1268238): Always do this once MPArch can handle
  // about:blank navigations.
  if (GetParam() != blink::features::FencedFramesImplementationType::kMPArch) {
    absl::optional<std::vector<GURL>> all_component_urls =
        GetAdAuctionComponentsInJS(ad_component_frame,
                                   blink::kMaxAdAuctionAdComponents);
    ASSERT_TRUE(all_component_urls);
    NavigateFencedFrameAndWait((*all_component_urls)[0],
                               GURL(url::kAboutBlankURL),
                               GetFencedFrameRenderFrameHost(shell()));
    NavigateFencedFrameAndWait(
        (*all_component_urls)[blink::kMaxAdAuctionAdComponents - 1],
        GURL(url::kAboutBlankURL), GetFencedFrameRenderFrameHost(shell()));
  }

  // Load a new URL in the top-level fenced frame, which should cause future
  // navigator.adComponents() calls to fail. Use a new URL, so can wait for the
  // server to see it. Same origin navigation so that the RenderFrameHost will
  // be reused.
  GURL new_url = https_server_->GetURL(
      ad_frame->GetLastCommittedOrigin().host(), "/echoall");

  // Use to wait for navigation completion in the ShadowDOM case only.
  // Harmlessly created but not used in the MPArch case.
  TestFrameNavigationObserver observer(ad_frame);

  EXPECT_TRUE(ExecJs(ad_frame, JsReplace("document.location = $1;", new_url)));

  // Wait for the URL to be requested, to make sure the fenced frame actually
  // made the request and, in the MPArch case, to make sure the load actually
  // started.
  WaitForURL(new_url);

  // Wait for the load to complete.
  switch (GetParam()) {
    case blink::features::FencedFramesImplementationType::kShadowDOM: {
      observer.Wait();
      break;
    }
    case blink::features::FencedFramesImplementationType::kMPArch: {
      // Wait for the load to complete.
      FencedFrame* fenced_frame = GetFencedFrame(shell());
      fenced_frame->WaitForDidStopLoadingForTesting();
    }
  }

  // Navigating the ad fenced frame may result in it using a new
  // RenderFrameHost, invalidating the old `ad_frame`.
  ad_frame = GetFencedFrameRenderFrameHost(shell());

  // Make sure the expected page has loaded in the ad frame.
  EXPECT_EQ(new_url, ad_frame->GetLastCommittedURL());

  // Calling navigator.adAuctionComponents on the new frame should fail.
  EXPECT_FALSE(GetAdAuctionComponentsInJS(ad_frame, 1));
}

// Test navigating multiple fenced frames to the same render URL from a single
// auction, when the winning bid included ad components. All fenced frames
// navigated to the URL should get ad component URLs from the winning bid.
IN_PROC_BROWSER_TEST_P(InterestGroupFencedFrameBrowserTest,
                       AdComponentsMainAdLoadedInMultipleFrames) {
  GURL ad_component_url = https_server_->GetURL(
      "d.test", "/set-header?Supports-Loading-Mode: fenced-frame");
  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/nested.html");
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/url::Origin::Create(test_url),
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}},
      /*ad_components=*/{{{ad_component_url, /*metadata=*/absl::nullopt}}}));

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
      EXPECT_TRUE(ExecJs(
          shell(),
          "document.querySelector('fencedframe').remove();"
          "document.body.appendChild(document.createElement('fencedframe'));"));
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

  GURL test_url = https_server_->GetURL("a.test", "/fenced_frames/basic.html");
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

  GURL ad_url = https_server_->GetURL("c.test", "/fenced_frames/nested.html");
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/url::Origin::Create(test_url),
      /*name=*/"cars", bidding_url,
      /*ads=*/{{{ad_url, /*metadata=*/absl::nullopt}}}, ad_components));

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
  // MPArch currently crashes on navigations to about:blank.
  //
  // TODO(https://crbug.com/1268238): Always do this once MPArch can handle
  // about:blank navigations.
  if (GetParam() != blink::features::FencedFramesImplementationType::kMPArch) {
    NavigateFencedFrameAndWait((*components)[2], GURL(url::kAboutBlankURL),
                               ad_frame);
  }
}

// These end-to-end tests validate that information from navigator-exposed APIs
// is correctly passed to worklets.

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       BuyerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/bidding_logic_throws.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2,3]}"}}},
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

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/second_bidder_origin,
      /*name=*/"boats",
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

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/bidder_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL(kBidderHost,
                            "/interest_group/bidding_argument_validator.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL(kBidderHost,
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2,3]}"}}},
      /*ad_components=*/
      {{{GURL("https://example.com/render-component"),
         /*metadata=*/absl::nullopt}}})));

  ASSERT_TRUE(
      NavigateToURL(shell(), https_server_->GetURL(kTopFrameHost, "/echo")));
  GURL seller_script_url = https_server_->GetURL(
      kSellerHost, "/interest_group/decision_argument_validator.js");
  EXPECT_EQ(
      "https://example.com/render",
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
    perBuyerSignals: {$4: {signalsForBuyer: 1}, $5: {signalsForBuyer: 2}}
  });
})())",
                          url::Origin::Create(seller_script_url),
                          seller_script_url,
                          https_server_->GetURL(
                              kSellerHost,
                              "/interest_group/trusted_scoring_signals.json"),
                          bidder_origin, second_bidder_origin))
                   .ExtractString()))
          ->spec());
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest,
                       SellerWorkletThrowsFailsAuction) {
  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2,3]}"}}},
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
(function() {
  try {
    navigator.joinAdInterestGroup(
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

  EXPECT_EQ("https://example.com/render",
            ConvertFencedFrameURNToURL(
                GURL(EvalJs(shell(), JsReplace(
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
                                         https_server_->GetURL(
                                             "a.test", kDecisionLogicPath)))
                         .ExtractString()))
                ->spec());
}

// Make sure that qutting with a live auction doesn't crash.
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, QuitWithRunningAuction) {
  URLLoaderMonitor url_loader_monitor;

  GURL test_url = https_server_->GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));

  GURL hanging_url = https_server_->GetURL("a.test", "/hung");
  url::Origin hanging_origin = url::Origin::Create(hanging_url);

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/hanging_origin,
      /*name=*/"cars",
      /*bidding_url=*/hanging_url,
      /*update_url=*/absl::nullopt,
      /*trusted_bidding_signals_url=*/absl::nullopt,
      /*trusted_bidding_signals_keys=*/absl::nullopt,
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2]}"}}},
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

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/
      https_server_->GetURL("a.test", kDailyUpdateUrlPath),
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2,3]}"}}},
      /*ad_components=*/absl::nullopt)));

  EXPECT_EQ("done", UpdateInterestGroupsInJS());

  WaitForInterestGroupsSatisfying(
      test_origin,
      base::BindLambdaForTesting(
          [](const std::vector<StorageInterestGroup>& groups) {
            if (groups.size() != 1)
              return false;
            const auto& group = groups[0].bidding_group->group;
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

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/"cars",
      /*bidding_url=*/
      https_server_->GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*update_url=*/
      https_server_->GetURL("a.test", kDailyUpdateUrlPath),
      /*trusted_bidding_signals_url=*/
      https_server_->GetURL("a.test",
                            "/interest_group/trusted_bidding_signals.json"),
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/"{some: 'json', data: {here: [1, 2, 3]}}",
      /*ads=*/
      {{{GURL("https://example.com/render"), "{ad:'metadata', here:[1,2,3]}"}}},
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
            const auto& group = groups[0].bidding_group->group;
            return group.name == "cars" && group.bidding_url.has_value() &&
                   group.bidding_url->path() ==
                       "/interest_group/bidding_logic.js" &&
                   group.update_url.has_value() &&
                   group.update_url->path() ==
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

// This test exercises the interest group and ad auction services directly,
// rather than via Blink, to ensure that those services running in the browser
// implement important security checks (Blink may also perform its own
// checking, but the render process is untrusted).
IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTest, RunAdAuctionBasicBypassBlink) {
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("a.test", "/echo")));

  mojo::Remote<blink::mojom::AdAuctionService> auction_service;
  AdAuctionServiceImpl::CreateMojoService(
      web_contents()->GetMainFrame(),
      auction_service.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;

  auction_service->RunAdAuction(
      blink::mojom::AuctionAdConfig::New(),
      base::BindLambdaForTesting([&run_loop](const absl::optional<GURL>& url) {
        EXPECT_THAT(url, Eq(absl::nullopt));
        run_loop.Quit();
      }));
  run_loop.Run();
}

// Fixture for Blink-bypassing auction tests that share the same interest group
// -- useful for checking auction service security validations.
class InterestGroupBrowserTestRunAdAuctionBypassBlink
    : public InterestGroupBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InterestGroupBrowserTest::SetUpOnMainThread();
    ad_url_ = https_server_->GetURL("c.test", "/echo?render_ad");

    GURL test_url_a = https_server_->GetURL("a.test", "/echo");
    test_origin_a_ = url::Origin::Create(test_url_a);
    ASSERT_TRUE(test_url_a.SchemeIs(url::kHttpsScheme));
    ASSERT_TRUE(NavigateToURL(shell(), test_url_a));

    mojo::Remote<blink::mojom::AdAuctionService> interest_service;
    AdAuctionServiceImpl::CreateMojoService(
        web_contents()->GetMainFrame(),
        interest_service.BindNewPipeAndPassReceiver());

    // Set up ad_url_ as the only interest group ad in the auction.
    blink::InterestGroup interest_group;
    interest_group.expiry = base::Time::Now() + base::Seconds(300);
    constexpr char kGroupName[] = "cars";
    interest_group.name = kGroupName;
    interest_group.owner = test_origin_a_;
    interest_group.bidding_url =
        https_server_->GetURL("a.test", "/interest_group/bidding_logic.js");
    interest_group.trusted_bidding_signals_url = https_server_->GetURL(
        "a.test", "/interest_group/trusted_bidding_signals.json");
    interest_group.trusted_bidding_signals_keys.emplace();
    interest_group.trusted_bidding_signals_keys->push_back("key1");
    interest_group.user_bidding_signals =
        "{\"some\": \"json\", \"data\": {\"here\": [1, 2, 3]}}";
    interest_group.ads.emplace();
    interest_group.ads->push_back(blink::InterestGroup::Ad(
        /* render_url = */ ad_url_,
        /* metadata = */ "{\"ad\": \"metadata\", \"here\": [1, 2, 3]}"));
    interest_service->JoinInterestGroup(std::move(interest_group));
    interest_service.FlushForTesting();
    EXPECT_EQ(1, GetJoinCount(test_origin_a_, kGroupName));
  }

  absl::optional<GURL> RunAuctionBypassBlink(
      blink::mojom::AuctionAdConfigPtr config) {
    absl::optional<GURL> maybe_url;
    base::RunLoop run_loop;
    mojo::Remote<blink::mojom::AdAuctionService> auction_service;
    AdAuctionServiceImpl::CreateMojoService(
        web_contents()->GetMainFrame(),
        auction_service.BindNewPipeAndPassReceiver());

    auction_service->RunAdAuction(
        std::move(config),
        base::BindLambdaForTesting(
            [&run_loop, &maybe_url](const absl::optional<GURL>& url) {
              maybe_url = url;
              run_loop.Quit();
            }));
    run_loop.Run();
    if (maybe_url) {
      absl::optional<GURL> decoded_URL = ConvertFencedFrameURNToURL(*maybe_url);
      NavigateIframeAndCheckURL(web_contents(), *maybe_url,
                                decoded_URL.value_or(GURL()));
      return decoded_URL;
    }
    return absl::nullopt;
  }

  url::Origin test_origin_a_;
  GURL ad_url_;
};

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       BasicSuccess) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Optional(Eq(ad_url_)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       SellerNotHttps) {
  GURL test_url_b = https_server_->GetURL("a.test", "/echo");
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url = embedded_test_server()->GetURL(
      "b.test", "/interest_group/decision_logic.js");
  ASSERT_TRUE(config->decision_logic_url.SchemeIs(url::kHttpScheme));
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       WrongDecisionUrlOrigin) {
  // The `decision_logic_url` origin doesn't match `seller`s, which is invalid.
  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_a_;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       InterestGroupBuyerOriginNotHttps) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Same hostname as `test_url_a_`, different scheme. This buyer is not valid
  // because it is not https, so the auction fails.
  GURL test_url_a_http = embedded_test_server()->GetURL("a.test", "/echo");
  ASSERT_TRUE(test_url_a_http.SchemeIs(url::kHttpScheme));
  url::Origin test_origin_a_http = url::Origin::Create(test_url_a_http);

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_http});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       InterestGroupBuyerOriginNotHttpsMultipleBuyers) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Same hostname as `test_url_a_`, different scheme. This buyer is not valid
  // because it is not https, so the auction fails, even though the other buyer
  // is valid.
  GURL test_url_a_http = embedded_test_server()->GetURL("a.test", "/echo");
  ASSERT_TRUE(test_url_a_http.SchemeIs(url::kHttpScheme));
  url::Origin test_origin_a_http = url::Origin::Create(test_url_a_http);

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers(
      {test_origin_a_, test_origin_a_http});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       BuyerWithNoRegisteredInterestGroupsIgnored) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // New valid origin, not associated with any registered interest group. Its
  // presence in the auctions `interest_group_buyers` shouldn't affect the
  // auction outcome.
  GURL test_url_c = https_server_->GetURL("c.test", "/echo");
  ASSERT_TRUE(test_url_c.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_c = url::Origin::Create(test_url_c);

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_, test_origin_c});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Optional(Eq(ad_url_)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       InterestGroupWildcardStarNotSupported) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_all_buyers(blink::mojom::AllBuyers::New());

  // All buyers isn't supported.
  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       PerBuyerSignalsValid) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Per-buyer signals are valid because `test_origin_a_` is in the set of
  // buyers, so the auction succeeds.
  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});
  config->per_buyer_signals.emplace();
  config->per_buyer_signals.value()[test_origin_a_] =
      "{\"even\": \"more\", \"x\": 4.5}";

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Optional(Eq(ad_url_)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       PerBuyerSignalsNotSubsetOfBuyers) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  // Per-buyer signals are invalid because `test_origin_a_` is not in the set of
  // buyers, so the auction fails.
  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});
  config->per_buyer_signals.emplace();
  // `test_origin_b` isn't in `interest_group_buyers`.
  config->per_buyer_signals.value()[test_origin_b] =
      "{\"even\": \"more\", \"x\": 4.5}";

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
}

IN_PROC_BROWSER_TEST_F(InterestGroupBrowserTestRunAdAuctionBypassBlink,
                       TrustedScoringSignalsUrlWrongOrigin) {
  GURL test_url_b = https_server_->GetURL("b.test", "/page_with_iframe.html");
  ASSERT_TRUE(test_url_b.SchemeIs(url::kHttpsScheme));
  url::Origin test_origin_b = url::Origin::Create(test_url_b);
  ASSERT_TRUE(NavigateToURL(shell(), test_url_b));

  auto config = blink::mojom::AuctionAdConfig::New();
  config->seller = test_origin_b;
  config->decision_logic_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");
  config->trusted_scoring_signals_url = https_server_->GetURL(
      "not-b.test", "/interest_group/trusted_scoring_signals.json");
  config->interest_group_buyers = blink::mojom::InterestGroupBuyers::New();
  config->interest_group_buyers->set_buyers({test_origin_a_});

  EXPECT_THAT(RunAuctionBypassBlink(std::move(config)), Eq(absl::nullopt));
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
  // Learn the bidder IG, served from the local server.
  GURL bidder_url =
      https_server_->GetURL("b.test", "/interest_group/bidding_logic.js");
  ASSERT_TRUE(NavigateToURL(shell(), https_server_->GetURL("b.test", "/echo")));
  url::Origin bidder_origin = url::Origin::Create(bidder_url);
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/bidder_origin,
      /*name=*/"Cthulhu", bidder_url,
      /*ads=*/
      {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}}));
  URLLoaderMonitor url_loader_monitor;

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
  EXPECT_THAT(
      bidder_status.cors_error_status,
      Optional(network::CorsErrorStatus(
          network::mojom::CorsError::kPreflightMissingAllowOriginHeader)));
}

IN_PROC_BROWSER_TEST_F(InterestGroupPrivateNetworkBrowserTest,
                       SellerOnPrivateNetwork) {
  GURL seller_url =
      https_server_->GetURL("b.test", "/interest_group/decision_logic.js");

  // Use `remote_test_server_` for all URLs except the seller worklet.
  GURL test_url = remote_test_server_.GetURL("a.test", "/echo");
  ASSERT_TRUE(NavigateToURL(shell(), test_url));
  url::Origin test_origin = url::Origin::Create(test_url);

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/test_origin,
      /*name=*/"Cthulhu",
      /*bidding_url=*/
      remote_test_server_.GetURL("a.test", "/interest_group/bidding_logic.js"),
      /*ads=*/
      {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}}));

  URLLoaderMonitor url_loader_monitor;
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
  EXPECT_THAT(
      seller_status.cors_error_status,
      Optional(network::CorsErrorStatus(
          network::mojom::CorsError::kPreflightMissingAllowOriginHeader)));
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
  URLLoaderMonitor url_loader_monitor;

  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      /*owner=*/test_origin,
      /*name=*/bidder_report_to_url.spec(),
      /*bidding_url=*/
      remote_test_server_.GetURL(
          "a.test", "/interest_group/bidding_logic_report_to_name.js"),
      /*ads=*/
      {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}}));

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

  const network::URLLoaderCompletionStatus& bidder_report_status =
      url_loader_monitor.WaitForRequestCompletion(bidder_report_to_url);
  EXPECT_EQ(net::ERR_FAILED, bidder_report_status.error_code);
  EXPECT_THAT(
      bidder_report_status.cors_error_status,
      Optional(network::CorsErrorStatus(
          network::mojom::CorsError::kPreflightMissingAllowOriginHeader)));

  const network::URLLoaderCompletionStatus& seller_report_status =
      url_loader_monitor.WaitForRequestCompletion(seller_report_to_url);
  EXPECT_EQ(net::ERR_FAILED, seller_report_status.error_code);
  EXPECT_THAT(
      seller_report_status.cors_error_status,
      Optional(network::CorsErrorStatus(
          network::mojom::CorsError::kPreflightMissingAllowOriginHeader)));
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

  // While reports should should be made to these URLs in this test, their
  // results don't matter, so there's no need for a test server respond to for
  // these URLs with anything other than errors.
  GURL bidder_report_to_url =
      remote_test_server_.GetURL("a.test", "/bidder_report");
  GURL seller_report_to_url =
      remote_test_server_.GetURL("a.test", "/seller_report");
  URLLoaderMonitor url_loader_monitor;

  ASSERT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
      /*expiry=*/base::Time(),
      /*owner=*/test_origin,
      /*name=*/bidder_report_to_url.spec(), bidder_url,
      /*update_url=*/absl::nullopt, trusted_bidding_signals_url,
      /*trusted_bidding_signals_keys=*/{{"key1"}},
      /*user_bidding_signals=*/absl::nullopt,
      /*ads=*/
      {{{ad_url, /*metadata=*/absl::nullopt}}},
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
            url_loader_monitor.GetRequestInfo(bidder_report_to_url)
                ->trusted_params->client_security_state->ip_address_space);
  EXPECT_EQ(network::mojom::IPAddressSpace::kPublic,
            url_loader_monitor.GetRequestInfo(seller_report_to_url)
                ->trusted_params->client_security_state->ip_address_space);

  // Check that both reports reached the server.
  WaitForURL(bidder_report_to_url);
  WaitForURL(seller_report_to_url);
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

    ASSERT_TRUE(JoinInterestGroupAndWaitInJs(blink::InterestGroup(
        /*expiry=*/base::Time(),
        /*owner=*/url::Origin::Create(test_url), group_name,
        initial_bidding_url, update_url,
        /*trusted_bidding_signals_url=*/absl::nullopt,
        /*trusted_bidding_signals_keys=*/absl::nullopt,
        /*user_bidding_signals=*/absl::nullopt,
        /*ads=*/
        {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}},
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
              const blink::InterestGroup& group =
                  storage_group.bidding_group->group;
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

    EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
        blink::InterestGroup(
            /*expiry=*/base::Time(),
            /*owner=*/origin,
            /*name=*/"cars",
            /*bidding_url=*/
            https_server_->GetURL(host, "/interest_group/bidding_logic.js"),
            /*update_url=*/
            https_server_->GetURL(host,
                                  "/interest_group/daily_update_partial.json"),
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
    EXPECT_TRUE(LeaveInterestGroupInJS(origin, "cars", execution_target));

    // It seems discard_duplicates of AddConsoleMessage works differently on
    // Android and other platforms. On Android, a message will be discarded if
    // it's not unique across all frames in a page. On other platforms, a
    // message will be discarded if it's not unique per origin (e.g., iframe
    // a.test and iframe b.test can have the same message, while the same
    // message from another a.test will be discarded).

#if defined(OS_ANDROID)
    RenderFrameHost* execution_targets_with_message[] = {cross_origin_iframe};
#else
    RenderFrameHost* execution_targets_with_message[] = {
        cross_origin_iframe, inner_cross_origin_iframe,
        same_origin_iframe_in_cross_origin_iframe};
#endif  // defined(OS_ANDROID)

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

    EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
        blink::InterestGroup(
            /*expiry=*/base::Time(),
            /*owner=*/origin,
            /*name=*/"cars",
            /*bidding_url=*/
            https_server_->GetURL(host, "/interest_group/bidding_logic.js"),
            /*update_url=*/
            https_server_->GetURL(host,
                                  "/interest_group/daily_update_partial.json"),
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
    EXPECT_TRUE(LeaveInterestGroupInJS(origin, "cars", execution_target));
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
  EXPECT_TRUE(JoinInterestGroupAndWaitInJs(
      blink::InterestGroup(
          /*expiry=*/base::Time(),
          /*owner=*/other_origin,
          /*name=*/"cars",
          /*bidding_url=*/
          https_server_->GetURL("b.test", "/interest_group/bidding_logic.js"),
          /*update_url=*/
          https_server_->GetURL("b.test",
                                "/interest_group/daily_update_partial.json"),
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/absl::nullopt,
          /*user_bidding_signals=*/absl::nullopt,
          /*ads=*/
          {{{GURL("https://example.com/render"), /*metadata=*/absl::nullopt}}},
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

  EXPECT_TRUE(
      LeaveInterestGroupInJS(other_origin, "cars", iframe_interest_group));
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

}  // namespace

}  // namespace content
