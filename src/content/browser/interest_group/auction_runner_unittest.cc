// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/public/test/test_renderer_host.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"

using auction_worklet::TestDevToolsAgentClient;

namespace content {
namespace {

const std::string kBidder1Name{"Ad Platform"};
const char kBidder1DebugLossReportUrl[] =
    "https://bidder1-debug-loss-reporting.com/";
const char kBidder1DebugWinReportUrl[] =
    "https://bidder1-debug-win-reporting.com/";
const char kBidder2DebugLossReportUrl[] =
    "https://bidder2-debug-loss-reporting.com/";
const char kBidder2DebugWinReportUrl[] =
    "https://bidder2-debug-win-reporting.com/";

// 0 `num_component_urls` means no component URLs, as opposed to an empty list
// (which isn't tested at this layer).
std::string MakeBidScript(const url::Origin& seller,
                          const std::string& bid,
                          const std::string& render_url,
                          int num_ad_components,
                          const url::Origin& interest_group_owner,
                          const std::string& interest_group_name,
                          bool has_signals,
                          const std::string& signal_key,
                          const std::string& signal_val,
                          const std::string& debug_loss_report_url = "",
                          const std::string& debug_win_report_url = "") {
  // TODO(morlovich): Use JsReplace.
  constexpr char kBidScript[] = R"(
    const seller = "%s";
    const bid = %s;
    const renderUrl = "%s";
    const numAdComponents = %i;
    const interestGroupOwner = "%s";
    const interestGroupName = "%s";
    const hasSignals = %s;
    const debugLossReportUrl = "%s";
    const debugWinReportUrl = "%s";

    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      var result = {ad: {"bidKey": "data for " + bid,
                         "groupName": interestGroupName,
                         "renderUrl": "data for " + renderUrl},
                    bid: bid, render: renderUrl};
      if (interestGroup.adComponents) {
        result.adComponents = [interestGroup.adComponents[0].renderUrl];
        result.ad.adComponentsUrl = interestGroup.adComponents[0].renderUrl;
      }

      if (interestGroup.name !== interestGroupName)
        throw new Error("wrong interestGroupName");
      if (interestGroup.owner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      if (interestGroup.ads.length != 1)
        throw new Error("wrong interestGroup.ads length");
      if (interestGroup.ads[0].renderUrl != renderUrl)
        throw new Error("wrong interestGroup.ads URL");
      if (numAdComponents == 0) {
        if (interestGroup.adComponents !== undefined)
          throw new Error("Non-empty adComponents");
      } else {
        if (interestGroup.adComponents.length !== numAdComponents)
          throw new Error("Wrong adComponents length");
        for (let i = 0; i < numAdComponents; ++i) {
          if (interestGroup.adComponents[i].renderUrl !=
              renderUrl.slice(0, -1) + "-component" + (i+1) + ".com/") {
            throw new Error("Wrong adComponents renderUrl");
          }
        }
      }
      // Skip the `perBuyerSignals` check if the interest group name matches
      // the bid. This is for auctions that use more than the two standard
      // bidders, since there's currently no way to inject new perBuyerSignals
      // into the top-level auction.
      // TODO(mmenke): Worth fixing that?
      if (interestGroupName !== bid + '') {
        if (perBuyerSignals[seller + 'Signals'] !==
            interestGroupName + 'Signals') {
          throw new Error("wrong perBuyerSignals");
        }
      }
      if (auctionSignals !== "auctionSignalsFor " + seller)
        throw new Error("wrong auctionSignals");
      if (hasSignals) {
        if ('extra' in trustedBiddingSignals)
          throw new Error("why extra?");
        if (trustedBiddingSignals["%s"] !== "%s")
          throw new Error("wrong signals");
      } else if (trustedBiddingSignals !== null) {
        throw new Error("Expected null trustedBiddingSignals");
      }
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");
      if (browserSignals.seller != seller)
         throw new Error("wrong seller");
      if (browserSignals.joinCount !== 3)
        throw new Error("joinCount")
      if (browserSignals.bidCount !== 5)
        throw new Error("bidCount");
      if (browserSignals.prevWins.length !== 3)
        throw new Error("prevWins");
      for (let i = 0; i < browserSignals.prevWins.length; ++i) {
        if (!(browserSignals.prevWins[i] instanceof Array))
          throw new Error("prevWins entry not an array");
        if (typeof browserSignals.prevWins[i][0] != "number")
          throw new Error("Not a Number in prevWin?");
        if (browserSignals.prevWins[i][1].winner !== -i)
          throw new Error("prevWin MD not what passed in");
      }
      if (debugLossReportUrl)
        forDebuggingOnly.reportAdAuctionLoss(debugLossReportUrl);
      if (debugWinReportUrl)
        forDebuggingOnly.reportAdAuctionWin(debugWinReportUrl);
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      return result;
    }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      if (auctionSignals !== "auctionSignalsFor " + seller)
        throw new Error("wrong auctionSignals");
      // Skip the `perBuyerSignals` check if the interest group name matches
      // the bid. This is for auctions that use more than the two standard
      // bidders, since there's currently no way to inject new perBuyerSignals
      // into the top-level auction.
      // TODO(mmenke): Worth fixing that?
      if (interestGroupName !== bid + '') {
        if (perBuyerSignals[seller + 'Signals'] !==
            interestGroupName + 'Signals') {
          throw new Error("wrong perBuyerSignals");
        }
      }

      // sellerSignals in these tests is just sellers' browserSignals, since
      // that's what reportResult passes through.
      if (sellerSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");
      if (sellerSignals.interestGroupOwner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      if (sellerSignals.renderUrl !== renderUrl)
        throw new Error("wrong renderUrl");
      if (sellerSignals.bid !== bid)
        throw new Error("wrong bid");

      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong browserSignals.topWindowHostname");
      if ("desirability" in browserSignals)
        throw new Error("why is desirability here?");
      if (browserSignals.interestGroupName !== interestGroupName)
        throw new Error("wrong browserSignals.interestGroupName");
      if (browserSignals.interestGroupOwner !== interestGroupOwner)
        throw new Error("wrong browserSignals.interestGroupOwner");

      if (browserSignals.renderUrl !== renderUrl)
        throw new Error("wrong browserSignals.renderUrl");
      if (browserSignals.bid !== bid)
        throw new Error("wrong browserSignals.bid");
      if (browserSignals.seller != seller)
         throw new Error("wrong seller");
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);

      sendReportTo("https://buyer-reporting.example.com/" + bid);
    }
  )";
  return base::StringPrintf(
      kBidScript, seller.Serialize().c_str(), bid.c_str(), render_url.c_str(),
      num_ad_components, interest_group_owner.Serialize().c_str(),
      interest_group_name.c_str(), has_signals ? "true" : "false",
      debug_loss_report_url.c_str(), debug_win_report_url.c_str(),
      signal_key.c_str(), signal_val.c_str());
}

// This can be appended to the standard script to override the function.
constexpr char kReportWinNoUrl[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
  }
)";

// This can be appended to the standard script to override the function.
constexpr char kReportWinExpectNullAuctionSignals[] = R"(
  function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
    if (sellerSignals === null)
      sendReportTo("https://seller.signals.were.null.test");
  }
)";

std::string MakeDecisionScript(
    const GURL& decision_logic_url,
    absl::optional<GURL> send_report_url = absl::nullopt,
    const std::string& debug_loss_report_url = "",
    const std::string& debug_win_report_url = "") {
  constexpr char kCheckingAuctionScript[] = R"(
    const decisionLogicUrl = "%s";
    const sendReportUrl = "%s";
    const debugLossReportUrl = "%s";
    const debugWinReportUrl = "%s";
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                     browserSignals) {
      if (adMetadata.bidKey !== ("data for " + bid)) {
        throw new Error("wrong data for bid:" +
                        JSON.stringify(adMetadata) + "/" + bid);
      }
      if (adMetadata.renderUrl !== ("data for " + browserSignals.renderUrl)) {
        throw new Error("wrong data for renderUrl:" +
                        JSON.stringify(adMetadata) + "/" +
                        browserSignals.renderUrl);
      }
      let components = browserSignals.adComponents;
      if (adMetadata.adComponentsUrl) {
        if (components.length !== 1 ||
            components[0] !== adMetadata.adComponentsUrl) {
          throw new Error("wrong data for adComponents:" +
                          JSON.stringify(adMetadata) + "/" +
                          browserSignals.adComponents);
        }
      } else if (components !== undefined) {
        throw new Error("wrong data for adComponents:" +
                        JSON.stringify(adMetadata) + "/" +
                        browserSignals.adComponents);
      }
      if (auctionConfig.decisionLogicUrl !== decisionLogicUrl)
        throw new Error("wrong decisionLogicUrl in auctionConfig");
      // Check `perBuyerSignals` for the first bidder.
      let signals1 = auctionConfig.perBuyerSignals['https://adplatform.com'];
      if (signals1[auctionConfig.seller + 'Signals'] !== 'Ad PlatformSignals')
        throw new Error("Wrong perBuyerSignals in auctionConfig");
      if (typeof auctionConfig.perBuyerTimeouts['https://adplatform.com'] !==
          "number") {
        throw new Error("timeout in auctionConfig.perBuyerTimeouts is not a " +
                        "number. huh");
      }
      if (typeof auctionConfig.perBuyerTimeouts['*'] !== "number") {
        throw new Error("timeout in auctionConfig.perBuyerTimeouts is not a " +
                        "number. huh");
      }
      if (auctionConfig.sellerSignals["url"] != decisionLogicUrl)
        throw new Error("Wrong sellerSignals");
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname");
      if ("joinCount" in browserSignals)
        throw new Error("wrong kind of browser signals");
      if (typeof browserSignals.biddingDurationMsec !== "number")
        throw new Error("biddingDurationMsec is not a number. huh");
      if (browserSignals.biddingDurationMsec < 0)
        throw new Error("biddingDurationMsec should be non-negative.");
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      if (debugLossReportUrl)
        forDebuggingOnly.reportAdAuctionLoss(debugLossReportUrl + bid);
      if (debugWinReportUrl)
        forDebuggingOnly.reportAdAuctionWin(debugWinReportUrl + bid);

      return computeScore(bid);
    }

    function reportResult(auctionConfig, browserSignals) {
      // Check `perBuyerSignals` for the first bidder.
      let signals1 = auctionConfig.perBuyerSignals['https://adplatform.com'];
      if (signals1[auctionConfig.seller + 'Signals'] !== 'Ad PlatformSignals')
        throw new Error("Wrong perBuyerSignals in auctionConfig");
      if (auctionConfig.decisionLogicUrl !== decisionLogicUrl)
        throw new Error("wrong decisionLogicUrl in auctionConfig");
      if (browserSignals.topWindowHostname !== 'publisher1.com')
        throw new Error("wrong topWindowHostname in browserSignals");
      if (browserSignals.desirability != computeScore(browserSignals.bid))
        throw new Error("wrong bid or desirability in browserSignals");
      if (browserSignals.dataVersion !== undefined)
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      if (sendReportUrl)
        sendReportTo(sendReportUrl + browserSignals.bid);
      return browserSignals;
    }

    // Use different scoring functions for the top-level seller and component
    // sellers, so can verify that each ReportResult() method gets the score
    // from the correct seller, and so that the the wrong bidder will win
    // in some tests if either component auction scores are used for the
    // top-level auction, or if all bidders from component auctions are passed
    // to the top-level auction.
    function computeScore(bid) {
      if (decisionLogicUrl == "https://adstuff.publisher1.com/auction.js")
        return 2 * bid;
      return 100 - bid;
    }
  )";

  return base::StringPrintf(
      kCheckingAuctionScript, decision_logic_url.spec().c_str(),
      send_report_url ? send_report_url->spec().c_str() : "",
      debug_loss_report_url.c_str(), debug_win_report_url.c_str());
}

std::string MakeAuctionScript(const GURL& decision_logic_url = GURL(
                                  "https://adstuff.publisher1.com/auction.js"),
                              const std::string& debug_loss_report_url = "",
                              const std::string& debug_win_report_url = "") {
  return MakeDecisionScript(
      decision_logic_url,
      /*send_report_url=*/GURL("https://reporting.example.com"),
      debug_loss_report_url, debug_win_report_url);
}

std::string MakeAuctionScriptNoReportUrl(
    const GURL& decision_logic_url =
        GURL("https://adstuff.publisher1.com/auction.js"),
    const std::string& debug_loss_report_url = "",
    const std::string& debug_win_report_url = "") {
  return MakeDecisionScript(decision_logic_url,
                            /*send_report_url=*/absl::nullopt,
                            debug_loss_report_url, debug_win_report_url);
}

const char kAuctionScriptRejects2[] = R"(
  function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
    if (bid === 2)
      return -1;
    return bid + 1;
  }
)";

const char kBasicReportResult[] = R"(
  function reportResult(auctionConfig, browserSignals) {
    sendReportTo("https://reporting.example.com/" + browserSignals.bid);
    return browserSignals;
  }
)";

std::string MakeAuctionScriptReject2() {
  return std::string(kAuctionScriptRejects2) + kBasicReportResult;
}

std::string MakeAuctionScriptReject1And2WithDebugReporting(
    const std::string& debug_loss_report_url = "",
    const std::string& debug_win_report_url = "") {
  constexpr char kReject1And2WithDebugReporting[] = R"(
    const debugLossReportUrl = "%s";
    const debugWinReportUrl = "%s";
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      let result = bid + 1;
      if (bid === 1 || bid === 2)
        result = -1;
      if (debugLossReportUrl)
        forDebuggingOnly.reportAdAuctionLoss(debugLossReportUrl + bid);
      if (debugWinReportUrl)
        forDebuggingOnly.reportAdAuctionWin(debugWinReportUrl + bid);
      return result;
    }
  )";
  return base::StringPrintf(kReject1And2WithDebugReporting,
                            debug_loss_report_url.c_str(),
                            debug_win_report_url.c_str()) +
         kBasicReportResult;
}

// Sorts a vector of PreviousWinPtr so that the most recent wins are last.
void SortPrevWins(
    std::vector<auction_worklet::mojom::PreviousWinPtr>& prev_wins) {
  std::sort(prev_wins.begin(), prev_wins.end(),
            [](const auction_worklet::mojom::PreviousWinPtr& prev_win1,
               const auction_worklet::mojom::PreviousWinPtr& prev_win2) {
              return prev_win1->time < prev_win2->time;
            });
}

// BidderWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockBidderWorklet : public auction_worklet::mojom::BidderWorklet {
 public:
  explicit MockBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockBidderWorklet::OnPipeClosed, base::Unretained(this)));
  }

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override {
    // `send_pending_signals_requests_called_` should always be called if any
    // bids are generated, except in the unlikely event that the Mojo pipe is
    // closed before a posted task is executed (this cannot be simulated by
    // closing a pipe in tests, due to vagaries of timing of the two messages).
    if (generate_bid_called_)
      EXPECT_TRUE(send_pending_signals_requests_called_);
  }

  // auction_worklet::mojom::BidderWorklet implementation:

  void GenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParamsPtr
          bidder_worklet_non_shared_params,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const absl::optional<base::TimeDelta> per_buyer_timeout,
      const url::Origin& seller_origin,
      auction_worklet::mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
      base::Time auction_start_time,
      GenerateBidCallback generate_bid_callback) override {
    generate_bid_called_ = true;
    // While the real BidderWorklet implementation supports multiple pending
    // callbacks, this class does not.
    DCHECK(!generate_bid_callback_);

    // per_buyer_timeout passed to GenerateBid() should not be empty, because
    // auction_config's all_buyers_timeout (which is the key of '*' in
    // perBuyerTimeouts) is set in the AuctionRunnerTest.
    EXPECT_TRUE(per_buyer_timeout.has_value());
    if (bidder_worklet_non_shared_params->name == kBidder1Name) {
      // Any per buyer timeout in auction_config higher than 500 ms should be
      // clamped to 500 ms by the AuctionRunner before passed to GenerateBid(),
      // and kBidder1's per buyer timeout is 1000 ms in auction_config so it
      // should be 500 ms here.
      EXPECT_EQ(per_buyer_timeout.value(), base::Milliseconds(500));
    } else {
      // Any other bidder's per buyer timeout should be 150 ms, since
      // auction_config's all_buyers_timeout is set to 150 ms in the
      // AuctionRunnerTest.
      EXPECT_EQ(per_buyer_timeout.value(), base::Milliseconds(150));
    }

    // Single auctions should invoke all GenerateBid() calls on a worklet
    // before invoking SendPendingSignalsRequests().
    EXPECT_FALSE(send_pending_signals_requests_called_);

    generate_bid_callback_ = std::move(generate_bid_callback);
    if (generate_bid_run_loop_)
      generate_bid_run_loop_->Quit();
  }

  void SendPendingSignalsRequests() override {
    // This allows multiple calls.
    send_pending_signals_requests_called_ = true;
  }

  void ReportWin(const std::string& interest_group_name,
                 const absl::optional<std::string>& auction_signals_json,
                 const absl::optional<std::string>& per_buyer_signals_json,
                 const std::string& seller_signals_json,
                 const GURL& browser_signal_render_url,
                 double browser_signal_bid,
                 const url::Origin& browser_signal_seller_origin,
                 uint32_t bidding_signals_data_version,
                 bool has_bidding_signals_data_version,
                 ReportWinCallback report_win_callback) override {
    // While the real BidderWorklet implementation supports multiple pending
    // callbacks, this class does not.
    DCHECK(!report_win_callback_);
    report_win_callback_ = std::move(report_win_callback);
    if (report_win_run_loop_)
      report_win_run_loop_->Quit();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockBidderWorklet";
  }

  void WaitForGenerateBid() {
    if (!generate_bid_callback_) {
      generate_bid_run_loop_ = std::make_unique<base::RunLoop>();
      generate_bid_run_loop_->Run();
      generate_bid_run_loop_.reset();
      DCHECK(generate_bid_callback_);
    }
  }

  // Invokes the GenerateBid callback. A bid of base::nullopt means no bid
  // should be offered. Waits for the GenerateBid() call first, if needed.
  void InvokeGenerateBidCallback(
      absl::optional<double> bid,
      const GURL& render_url = GURL(),
      absl::optional<std::vector<GURL>> ad_component_urls = absl::nullopt,
      base::TimeDelta duration = base::TimeDelta(),
      const absl::optional<uint32_t>& bidding_signals_data_version =
          absl::nullopt,
      const absl::optional<GURL>& debug_loss_report_url = absl::nullopt,
      const absl::optional<GURL>& debug_win_report_url = absl::nullopt) {
    WaitForGenerateBid();

    if (!bid.has_value()) {
      std::move(generate_bid_callback_)
          .Run(/*bid=*/nullptr,
               /*bidding_signals_data_version=*/0,
               /*has_bidding_signals_data_version=*/false,
               debug_loss_report_url,
               /*debug_win_report_url=*/absl::nullopt,
               /*errors=*/std::vector<std::string>());
      return;
    }

    std::move(generate_bid_callback_)
        .Run(auction_worklet::mojom::BidderWorkletBid::New(
                 "ad", *bid, render_url, ad_component_urls, duration),
             bidding_signals_data_version.value_or(0),
             bidding_signals_data_version.has_value(), debug_loss_report_url,
             debug_win_report_url,
             /*errors=*/std::vector<std::string>());
  }

  void WaitForReportWin() {
    DCHECK(!generate_bid_callback_);
    DCHECK(!report_win_run_loop_);
    if (!report_win_callback_) {
      report_win_run_loop_ = std::make_unique<base::RunLoop>();
      report_win_run_loop_->Run();
      report_win_run_loop_.reset();
      DCHECK(report_win_callback_);
    }
  }

  void InvokeReportWinCallback(
      absl::optional<GURL> report_url = absl::nullopt) {
    DCHECK(report_win_callback_);
    std::move(report_win_callback_)
        .Run(report_url, /*errors=*/std::vector<std::string>());
  }

  // Flush the receiver pipe and return whether or not its closed.
  bool PipeIsClosed() {
    receiver_.FlushForTesting();
    return pipe_closed_;
  }

 private:
  void OnPipeClosed() { pipe_closed_ = true; }

  BidderWorklet::GenerateBidCallback generate_bid_callback_;

  bool pipe_closed_ = false;

  std::unique_ptr<base::RunLoop> generate_bid_run_loop_;
  std::unique_ptr<base::RunLoop> report_win_run_loop_;
  ReportWinCallback report_win_callback_;

  bool generate_bid_called_ = false;
  bool send_pending_signals_requests_called_ = false;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::BidderWorklet> receiver_;
};

// SellerWorklet that holds onto passed in callbacks, to let the test fixture
// invoke them.
class MockSellerWorklet : public auction_worklet::mojom::SellerWorklet {
 public:
  // Subset of parameters passed to SellerWorklet's ScoreAd method.
  struct ScoreAdParams {
    ScoreAdCallback callback;
    double bid;
    url::Origin interest_group_owner;
  };

  explicit MockSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          pending_receiver)
      : receiver_(this, std::move(pending_receiver)) {}

  MockSellerWorklet(const MockSellerWorklet&) = delete;
  const MockSellerWorklet& operator=(const MockSellerWorklet&) = delete;

  ~MockSellerWorklet() override {
    EXPECT_EQ(expect_send_pending_signals_requests_called_,
              send_pending_signals_requests_called_);

    // Every received ScoreAd() call should have been waited for.
    EXPECT_TRUE(score_ad_params_.empty());
  }

  // auction_worklet::mojom::SellerWorklet implementation:

  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               blink::mojom::AuctionAdConfigNonSharedParamsPtr
                   auction_ad_config_non_shared_params,
               const url::Origin& browser_signal_interest_group_owner,
               const GURL& browser_signal_render_url,
               const std::vector<GURL>& browser_signal_ad_components,
               uint32_t browser_signal_bidding_duration_msecs,
               ScoreAdCallback score_ad_callback) override {
    // SendPendingSignalsRequests() should only be called once all ads are
    // scored.
    EXPECT_FALSE(send_pending_signals_requests_called_);

    ScoreAdParams score_ad_params;
    score_ad_params.callback = std::move(score_ad_callback);
    score_ad_params.bid = bid;
    score_ad_params.interest_group_owner = browser_signal_interest_group_owner;
    score_ad_params_.emplace_front(std::move(score_ad_params));
    if (score_ad_run_loop_)
      score_ad_run_loop_->Quit();
  }

  void SendPendingSignalsRequests() override {
    // SendPendingSignalsRequests() should only be called once by a single
    // AuctionRunner.
    EXPECT_FALSE(send_pending_signals_requests_called_);

    send_pending_signals_requests_called_ = true;
  }

  void ReportResult(blink::mojom::AuctionAdConfigNonSharedParamsPtr
                        auction_ad_config_non_shared_params,
                    const url::Origin& browser_signal_interest_group_owner,
                    const GURL& browser_signal_render_url,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    uint32_t browser_signal_data_version,
                    bool browser_signal_has_data_version,
                    ReportResultCallback report_result_callback) override {
    report_result_callback_ = std::move(report_result_callback);
    if (report_result_run_loop_)
      report_result_run_loop_->Quit();
  }

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent)
      override {
    ADD_FAILURE()
        << "ConnectDevToolsAgent should not be called on MockSellerWorklet";
  }

  void ResetReceiverWithReason(const std::string& reason) {
    receiver_.ResetWithReason(/*custom_reason_code=*/0, reason);
  }

  // Waits until ScoreAd() has been invoked, if it hasn't been already. It's up
  // to the caller to invoke the returned ScoreAdParams::callback to continue
  // the auction.
  ScoreAdParams WaitForScoreAd() {
    DCHECK(!score_ad_run_loop_);
    if (score_ad_params_.empty()) {
      score_ad_run_loop_ = std::make_unique<base::RunLoop>();
      score_ad_run_loop_->Run();
      score_ad_run_loop_.reset();
      DCHECK(!score_ad_params_.empty());
    }
    ScoreAdParams out = std::move(score_ad_params_.front());
    score_ad_params_.pop_front();
    return out;
  }

  void WaitForReportResult() {
    DCHECK(!report_result_run_loop_);
    if (!report_result_callback_) {
      report_result_run_loop_ = std::make_unique<base::RunLoop>();
      report_result_run_loop_->Run();
      report_result_run_loop_.reset();
      DCHECK(report_result_callback_);
    }
  }

  // Invokes the ReportResultCallback for the most recent ScoreAd() call with
  // the provided score. WaitForReportResult() must have been invoked first.
  void InvokeReportResultCallback(
      absl::optional<GURL> report_url = absl::nullopt,
      std::vector<std::string> errors = {}) {
    DCHECK(report_result_callback_);
    std::move(report_result_callback_)
        .Run(/*signals_for_winner=*/absl::nullopt, std::move(report_url),
             errors);
  }

  void Flush() { receiver_.FlushForTesting(); }

  // `expect_send_pending_signals_requests_called_` needs to be set to false in
  // the case a SellerWorklet is destroyed before it receives a request to score
  // the final bid.
  void set_expect_send_pending_signals_requests_called(bool value) {
    expect_send_pending_signals_requests_called_ = value;
  }

 private:
  std::unique_ptr<base::RunLoop> score_ad_run_loop_;
  std::list<ScoreAdParams> score_ad_params_;

  std::unique_ptr<base::RunLoop> report_result_run_loop_;
  ReportResultCallback report_result_callback_;

  bool expect_send_pending_signals_requests_called_ = true;
  bool send_pending_signals_requests_called_ = false;

  // Receiver is last so that destroying `this` while there's a pending callback
  // over the pipe will not DCHECK.
  mojo::Receiver<auction_worklet::mojom::SellerWorklet> receiver_;
};

// AuctionWorkletService that creates MockBidderWorklets and MockSellerWorklets
// to hold onto passed in PendingReceivers and Callbacks.

// AuctionProcessManager and AuctionWorkletService - combining the two with a
// mojo::ReceiverSet makes it easier to track which call came over which
// receiver than using separate classes.
class MockAuctionProcessManager
    : public AuctionProcessManager,
      public auction_worklet::mojom::AuctionWorkletService {
 public:
  MockAuctionProcessManager() = default;
  ~MockAuctionProcessManager() override = default;

  // AuctionProcessManager implementation:
  void LaunchProcess(
      mojo::PendingReceiver<auction_worklet::mojom::AuctionWorkletService>
          auction_worklet_service_receiver,
      const std::string& display_name) override {
    mojo::ReceiverId receiver_id =
        receiver_set_.Add(this, std::move(auction_worklet_service_receiver));

    // Have to flush the receiver set, so that any closed receivers are removed,
    // before searching for duplicate process names.
    receiver_set_.FlushForTesting();

    // Each receiver should get a unique display name. This check serves to help
    // ensure that processes are correctly reused.
    EXPECT_EQ(0u, receiver_display_name_map_.count(receiver_id));
    for (auto receiver : receiver_display_name_map_) {
      // Ignore closed receivers. ReportWin() will result in re-loading a
      // worklet, after closing the original worklet, which may require
      // re-creating the AuctionWorkletService.
      if (receiver_set_.HasReceiver(receiver.first))
        EXPECT_NE(receiver.second, display_name);
    }

    receiver_display_name_map_[receiver_id] = display_name;
  }

  // auction_worklet::mojom::AuctionWorkletService implementation:
  void LoadBidderWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      bool pause_for_debugger_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& bidding_wasm_helper_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const url::Origin& top_window_origin) override {
    // Make sure this request came over the right pipe.
    url::Origin owner = url::Origin::Create(script_source_url);
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                                 url::Origin::Create(script_source_url)));

    EXPECT_EQ(0u, bidder_worklets_.count(script_source_url));
    bidder_worklets_.emplace(std::make_pair(
        script_source_url, std::make_unique<MockBidderWorklet>(
                               std::move(bidder_worklet_receiver))));
    // Whenever a worklet is created, one of the RunLoops should be waiting for
    // worklet creation.
    if (wait_for_bidder_reload_run_loop_) {
      wait_for_bidder_reload_run_loop_->Quit();
    } else {
      ASSERT_GT(waiting_for_num_bidders_, 0);
      --waiting_for_num_bidders_;
      MaybeQuitWaitForWorkletsRunLoop();
    }
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet_receiver,
      bool should_pause_on_start,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      const absl::optional<GURL>& trusted_scoring_signals_url,
      const url::Origin& top_window_origin) override {
    EXPECT_EQ(0u, seller_worklets_.count(script_source_url));

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kSeller,
                                 url::Origin::Create(script_source_url)));

    seller_worklets_.emplace(std::make_pair(
        script_source_url, std::make_unique<MockSellerWorklet>(
                               std::move(seller_worklet_receiver))));

    // Whenever a worklet is created, one of the RunLoops should be waiting for
    // worklet creation.
    if (wait_for_seller_reload_run_loop_) {
      wait_for_seller_reload_run_loop_->Quit();
    } else {
      EXPECT_GT(waiting_for_num_sellers_, 0);
      --waiting_for_num_sellers_;
      MaybeQuitWaitForWorkletsRunLoop();
    }
  }

  // Waits for `num_bidders` bidder worklets and `num_sellers` seller worklets
  // to be created.
  void WaitForWorklets(int num_bidders, int num_sellers = 1) {
    waiting_for_num_bidders_ = num_bidders;
    waiting_for_num_sellers_ = num_sellers;
    wait_for_worklets_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_worklets_run_loop_->Run();
    wait_for_worklets_run_loop_.reset();
  }

  // Waits for a single bidder script to be loaded. Intended to be used to wait
  // for the winning bidder script to be reloaded. WaitForWorklets() should be
  // used when waiting for worklets to be loaded at the start of an auction.
  void WaitForWinningBidderReload() {
    EXPECT_TRUE(bidder_worklets_.empty());
    wait_for_bidder_reload_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_bidder_reload_run_loop_->Run();
    wait_for_bidder_reload_run_loop_.reset();
    EXPECT_EQ(1u, bidder_worklets_.size());
  }

  void WaitForWinningSellerReload() {
    EXPECT_TRUE(seller_worklets_.empty());
    wait_for_seller_reload_run_loop_ = std::make_unique<base::RunLoop>();
    wait_for_seller_reload_run_loop_->Run();
    wait_for_seller_reload_run_loop_.reset();
    EXPECT_EQ(1u, seller_worklets_.size());
  }

  // Returns the MockBidderWorklet created for the specified script URL, if
  // there is one.
  std::unique_ptr<MockBidderWorklet> TakeBidderWorklet(
      const GURL& script_source_url) {
    auto it = bidder_worklets_.find(script_source_url);
    if (it == bidder_worklets_.end())
      return nullptr;
    std::unique_ptr<MockBidderWorklet> out = std::move(it->second);
    bidder_worklets_.erase(it);
    return out;
  }

  // Returns the MockSellerWorklet created for the specified script URL, if
  // there is one. If no URL is provided, and there's only one pending seller
  // worklet, returns that seller worklet.
  std::unique_ptr<MockSellerWorklet> TakeSellerWorklet(
      GURL script_source_url = GURL()) {
    if (seller_worklets_.empty())
      return nullptr;

    if (script_source_url.is_empty()) {
      CHECK_EQ(1u, seller_worklets_.size());
      script_source_url = seller_worklets_.begin()->first;
    }

    auto it = seller_worklets_.find(script_source_url);
    if (it == seller_worklets_.end())
      return nullptr;
    std::unique_ptr<MockSellerWorklet> out = std::move(it->second);
    seller_worklets_.erase(it);
    return out;
  }

  void Flush() { receiver_set_.FlushForTesting(); }

 private:
  void MaybeQuitWaitForWorkletsRunLoop() {
    DCHECK(wait_for_worklets_run_loop_);
    if (waiting_for_num_bidders_ == 0 && waiting_for_num_sellers_ == 0)
      wait_for_worklets_run_loop_->Quit();
  }

  // Maps of script URLs to worklets.
  std::map<GURL, std::unique_ptr<MockBidderWorklet>> bidder_worklets_;
  std::map<GURL, std::unique_ptr<MockSellerWorklet>> seller_worklets_;

  // Used to wait for the worklets to be loaded at the start of the auction.
  std::unique_ptr<base::RunLoop> wait_for_worklets_run_loop_;
  int waiting_for_num_bidders_ = 0;
  int waiting_for_num_sellers_ = 0;

  // Used to wait for a worklet to be reloaded at the end of an auction.
  std::unique_ptr<base::RunLoop> wait_for_bidder_reload_run_loop_;
  std::unique_ptr<base::RunLoop> wait_for_seller_reload_run_loop_;

  // Map from ReceiverSet IDs to display name when the process was launched.
  // Used to verify that worklets are created in the right process.
  std::map<mojo::ReceiverId, std::string> receiver_display_name_map_;

  // ReceiverSet is last so that destroying `this` while there's a pending
  // callback over the pipe will not DCHECK.
  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class SameProcessAuctionProcessManager : public AuctionProcessManager {
 public:
  SameProcessAuctionProcessManager() = default;
  SameProcessAuctionProcessManager(const SameProcessAuctionProcessManager&) =
      delete;
  SameProcessAuctionProcessManager& operator=(
      const SameProcessAuctionProcessManager&) = delete;
  ~SameProcessAuctionProcessManager() override = default;

  // Resume all worklets paused waiting for debugger on startup.
  void ResumeAllPaused() {
    for (const auto& svc : auction_worklet_services_) {
      svc->AuctionV8HelperForTesting()->v8_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](scoped_refptr<auction_worklet::AuctionV8Helper> v8_helper) {
                v8_helper->ResumeAllForTesting();
              },
              svc->AuctionV8HelperForTesting()));
    }
  }

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

class AuctionRunnerTest : public testing::Test,
                          public AuctionWorkletManager::Delegate,
                          public DebuggableAuctionWorkletTracker::Observer {
 protected:
  // Output of the RunAuctionCallback passed to AuctionRunner::CreateAndStart().
  struct Result {
    Result() = default;
    // Can't use default copy logic, since it contains Mojo types.
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    absl::optional<GURL> ad_url;
    absl::optional<std::vector<GURL>> ad_component_urls;
    std::vector<GURL> report_urls;
    std::vector<GURL> debug_loss_report_urls;
    std::vector<GURL> debug_win_report_urls;
    std::vector<std::string> errors;

    // Metadata about `bidder1` and `bidder2`, pulled from the
    // InterestGroupManager on auction complete. Used to make sure number of
    // bids and win list are updated on auction complete. Previous wins arrays
    // are guaranteed to be sorted in chronological order.
    int bidder1_bid_count;
    std::vector<auction_worklet::mojom::PreviousWinPtr> bidder1_prev_wins;
    int bidder2_bid_count;
    std::vector<auction_worklet::mojom::PreviousWinPtr> bidder2_prev_wins;
  };

  AuctionRunnerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &AuctionRunnerTest::OnBadMessage, base::Unretained(this)));
    DebuggableAuctionWorkletTracker::GetInstance()->AddObserver(this);
  }

  ~AuctionRunnerTest() override {
    DebuggableAuctionWorkletTracker::GetInstance()->RemoveObserver(this);

    // Any bad message should have been inspected and cleared before the end of
    // the test.
    EXPECT_EQ(std::string(), bad_message_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());

    // Give off-thread things a chance to delete.
    task_environment_.RunUntilIdle();
  }

  void OnBadMessage(const std::string& reason) {
    // No test expects multiple bad messages at a time
    EXPECT_EQ(std::string(), bad_message_);
    // Empty bad messages aren't expected. This check allows an empty
    // `bad_message_` field to mean no bad message, avoiding using an optional,
    // which has less helpful output on EXPECT failures.
    EXPECT_FALSE(reason.empty());

    bad_message_ = reason;
  }

  // Gets and clear most recent bad Mojo message.
  std::string TakeBadMessage() { return std::move(bad_message_); }

  // Helper to create an auction config with the specified values.
  blink::mojom::AuctionAdConfigPtr CreateAuctionConfig(
      const GURL& seller_decision_logic_url,
      absl::optional<std::vector<url::Origin>> buyers) {
    blink::mojom::AuctionAdConfigPtr auction_config =
        blink::mojom::AuctionAdConfig::New();
    auction_config->seller = url::Origin::Create(seller_decision_logic_url);
    auction_config->decision_logic_url = seller_decision_logic_url;

    auction_config->auction_ad_config_non_shared_params =
        blink::mojom::AuctionAdConfigNonSharedParams::New();
    auction_config->auction_ad_config_non_shared_params->interest_group_buyers =
        std::move(buyers);

    auction_config->auction_ad_config_non_shared_params->seller_signals =
        base::StringPrintf(R"({"url": "%s"})",
                           seller_decision_logic_url.spec().c_str());

    base::flat_map<url::Origin, std::string> per_buyer_signals;
    // Use a combination of bidder and seller values, so can make sure bidders
    // get the value from the correct seller script. Also append a fixed string,
    // as a defense against pulling the right values from the wrong places.
    per_buyer_signals[kBidder1] = base::StringPrintf(
        R"({"%sSignals": "%sSignals"})",
        auction_config->seller.Serialize().c_str(), kBidder1Name.c_str());
    per_buyer_signals[kBidder2] = base::StringPrintf(
        R"({"%sSignals": "%sSignals"})",
        auction_config->seller.Serialize().c_str(), kBidder2Name.c_str());
    auction_config->auction_ad_config_non_shared_params->per_buyer_signals =
        std::move(per_buyer_signals);

    base::flat_map<url::Origin, base::TimeDelta> per_buyer_timeouts;
    // Any per buyer timeout higher than 500 ms will be clamped to 500 ms by the
    // AuctionRunner.
    per_buyer_timeouts[kBidder1] = base::Milliseconds(1000);
    auction_config->auction_ad_config_non_shared_params->per_buyer_timeouts =
        std::move(per_buyer_timeouts);
    auction_config->auction_ad_config_non_shared_params->all_buyers_timeout =
        base::Milliseconds(150);

    auction_config->auction_ad_config_non_shared_params->auction_signals =
        base::StringPrintf(R"("auctionSignalsFor %s")",
                           auction_config->seller.Serialize().c_str());

    return auction_config;
  }

  // Starts an auction without waiting for it to complete. Useful when using
  // MockAuctionProcessManager.
  //
  // `bidders` are added to a new InterestGroupManager before running the
  // auction. The times of their previous wins are ignored, as the
  // InterestGroupManager automatically attaches the current time, though their
  // wins will be added in order, with chronologically increasing times within
  // each InterestGroup.
  void StartAuction(const GURL& seller_decision_logic_url,
                    std::vector<StorageInterestGroup> bidders) {
    auction_complete_ = false;

    blink::mojom::AuctionAdConfigPtr auction_config =
        CreateAuctionConfig(seller_decision_logic_url, interest_group_buyers_);

    auction_config->trusted_scoring_signals_url = trusted_scoring_signals_url_;

    for (const auto& component_auction : component_auctions_) {
      auction_config->component_auctions.emplace_back(
          component_auction.Clone());
    }

    interest_group_manager_ = std::make_unique<InterestGroupManagerImpl>(
        base::FilePath(), /*in_memory=*/true,
        /*url_loader_factory=*/nullptr);
    if (!auction_process_manager_) {
      auction_process_manager_ =
          std::make_unique<SameProcessAuctionProcessManager>();
    }
    auction_worklet_manager_ = std::make_unique<AuctionWorkletManager>(
        auction_process_manager_.get(), top_frame_origin_, frame_origin_, this);
    interest_group_manager_->set_auction_process_manager_for_testing(
        std::move(auction_process_manager_));

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Add previous wins and bids to the interest group manager.
    for (auto& bidder : bidders) {
      for (int i = 0; i < bidder.bidding_browser_signals->join_count; i++) {
        interest_group_manager_->JoinInterestGroup(
            bidder.interest_group, bidder.interest_group.owner.GetURL());
      }
      for (int i = 0; i < bidder.bidding_browser_signals->bid_count; i++) {
        interest_group_manager_->RecordInterestGroupBid(
            bidder.interest_group.owner, bidder.interest_group.name);
      }
      for (const auto& prev_win : bidder.bidding_browser_signals->prev_wins) {
        interest_group_manager_->RecordInterestGroupWin(
            bidder.interest_group.owner, bidder.interest_group.name,
            prev_win->ad_json);
        // Add some time between interest group wins, so that they'll be added
        // to the database in the order they appear. Their times will *not*
        // match those in `prev_wins`.
        task_environment_.FastForwardBy(base::Seconds(1));
      }
    }

    auction_run_loop_ = std::make_unique<base::RunLoop>();
    auction_runner_ = AuctionRunner::CreateAndStart(
        auction_worklet_manager_.get(), interest_group_manager_.get(),
        std::move(auction_config), IsInterestGroupApiAllowedCallback(),
        base::BindOnce(&AuctionRunnerTest::OnAuctionComplete,
                       base::Unretained(this)));
  }

  const Result& RunAuctionAndWait(const GURL& seller_decision_logic_url,
                                  std::vector<StorageInterestGroup> bidders) {
    StartAuction(seller_decision_logic_url, std::move(bidders));
    auction_run_loop_->Run();
    return result_;
  }

  void OnAuctionComplete(AuctionRunner* auction_runner,
                         absl::optional<GURL> ad_url,
                         absl::optional<std::vector<GURL>> ad_component_urls,
                         std::vector<GURL> report_urls,
                         std::vector<GURL> debug_loss_report_urls,
                         std::vector<GURL> debug_win_report_urls,
                         std::vector<std::string> errors) {
    DCHECK(auction_run_loop_);
    DCHECK(!auction_complete_);
    DCHECK_EQ(auction_runner, auction_runner_.get());

    // Delete the auction runner, which is needed to update histograms.
    auction_runner_.reset();

    auction_complete_ = true;
    result_.ad_url = std::move(ad_url);
    result_.ad_component_urls = std::move(ad_component_urls);
    result_.report_urls = std::move(report_urls);
    result_.errors = std::move(errors);
    result_.bidder1_bid_count = -1;
    result_.bidder1_prev_wins.clear();
    result_.bidder2_bid_count = -1;
    result_.bidder2_prev_wins.clear();
    result_.debug_loss_report_urls = std::move(debug_loss_report_urls);
    result_.debug_win_report_urls = std::move(debug_win_report_urls);

    // Retrieve bid count and previous wins for kBidder1 (and subsequently
    // kBidder2).
    interest_group_manager_->GetInterestGroupsForOwner(
        kBidder1, base::BindOnce(&AuctionRunnerTest::OnBidder1GroupsRetrieved,
                                 base::Unretained(this)));
  }

  void OnBidder1GroupsRetrieved(
      std::vector<StorageInterestGroup> storage_interest_groups) {
    for (auto& bidder : storage_interest_groups) {
      if (bidder.interest_group.owner == kBidder1 &&
          bidder.interest_group.name == kBidder1Name) {
        result_.bidder1_bid_count = bidder.bidding_browser_signals->bid_count;
        result_.bidder1_prev_wins =
            std::move(bidder.bidding_browser_signals->prev_wins);
        SortPrevWins(result_.bidder1_prev_wins);
        break;
      }
    }
    interest_group_manager_->GetInterestGroupsForOwner(
        kBidder2, base::BindOnce(&AuctionRunnerTest::OnBidder2GroupsRetrieved,
                                 base::Unretained(this)));
  }

  void OnBidder2GroupsRetrieved(
      std::vector<StorageInterestGroup> storage_interest_groups) {
    for (auto& bidder : storage_interest_groups) {
      if (bidder.interest_group.owner == kBidder2 &&
          bidder.interest_group.name == kBidder2Name) {
        result_.bidder2_bid_count = bidder.bidding_browser_signals->bid_count;
        result_.bidder2_prev_wins =
            std::move(bidder.bidding_browser_signals->prev_wins);
        SortPrevWins(result_.bidder2_prev_wins);
        break;
      }
    }

    auction_run_loop_->Quit();
  }

  StorageInterestGroup MakeInterestGroup(
      url::Origin owner,
      std::string name,
      absl::optional<GURL> bidding_url,
      absl::optional<GURL> trusted_bidding_signals_url,
      std::vector<std::string> trusted_bidding_signals_keys,
      absl::optional<GURL> ad_url,
      absl::optional<std::vector<GURL>> ad_component_urls = absl::nullopt) {
    std::vector<blink::InterestGroup::Ad> ads;
    // Give only kBidder1 an InterestGroupAd ad with non-empty metadata, to
    // better test the `ad_metadata` output.
    if (ad_url) {
      if (owner == kBidder1) {
        ads.emplace_back(blink::InterestGroup::Ad(*ad_url, R"({"ads": true})"));
      } else {
        ads.emplace_back(blink::InterestGroup::Ad(*ad_url, absl::nullopt));
      }
    }

    absl::optional<std::vector<blink::InterestGroup::Ad>> ad_components;
    if (ad_component_urls) {
      ad_components.emplace();
      for (const GURL& ad_component_url : *ad_component_urls)
        ad_components->emplace_back(
            blink::InterestGroup::Ad(ad_component_url, absl::nullopt));
    }

    // Create fake previous wins. The time of these wins is ignored, since the
    // InterestGroupManager attaches the current time when logging a win.
    std::vector<auction_worklet::mojom::PreviousWinPtr> previous_wins;
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": 0})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -1})"));
    previous_wins.push_back(auction_worklet::mojom::PreviousWin::New(
        base::Time::Now(), R"({"winner": -2})"));

    StorageInterestGroup storage_group;
    storage_group.interest_group = blink::InterestGroup(
        base::Time::Max(), std::move(owner), std::move(name),
        std::move(bidding_url),
        /*bidding_wasm_helper_url=*/absl::nullopt,
        /*update_url=*/GURL(), std::move(trusted_bidding_signals_url),
        std::move(trusted_bidding_signals_keys), absl::nullopt, std::move(ads),
        std::move(ad_components));
    storage_group.bidding_browser_signals =
        auction_worklet::mojom::BiddingBrowserSignals::New(
            3, 5, std::move(previous_wins));
    return storage_group;
  }

  void StartStandardAuction() {
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
        {"k1", "k2"}, GURL("https://ad1.com"),
        std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                          GURL("https://ad1.com-component2.com")}));
    bidders.emplace_back(MakeInterestGroup(
        kBidder2, kBidder2Name, kBidder2Url, kBidder2TrustedSignalsUrl,
        {"l1", "l2"}, GURL("https://ad2.com"),
        std::vector<GURL>{GURL("https://ad2.com-component1.com"),
                          GURL("https://ad2.com-component2.com")}));

    StartAuction(kSellerUrl, std::move(bidders));
  }

  const Result& RunStandardAuction() {
    StartStandardAuction();
    auction_run_loop_->Run();
    return result_;
  }

  // Starts the standard auction with the mock worklet service, and waits for
  // the service to receive the worklet construction calls.
  void StartStandardAuctionWithMockService() {
    UseMockWorkletService();
    StartStandardAuction();
    mock_auction_process_manager_->WaitForWorklets(
        /*num_bidders=*/2, /*num_sellers=*/1 + component_auctions_.size());
  }

  // AuctionWorkletManager::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  RenderFrameHostImpl* GetFrame() override { return nullptr; }
  network::mojom::ClientSecurityStatePtr GetClientSecurityState() override {
    return network::mojom::ClientSecurityState::New();
  }

  // DebuggableAuctionWorkletTracker::Observer implementation
  void AuctionWorkletCreated(DebuggableAuctionWorklet* worklet,
                             bool& should_pause_on_start) override {
    should_pause_on_start = (worklet->url() == pause_worklet_url_);
    observer_log_.push_back(base::StrCat({"Create ", worklet->url().spec()}));
    title_log_.push_back(worklet->Title());
  }

  void AuctionWorkletDestroyed(DebuggableAuctionWorklet* worklet) override {
    observer_log_.push_back(base::StrCat({"Destroy ", worklet->url().spec()}));
  }

  // Gets script URLs of currently live DebuggableAuctionWorklet.
  std::vector<std::string> LiveDebuggables() {
    std::vector<std::string> result;
    for (DebuggableAuctionWorklet* debuggable :
         DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
      result.push_back(debuggable->url().spec());
    }
    return result;
  }

  // Enables use of a mock AuctionProcessManager when the next auction is run.
  void UseMockWorkletService() {
    auto mock_auction_process_manager =
        std::make_unique<MockAuctionProcessManager>();
    mock_auction_process_manager_ = mock_auction_process_manager.get();
    auction_process_manager_ = std::move(mock_auction_process_manager);
  }

  // Check histogram values. If `expected_interest_groups` or `expected_owners`
  // is null, expect the auction to be aborted before the corresponding
  // histograms are recorded.
  void CheckHistograms(AuctionRunner::AuctionResult expected_result,
                       absl::optional<int> expected_interest_groups,
                       absl::optional<int> expected_owners) {
    histogram_tester_->ExpectUniqueSample("Ads.InterestGroup.Auction.Result",
                                          expected_result, 1);

    if (expected_interest_groups.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumInterestGroups",
          *expected_interest_groups, 1);
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumInterestGroups", 0);
    }

    if (expected_owners.has_value()) {
      histogram_tester_->ExpectUniqueSample(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
          *expected_owners, 1);
    } else {
      histogram_tester_->ExpectTotalCount(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups", 0);
    }
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AbortTime",
        expected_result == AuctionRunner::AuctionResult::kAborted);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
        expected_result == AuctionRunner::AuctionResult::kNoBids ||
            expected_result == AuctionRunner::AuctionResult::kAllBidsRejected);
    histogram_tester_->ExpectTotalCount(
        "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
        expected_result == AuctionRunner::AuctionResult::kSuccess);
  }

  AuctionRunner::IsInterestGroupApiAllowedCallback
  IsInterestGroupApiAllowedCallback() {
    return base::BindRepeating(&AuctionRunnerTest::IsInterestGroupAPIAllowed,
                               base::Unretained(this));
  }

  bool IsInterestGroupAPIAllowed(ContentBrowserClient::InterestGroupApiOperation
                                     interest_group_api_operation,
                                 const url::Origin& origin) {
    if (interest_group_api_operation ==
        ContentBrowserClient::InterestGroupApiOperation::kSell) {
      return disallowed_sellers_.find(origin) == disallowed_sellers_.end();
    }
    DCHECK_EQ(ContentBrowserClient::InterestGroupApiOperation::kBuy,
              interest_group_api_operation);
    return disallowed_buyers_.find(origin) == disallowed_buyers_.end();
  }

  // Creates a an auction with 1-2 component sellers and 2 bidders, and sets up
  // `url_loader_factory_` to provide the standard responses needed to run the
  // auction. `bidder1_seller` and `bidder2_seller` identify the seller whose
  // auction each bidder is in, and must be one of kSeller, kComponentSeller1,
  // and kComponentSeller2. kComponentSeller1 is always added to the auction,
  // kComponentSeller2 is only added to the auction if one of the bidders uses
  // it as a seller.
  void SetUpComponentAuctionAndResponses(const url::Origin& bidder1_seller,
                                         const url::Origin& bidder2_seller) {
    interest_group_buyers_.emplace();
    std::vector<url::Origin> component1_buyers;
    std::vector<url::Origin> component2_buyers;

    if (bidder1_seller == kSeller) {
      interest_group_buyers_->push_back(kBidder1);
    } else if (bidder1_seller == kComponentSeller1) {
      component1_buyers.push_back(kBidder1);
    } else if (bidder1_seller == kComponentSeller2) {
      component2_buyers.push_back(kBidder1);
    } else {
      NOTREACHED();
    }

    if (bidder2_seller == kSeller) {
      interest_group_buyers_->push_back(kBidder2);
    } else if (bidder2_seller == kComponentSeller1) {
      component1_buyers.push_back(kBidder2);
    } else if (bidder2_seller == kComponentSeller2) {
      component2_buyers.push_back(kBidder2);
    } else {
      NOTREACHED();
    }

    component_auctions_.emplace_back(CreateAuctionConfig(
        kComponentSeller1Url, std::move(component1_buyers)));
    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kComponentSeller1Url,
        MakeDecisionScript(kComponentSeller1Url, /*send_report_url=*/GURL(
                               "https://component1-report.test/")));

    if (!component2_buyers.empty()) {
      component_auctions_.emplace_back(CreateAuctionConfig(
          kComponentSeller2Url, std::move(component2_buyers)));
      auction_worklet::AddJavascriptResponse(
          &url_loader_factory_, kComponentSeller2Url,
          MakeDecisionScript(kComponentSeller2Url, /*send_report_url=*/GURL(
                                 "https://component2-report.test/")));
    }

    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kBidder1Url,
        MakeBidScript(bidder1_seller, "1", "https://ad1.com/",
                      /*num_ad_components=*/2, kBidder1, kBidder1Name,
                      /*has_signals=*/true, "k1", "a"));
    auction_worklet::AddJsonResponse(
        &url_loader_factory_,
        GURL(kBidder1TrustedSignalsUrl.spec() +
             "?hostname=publisher1.com&keys=k1,k2"),
        R"({"k1":"a", "k2": "b", "extra": "c"})");

    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kBidder2Url,
        MakeBidScript(bidder2_seller, "2", "https://ad2.com/",
                      /*num_ad_components=*/2, kBidder2, kBidder2Name,
                      /*has_signals=*/true, "l2", "b"));
    auction_worklet::AddJsonResponse(
        &url_loader_factory_,
        GURL(kBidder2TrustedSignalsUrl.spec() +
             "?hostname=publisher1.com&keys=l1,l2"),
        R"({"l1":"a", "l2": "b", "extra": "c"})");

    auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                           MakeAuctionScript());
  }

  const url::Origin top_frame_origin_ =
      url::Origin::Create(GURL("https://publisher1.com"));
  const url::Origin frame_origin_ =
      url::Origin::Create(GURL("https://frame.origin.test"));
  const GURL kSellerUrl{"https://adstuff.publisher1.com/auction.js"};
  const url::Origin kSeller = url::Origin::Create(kSellerUrl);
  absl::optional<GURL> trusted_scoring_signals_url_;

  const GURL kComponentSeller1Url{"https://component.seller1.test/foo.js"};
  const url::Origin kComponentSeller1 =
      url::Origin::Create(kComponentSeller1Url);
  const GURL kComponentSeller2Url{"https://component.seller2.test/bar.js"};
  const url::Origin kComponentSeller2 =
      url::Origin::Create(kComponentSeller2Url);

  const GURL kBidder1Url{"https://adplatform.com/offers.js"};
  const url::Origin kBidder1 = url::Origin::Create(kBidder1Url);
  const GURL kBidder1TrustedSignalsUrl{"https://adplatform.com/signals1"};

  const GURL kBidder2Url{"https://anotheradthing.com/bids.js"};
  const url::Origin kBidder2 = url::Origin::Create(kBidder2Url);
  const std::string kBidder2Name{"Another Ad Thing"};
  const GURL kBidder2TrustedSignalsUrl{"https://anotheradthing.com/signals2"};

  absl::optional<std::vector<url::Origin>> interest_group_buyers_ = {
      {kBidder1, kBidder2}};

  std::vector<blink::mojom::AuctionAdConfigPtr> component_auctions_;

  // Origins which are not allowed to take part in auctions, as the
  // corresponding participant types.
  std::set<url::Origin> disallowed_sellers_;
  std::set<url::Origin> disallowed_buyers_;

  base::test::TaskEnvironment task_environment_;

  // RunLoop that's quit on auction completion.
  std::unique_ptr<base::RunLoop> auction_run_loop_;
  // True if the most recently started auction has completed.
  bool auction_complete_ = false;
  // Result of the most recent auction.
  Result result_;

  network::TestURLLoaderFactory url_loader_factory_;

  std::unique_ptr<AuctionWorkletManager> auction_worklet_manager_;

  // This is used (and consumed) when starting an auction, if non-null. Allows
  // either using a MockAuctionProcessManager instead of a
  // SameProcessAuctionProcessManager, or using a SameProcessAuctionProcessManager
  // that has already vended processes. If nullptr, a new
  // SameProcessAuctionProcessManager() is created when an auction is started.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  // Set by UseMockWorkletService(). Non-owning reference to the
  // AuctionProcessManager that will be / has been passed to the
  // InterestGroupManager.
  raw_ptr<MockAuctionProcessManager> mock_auction_process_manager_ = nullptr;

  // The InterestGroupManager is recreated and repopulated for each auction.
  std::unique_ptr<InterestGroupManagerImpl> interest_group_manager_;

  std::unique_ptr<AuctionRunner> auction_runner_;
  // This should be inspected using TakeBadMessage(), which also clears it.
  std::string bad_message_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;

  std::vector<std::string> observer_log_;
  std::vector<std::string> title_log_;

  // Which worklet to pause, if any.
  GURL pause_worklet_url_;
};

// Runs an auction with an empty buyers field.
TEST_F(AuctionRunnerTest, NullBuyers) {
  interest_group_buyers_->clear();
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);
}

// Runs a component auction with all buyers fields null.
TEST_F(AuctionRunnerTest, ComponentAuctionNullBuyers) {
  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, /*buyers=*/absl::nullopt));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);
}

// Runs an auction with an empty buyers field.
TEST_F(AuctionRunnerTest, EmptyBuyers) {
  interest_group_buyers_->clear();
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);
}

// Runs a component auction with all buyers fields empty.
TEST_F(AuctionRunnerTest, ComponentAuctionEmptyBuyers) {
  interest_group_buyers_->clear();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, /*buyers=*/{}));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);
}

// Runs the standard auction, but without adding any interest groups to the
// manager.
TEST_F(AuctionRunnerTest, NoInterestGroups) {
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/0);
}

// Runs a component auction, but without adding any interest groups to the
// manager.
TEST_F(AuctionRunnerTest, ComponentAuctionNoInterestGroups) {
  interest_group_buyers_->clear();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/0);
}

// Runs an standard auction, but with an interest group that does not list any
// ads.
TEST_F(AuctionRunnerTest, OneInterestGroupNoAds) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, /*ad_url=*/absl::nullopt));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/1);
}

// Runs an auction with one component that has a buyer with an interest group,
// but that group has no ads.
TEST_F(AuctionRunnerTest, ComponentAuctionOneInterestGroupNoAds) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, /*ad_url=*/absl::nullopt));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/1);
}

// Runs an standard auction, but with an interest group that does not list a
// bidding script.
TEST_F(AuctionRunnerTest, OneInterestGroupNoBidScript) {
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, /*bidding_url=*/absl::nullopt,
      kBidder1TrustedSignalsUrl, {"k1", "k2"}, GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/0, /*expected_owners=*/1);
}

// Runs the standard auction, but with only adding one of the two standard
// interest groups to the manager.
TEST_F(AuctionRunnerTest, OneInterestGroup) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
      {"k1", "k2"}, GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1);
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js"));
}

// An auction with two successful bids.
TEST_F(AuctionRunnerTest, Basic) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://anotheradthing.com/bids.js"));
  EXPECT_THAT(
      title_log_,
      testing::UnorderedElementsAre(
          "FLEDGE seller worklet for https://adstuff.publisher1.com/auction.js",
          "FLEDGE bidder worklet for https://adplatform.com/offers.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js",
          "FLEDGE bidder worklet for https://anotheradthing.com/bids.js"));
}

TEST_F(AuctionRunnerTest, BasicDebug) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name, /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name, /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  for (const GURL& debug_url : {kBidder1Url, kBidder2Url, kSellerUrl}) {
    SCOPED_TRACE(debug_url);
    pause_worklet_url_ = debug_url;

    // Seller breakpoint is expected to hit twice.
    int expected_hits = (debug_url == kSellerUrl ? 2 : 1);

    StartStandardAuction();
    task_environment_.RunUntilIdle();

    bool found = false;
    mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent;

    for (DebuggableAuctionWorklet* debuggable :
         DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
      if (debuggable->url() == debug_url) {
        found = true;
        debuggable->ConnectDevToolsAgent(
            agent.BindNewEndpointAndPassReceiver());
      }
    }
    ASSERT_TRUE(found);

    TestDevToolsAgentClient debug(std::move(agent), "S1",
                                  /*use_binary_protocol=*/true);
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
        R"({"id":1,"method":"Runtime.enable","params":{}})");
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
        R"({"id":2,"method":"Debugger.enable","params":{}})");

    // Set a breakpoint, and let the worklet run.
    const char kBreakpointTemplate[] = R"({
        "id":3,
        "method":"Debugger.setBreakpointByUrl",
        "params": {
          "lineNumber": 5,
          "url": "%s",
          "columnNumber": 0,
          "condition": ""
        }})";

    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 3,
        "Debugger.setBreakpointByUrl",
        base::StringPrintf(kBreakpointTemplate, debug_url.spec().c_str()));
    debug.RunCommandAndWaitForResult(
        TestDevToolsAgentClient::Channel::kMain, 4,
        "Runtime.runIfWaitingForDebugger",
        R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

    // Should get breakpoint hit eventually.
    for (int hit = 0; hit < expected_hits; ++hit) {
      TestDevToolsAgentClient::Event breakpoint_hit =
          debug.WaitForMethodNotification("Debugger.paused");

      base::Value* hit_breakpoints =
          breakpoint_hit.value.FindListPath("params.hitBreakpoints");
      ASSERT_TRUE(hit_breakpoints);
      base::Value::ConstListView hit_breakpoints_list =
          hit_breakpoints->GetListDeprecated();
      ASSERT_EQ(1u, hit_breakpoints_list.size());
      ASSERT_TRUE(hit_breakpoints_list[0].is_string());
      EXPECT_EQ(base::StringPrintf("1:5:0:%s", debug_url.spec().c_str()),
                hit_breakpoints_list[0].GetString());

      // Just resume execution.
      debug.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
          R"({"id":6,"method":"Debugger.resume","params":{}})");
    }

    // In the case bidder 2 wins the auction, the script will be reloaded, and
    // the second time it's loaded the worklet will also start in the paused
    // state. Resume it, so the test doesn't hang.
    if (debug_url == kBidder2Url) {
      task_environment_.RunUntilIdle();
      found = false;
      mojo::AssociatedRemote<blink::mojom::DevToolsAgent> agent;
      for (DebuggableAuctionWorklet* debuggable :
           DebuggableAuctionWorkletTracker::GetInstance()->GetAll()) {
        if (debuggable->url() == debug_url) {
          found = true;
          debuggable->ConnectDevToolsAgent(
              agent.BindNewEndpointAndPassReceiver());
        }
      }
      ASSERT_TRUE(found);

      TestDevToolsAgentClient debug(std::move(agent), "S1",
                                    /*use_binary_protocol=*/true);

      debug.RunCommandAndWaitForResult(
          TestDevToolsAgentClient::Channel::kMain, 1,
          "Runtime.runIfWaitingForDebugger",
          R"({"id":1,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
    }

    // Let it finish --- result should as in Basic test since this didn't
    // actually change anything.
    auction_run_loop_->Run();
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
    EXPECT_THAT(result_.report_urls,
                testing::UnorderedElementsAre(
                    GURL("https://reporting.example.com/2"),
                    GURL("https://buyer-reporting.example.com/2")));
  }
}

TEST_F(AuctionRunnerTest, PauseBidder) {
  pause_worklet_url_ = kBidder2Url;

  // Save a pointer to SameProcessAuctionProcessManager since we'll need its help
  // to resume things.
  auto process_manager = std::make_unique<SameProcessAuctionProcessManager>();
  SameProcessAuctionProcessManager* process_manager_impl = process_manager.get();
  auction_process_manager_ = std::move(process_manager);

  // Have a 404 for script 2 until ready to resume.
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  StartStandardAuction();
  // Run all threads as far as they can get.
  task_environment_.RunUntilIdle();
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));

  process_manager_impl->ResumeAllPaused();

  // Need to resume a second time, when the script is re-loaded to run
  // ReportWin().
  task_environment_.RunUntilIdle();
  process_manager_impl->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

TEST_F(AuctionRunnerTest, PauseSeller) {
  pause_worklet_url_ = kSellerUrl;

  // Save a pointer to SameProcessAuctionProcessManager since we'll need its help
  // to resume things.
  auto process_manager = std::make_unique<SameProcessAuctionProcessManager>();
  SameProcessAuctionProcessManager* process_manager_impl = process_manager.get();
  auction_process_manager_ = std::move(process_manager);

  // Have a 404 for seller until ready to resume.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  StartStandardAuction();
  // Run all threads as far as they can get.
  task_environment_.RunUntilIdle();
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  process_manager_impl->ResumeAllPaused();

  auction_run_loop_->Run();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

// A component auction with two successful bids from different components.
TEST_F(AuctionRunnerTest, ComponentAuction) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2);

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://component2-report.test/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// A component auction with two buyers in the top-level auction. The component
// seller has no buyers.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellersHaveNoBuyers) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kSeller,
                                    /*bidder2_seller=*/kSeller);

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// A component auction with one component. Both the top-level and component
// auction have one buyer. The top-level seller worklet has the winning buyer.
TEST_F(AuctionRunnerTest, ComponentAuctionTopLeverSellerBidWins) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kSeller);

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// A component auction with one component. Both the top-level and component
// auction have one buyer. The component seller worklet has the winning buyer.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellerBidWins) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kSeller,
                                    /*bidder2_seller=*/kComponentSeller1);

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://component1-report.test/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test case where the top-level and a component auction share the same buyer,
// which makes different bids for both auctions. Tests both the case the bid
// made in the main auction wins, and the case the bid made in the component
// auction wins.
//
// This tests that parameters are separated, that bid counts are updated
// correctly, and how histograms are updated in these cases.
TEST_F(AuctionRunnerTest, ComponentAuctionSharedBuyer) {
  const GURL kTopLevelBidUrl = GURL("https://top-level-bid.test/");
  const GURL kComponentBidUrl = GURL("https://component-bid.test/");

  // Bid script used in both auctions. The bid amount is based on the seller:
  // It bids the most in auctions run by kComponentSeller2Url, and the least in
  // auctions run by kComponentSeller1Url, so one script can handle both test
  // cases.
  const char kBidScript[] = R"(
      function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                           trustedBiddingSignals, browserSignals) {
        if (browserSignals.seller == "https://component.seller1.test")
          return {ad: [], bid: 1, render: "https://component-bid.test/"};
        if (browserSignals.seller == "https://component.seller2.test")
          return {ad: [], bid: 3, render: "https://component-bid.test/"};
        return {ad: [], bid: 2, render: "https://top-level-bid.test/"};
      }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      sendReportTo("https://buyer-reporting.example.com/" + browserSignals.bid);
    }
  )";

  // Script used for both sellers. Return different desireability scores based
  // on bid and seller, to make sure correct values are plumbed through.
  const std::string kSellerScript = R"(
    function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
      if (auctionConfig.seller == "https://adstuff.publisher1.com")
        return 20 + bid;
      return 10 + bid;
    }

    function reportResult(auctionConfig, browserSignals) {
      sendReportTo(auctionConfig.seller + "/" +
                   browserSignals.desirability);
    }
  )";

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_,
                                         kComponentSeller1Url, kSellerScript);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_,
                                         kComponentSeller2Url, kSellerScript);

  //--------------------------------------
  // Case the top-level auction's bid wins
  //--------------------------------------

  interest_group_buyers_ = {{kBidder1}};
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));

  // Custom interest group with two ads, so both bid URLs are valid.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(
      MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                        /*trusted_bidding_signals_url=*/absl::nullopt,
                        /*trusted_bidding_signals_keys=*/{}, kTopLevelBidUrl));
  bidders[0].interest_group.ads->push_back(
      blink::InterestGroup::Ad(kComponentBidUrl, absl::nullopt));

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  EXPECT_EQ(kTopLevelBidUrl, result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://adstuff.publisher1.com/22"),
                  GURL("https://buyer-reporting.example.com/2")));
  // Bid count should only be incremented by 1.
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://top-level-bid.test/",)"
            R"("metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  // Currently an interest groups participating twice in an auction is counted
  // twice.
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);

  //--------------------------------------
  // Case the component auction's bid wins
  //--------------------------------------

  histogram_tester_ = std::make_unique<base::HistogramTester>();

  // Add another kComponentSeller2Url as another seller, for a total of 2
  // component sellers.
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder1}}));

  // Custom interest group with two ads, so both bid URLs are valid.
  bidders = std::vector<StorageInterestGroup>();
  bidders.emplace_back(
      MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                        /*trusted_bidding_signals_url=*/absl::nullopt,
                        /*trusted_bidding_signals_keys=*/{}, kTopLevelBidUrl));
  bidders[0].interest_group.ads->push_back(
      blink::InterestGroup::Ad(kComponentBidUrl, absl::nullopt));

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_THAT(result_.errors, testing::ElementsAre());

  EXPECT_EQ(kComponentBidUrl, result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://adstuff.publisher1.com/23"),
                  GURL("https://component.seller2.test/13"),
                  GURL("https://buyer-reporting.example.com/3")));
  // Bid count should only be incremented by 1.
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://component-bid.test/"})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  // Currently a bidder participating twice in an auction is counted as two
  // participating interest groups.
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/3, /*expected_owners=*/3);
}

// A component auction with one component that has two buyers. In this auction,
// the top-level auction would score kBidder2 higher (since it bids more), but
// kBidder1 wins this auction, because the component auctions use a different
// scoring function, which favors kBidder1's lower bid.
TEST_F(AuctionRunnerTest, ComponentAuctionOneComponentTwoBidders) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1);

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(6, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction in which the seller origin is not allowed to use the interest
// group API.
TEST_F(AuctionRunnerTest, DisallowedSeller) {
  disallowed_sellers_.insert(url::Origin::Create(kSellerUrl));

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSellerRejected,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);

  // No requests for the bidder worklet URLs should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which the component seller is disallowed, and the
// top-level seller has no buyers.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionSeller) {
  interest_group_buyers_.reset();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));

  disallowed_sellers_.insert(kComponentSeller1);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);

  // No requests for the bidder worklet URLs should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which the one component seller is disallowed, but the
// other is not.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionOneSeller) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2);

  // Bidder 2 bids more, so would win the auction if component seller 2 were
  // allowed to participate.
  disallowed_sellers_.insert(kComponentSeller2);

  RunAuctionAndWait(kSellerUrl, std::vector<StorageInterestGroup>());

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1);
}

// An auction in which the buyer origins are not allowed to use the interest
// group API.
TEST_F(AuctionRunnerTest, DisallowedBuyers) {
  disallowed_buyers_.insert(kBidder1);
  disallowed_buyers_.insert(kBidder2);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);

  // No requests for the seller worklet URL should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// Run the standard auction, but disallow one bidder from participating.
TEST_F(AuctionRunnerTest, DisallowedSingleBuyer) {
  // The lack of a bidder script 2 means that this test should hang if bidder
  // 2's script is requested.
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");

  disallowed_buyers_.insert(kBidder2);
  RunStandardAuction();

  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1);

  // No requests for bidder2's worklet URL should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which all buyers are disallowed.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionBuyers) {
  interest_group_buyers_->clear();
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));

  disallowed_buyers_.insert(kBidder1);
  disallowed_buyers_.insert(kBidder2);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  /*expected_interest_groups=*/absl::nullopt,
                  /*expected_owners=*/absl::nullopt);

  // No requests for the bidder worklet URLs should be made.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0, url_loader_factory_.NumPending());
}

// A component auction in which a single buyer is disallowed.
TEST_F(AuctionRunnerTest, DisallowedComponentAuctionSingleBuyer) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2);

  disallowed_buyers_.insert(kBidder2);

  // The lack of Javascript responses means the auction should hang if any
  // script URLs are incorrectly requested.
  RunStandardAuction();

  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/1, /*expected_owners=*/1);
}

// Disallow bidders as sellers and disallow seller as bidder. Auction should
// still succeed.
TEST_F(AuctionRunnerTest, DisallowedAsOtherParticipant) {
  disallowed_sellers_.insert(kBidder1);
  disallowed_sellers_.insert(kBidder2);
  disallowed_buyers_.insert(url::Origin::Create(kSellerUrl));

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where one bid is successful, another's script 404s.
TEST_F(AuctionRunnerTest, OneBidOne404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  ASSERT_EQ(4u, res.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            res.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(
      res.errors,
      testing::ElementsAre("Failed to load https://anotheradthing.com/bids.js "
                           "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);

  // 404 is detected after the worklet is created, so there are still events
  // for it.
  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Destroy https://adplatform.com/offers.js"));
}

// An auction where one component seller fails to load, but the other loads, so
// the auction succeeds.
TEST_F(AuctionRunnerTest, ComponentAuctionOneSeller404) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2);
  url_loader_factory_.AddResponse(kComponentSeller2Url.spec(), "",
                                  net::HTTP_NOT_FOUND);

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://component1-report.test/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  // The bid send to the failing component seller worklet isn't counted,
  // regardless of whether the bid completed before the worklet failed to load.
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_THAT(result_.errors,
              testing::ElementsAre(
                  "Failed to load https://component.seller2.test/bar.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where one bid is successful, another's script does not provide a
// bidding function.
TEST_F(AuctionRunnerTest, OneBidOneNotMade) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));

  // The auction script doesn't make any bids.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  ASSERT_EQ(4u, res.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            res.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors,
              testing::ElementsAre("https://anotheradthing.com/bids.js "
                                   "`generateBid` is not a function."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where no bidding scripts load successfully.
TEST_F(AuctionRunnerTest, NoBids) {
  url_loader_factory_.AddResponse(kBidder1Url.spec(), "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(kBidder2Url.spec(), "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra":"c"})");

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors,
              testing::UnorderedElementsAre(
                  "Failed to load https://adplatform.com/offers.js "
                  "HTTP status = 404 Not Found.",
                  "Failed to load https://anotheradthing.com/bids.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where none of the bidding scripts has a valid bidding function.
TEST_F(AuctionRunnerTest, NoBidMadeByScript) {
  // MakeAuctionScript() is a valid script that doesn't have a bidding function.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder2Url,
                                         MakeAuctionScript());
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra":"c"})");

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(
      res.errors,
      testing::UnorderedElementsAre(
          "https://adplatform.com/offers.js `generateBid` is not a function.",
          "https://anotheradthing.com/bids.js `generateBid` is not a "
          "function."));
  CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where the seller script doesn't have a scoring function.
TEST_F(AuctionRunnerTest, SellerRejectsAll) {
  std::string bid_script1 =
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a");
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         bid_script1);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));

  // No seller scoring function in a bid script.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         bid_script1);
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2":"b", "extra":"c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra":"c"})");

  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors, testing::UnorderedElementsAre(
                              "https://adstuff.publisher1.com/auction.js "
                              "`scoreAd` is not a function.",
                              "https://adstuff.publisher1.com/auction.js "
                              "`scoreAd` is not a function."));
  CheckHistograms(AuctionRunner::AuctionResult::kAllBidsRejected,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where seller rejects one bid when scoring.
TEST_F(AuctionRunnerTest, SellerRejectsOne) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptReject2());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  ASSERT_EQ(4u, res.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            res.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(6, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where the seller script fails to load.
TEST_F(AuctionRunnerTest, NoSellerScript) {
  // Tests to make sure that if seller script fails the other fetches are
  // cancelled, too.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());

  EXPECT_EQ(0, url_loader_factory_.NumPending());
  EXPECT_EQ(5, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(5, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSellerWorkletLoadFailed,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where bidders don't request trusted bidding signals.
TEST_F(AuctionRunnerTest, NoTrustedBiddingSignals) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/0,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/0,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                         absl::nullopt, {"k1", "k2"},
                                         GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                         absl::nullopt, {"l1", "l2"},
                                         GURL("https://ad2.com")));

  const Result& res = RunAuctionAndWait(kSellerUrl, std::move(bidders));

  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// An auction where trusted bidding signals are requested, but the fetch 404s.
TEST_F(AuctionRunnerTest, TrustedBiddingSignals404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b"));
  url_loader_factory_.AddResponse(
      kBidder1TrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=k1,k2",
      "", net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(
      kBidder2TrustedSignalsUrl.spec() + "?hostname=publisher1.com&keys=l1,l2",
      "", net::HTTP_NOT_FOUND);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::UnorderedElementsAre(
                              "Failed to load "
                              "https://adplatform.com/"
                              "signals1?hostname=publisher1.com&keys=k1,k2 "
                              "HTTP status = 404 Not Found.",
                              "Failed to load "
                              "https://anotheradthing.com/"
                              "signals2?hostname=publisher1.com&keys=l1,l2 "
                              "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// A successful auction where seller reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportResultUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre(GURL(
                                   "https://buyer-reporting.example.com/2")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// A successful auction where bidder reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportWinUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre(
                                   GURL("https://reporting.example.com/2")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// A successful auction where neither reporting worklets sets a URL.
TEST_F(AuctionRunnerTest, NeitherReportUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScriptNoReportUrl());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test the case where the seller worklet provides no signals for the winner,
// since it has no reportResult() method. The winning bidder's reportWin()
// function should be passed null as `sellerSignals`, and should still be able
// to send a report.
TEST_F(AuctionRunnerTest, NoReportResult) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinExpectNullAuctionSignals);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                  browserSignals) {
  return bid * 2;
}
                                         )");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  const Result& res = RunStandardAuction();
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            res.ad_component_urls);
  EXPECT_THAT(res.report_urls, testing::UnorderedElementsAre(GURL(
                                   "https://seller.signals.were.null.test/")));
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre(base::StringPrintf(
                              "%s `reportResult` is not a function.",
                              kSellerUrl.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

TEST_F(AuctionRunnerTest, TrustedScoringSignals) {
  trusted_scoring_signals_url_ =
      GURL("https://adstuff.publisher1.com/seller_signals");

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b") +
          kReportWinExpectNullAuctionSignals);
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  // scoreAd() that only accepts bids where the scoring signals of the
  // `renderUrl` is "accept".
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         std::string(R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  let signal = trustedScoringSignals.renderUrl[browserSignals.renderUrl];
  if (browserSignals.dataVersion !== 2) {
    throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
  }
  // 2 * bid is expected by the BidderWorklet ReportWin() script.
  if (signal == "accept")
    return 2 * bid;
  if (signal == "reject")
    return 0;
  throw "incorrect trustedScoringSignals";
}

function reportResult(auctionConfig, browserSignals) {
  sendReportTo("https://reporting.example.com/" + browserSignals.bid);
  if (browserSignals.dataVersion !== 2) {
    throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
  }
  return browserSignals;
}
                                         )"));

  // Only accept first bidder's bid. Requests will always be batched non-racily,
  // since a mock time is in use.
  auction_worklet::AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL(trusted_scoring_signals_url_->spec() +
           "?hostname=publisher1.com"
           "&renderUrls=https%3A%2F%2Fad1.com%2F,https%3A%2F%2Fad2.com%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fad1.com-component1.com%2F,"
           "https%3A%2F%2Fad2.com-component1.com%2F"),
      R"(
{"renderUrls":{"https://ad1.com/":"accept", "https://ad2.com/":"reject"}}
      )",
      /*data_version=*/2);

  RunStandardAuction();
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad1.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/1"),
                  GURL("https://buyer-reporting.example.com/1")));
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(3u, result_.bidder2_prev_wins.size());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test the case where the ProcessManager initially prevents creating worklets,
// due to being at its process limit.
TEST_F(AuctionRunnerTest, ProcessManagerBlocksWorkletCreation) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  // For the seller worklet, it only matters if the worklet process limit has
  // been hit or not.
  for (bool seller_worklet_creation_delayed : {false, true}) {
    SCOPED_TRACE(seller_worklet_creation_delayed);

    // For bidder worklets, in addition to testing the cases with no processes
    // and at the process limit, also test the case where we're one below the
    // limit, which should serialize bidder worklet creation and execution.
    for (size_t num_used_bidder_worklet_processes :
         std::vector<size_t>{static_cast<size_t>(0),
                             AuctionProcessManager::kMaxBidderProcesses - 1,
                             AuctionProcessManager::kMaxBidderProcesses}) {
      SCOPED_TRACE(num_used_bidder_worklet_processes);

      bool bidder_worklet_creation_delayed =
          num_used_bidder_worklet_processes ==
          AuctionProcessManager::kMaxBidderProcesses;

      // Create AuctionProcessManager in advance of starting the auction so can
      // create worklets before the auction starts.
      auction_process_manager_ =
          std::make_unique<SameProcessAuctionProcessManager>();

      AuctionProcessManager* auction_process_manager =
          auction_process_manager_.get();

      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
      if (seller_worklet_creation_delayed) {
        // Make kMaxSellerProcesses seller worklet requests for other origins so
        // seller worklet creation will be blocked by the process limit.
        for (size_t i = 0; i < AuctionProcessManager::kMaxSellerProcesses;
             ++i) {
          sellers.push_back(
              std::make_unique<AuctionProcessManager::ProcessHandle>());
          url::Origin origin = url::Origin::Create(
              GURL(base::StringPrintf("https://%zu.test", i)));
          EXPECT_TRUE(auction_process_manager->RequestWorkletService(
              AuctionProcessManager::WorkletType::kSeller, origin,
              &*sellers.back(), base::BindOnce([]() {
                ADD_FAILURE() << "This should not be called";
              })));
        }
      }

      // Make `num_used_bidder_worklet_processes` bidder worklet requests for
      // different origins.
      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
      for (size_t i = 0; i < num_used_bidder_worklet_processes; ++i) {
        bidders.push_back(
            std::make_unique<AuctionProcessManager::ProcessHandle>());
        url::Origin origin = url::Origin::Create(
            GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
        EXPECT_TRUE(auction_process_manager->RequestWorkletService(
            AuctionProcessManager::WorkletType::kBidder, origin,
            &*bidders.back(), base::BindOnce([]() {
              ADD_FAILURE() << "This should not be called";
            })));
      }

      // If neither sellers nor bidders are at their limit, the auction should
      // complete.
      if (!seller_worklet_creation_delayed &&
          !bidder_worklet_creation_delayed) {
        RunStandardAuction();
      } else {
        // Otherwise, the auction should be blocked.
        StartStandardAuction();
        task_environment_.RunUntilIdle();

        EXPECT_EQ(
            seller_worklet_creation_delayed ? 1u : 0u,
            auction_process_manager->GetPendingSellerRequestsForTesting());
        EXPECT_EQ(
            bidder_worklet_creation_delayed ? 2u : 0u,
            auction_process_manager->GetPendingBidderRequestsForTesting());
        EXPECT_FALSE(auction_complete_);

        // Free up a seller slot, if needed.
        if (seller_worklet_creation_delayed) {
          sellers.pop_front();
          task_environment_.RunUntilIdle();
          EXPECT_EQ(
              0u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
          EXPECT_EQ(
              bidder_worklet_creation_delayed ? 2u : 0,
              auction_process_manager->GetPendingBidderRequestsForTesting());
        }

        // Free up a single bidder slot, if needed.
        if (bidder_worklet_creation_delayed) {
          EXPECT_FALSE(auction_complete_);
          bidders.pop_front();
        }

        // The auction should now be able to run to completion.
        auction_run_loop_->Run();
      }
      EXPECT_TRUE(auction_complete_);

      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
                result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/2"),
                      GURL("https://buyer-reporting.example.com/2")));
      EXPECT_EQ(6, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(6, result_.bidder2_bid_count);
      ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.bidder2_prev_wins[3]->ad_json);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/2,
                      /*expected_owners=*/2);
    }
  }
}

// Tests ComponentAuctions and their interactions with the ProcessManager
// delaying worklet creation.
TEST_F(AuctionRunnerTest, ComponentAuctionProcessManagerBlocksWorkletCreation) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2);

  // For both worklet types, in addition to testing the cases with no processes
  // and at the process limit, also test the case where we're one below the
  // limit, which should serialize worklet creation and execution.
  for (size_t num_used_seller_worklet_processes :
       {static_cast<size_t>(0), AuctionProcessManager::kMaxSellerProcesses - 1,
        AuctionProcessManager::kMaxSellerProcesses}) {
    SCOPED_TRACE(num_used_seller_worklet_processes);

    bool seller_worklet_creation_delayed =
        num_used_seller_worklet_processes ==
        AuctionProcessManager::kMaxSellerProcesses;

    for (size_t num_used_bidder_worklet_processes :
         std::vector<size_t>{static_cast<size_t>(0),
                             AuctionProcessManager::kMaxBidderProcesses - 1,
                             AuctionProcessManager::kMaxBidderProcesses}) {
      SCOPED_TRACE(num_used_bidder_worklet_processes);

      bool bidder_worklet_creation_delayed =
          num_used_bidder_worklet_processes ==
          AuctionProcessManager::kMaxBidderProcesses;

      // Create AuctionProcessManager in advance of starting the auction so can
      // create worklets before the auction starts.
      auction_process_manager_ =
          std::make_unique<SameProcessAuctionProcessManager>();

      AuctionProcessManager* auction_process_manager =
          auction_process_manager_.get();

      // Make `num_used_seller_worklet_processes` bidder worklet requests for
      // different origins.
      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
      for (size_t i = 0; i < num_used_seller_worklet_processes; ++i) {
        sellers.push_back(
            std::make_unique<AuctionProcessManager::ProcessHandle>());
        url::Origin origin = url::Origin::Create(
            GURL(base::StringPrintf("https://%zu.test", i)));
        EXPECT_TRUE(auction_process_manager->RequestWorkletService(
            AuctionProcessManager::WorkletType::kSeller, origin,
            &*sellers.back(), base::BindOnce([]() {
              ADD_FAILURE() << "This should not be called";
            })));
      }

      // Make `num_used_bidder_worklet_processes` bidder worklet requests for
      // different origins.
      std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
      for (size_t i = 0; i < num_used_bidder_worklet_processes; ++i) {
        bidders.push_back(
            std::make_unique<AuctionProcessManager::ProcessHandle>());
        url::Origin origin = url::Origin::Create(
            GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
        EXPECT_TRUE(auction_process_manager->RequestWorkletService(
            AuctionProcessManager::WorkletType::kBidder, origin,
            &*bidders.back(), base::BindOnce([]() {
              ADD_FAILURE() << "This should not be called";
            })));
      }

      // If neither sellers nor bidders are at their limit, the auction should
      // complete.
      if (!seller_worklet_creation_delayed &&
          !bidder_worklet_creation_delayed) {
        RunStandardAuction();
      } else {
        // Otherwise, the auction should be blocked.
        StartStandardAuction();
        task_environment_.RunUntilIdle();

        if (seller_worklet_creation_delayed) {
          // In the case of `seller_worklet_creation_delayed`, only the two
          // component worklet's loads should have been queued.
          EXPECT_EQ(
              2u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
        } else if (num_used_seller_worklet_processes ==
                       AuctionProcessManager::kMaxSellerProcesses - 1 &&
                   bidder_worklet_creation_delayed) {
          // IF there's only one available seller worklet process, and
          // `bidder_worklet_creation_delayed` is true, one component seller
          // should have been created, the component seller should be queued,
          // waiting on a process slot, and the top-level seller should not have
          // been requested yet, waiting on the component sellers to both be
          // loaded.
          EXPECT_EQ(
              1u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
        } else {
          // Otherwise, no seller worklet requests should be pending..
          EXPECT_EQ(
              0u,
              auction_process_manager->GetPendingSellerRequestsForTesting());
        }

        EXPECT_EQ(
            bidder_worklet_creation_delayed ? 2u : 0u,
            auction_process_manager->GetPendingBidderRequestsForTesting());

        // Free up a seller slot, if needed.
        if (seller_worklet_creation_delayed) {
          sellers.pop_front();
          task_environment_.RunUntilIdle();
          if (bidder_worklet_creation_delayed) {
            // If bidder creation was also delayed, one component seller should
            // have been made, but is waiting on a bid. Creating the other
            // component seller should be queued. The main seller should be
            // blocked on loading that component seller.
            EXPECT_EQ(
                1u,
                auction_process_manager->GetPendingSellerRequestsForTesting());
            EXPECT_EQ(
                2u,
                auction_process_manager->GetPendingBidderRequestsForTesting());
          } else {
            // Otherwise, the auction should have completed.
            EXPECT_TRUE(auction_complete_);
          }
        }

        // Free up a single bidder slot, if needed.
        if (bidder_worklet_creation_delayed) {
          EXPECT_FALSE(auction_complete_);
          bidders.pop_front();
        }

        // The auction should now be able to run to completion.
        auction_run_loop_->Run();
      }
      EXPECT_TRUE(auction_complete_);

      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
                result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(
                      GURL("https://reporting.example.com/2"),
                      GURL("https://component2-report.test/2"),
                      GURL("https://buyer-reporting.example.com/2")));
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/2, /*expected_owners=*/2);
    }
  }
}

// Test a seller worklet load failure while waiting on bidder worklet processes
// to be allocated. Most of the tests for global seller worklet failures at a
// particular phase use seller crashes instead of load errors (see SellerCrash
// test), but this case is simplest to test with a seller load error.
TEST_F(AuctionRunnerTest, SellerLoadErrorWhileWaitingForBidders) {
  // Create AuctionProcessManager in advance of starting the auction so can
  // create worklets before the auction starts.
  auction_process_manager_ =
      std::make_unique<SameProcessAuctionProcessManager>();

  // Make kMaxBidderProcesses bidder worklet requests for different origins.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>>
      other_bidders;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    other_bidders.push_back(
        std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin,
        &*other_bidders.back(), base::BindOnce([]() {
          ADD_FAILURE() << "This should not be called";
        })));
  }

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);

  RunStandardAuction();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  ASSERT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSellerWorkletLoadFailed,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Tests ComponentAuction where a component seller worklet has a load error with
// a hanging bidder worklet request. The auction runs when the process manager
// only has 1 bidder and 1 seller slot, so this test makes sure that in this
// case the bidder and seller processes are freed up, so they don't potentially
// cause deadlock preventing the auction from completing.
TEST_F(AuctionRunnerTest,
       ComponentAuctionSellerWorkletLoadErrorWithPendingBidderLoad) {
  interest_group_buyers_.emplace();

  // First component seller worklet request fails. No response is returned for
  // the bidder worklet, so it hangs.
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  url_loader_factory_.AddResponse(kComponentSeller1Url.spec(), "",
                                  net::HTTP_NOT_FOUND);

  // Second component worklet loads as normal, as does its bidder.
  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller2Url,
      MakeDecisionScript(kComponentSeller2Url, /*send_report_url=*/GURL(
                             "https://component2-report.test/")));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller2, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b"));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  // Top-level seller uses the default script.
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  auction_process_manager_ =
      std::make_unique<SameProcessAuctionProcessManager>();

  // Take up all but 1 of the seller worklet process slots.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
  for (size_t i = 0; i < AuctionProcessManager::kMaxSellerProcesses - 1; ++i) {
    sellers.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin =
        url::Origin::Create(GURL(base::StringPrintf("https://%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kSeller, origin, &*sellers.back(),
        base::BindOnce(
            []() { ADD_FAILURE() << "This should not be called"; })));
  }

  // Take up but 1 of the bidder worklet process slots.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses - 1; ++i) {
    bidders.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager_->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin, &*bidders.back(),
        base::BindOnce(
            []() { ADD_FAILURE() << "This should not be called"; })));
  }

  RunStandardAuction();

  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(std::vector<GURL>{GURL("https://ad2.com-component1.com")},
            result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(
                  GURL("https://reporting.example.com/2"),
                  GURL("https://component2-report.test/2"),
                  GURL("https://buyer-reporting.example.com/2")));
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  "Failed to load https://component.seller1.test/foo.js HTTP "
                  "status = 404 Not Found."));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test the case where two interest groups use the same BidderWorklet, with a
// trusted bidding signals URL. The requests should be batched. This test
// basically makes sure that SendPendingSignalsRequests() is only invoked on the
// BidderWorklet after both GenerateBid() calls have been invoked.
TEST_F(AuctionRunnerTest, ReusedBidderWorkletBatchesSignalsRequests) {
  // Bidding script used by both interest groups. Since the default bid script
  // checks the interest group name, and this test uses two interest groups with
  // the same bidder script, have to use a different script for this test.
  //
  // This script uses trusted bidding signals and the interest group name to
  // select a winner, to make sure the trusted bidding signals makes it to the
  // bidder.
  const char kBidderScript[] = R"(
    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      if (browserSignals.dataVersion !== 4) {
       throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      }
      return {
        ad: 0,
        bid: trustedBiddingSignals[interestGroup.name],
        render: interestGroup.ads[0].renderUrl
      };
    }

    // Prevent an error about this method not existing.
    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                     browserSignals) {
      if (browserSignals.dataVersion !== 4) {
        throw new Error(`wrong dataVersion (${browserSignals.dataVersion})`);
      }
    }
  )";

  // Need to use a different seller script as well, due to the validation logic
  // in the default one being dependent on the details of the default bidder
  // script.
  const char kSellerScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                     browserSignals) {
      return 2 * bid;
    }

    // Prevent an error about this method not existing.
    function reportResult() {}
  )";

  // Two interest groups with all of the same URLs. They vary only in name,
  // render URL, and bidding signals key.
  std::vector<StorageInterestGroup> bidders;
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"0", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{"0"}, GURL("https://ad1.com")));
  bidders.emplace_back(MakeInterestGroup(
      kBidder1, /*name=*/"1", kBidder1Url, kBidder1TrustedSignalsUrl,
      /*trusted_bidding_signals_keys=*/{"1"}, GURL("https://ad2.com")));

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         kBidderScript);

  // Trusted signals response for the single expected request. Interest group
  // "0" bids 2, interest group "1" bids 1.
  auction_worklet::AddVersionedJsonResponse(
      &url_loader_factory_,
      GURL(kBidder1TrustedSignalsUrl.spec() +
           "?hostname=publisher1.com&keys=0,1"),
      R"({"0":2, "1": 1})", 4);

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kSellerScript);

  StartAuction(kSellerUrl, std::move(bidders));
  auction_run_loop_->Run();
  EXPECT_TRUE(auction_complete_);

  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
}

TEST_F(AuctionRunnerTest, AllBiddersCrashBeforeBidding) {
  StartStandardAuctionWithMockService();
  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  EXPECT_FALSE(auction_complete_);

  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js"));

  EXPECT_THAT(LiveDebuggables(),
              testing::UnorderedElementsAre(
                  "https://adplatform.com/offers.js",
                  "https://anotheradthing.com/bids.js",
                  "https://adstuff.publisher1.com/auction.js"));

  bidder1_worklet.reset();
  bidder2_worklet.reset();

  task_environment_.RunUntilIdle();

  EXPECT_THAT(observer_log_,
              testing::UnorderedElementsAre(
                  "Create https://adstuff.publisher1.com/auction.js",
                  "Create https://adplatform.com/offers.js",
                  "Create https://anotheradthing.com/bids.js",
                  "Destroy https://adplatform.com/offers.js",
                  "Destroy https://anotheradthing.com/bids.js",
                  "Destroy https://adstuff.publisher1.com/auction.js"));

  EXPECT_THAT(LiveDebuggables(), testing::ElementsAre());

  auction_run_loop_->Run();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder1Url.spec().c_str()),
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder2Url.spec().c_str())));

  CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test the case a single bidder worklet crashes before bidding. The auction
// should continue, without that bidder's bid.
TEST_F(AuctionRunnerTest, BidderCrashBeforeBidding) {
  for (bool other_bidder_finishes_first : {false, true}) {
    SCOPED_TRACE(other_bidder_finishes_first);

    observer_log_.clear();
    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    ASSERT_FALSE(auction_complete_);
    if (other_bidder_finishes_first) {
      bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                                 GURL("https://ad2.com/"));
      // The bidder pipe should be closed after it bids.
      EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
      bidder2_worklet.reset();
    }
    mock_auction_process_manager_->Flush();

    ASSERT_FALSE(auction_complete_);

    // Close Bidder1's pipe.
    bidder1_worklet.reset();
    // Can't flush the closed pipe without reaching into AuctionRunner, so use
    // RunUntilIdle() instead.
    task_environment_.RunUntilIdle();

    if (!other_bidder_finishes_first) {
      bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                                 GURL("https://ad2.com/"));
      // The bidder pipe should be closed after it bids.
      EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
      bidder2_worklet.reset();
    }
    mock_auction_process_manager_->Flush();

    EXPECT_THAT(observer_log_,
                testing::UnorderedElementsAre(
                    "Create https://adstuff.publisher1.com/auction.js",
                    "Create https://adplatform.com/offers.js",
                    "Create https://anotheradthing.com/bids.js",
                    "Destroy https://adplatform.com/offers.js",
                    "Destroy https://anotheradthing.com/bids.js"));

    EXPECT_THAT(
        LiveDebuggables(),
        testing::ElementsAre("https://adstuff.publisher1.com/auction.js"));

    // The auction should be scored without waiting on the crashed kBidder1.
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
    EXPECT_EQ(7, score_ad_params.bid);
    std::move(score_ad_params.callback)
        .Run(/*score=*/11, /*data_version=*/0, /*has_data_version=*/false,
             /*debug_loss_report_url=*/absl::nullopt,
             /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

    // Finish the auction.
    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();

    // Worklet 2 should be reloaded and ReportWin() invoked.
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    bidder2_worklet->WaitForReportWin();
    bidder2_worklet->InvokeReportWinCallback();

    // Bidder2 won, Bidder1 crashed.
    auction_run_loop_->Run();
    EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
    EXPECT_FALSE(result_.ad_component_urls);
    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_EQ(5, result_.bidder1_bid_count);
    EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
    EXPECT_EQ(6, result_.bidder2_bid_count);
    ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
    EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
              result_.bidder2_prev_wins[3]->ad_json);
    EXPECT_THAT(result_.errors,
                testing::ElementsAre(base::StringPrintf(
                    "%s crashed while trying to run generateBid().",
                    kBidder1Url.spec().c_str())));
    CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2);
  }
}

// If the winning bidder crashes while coming up with the reporting URL, the
// auction should fail.
TEST_F(AuctionRunnerTest, WinningBidderCrashWhileReporting) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                             GURL("https://ad1.com/"));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());
  bidder1_worklet.reset();
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad2.com/"));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
  bidder2_worklet.reset();

  // Score Bidder1's bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(7, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/11, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  // Bidder1 crashes while running ReportWin.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet.reset();
  auction_run_loop_->Run();

  // No bidder won, Bidder1 crashed.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                  "%s crashed while trying to run reportWin().",
                                  kBidder1Url.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kWinningBidderWorkletCrashed,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Should not have any debugging win/loss report URLs after auction when feature
// kBiddingAndScoringDebugReportingAPI is not enabled.
TEST_F(AuctionRunnerTest, ForDebuggingOnlyReporting) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(GURL("https://adstuff.publisher1.com/auction.js"),
                        "https://seller-debug-loss-reporting.com/",
                        "https://seller-debug-win-reporting.com/"));

  const Result& res = RunStandardAuction();
  // Bidder 2 won the auction.
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);

  EXPECT_EQ(0u, res.debug_loss_report_urls.size());
  EXPECT_EQ(0u, res.debug_win_report_urls.size());
}

// If the seller crashes at several points in the auction, the auction fails.
// Seller load failures look the same to auctions, so this test also covers
// load failures in the same places. Note that a seller worklet load error while
// waiting for bidder worklet processes is covered in another test, and looks
// exactly like a crash at the same point to the AuctionRunner.
TEST_F(AuctionRunnerTest, SellerCrash) {
  enum class CrashPhase {
    kLoad,
    kScoreBid,
    kReportResult,
  };
  for (CrashPhase crash_phase :
       {CrashPhase::kLoad, CrashPhase::kScoreBid, CrashPhase::kReportResult}) {
    SCOPED_TRACE(static_cast<int>(crash_phase));

    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // While loop to allow breaking when the crash stage is reached.
    while (true) {
      if (crash_phase == CrashPhase::kLoad) {
        seller_worklet->set_expect_send_pending_signals_requests_called(false);
        seller_worklet.reset();
        break;
      }

      // Generate both bids, wait for seller to receive them..
      bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                                 GURL("https://ad1.com/"));
      bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                                 GURL("https://ad2.com/"));
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      auto score_ad_params2 = seller_worklet->WaitForScoreAd();

      // Wait for SendPendingSignalsRequests() invocation.
      task_environment_.RunUntilIdle();

      if (crash_phase == CrashPhase::kScoreBid) {
        seller_worklet.reset();
        break;
      }
      // Score Bidder1's bid.
      bidder1_worklet.reset();
      EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
      EXPECT_EQ(5, score_ad_params.bid);
      std::move(score_ad_params.callback)
          .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
               /*debug_loss_report_url=*/absl::nullopt,
               /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

      // Score Bidder2's bid.
      EXPECT_EQ(kBidder2, score_ad_params2.interest_group_owner);
      EXPECT_EQ(7, score_ad_params2.bid);
      std::move(score_ad_params2.callback)
          .Run(/*score=*/11, /*data_version=*/0, /*has_data_version=*/false,
               /*debug_loss_report_url=*/absl::nullopt,
               /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

      seller_worklet->WaitForReportResult();
      DCHECK_EQ(CrashPhase::kReportResult, crash_phase);
      seller_worklet.reset();
      break;
    }

    // Wait for auction to complete.
    auction_run_loop_->Run();

    // No bidder won, seller crashed.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.ad_component_urls);
    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    if (crash_phase != CrashPhase::kReportResult) {
      EXPECT_EQ(5, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(5, result_.bidder2_bid_count);
      EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    } else {
      // If the seller worklet crashes while calculating the report URL, still
      // report bids.
      EXPECT_EQ(6, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(6, result_.bidder2_bid_count);
      EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    }
    CheckHistograms(AuctionRunner::AuctionResult::kSellerWorkletCrashed,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2);
    EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                    "%s crashed.", kSellerUrl.spec().c_str())));
  }
}

TEST_F(AuctionRunnerTest, ComponentAuctionAllBiddersCrashBeforeBidding) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2);
  StartStandardAuctionWithMockService();

  EXPECT_FALSE(auction_complete_);

  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  bidder1_worklet.reset();

  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);
  bidder2_worklet.reset();

  auction_run_loop_->Run();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_THAT(
      result_.errors,
      testing::UnorderedElementsAre(
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder1Url.spec().c_str()),
          base::StringPrintf("%s crashed while trying to run generateBid().",
                             kBidder2Url.spec().c_str())));

  CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test the case that one component has both bidders, one of which crashes, to
// make sure a single bidder crash doesn't result in the component auction
// failing.
TEST_F(AuctionRunnerTest, ComponentAuctionOneBidderCrashesBeforeBidding) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1);
  StartStandardAuctionWithMockService();

  EXPECT_FALSE(auction_complete_);

  // Close the first bidder worklet's pipe, simulating a crash.
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  bidder1_worklet.reset();
  // Wait for the AuctionRunner to observe the crash.
  task_environment_.RunUntilIdle();

  // The second bidder worklet bids.
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);
  bidder2_worklet->InvokeGenerateBidCallback(2, GURL("https://ad2.com/"));

  // Component worklet scores the bid.
  auto component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  ASSERT_TRUE(component_seller_worklet);
  auto score_ad_params = component_seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(2, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/3, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt,
           /*errors=*/{});

  // Top-level seller worklet scores the bid.
  auto top_level_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kSellerUrl);
  ASSERT_TRUE(top_level_seller_worklet);
  score_ad_params = top_level_seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(2, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/4, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  // Top-level seller worklet returns a report url.
  top_level_seller_worklet->WaitForReportResult();
  top_level_seller_worklet->InvokeReportResultCallback(
      GURL("https://report1.test/"));

  // The component seller worklet should be reloaded and ReportResult() invoked.
  mock_auction_process_manager_->WaitForWinningSellerReload();
  component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  component_seller_worklet->set_expect_send_pending_signals_requests_called(
      false);
  ASSERT_TRUE(component_seller_worklet);
  component_seller_worklet->WaitForReportResult();
  component_seller_worklet->InvokeReportResultCallback(
      GURL("https://report2.test/"));

  // Bidder worklet 2 should be reloaded and ReportWin() invoked.
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback(GURL("https://report3.test/"));

  // Bidder2 won, Bidder1 crashed.
  auction_run_loop_->Run();
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_THAT(result_.report_urls,
              testing::UnorderedElementsAre(GURL("https://report1.test/"),
                                            GURL("https://report2.test/"),
                                            GURL("https://report3.test/")));
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(6, result_.bidder2_bid_count);
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(base::StringPrintf(
                  "%s crashed while trying to run generateBid().",
                  kBidder1Url.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test the three case where a component seller worklet fails during
// ReportResult:
//
// * Crash
// * Load failure
// * Error running the script.
//
// Only the first case should fail the auction.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellersReportResultFails) {
  interest_group_buyers_.emplace();
  // It's simpler to start a two bidder auction and throw away one of the
  // bidders rather than start a one-bidder auction.
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller1);

  enum class TestCase { kCrash, kLoadError, kScriptError };

  // When false, simulates a seller workloet load failure instead.
  for (auto test_case :
       {TestCase::kCrash, TestCase::kLoadError, TestCase::kScriptError}) {
    SCOPED_TRACE(static_cast<int>(test_case));

    StartStandardAuctionWithMockService();

    EXPECT_FALSE(auction_complete_);

    // Bidder worklet 1 bids.
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    bidder1_worklet->InvokeGenerateBidCallback(2, GURL("https://ad1.com/"));

    // Bidder worklet 2 makes no bid.
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);
    bidder2_worklet->InvokeGenerateBidCallback(absl::nullopt);

    // Component worklet scores the bid.
    auto component_seller_worklet =
        mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
    ASSERT_TRUE(component_seller_worklet);
    auto score_ad_params = component_seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(2, score_ad_params.bid);
    std::move(score_ad_params.callback)
        .Run(/*score=*/3, /*data_version=*/0, /*has_data_version=*/false,
             /*debug_loss_report_url=*/absl::nullopt,
             /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

    // Top-level seller worklet scores the bid.
    auto top_level_seller_worklet =
        mock_auction_process_manager_->TakeSellerWorklet(kSellerUrl);
    ASSERT_TRUE(top_level_seller_worklet);
    score_ad_params = top_level_seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(2, score_ad_params.bid);
    std::move(score_ad_params.callback)
        .Run(/*score=*/4, /*data_version=*/0, /*has_data_version=*/false,
             /*debug_loss_report_url=*/absl::nullopt,
             /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

    // Top-level seller worklet returns a report url.
    top_level_seller_worklet->WaitForReportResult();
    top_level_seller_worklet->InvokeReportResultCallback(
        GURL("https://report1.test/"));

    // The component seller worklet should be reloaded and ReportResult()
    // invoked.
    mock_auction_process_manager_->WaitForWinningSellerReload();
    component_seller_worklet =
        mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
    component_seller_worklet->set_expect_send_pending_signals_requests_called(
        false);
    ASSERT_TRUE(component_seller_worklet);
    component_seller_worklet->WaitForReportResult();
    if (test_case == TestCase::kCrash) {
      // A crash in the winning component seller worklet will cause the auction
      // to immediately abort at this phase of the auction.
      component_seller_worklet.reset();
      auction_run_loop_->Run();

      EXPECT_FALSE(result_.ad_url);
      EXPECT_EQ(6, result_.bidder1_bid_count);
      EXPECT_THAT(result_.errors,
                  testing::UnorderedElementsAre(base::StringPrintf(
                      "%s crashed while trying to run reportResult().",
                      kComponentSeller1Url.spec().c_str())));
      CheckHistograms(
          AuctionRunner::AuctionResult::kWinningComponentSellerWorkletCrashed,
          /*expected_interest_groups=*/2, /*expected_owners=*/2);
    } else if (test_case == TestCase::kLoadError) {
      // A load error in the winning component seller worklet will cause the
      // auction to continue to completion.
      component_seller_worklet->ResetReceiverWithReason(
          "Component seller reset reason");

      // Winning bidder worklet should be reloaded and ReportWin() invoked.
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder1_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      bidder1_worklet->WaitForReportWin();
      bidder1_worklet->InvokeReportWinCallback(GURL("https://report3.test/"));

      // Auction completes.
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(GURL("https://report1.test/"),
                                                GURL("https://report3.test/")));
      EXPECT_EQ(6, result_.bidder1_bid_count);
      EXPECT_THAT(result_.errors, testing::UnorderedElementsAre(
                                      "Component seller reset reason"));
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/2, /*expected_owners=*/2);
    } else if (test_case == TestCase::kScriptError) {
      // A script error in the winning component seller worklet will cause the
      // auction to continue to completion.
      const char kScriptError[] = "Script error";

      component_seller_worklet->InvokeReportResultCallback(
          /*report_url=*/absl::nullopt, {kScriptError});

      // Winning bidder worklet should be reloaded and ReportWin() invoked.
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder1_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      bidder1_worklet->WaitForReportWin();
      bidder1_worklet->InvokeReportWinCallback(GURL("https://report3.test/"));

      // Auction completes.
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_THAT(result_.report_urls,
                  testing::UnorderedElementsAre(GURL("https://report1.test/"),
                                                GURL("https://report3.test/")));
      EXPECT_EQ(6, result_.bidder1_bid_count);
      EXPECT_THAT(result_.errors, testing::UnorderedElementsAre(kScriptError));
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/2, /*expected_owners=*/2);
    }
  }
}

// Test the case that all component sellers crash.
TEST_F(AuctionRunnerTest, ComponentAuctionComponentSellersAllCrash) {
  SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                    /*bidder2_seller=*/kComponentSeller2);
  StartStandardAuctionWithMockService();

  EXPECT_FALSE(auction_complete_);

  // First component seller worklet crashes. Auction should not complete.
  auto component_seller_worklet1 =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  component_seller_worklet1->set_expect_send_pending_signals_requests_called(
      false);
  component_seller_worklet1.reset();
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(auction_complete_);

  // Second component seller worklet crashes. Auction should complete.
  auto component_seller_worklet2 =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller2Url);
  component_seller_worklet2->set_expect_send_pending_signals_requests_called(
      false);
  component_seller_worklet2.reset();
  auction_run_loop_->Run();

  EXPECT_FALSE(result_.ad_url);
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_THAT(result_.errors,
              testing::UnorderedElementsAre(
                  base::StringPrintf("%s crashed.",
                                     kComponentSeller1Url.spec().c_str()),
                  base::StringPrintf("%s crashed.",
                                     kComponentSeller2Url.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

TEST_F(AuctionRunnerTest, NullAdComponents) {
  const GURL kRenderUrl = GURL("https://ad1.com");
  const struct {
    absl::optional<std::vector<GURL>> bid_ad_component_urls;
    bool expect_successful_bid;
  } kTestCases[] = {
      {absl::nullopt, true},
      {std::vector<GURL>{}, false},
      {std::vector<GURL>{GURL("https://ad1.com-component1.com")}, false},
  };

  for (const auto& test_case : kTestCases) {
    UseMockWorkletService();
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(
        MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                          kBidder1TrustedSignalsUrl, {"k1", "k2"}, kRenderUrl,
                          /*ad_component_urls=*/absl::nullopt));

    StartAuction(kSellerUrl, std::move(bidders));

    mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder_worklet);

    bidder_worklet->InvokeGenerateBidCallback(
        /*bid=*/1, kRenderUrl, test_case.bid_ad_component_urls,
        base::TimeDelta());

    if (test_case.expect_successful_bid) {
      // Since the bid was valid, it should be scored.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
      EXPECT_EQ(1, score_ad_params.bid);
      std::move(score_ad_params.callback)
          .Run(/*score=*/11, /*data_version=*/0, /*has_data_version=*/false,
               /*debug_loss_report_url=*/absl::nullopt,
               /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      bidder_worklet->WaitForReportWin();
      bidder_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      // The bidder should win the auction.
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_EQ(6, result_.bidder1_bid_count);
      ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.bidder1_prev_wins[3]->ad_json);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/1,
                      /*expected_owners=*/1);
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Unexpected non-null ad component list", TakeBadMessage());

      // No bidder won.
      EXPECT_FALSE(result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_EQ(5, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                      /*expected_interest_groups=*/1, /*expected_owners=*/1);
    }
  }
}

// Test that the limit of kMaxAdComponents ad components per bid is enforced.
TEST_F(AuctionRunnerTest, AdComponentsLimit) {
  const GURL kRenderUrl = GURL("https://ad1.com");

  for (size_t num_components = 1;
       num_components < blink::kMaxAdAuctionAdComponents + 2;
       num_components++) {
    std::vector<GURL> ad_component_urls;
    for (size_t i = 0; i < num_components; ++i) {
      ad_component_urls.emplace_back(
          GURL(base::StringPrintf("https://%zu.com", i)));
    }
    UseMockWorkletService();
    std::vector<StorageInterestGroup> bidders;
    bidders.emplace_back(MakeInterestGroup(
        kBidder1, kBidder1Name, kBidder1Url, kBidder1TrustedSignalsUrl,
        {"k1", "k2"}, kRenderUrl, ad_component_urls));

    StartAuction(kSellerUrl, std::move(bidders));

    mock_auction_process_manager_->WaitForWorklets(/*num_bidders=*/1);

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder_worklet);

    bidder_worklet->InvokeGenerateBidCallback(
        /*bid=*/1, kRenderUrl, ad_component_urls, base::TimeDelta());

    if (num_components <= blink::kMaxAdAuctionAdComponents) {
      // Since the bid was valid, it should be scored.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
      EXPECT_EQ(1, score_ad_params.bid);
      std::move(score_ad_params.callback)
          .Run(/*score=*/11, /*data_version=*/0, /*has_data_version=*/false,
               /*debug_loss_report_url=*/absl::nullopt,
               /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      mock_auction_process_manager_->WaitForWinningBidderReload();
      bidder_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      bidder_worklet->WaitForReportWin();
      bidder_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      // The bidder should win the auction.
      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_EQ(ad_component_urls, result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_EQ(6, result_.bidder1_bid_count);
      ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.bidder1_prev_wins[3]->ad_json);
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                      /*expected_interest_groups=*/1,
                      /*expected_owners=*/1);
    } else {
      // Since there's no acceptable bid, the seller worklet is never asked to
      // score a bid.
      auction_run_loop_->Run();

      EXPECT_EQ("Too many ad component URLs", TakeBadMessage());

      // No bidder won.
      EXPECT_FALSE(result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
      EXPECT_EQ(5, result_.bidder1_bid_count);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      EXPECT_THAT(result_.errors, testing::ElementsAre());
      CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                      /*expected_interest_groups=*/1, /*expected_owners=*/1);
    }
  }
}

// Test cases where a bad bid is received over Mojo. Bad bids should be rejected
// in the Mojo process, so these are treated as security errors.
TEST_F(AuctionRunnerTest, BadBid) {
  const struct TestCase {
    const char* expected_error_message;
    double bid;
    GURL render_url;
    absl::optional<std::vector<GURL>> ad_component_urls;
    base::TimeDelta duration;
  } kTestCases[] = {
      // Bids that aren't positive integers.
      {
          "Invalid bid value",
          -10,
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          0,
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::infinity(),
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::quiet_NaN(),
          GURL("https://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid render URL.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL(":"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Non-HTTPS render URLs.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("data:,foo"),
          absl::nullopt,
          base::TimeDelta(),
      },
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("http://ad1.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // HTTPS render URL that's not in the list of allowed renderUrls.
      {
          "Bid render URL must be a valid ad URL",
          1,
          GURL("https://ad2.com"),
          absl::nullopt,
          base::TimeDelta(),
      },

      // Invalid component URL.
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL(":")},
          base::TimeDelta(),
      },

      // HTTPS component URL that's not in the list of allowed ad component
      // URLs.
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL("https://ad2.com-component1.com")},
          base::TimeDelta(),
      },
      {
          "Bid ad components URL must match a valid ad component URL",
          1,
          GURL("https://ad1.com"),
          std::vector<GURL>{GURL("https://ad1.com-component1.com"),
                            GURL("https://ad2.com-component1.com")},
          base::TimeDelta(),
      },

      // Negative time.
      {
          "Invalid bid duration",
          1,
          GURL("https://ad2.com"),
          absl::nullopt,
          base::Milliseconds(-1),
      },
  };

  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    bidder1_worklet->InvokeGenerateBidCallback(
        test_case.bid, test_case.render_url, test_case.ad_component_urls,
        test_case.duration);
    // Bidder 2 doesn't bid.
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    // Since there's no acceptable bid, the seller worklet is never asked to
    // score a bid.
    auction_run_loop_->Run();

    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.ad_component_urls);
    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_EQ(5, result_.bidder1_bid_count);
    EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
    EXPECT_EQ(5, result_.bidder2_bid_count);
    EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2);
  }
}

// Test cases where bad a report URL is received over Mojo from the seller
// worklet. Bad report URLs should be rejected in the Mojo process, so these are
// treated as security errors.
TEST_F(AuctionRunnerTest, BadSellerReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  // Bidder1 never gets to report anything, since the seller providing a bad
  // report URL aborts the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(GURL("http://not.https.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  // No bidder won.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kBadMojoMessage,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test cases where bad a report URL is received over Mojo from the winning
// component seller worklet. Bad report URLs should be rejected in the Mojo
// process, so these are treated as security errors.
TEST_F(AuctionRunnerTest, BadComponentSellerReportUrl) {
  this->SetUpComponentAuctionAndResponses(/*bidder1_seller=*/kComponentSeller1,
                                          /*bidder2_seller=*/kComponentSeller1);
  StartStandardAuctionWithMockService();

  auto component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  ASSERT_TRUE(component_seller_worklet);
  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  // Component seller scores the bid.
  auto score_ad_params = component_seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  // Top-level seller scores the bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  // Top-level seller worklet returns a valid HTTPS report URL.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(GURL("https://ok.test/"));

  mock_auction_process_manager_->WaitForWinningSellerReload();
  component_seller_worklet =
      mock_auction_process_manager_->TakeSellerWorklet(kComponentSeller1Url);
  component_seller_worklet->set_expect_send_pending_signals_requests_called(
      false);
  component_seller_worklet->WaitForReportResult();
  component_seller_worklet->InvokeReportResultCallback(GURL("Invalid URL"));

  // Bidder1 never gets to report anything, since the seller providing a bad
  // report URL aborts the auction.
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  // No bidder won.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kBadMojoMessage,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Test cases where bad a report URL is received over Mojo from the bidder
// worklet. Bad report URLs should be rejected in the Mojo process, so these are
// treated as security errors.
TEST_F(AuctionRunnerTest, BadBidderReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                             GURL("https://ad1.com/"));
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://valid.url.that.is.thrown.out.test/"));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("http://not.https.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid bidder report URL", TakeBadMessage());

  // No bidder won.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kBadMojoMessage,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Check that BidderWorklets that don't make a bid are destroyed immediately.
TEST_F(AuctionRunnerTest, DestroyBidderWorkletWithoutBid) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);
  // Need to flush the service pipe to make sure the AuctionRunner has received
  // the bid.
  mock_auction_process_manager_->Flush();
  // The AuctionRunner should have closed the pipe.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());

  // Bidder2 returns a bid, which is then scored.
  bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/7,
                                             GURL("https://ad2.com/"));
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
  EXPECT_EQ(7, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/11, /*data_version=*/0, /*has_data_version=*/false,
           /*debug_loss_report_url=*/absl::nullopt,
           /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder2 won.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_FALSE(result_.ad_component_urls);
  EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  /*expected_interest_groups=*/2, /*expected_owners=*/2);
}

// Check that the winner of ties is randomized. Mock out bidders so can make
// sure that which bidder wins isn't changed just due to script execution order
// changing.
TEST_F(AuctionRunnerTest, Tie) {
  bool seen_bidder1_win = false;
  bool seen_bidder2_win = false;

  while (!seen_bidder1_win || !seen_bidder2_win) {
    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // Bidder1 returns a bid, which is then scored.
    bidder1_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                               GURL("https://ad1.com/"));
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(5, score_ad_params.bid);
    std::move(score_ad_params.callback)
        .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
             /*debug_loss_report_url=*/absl::nullopt,
             /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});

    // Bidder2 returns a bid, which is then scored.
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/5,
                                               GURL("https://ad2.com/"));
    score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder2, score_ad_params.interest_group_owner);
    EXPECT_EQ(5, score_ad_params.bid);
    std::move(score_ad_params.callback)
        .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
             /*debug_loss_report_url=*/absl::nullopt,
             /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});
    // Need to flush the service pipe to make sure the AuctionRunner has
    // received the score.
    seller_worklet->Flush();

    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();

    // Wait for a worklet to be reloaded, and try to get worklets for both
    // InterestGroups - only the InterestGroup that was picked as the winner
    // will be non-null.
    mock_auction_process_manager_->WaitForWinningBidderReload();
    bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);

    if (bidder1_worklet) {
      seen_bidder1_win = true;
      bidder1_worklet->WaitForReportWin();
      bidder1_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.bidder1_prev_wins[3]->ad_json);
    } else {
      seen_bidder2_win = true;

      ASSERT_TRUE(bidder2_worklet);
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_FALSE(result_.ad_component_urls);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.bidder2_prev_wins[3]->ad_json);
    }

    EXPECT_THAT(result_.report_urls, testing::UnorderedElementsAre());
    EXPECT_EQ(6, result_.bidder1_bid_count);
    EXPECT_EQ(6, result_.bidder2_bid_count);
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                    /*expected_interest_groups=*/2, /*expected_owners=*/2);
  }
}

// Test worklets completing in an order different from the one in which they're
// invoked.
TEST_F(AuctionRunnerTest, WorkletOrder) {
  // Events that can ordered differently for each loop iteration. All events
  // must happen, and a bid must be generated before it is scored.
  enum class Event {
    kBid1Generated,
    kBid2Generated,
    kBid1Scored,
    kBid2Scored,
  };

  // All possible orderings. This test assumes the order bidders are loaded in
  // is deterministic, which currently is the case (though that may change down
  // the line).
  const Event kTestCases[][4] = {
      {Event::kBid1Generated, Event::kBid1Scored, Event::kBid2Generated,
       Event::kBid2Scored},
      {Event::kBid1Generated, Event::kBid2Generated, Event::kBid1Scored,
       Event::kBid2Scored},
      {Event::kBid1Generated, Event::kBid2Generated, Event::kBid2Scored,
       Event::kBid1Scored},
      {Event::kBid2Generated, Event::kBid2Scored, Event::kBid1Generated,
       Event::kBid1Scored},
      {Event::kBid2Generated, Event::kBid1Generated, Event::kBid2Scored,
       Event::kBid1Scored},
      {Event::kBid2Generated, Event::kBid1Generated, Event::kBid1Scored,
       Event::kBid2Scored},
  };

  for (const auto& test_case : kTestCases) {
    for (bool bidder1_wins : {false, true}) {
      StartStandardAuctionWithMockService();

      auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
      ASSERT_TRUE(seller_worklet);
      auto bidder1_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
      ASSERT_TRUE(bidder1_worklet);
      auto bidder2_worklet =
          mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
      ASSERT_TRUE(bidder2_worklet);

      MockSellerWorklet::ScoreAdParams score_ad_params1;
      MockSellerWorklet::ScoreAdParams score_ad_params2;

      for (Event event : test_case) {
        switch (event) {
          case Event::kBid1Generated:
            bidder1_worklet->InvokeGenerateBidCallback(
                /*bid=*/9, GURL("https://ad1.com/"));
            score_ad_params1 = seller_worklet->WaitForScoreAd();
            EXPECT_EQ(kBidder1, score_ad_params1.interest_group_owner);
            EXPECT_EQ(9, score_ad_params1.bid);
            break;
          case Event::kBid2Generated:
            bidder2_worklet->InvokeGenerateBidCallback(
                /*bid=*/10, GURL("https://ad2.com/"));
            score_ad_params2 = seller_worklet->WaitForScoreAd();
            EXPECT_EQ(kBidder2, score_ad_params2.interest_group_owner);
            EXPECT_EQ(10, score_ad_params2.bid);
            break;
          case Event::kBid1Scored:
            std::move(score_ad_params1.callback)
                .Run(/*score=*/bidder1_wins ? 11 : 9,
                     /*data_version=*/0, /*has_data_version=*/false,
                     /*debug_loss_report_url=*/absl::nullopt,
                     /*debug_win_report_url=*/absl::nullopt, /*errors=*/{});
            // Wait for the AuctionRunner to receive the score.
            task_environment_.RunUntilIdle();
            break;
          case Event::kBid2Scored:
            std::move(score_ad_params2.callback)
                .Run(/*score=*/10, /*data_version=*/0,
                     /*has_data_version=*/false,
                     /*debug_loss_report_url=*/absl::nullopt,
                     /*debug_win_report_url=*/absl::nullopt,
                     /*errors=*/{});
            // Wait for the AuctionRunner to receive the score.
            task_environment_.RunUntilIdle();
            break;
        }
      }

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();

      mock_auction_process_manager_->WaitForWinningBidderReload();
      auto winning_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          bidder1_wins ? kBidder1Url : kBidder2Url);
      winning_worklet->WaitForReportWin();
      winning_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();
      EXPECT_THAT(result_.errors, testing::ElementsAre());

      if (bidder1_wins) {
        EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
        EXPECT_EQ(4u, result_.bidder1_prev_wins.size());
        ASSERT_EQ(3u, result_.bidder2_prev_wins.size());
      } else {
        EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
        EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
        ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
      }
    }
  }
}

// Enable and test forDebuggingOnly.reportAdAuctionLoss() and
// forDebuggingOnly.reportAdAuctionWin() APIs.
class AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest
    : public AuctionRunnerTest {
 public:
  AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest() {
    feature_list_.InitAndEnableFeature(
        blink::features::kBiddingAndScoringDebugReportingAPI);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReporting) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(GURL("https://adstuff.publisher1.com/auction.js"),
                        "https://seller-debug-loss-reporting.com/",
                        "https://seller-debug-win-reporting.com/"));

  const Result& res = RunStandardAuction();
  // Bidder 2 won the auction.
  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);

  EXPECT_EQ(2u, res.debug_loss_report_urls.size());
  EXPECT_THAT(res.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  GURL(kBidder1DebugLossReportUrl),
                  GURL("https://seller-debug-loss-reporting.com/1")));

  EXPECT_EQ(2u, res.debug_win_report_urls.size());
  EXPECT_THAT(res.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  GURL(kBidder2DebugWinReportUrl),
                  GURL("https://seller-debug-win-reporting.com/2")));
}

// Should send loss report to seller and bidders when auction fails due to
// AllBidsRejected.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingAuctionFailAllBidsRejected) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScriptReject1And2WithDebugReporting(
          "https://seller-debug-loss-reporting.com/",
          "https://seller-debug-win-reporting.com/"));

  const Result& res = RunStandardAuction();
  // No winner since both bidders are rejected by seller.
  EXPECT_FALSE(res.ad_url);

  EXPECT_EQ(4u, res.debug_loss_report_urls.size());
  EXPECT_THAT(
      res.debug_loss_report_urls,
      testing::UnorderedElementsAre(
          GURL(kBidder1DebugLossReportUrl), GURL(kBidder2DebugLossReportUrl),
          GURL("https://seller-debug-loss-reporting.com/1"),
          GURL("https://seller-debug-loss-reporting.com/2")));

  EXPECT_EQ(0u, res.debug_win_report_urls.size());
}

// Test win/loss reporting in a component auction with two components with one
// bidder each.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingComponentAuctionTwoComponents) {
  interest_group_buyers_.emplace();

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      MakeDecisionScript(
          kComponentSeller1Url,
          /*send_report_url=*/GURL("https://component1-report.test/"),
          /*debug_loss_report_url=*/"https://component1-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component1-win-reporting.test/"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kComponentSeller1, "1", "https://ad1.com/",
                    /*num_ad_components=*/2, kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a", kBidder1DebugLossReportUrl,
                    kBidder1DebugWinReportUrl));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller2Url,
      MakeDecisionScript(
          kComponentSeller2Url,
          /*send_report_url=*/GURL("https://component2-report.test/"),
          /*debug_loss_report_url=*/"https://component2-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component2-win-reporting.test/"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller2, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b", kBidder2DebugLossReportUrl,
                    kBidder2DebugWinReportUrl));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(
          kSellerUrl,
          /*debug_loss_report_url=*/"https://top-seller-loss-reporting.test/",
          /*debug_win_report_url=*/"https://top-seller-win-reporting.test/"));

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 2 won the auction.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);

  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  GURL(kBidder1DebugLossReportUrl),
                  GURL("https://component1-loss-reporting.test/1"),
                  GURL("https://top-seller-loss-reporting.test/1")));

  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  GURL(kBidder2DebugWinReportUrl),
                  GURL("https://component2-win-reporting.test/2"),
                  GURL("https://top-seller-win-reporting.test/2")));
}

// Test win/loss reporting in a component auction with one component with two
// bidders.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingComponentAuctionOneComponent) {
  interest_group_buyers_.emplace();

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1, kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      MakeDecisionScript(
          kComponentSeller1Url,
          /*send_report_url=*/GURL("https://component-report.test/"),
          /*debug_loss_report_url=*/"https://component-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component-win-reporting.test/"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kComponentSeller1, "1", "https://ad1.com/",
                    /*num_ad_components=*/2, kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a", kBidder1DebugLossReportUrl,
                    kBidder1DebugWinReportUrl));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller1, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b", kBidder2DebugLossReportUrl,
                    kBidder2DebugWinReportUrl));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kSellerUrl,
      MakeAuctionScript(
          kSellerUrl,
          /*debug_loss_report_url=*/"https://top-seller-loss-reporting.test/",
          /*debug_win_report_url=*/"https://top-seller-win-reporting.test/"));

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 1 won the auction, since component auctions give lower bidders
  // higher desireability scores.
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);

  // Top seller doesn't report a loss, since it never saw the bid from the
  // second bidder.
  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  GURL(kBidder2DebugLossReportUrl),
                  GURL("https://component-loss-reporting.test/2")));

  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  GURL(kBidder1DebugWinReportUrl),
                  GURL("https://component-win-reporting.test/1"),
                  GURL("https://top-seller-win-reporting.test/1")));
}

// Like above test, but top-level seller rejects all bidders.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingComponentAuctionNoWinner) {
  interest_group_buyers_.emplace();

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller1Url, {{kBidder1}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller1Url,
      MakeDecisionScript(
          kComponentSeller1Url,
          /*send_report_url=*/GURL("https://component1-report.test/"),
          /*debug_loss_report_url=*/"https://component1-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component1-win-reporting.test/"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kComponentSeller1, "1", "https://ad1.com/",
                    /*num_ad_components=*/2, kBidder1, kBidder1Name,
                    /*has_signals=*/true, "k1", "a", kBidder1DebugLossReportUrl,
                    kBidder1DebugWinReportUrl));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");

  component_auctions_.emplace_back(
      CreateAuctionConfig(kComponentSeller2Url, {{kBidder2}}));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kComponentSeller2Url,
      MakeDecisionScript(
          kComponentSeller2Url,
          /*send_report_url=*/GURL("https://component2-report.test/"),
          /*debug_loss_report_url=*/"https://component2-loss-reporting.test/",
          /*debug_win_report_url=*/"https://component2-win-reporting.test/"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kComponentSeller2, "2", "https://ad2.com/",
                    /*num_ad_components=*/2, kBidder2, kBidder2Name,
                    /*has_signals=*/true, "l2", "b", kBidder2DebugLossReportUrl,
                    kBidder2DebugWinReportUrl));
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder2TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=l1,l2"),
                                   R"({"l1":"a", "l2": "b", "extra": "c"})");

  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         R"(
function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                 browserSignals) {
  forDebuggingOnly.reportAdAuctionLoss(
      "https://top-seller-loss-reporting.test/" + bid);
  forDebuggingOnly.reportAdAuctionWin(
      "https://top-seller-win-reporting.test/" + bid);
  return 0;
}
  )");

  RunStandardAuction();
  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // No interest group won the auction.
  EXPECT_FALSE(result_.ad_url);

  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  GURL(kBidder1DebugLossReportUrl),
                  GURL("https://component1-loss-reporting.test/1"),
                  GURL("https://top-seller-loss-reporting.test/1"),
                  GURL(kBidder2DebugLossReportUrl),
                  GURL("https://component2-loss-reporting.test/2"),
                  GURL("https://top-seller-loss-reporting.test/2")));

  EXPECT_THAT(result_.debug_win_report_urls, testing::UnorderedElementsAre());
}

// Loss report URLs should be dropped when the seller worklet fails to load.
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingSellerWorkletFailToLoad) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript(kSeller, "1", "https://ad1.com/", /*num_ad_components=*/2,
                    kBidder1, kBidder1Name,
                    /*has_signals=*/false, "k1", "a",
                    kBidder1DebugLossReportUrl, kBidder1DebugWinReportUrl));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript(kSeller, "2", "https://ad2.com/", /*num_ad_components=*/2,
                    kBidder2, kBidder2Name,
                    /*has_signals=*/false, "l2", "b",
                    kBidder2DebugLossReportUrl, kBidder2DebugWinReportUrl));

  StartStandardAuction();
  // Wait for the bids to be generated.
  task_environment_.RunUntilIdle();
  // The seller script fails to load.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  // Wait for the auction to complete.
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors,
              testing::ElementsAre(
                  "Failed to load https://adstuff.publisher1.com/auction.js "
                  "HTTP status = 404 Not Found."));

  // There should be no debug win report URLs.
  EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  // Bidders' debug loss report URLs should be dropped as well.
  EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
}

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingBidderBadUrls) {
  const struct TestCase {
    const char* expected_error_message;
    absl::optional<GURL> bidder_debug_loss_report_url;
    absl::optional<GURL> bidder_debug_win_report_url;
  } kTestCases[] = {
      {
          "Invalid bidder debugging loss report URL",
          GURL("http://bidder-debug-loss-report.com/"),
          GURL("http://bidder-debug-win-report.com/"),
      },
      {
          "Invalid bidder debugging win report URL",
          GURL("https://bidder-debug-loss-report.com/"),
          GURL("http://bidder-debug-win-report.com/"),
      },
      {
          "Invalid bidder debugging loss report URL",
          GURL("file:///foo/"),
          GURL("https://bidder-debug-win-report.com/"),
      },
      {
          "Invalid bidder debugging loss report URL",
          GURL("Not a URL"),
          GURL("https://bidder-debug-win-report.com/"),
      },
  };
  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // Only Bidder1 bids, to keep things simple.
    bidder1_worklet->InvokeGenerateBidCallback(
        /*bid=*/5, GURL("https://ad1.com/"),
        /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
        /*bidding_signals_data_version=*/absl::nullopt,
        test_case.bidder_debug_loss_report_url,
        test_case.bidder_debug_win_report_url);
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    // Since there's no acceptable bid, the seller worklet is never asked to
    // score a bid.
    auction_run_loop_->Run();
    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_EQ(5, result_.bidder1_bid_count);
    EXPECT_EQ(3u, result_.bidder1_prev_wins.size());

    EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  }
}

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingSellerBadUrls) {
  const struct TestCase {
    const char* expected_error_message;
    absl::optional<GURL> seller_debug_loss_report_url;
    absl::optional<GURL> seller_debug_win_report_url;
  } kTestCases[] = {
      {
          "Invalid seller debugging loss report URL",
          GURL("http://seller-debug-loss-report.com/"),
          GURL("http://seller-debug-win-report.com/"),
      },
      {
          "Invalid seller debugging win report URL",
          GURL("https://seller-debug-loss-report.com/"),
          GURL("http://seller-debug-win-report.com/"),
      },
      {
          "Invalid seller debugging loss report URL",
          GURL("file:///foo/"),
          GURL("https://seller-debug-win-report.com/"),
      },
      {
          "Invalid seller debugging loss report URL",
          GURL("Not a URL"),
          GURL("https://seller-debug-win-report.com/"),
      },
  };
  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet =
        mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
    ASSERT_TRUE(bidder2_worklet);

    // Only Bidder1 bids, to keep things simple.
    bidder1_worklet->InvokeGenerateBidCallback(
        /*bid=*/5, GURL("https://ad1.com/"),
        /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
        /*bidding_signals_data_version=*/absl::nullopt,
        GURL("https://bidder-debug-loss-report.com/"),
        GURL("https://bidder-debug-win-report.com/"));
    bidder2_worklet->InvokeGenerateBidCallback(/*bid=*/absl::nullopt);

    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
    EXPECT_EQ(5, score_ad_params.bid);
    std::move(score_ad_params.callback)
        .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
             test_case.seller_debug_loss_report_url,
             test_case.seller_debug_win_report_url, /*errors=*/{});
    auction_run_loop_->Run();
    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_EQ(5, result_.bidder1_bid_count);
    EXPECT_EQ(3u, result_.bidder1_prev_wins.size());

    EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
    EXPECT_EQ(0u, result_.debug_win_report_urls.size());
  }
}

TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       ForDebuggingOnlyReportingGoodAndBadUrl) {
  StartStandardAuctionWithMockService();
  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2Url);
  ASSERT_TRUE(bidder2_worklet);

  // Bidder1 returns a bid, which is then scored.
  bidder1_worklet->InvokeGenerateBidCallback(
      /*bid=*/5, GURL("https://ad1.com/"),
      /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
      /*bidding_signals_data_version=*/absl::nullopt,
      GURL(kBidder1DebugLossReportUrl), GURL(kBidder1DebugWinReportUrl));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());
  bidder1_worklet.reset();
  EXPECT_EQ("", TakeBadMessage());

  // Bidder2 returns a bid with an invalid debug report url. This could only
  // happen when the bidder worklet is compromised. It will be filtered out
  // and not be scored.
  bidder2_worklet->InvokeGenerateBidCallback(
      /*bid=*/10, GURL("https://ad2.com/"),
      /*ad_component_urls=*/absl::nullopt, base::TimeDelta(),
      /*bidding_signals_data_version=*/absl::nullopt,
      GURL("http://not-https.com/"), GURL(kBidder2DebugWinReportUrl));
  // The bidder pipe should be closed after it bids.
  EXPECT_TRUE(bidder2_worklet->PipeIsClosed());
  bidder2_worklet.reset();
  EXPECT_EQ("Invalid bidder debugging loss report URL", TakeBadMessage());

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params.interest_group_owner);
  EXPECT_EQ(5, score_ad_params.bid);
  std::move(score_ad_params.callback)
      .Run(/*score=*/10, /*data_version=*/0, /*has_data_version=*/false,
           GURL("https://seller-debug-loss-reporting.com/1"),
           GURL("https://seller-debug-win-reporting.com/1"), /*errors=*/{});

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://seller.report.result/"));
  mock_auction_process_manager_->WaitForWinningBidderReload();
  bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1Url);
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("https://bidder1.report.win/"));
  auction_run_loop_->Run();

  // Bidder1 won. Bidder2 was filtered out as an invalid bid because its debug
  // loss report url is not a valid HTTPS URL.
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  ASSERT_EQ(3u, result_.bidder2_prev_wins.size());

  // Bidder2 lost, but debug_loss_report_urls is empty because bidder2's
  // `debug_loss_report_url` is not a valid HTTPS URL. There's no seller debug
  // loss report url neither because bidder2 was filtered out and its bid was
  // not scored by seller.
  EXPECT_EQ(0u, result_.debug_loss_report_urls.size());
  EXPECT_EQ(2u, result_.debug_win_report_urls.size());
  EXPECT_THAT(result_.debug_win_report_urls,
              testing::UnorderedElementsAre(
                  kBidder1DebugWinReportUrl,
                  "https://seller-debug-win-reporting.com/1"));
}

// This tests the component auction state machine in the case of a large
// component auction. It uses the debug reporting API just to make sure all
// scripts were run to completion. The main thing this test serves to do is to
// validate the component auction state machinery works (Waits for all bids to
// be generated/scored, doesn't abort them early, doesn't wait for extra bids).
TEST_F(AuctionRunnerBiddingAndScoringDebugReportingAPIEnabledTest,
       LargeComponentAuction) {
  const GURL kComponentSeller3Url{"https://component.seller3.test/baz.js"};

  // Seller URLs and number of bidders for each Auction.
  const struct {
    GURL seller_url;
    int num_bidders;
  } kSellerInfo[] = {
      {kSellerUrl, 2},
      {kComponentSeller1Url, 3},
      {kComponentSeller2Url, 5},
      {kComponentSeller3Url, 7},
  };

  // Set up auction, including bidder and seller Javascript responses,
  // AuctionConfig fields, etc.
  size_t bidder_index = 1;
  std::vector<StorageInterestGroup> all_bidders;
  for (size_t i = 0; i < std::size(kSellerInfo); ++i) {
    url::Origin seller = url::Origin::Create(kSellerInfo[i].seller_url);
    GURL send_report_url =
        GURL(base::StringPrintf("https://seller%zu.test/report/", i));
    GURL debug_loss_report_url =
        GURL(base::StringPrintf("https://seller%zu.test/loss/", i));
    GURL debug_win_report_url =
        GURL(base::StringPrintf("https://seller%zu.test/win/", i));

    auction_worklet::AddJavascriptResponse(
        &url_loader_factory_, kSellerInfo[i].seller_url,
        MakeDecisionScript(kSellerInfo[i].seller_url, send_report_url,
                           debug_loss_report_url.spec(),
                           debug_win_report_url.spec()));

    std::vector<url::Origin> bidders;
    for (int j = 0; j < kSellerInfo[i].num_bidders; ++j, ++bidder_index) {
      GURL bidder_url = GURL(
          base::StringPrintf("https://bidder%zu.test/script.js", bidder_index));
      url::Origin bidder = url::Origin::Create(bidder_url);
      GURL ad_url =
          GURL(base::StringPrintf("https://bidder%zu.ad.test/", bidder_index));
      GURL bidder_debug_loss_report_url = GURL(
          base::StringPrintf("https://bidder%zu.test/loss/", bidder_index));
      GURL bidder_debug_win_report_url =
          GURL(base::StringPrintf("https://bidder%zu.test/win/", bidder_index));

      all_bidders.emplace_back(MakeInterestGroup(
          bidder, /*name=*/base::NumberToString(bidder_index), bidder_url,
          /*trusted_bidding_signals_url=*/absl::nullopt,
          /*trusted_bidding_signals_keys=*/{}, ad_url,
          /*ad_component_urls=*/absl::nullopt));

      auction_worklet::AddJavascriptResponse(
          &url_loader_factory_, bidder_url,
          MakeBidScript(
              seller, /*bid=*/base::NumberToString(bidder_index), ad_url.spec(),
              /*num_ad_components=*/0, bidder,
              /*interest_group_name=*/base::NumberToString(bidder_index),
              /*has_signals=*/false, /*signal_key=*/"", /*signal_val=*/"",
              bidder_debug_loss_report_url.spec(),
              bidder_debug_win_report_url.spec()));

      bidders.push_back(bidder);
    }

    // For the top-most auction, only need to set `interest_group_buyers_`. For
    // others, need to append to `component_auctions_`.
    if (kSellerInfo[i].seller_url == kSellerUrl) {
      interest_group_buyers_ = std::move(bidders);
    } else {
      component_auctions_.emplace_back(
          CreateAuctionConfig(kSellerInfo[i].seller_url, std::move(bidders)));
    }
  }

  StartAuction(kSellerInfo[0].seller_url, std::move(all_bidders));
  auction_run_loop_->Run();

  EXPECT_THAT(result_.errors, testing::UnorderedElementsAre());

  // Bidder 11 won - the first bidder for the third component auction. Higher
  // bidders bid more, but component sellers use a script that favors lower
  // bidders, while the top-level seller favors higher bidders.
  EXPECT_EQ(GURL("https://bidder11.ad.test/"), result_.ad_url);

  // Top seller doesn't report a loss, since it never saw the bid from the
  // second bidder.
  EXPECT_THAT(result_.debug_loss_report_urls,
              testing::UnorderedElementsAre(
                  // kSeller's bidders.
                  GURL("https://bidder1.test/loss/"),
                  GURL("https://seller0.test/loss/1"),
                  GURL("https://bidder2.test/loss/"),
                  GURL("https://seller0.test/loss/2"),
                  // kComponentSeller1's bidders. The first makes it to the
                  // top-level auction, the others do not.
                  GURL("https://bidder3.test/loss/"),
                  GURL("https://seller1.test/loss/3"),
                  GURL("https://seller0.test/loss/3"),
                  GURL("https://bidder4.test/loss/"),
                  GURL("https://seller1.test/loss/4"),
                  GURL("https://bidder5.test/loss/"),
                  GURL("https://seller1.test/loss/5"),
                  // kComponentSeller2's bidders. The first makes it to the
                  // top-level auction, the others do not.
                  GURL("https://bidder6.test/loss/"),
                  GURL("https://seller2.test/loss/6"),
                  GURL("https://seller0.test/loss/6"),
                  GURL("https://bidder7.test/loss/"),
                  GURL("https://seller2.test/loss/7"),
                  GURL("https://bidder8.test/loss/"),
                  GURL("https://seller2.test/loss/8"),
                  GURL("https://bidder9.test/loss/"),
                  GURL("https://seller2.test/loss/9"),
                  GURL("https://bidder10.test/loss/"),
                  GURL("https://seller2.test/loss/10"),
                  // kComponentSeller3's bidders. Bidder 11 won the entire
                  // auction, all the others lose component seller 3's auction.
                  GURL("https://bidder12.test/loss/"),
                  GURL("https://seller3.test/loss/12"),
                  GURL("https://bidder13.test/loss/"),
                  GURL("https://seller3.test/loss/13"),
                  GURL("https://bidder14.test/loss/"),
                  GURL("https://seller3.test/loss/14"),
                  GURL("https://bidder15.test/loss/"),
                  GURL("https://seller3.test/loss/15"),
                  GURL("https://bidder16.test/loss/"),
                  GURL("https://seller3.test/loss/16"),
                  GURL("https://bidder17.test/loss/"),
                  GURL("https://seller3.test/loss/17")));

  EXPECT_THAT(
      result_.debug_win_report_urls,
      testing::UnorderedElementsAre(GURL("https://bidder11.test/win/"),
                                    GURL("https://seller3.test/win/11"),
                                    GURL("https://seller0.test/win/11")));
}

}  // namespace
}  // namespace content
