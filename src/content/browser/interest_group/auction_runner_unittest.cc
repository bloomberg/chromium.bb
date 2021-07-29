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
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "mojo/public/cpp/system/functions.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

std::string MakeBidScript(const std::string& bid,
                          const std::string& render_url,
                          const url::Origin& interest_group_owner,
                          const std::string& interest_group_name,
                          bool has_signals,
                          const std::string& signal_key,
                          const std::string& signal_val) {
  // TODO(morlovich): Use JsReplace.
  constexpr char kBidScript[] = R"(
    const bid = %s;
    const renderUrl = "%s";
    const interestGroupOwner = "%s";
    const interestGroupName = "%s";
    const hasSignals = %s;

    function generateBid(interestGroup, auctionSignals, perBuyerSignals,
                         trustedBiddingSignals, browserSignals) {
      var result = {ad: {bidKey: "data for " + bid,
                         groupName: interestGroupName},
                    bid: bid, render: renderUrl};
      if (interestGroup.name !== interestGroupName)
        throw new Error("wrong interestGroupName");
      if (interestGroup.owner !== interestGroupOwner)
        throw new Error("wrong interestGroupOwner");
      if (interestGroup.ads.length != 1)
        throw new Error("wrong interestGroup.ads length");
      if (interestGroup.ads[0].renderUrl != renderUrl)
        throw new Error("wrong interestGroup.ads URL");
      if (perBuyerSignals.signalsFor !== interestGroupName)
        throw new Error("wrong perBuyerSignals");
      if (!auctionSignals.isAuctionSignals)
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
      if (browserSignals.seller != 'https://adstuff.publisher1.com')
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
      return result;
    }

    function reportWin(auctionSignals, perBuyerSignals, sellerSignals,
                       browserSignals) {
      if (!auctionSignals.isAuctionSignals)
        throw new Error("wrong auctionSignals");
      if (perBuyerSignals.signalsFor !== interestGroupName)
        throw new Error("wrong perBuyerSignals");

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
      if (sellerSignals.desirability !== (bid * 2))
        throw new Error("wrong desirability");

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

      sendReportTo("https://buyer-reporting.example.com");
    }
  )";
  return base::StringPrintf(
      kBidScript, bid.c_str(), render_url.c_str(),
      interest_group_owner.Serialize().c_str(), interest_group_name.c_str(),
      has_signals ? "true" : "false", signal_key.c_str(), signal_val.c_str());
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

constexpr char kCheckingAuctionScript[] = R"(
  function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
                   browserSignals) {
    if (adMetadata.bidKey !== ("data for " + bid)) {
      throw new Error("wrong data for bid:" +
                      JSON.stringify(adMetadata) + "/" + bid);
    }
    if (auctionConfig.decisionLogicUrl
        !== "https://adstuff.publisher1.com/auction.js") {
      throw new Error("wrong auctionConfig");
    }
    if (auctionConfig.perBuyerSignals['adplatform.com'].signalsFor
        !== 'Ad Platform') {
      throw new Error("Wrong perBuyerSignals in auctionConfig");
    }
    if (!auctionConfig.sellerSignals.isSellerSignals)
      throw new Error("Wrong sellerSignals");
    if (browserSignals.topWindowHostname !== 'publisher1.com')
      throw new Error("wrong topWindowHostname");
    if ("joinCount" in browserSignals)
      throw new Error("wrong kind of browser signals");
    if (browserSignals.adRenderFingerprint !== "#####")
      throw new Error("wrong adRenderFingerprint");
    if (typeof browserSignals.biddingDurationMsec !== "number")
      throw new Error("biddingDurationMsec is not a number. huh");
    if (browserSignals.biddingDurationMsec < 0)
      throw new Error("biddingDurationMsec should be non-negative.");

    return bid * 2;
  }
)";

constexpr char kReportResultScript[] = R"(
  function reportResult(auctionConfig, browserSignals) {
    if (auctionConfig.decisionLogicUrl
        !== "https://adstuff.publisher1.com/auction.js") {
      throw new Error("wrong auctionConfig");
    }
    if (browserSignals.topWindowHostname !== 'publisher1.com')
      throw new Error("wrong topWindowHostname");
    sendReportTo("https://reporting.example.com");
    return browserSignals;
  }
)";

constexpr char kReportResultScriptNoUrl[] = R"(
  function reportResult(auctionConfig, browserSignals) {
    return browserSignals;
  }
)";

std::string MakeAuctionScript() {
  return std::string(kCheckingAuctionScript) + kReportResultScript;
}

std::string MakeAuctionScriptNoReportUrl() {
  return std::string(kCheckingAuctionScript) + kReportResultScriptNoUrl;
}

const char kAuctionScriptRejects2[] = R"(
  function scoreAd(adMetadata, bid, auctionConfig, browserSignals) {
    if (bid === 2)
      return -1;
    return bid + 1;
  }
)";

std::string MakeAuctionScriptReject2() {
  return std::string(kAuctionScriptRejects2) + kReportResultScript;
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
          pending_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      auction_worklet::mojom::AuctionWorkletService::
          LoadBidderWorkletAndGenerateBidCallback
              load_bidder_worklet_and_generate_bid_callback)
      : load_bidder_worklet_and_generate_bid_callback_(
            std::move(load_bidder_worklet_and_generate_bid_callback)),
        url_loader_factory_(std::move(pending_url_loader_factory)),
        receiver_(this, std::move(pending_receiver)) {
    receiver_.set_disconnect_handler(base::BindOnce(
        &MockBidderWorklet::OnPipeClosed, base::Unretained(this)));
  }

  MockBidderWorklet(const MockBidderWorklet&) = delete;
  const MockBidderWorklet& operator=(const MockBidderWorklet&) = delete;

  ~MockBidderWorklet() override = default;

  // auction_worklet::mojom::BidderWorklet implementation:
  void ReportWin(const std::string& seller_signals_json,
                 const GURL& browser_signal_render_url,
                 const std::string& browser_signal_ad_render_fingerprint,
                 double browser_signal_bid,
                 ReportWinCallback report_win_callback) override {
    report_win_callback_ = std::move(report_win_callback);
    if (report_win_run_loop_)
      report_win_run_loop_->Quit();
  }

  void CompleteLoadingAndBid(double bid,
                             const GURL& render_url,
                             base::TimeDelta duration = base::TimeDelta()) {
    DCHECK(load_bidder_worklet_and_generate_bid_callback_);
    std::move(load_bidder_worklet_and_generate_bid_callback_)
        .Run(auction_worklet::mojom::BidderWorkletBid::New(
                 "ad", bid, render_url, duration),
             std::vector<std::string>() /* errors */);
  }

  void CompleteLoadingWithoutBid() {
    DCHECK(load_bidder_worklet_and_generate_bid_callback_);
    std::move(load_bidder_worklet_and_generate_bid_callback_)
        .Run(nullptr /* bid */, std::vector<std::string>() /* errors */);
  }

  // Returns the LoadBidderWorkletAndGenerateBidCallback for a worklet. Needed
  // for cases when the BidderWorklet is destroyed (to close its pipe) before
  // the AuctionWorkletService is destroyed, since Mojo DCHECKs if a callback is
  // destroyed when the pipe its over is still live.
  //
  // TODO(mmenke): To better simulate real crashes, give worklets their own
  // AuctionWorkletService pipes, and remove this method.
  auction_worklet::mojom::AuctionWorkletService::
      LoadBidderWorkletAndGenerateBidCallback
      TakeLoadCallback() {
    return std::move(load_bidder_worklet_and_generate_bid_callback_);
  }

  void WaitForReportWin() {
    DCHECK(!load_bidder_worklet_and_generate_bid_callback_);
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
        .Run(report_url, std::vector<std::string>() /* errors */);
  }

  mojo::Remote<network::mojom::URLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }

  // Flush the receiver pipe and return whether or not its closed.
  bool PipeIsClosed() {
    receiver_.FlushForTesting();
    return pipe_closed_;
  }

 private:
  void OnPipeClosed() { pipe_closed_ = true; }

  auction_worklet::mojom::AuctionWorkletService::
      LoadBidderWorkletAndGenerateBidCallback
          load_bidder_worklet_and_generate_bid_callback_;

  bool pipe_closed_ = false;

  std::unique_ptr<base::RunLoop> report_win_run_loop_;
  ReportWinCallback report_win_callback_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

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
    double bid;
    url::Origin interest_group_owner;
  };

  explicit MockSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          pending_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      auction_worklet::mojom::AuctionWorkletService::LoadSellerWorkletCallback
          load_worklet_callback)
      : load_worklet_callback_(std::move(load_worklet_callback)),
        url_loader_factory_(std::move(pending_url_loader_factory)),
        receiver_(this, std::move(pending_receiver)) {}

  MockSellerWorklet(const MockSellerWorklet&) = delete;
  const MockSellerWorklet& operator=(const MockSellerWorklet&) = delete;

  ~MockSellerWorklet() override = default;

  // auction_worklet::mojom::SellerWorklet implementation:

  void ScoreAd(const std::string& ad_metadata_json,
               double bid,
               blink::mojom::AuctionAdConfigPtr auction_config,
               const url::Origin& browser_signal_top_window_origin,
               const url::Origin& browser_signal_interest_group_owner,
               const std::string& browser_signal_ad_render_fingerprint,
               uint32_t browser_signal_bidding_duration_msecs,
               ScoreAdCallback score_ad_callback) override {
    score_ad_callback_ = std::move(score_ad_callback);
    score_ad_params_ = std::make_unique<ScoreAdParams>();
    score_ad_params_->bid = bid;
    score_ad_params_->interest_group_owner =
        browser_signal_interest_group_owner;
    if (score_ad_run_loop_)
      score_ad_run_loop_->Quit();
  }

  void ReportResult(blink::mojom::AuctionAdConfigPtr auction_config,
                    const url::Origin& browser_signal_top_window_origin,
                    const url::Origin& browser_signal_interest_group_owner,
                    const GURL& browser_signal_render_url,
                    const std::string& browser_signal_ad_render_fingerprint,
                    double browser_signal_bid,
                    double browser_signal_desirability,
                    ReportResultCallback report_result_callback) override {
    report_result_callback_ = std::move(report_result_callback);
    if (report_result_run_loop_)
      report_result_run_loop_->Quit();
  }

  // Informs the consumer that the seller worklet has successfully loaded.
  void CompleteLoading() {
    DCHECK(load_worklet_callback_);
    std::move(load_worklet_callback_)
        .Run(true /* success */, std::vector<std::string>() /* errors */);
  }

  // Waits until ScoreAd() has been invoked, if it hasn't been already.
  std::unique_ptr<ScoreAdParams> WaitForScoreAd() {
    DCHECK(!score_ad_run_loop_);
    DCHECK(!load_worklet_callback_);
    if (!score_ad_params_) {
      score_ad_run_loop_ = std::make_unique<base::RunLoop>();
      score_ad_run_loop_->Run();
      score_ad_run_loop_.reset();
      DCHECK(score_ad_params_);
    }
    return std::move(score_ad_params_);
  }

  // Invokes the ScoreAdCallback for the most recent ScoreAd() call with the
  // provided score. WaitForScoreAd() must have been invoked first.
  void InvokeScoreAdCallback(double score) {
    DCHECK(score_ad_callback_);
    DCHECK(!score_ad_params_);
    std::move(score_ad_callback_)
        .Run(score, std::vector<std::string>() /* errors */);
  }

  void WaitForReportResult() {
    DCHECK(!report_result_run_loop_);
    DCHECK(!load_worklet_callback_);
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
      absl::optional<GURL> report_url = absl::nullopt) {
    DCHECK(report_result_callback_);
    std::move(report_result_callback_)
        .Run(absl::nullopt /* signals_for_winner */, std::move(report_url),
             std::vector<std::string>() /* errors */);
  }

  mojo::Remote<network::mojom::URLLoaderFactory>& url_loader_factory() {
    return url_loader_factory_;
  }

  void Flush() { receiver_.FlushForTesting(); }

 private:
  auction_worklet::mojom::AuctionWorkletService::LoadSellerWorkletCallback
      load_worklet_callback_;

  std::unique_ptr<base::RunLoop> score_ad_run_loop_;
  std::unique_ptr<ScoreAdParams> score_ad_params_;
  ScoreAdCallback score_ad_callback_;

  std::unique_ptr<base::RunLoop> report_result_run_loop_;
  ReportResultCallback report_result_callback_;

  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_;

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

    // Each receiver should get a unique display name. This check serves to help
    // ensure that processes are correctly reused.
    EXPECT_EQ(0u, receiver_display_name_map_.count(receiver_id));
    for (auto receiver : receiver_display_name_map_) {
      EXPECT_NE(receiver.second, display_name);
    }

    receiver_display_name_map_[receiver_id] = display_name;
  }

  // auction_worklet::mojom::AuctionWorkletService implementation:
  void LoadBidderWorkletAndGenerateBid(
      mojo::PendingReceiver<auction_worklet::mojom::BidderWorklet>
          bidder_worklet_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      auction_worklet::mojom::BiddingInterestGroupPtr bidding_interest_group,
      const absl::optional<std::string>& auction_signals_json,
      const absl::optional<std::string>& per_buyer_signals_json,
      const url::Origin& top_window_origin,
      const url::Origin& seller_origin,
      base::Time auction_start_time,
      LoadBidderWorkletAndGenerateBidCallback
          load_bidder_worklet_and_generate_bid_callback) override {
    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kBidder,
                                 bidding_interest_group->group->owner));

    InterestGroupId interest_group_id(bidding_interest_group->group->owner,
                                      bidding_interest_group->group->name);
    EXPECT_EQ(0u, bidder_worklets_.count(interest_group_id));
    bidder_worklets_.emplace(std::make_pair(
        interest_group_id,
        std::make_unique<MockBidderWorklet>(
            std::move(bidder_worklet_receiver),
            std::move(pending_url_loader_factory),
            std::move(load_bidder_worklet_and_generate_bid_callback))));

    ASSERT_GT(waiting_for_num_bidders_, 0);
    --waiting_for_num_bidders_;
    MaybeQuitRunLoop();
  }

  void LoadSellerWorklet(
      mojo::PendingReceiver<auction_worklet::mojom::SellerWorklet>
          seller_worklet_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          pending_url_loader_factory,
      const GURL& script_source_url,
      LoadSellerWorkletCallback load_seller_worklet_callback) override {
    DCHECK(!seller_worklet_);

    // Make sure this request came over the right pipe.
    EXPECT_EQ(receiver_display_name_map_[receiver_set_.current_receiver()],
              ComputeDisplayName(AuctionProcessManager::WorkletType::kSeller,
                                 url::Origin::Create(script_source_url)));

    seller_worklet_ = std::make_unique<MockSellerWorklet>(
        std::move(seller_worklet_receiver),
        std::move(pending_url_loader_factory),
        std::move(load_seller_worklet_callback));

    ASSERT_TRUE(waiting_on_seller_);
    waiting_on_seller_ = false;
    MaybeQuitRunLoop();
  }

  // Waits for a SellerWorklet and `num_bidders` bidder worklets to be created.
  void WaitForWorklets(int num_bidders) {
    waiting_on_seller_ = true;
    waiting_for_num_bidders_ = num_bidders;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  // Returns the MockBidderWorklet created for the specified interest group
  // origin and name, if there is one.
  std::unique_ptr<MockBidderWorklet> TakeBidderWorklet(
      const url::Origin& interest_group_owner_origin,
      const std::string& interest_group_name) {
    InterestGroupId interest_group_id(interest_group_owner_origin,
                                      interest_group_name);
    auto it = bidder_worklets_.find(interest_group_id);
    if (it == bidder_worklets_.end())
      return nullptr;
    std::unique_ptr<MockBidderWorklet> out = std::move(it->second);
    bidder_worklets_.erase(it);
    return out;
  }

  // Returns the MockSellerWorklet, if one has been created.
  std::unique_ptr<MockSellerWorklet> TakeSellerWorklet() {
    return std::move(seller_worklet_);
  }

  void Flush() { receiver_set_.FlushForTesting(); }

  // Close all the AuctionWorkletService pipes. Needs to be called before
  // uninvoked worklet callbacks can be destroyed, which is useful after
  // simulating a worklet crash.
  void ClosePipes() { receiver_set_.Clear(); }

 private:
  void MaybeQuitRunLoop() {
    if (!waiting_on_seller_ && waiting_for_num_bidders_ == 0)
      run_loop_->Quit();
  }

  // An interest group is uniquely identified by its owner's origin and name.
  using InterestGroupId = std::pair<url::Origin, std::string>;

  std::map<InterestGroupId, std::unique_ptr<MockBidderWorklet>>
      bidder_worklets_;

  std::unique_ptr<MockSellerWorklet> seller_worklet_;

  std::unique_ptr<base::RunLoop> run_loop_;
  bool waiting_on_seller_ = false;
  int waiting_for_num_bidders_ = 0;

  // Map from ReceiverSet IDs to display name when the process was launched.
  // Used to verify that worklets are created in the right process.
  std::map<mojo::ReceiverId, std::string> receiver_display_name_map_;

  // ReceiverSet is last so that destroying `this` while there's a pending
  // callback over the pipe will not DCHECK.
  mojo::ReceiverSet<auction_worklet::mojom::AuctionWorkletService>
      receiver_set_;
};

class SameThreadAuctionProcessManager : public AuctionProcessManager {
 public:
  SameThreadAuctionProcessManager() = default;
  SameThreadAuctionProcessManager(const SameThreadAuctionProcessManager&) =
      delete;
  SameThreadAuctionProcessManager& operator=(
      const SameThreadAuctionProcessManager&) = delete;
  ~SameThreadAuctionProcessManager() override = default;

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

class AuctionRunnerTest : public testing::Test, public AuctionRunner::Delegate {
 protected:
  // Output of the RunAuctionCallback passed to AuctionRunner::CreateAndStart().
  struct Result {
    Result() = default;
    // Can't use default copy logic, since it contains Mojo types.
    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

    absl::optional<GURL> ad_url;
    absl::optional<GURL> bidder_report_url;
    absl::optional<GURL> seller_report_url;
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
  }

  ~AuctionRunnerTest() override {
    // Any bad message should have been inspected and cleared before the end of
    // the test.
    EXPECT_EQ(std::string(), bad_message_);
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
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

  // Starts an auction without waiting for it to complete. Useful when using
  // MockAuctionProcessManager.
  //
  // `bidders` are added to a new InterestGroupManager before running the
  // auction. The times of their previous wins are ignored, as the
  // InterestGroupManager automatically attaches the current time, though their
  // wins will be added in order, with chronologically increasing times within
  // each InterestGroup.
  void StartAuction(
      const GURL& seller_decision_logic_url,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      const std::string& auction_signals_json,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals) {
    auction_complete_ = false;

    blink::mojom::AuctionAdConfigPtr auction_config =
        blink::mojom::AuctionAdConfig::New();
    auction_config->seller = url::Origin::Create(seller_decision_logic_url);
    auction_config->decision_logic_url = seller_decision_logic_url;
    // This is ignored by AuctionRunner, in favor of its `filtered_buyers`
    // parameter.
    auction_config->interest_group_buyers =
        blink::mojom::InterestGroupBuyers::NewAllBuyers(
            blink::mojom::AllBuyers::New());
    auction_config->auction_signals = auction_signals_json;
    auction_config->seller_signals = R"({"isSellerSignals": true})";

    base::flat_map<url::Origin, std::string> per_buyer_signals;
    per_buyer_signals[kBidder1] = R"({"signalsFor": ")" + kBidder1Name + "\"}";
    per_buyer_signals[kBidder2] = R"({"signalsFor": ")" + kBidder2Name + "\"}";
    auction_config->per_buyer_signals = std::move(per_buyer_signals);

    interest_group_manager_ = std::make_unique<InterestGroupManager>(
        base::FilePath(), true /* in_memory */);
    if (auction_process_manager_) {
      interest_group_manager_->set_auction_process_manager_for_testing(
          std::move(auction_process_manager_));
    } else {
      interest_group_manager_->set_auction_process_manager_for_testing(
          std::make_unique<SameThreadAuctionProcessManager>());
    }

    histogram_tester_ = std::make_unique<base::HistogramTester>();

    // Add previous wins and bids to the interest group manager.
    for (auto& bidder : bidders) {
      for (int i = 0; i < bidder->signals->join_count; i++) {
        interest_group_manager_->JoinInterestGroup(bidder->group.Clone());
      }
      for (int i = 0; i < bidder->signals->bid_count; i++) {
        interest_group_manager_->RecordInterestGroupBid(bidder->group->owner,
                                                        bidder->group->name);
      }
      for (const auto& prev_win : bidder->signals->prev_wins) {
        interest_group_manager_->RecordInterestGroupWin(
            bidder->group->owner, bidder->group->name, prev_win->ad_json);
        // Add some time between interest group wins, so that they'll be added
        // to the database in the order they appear. Their times will *not*
        // match those in `prev_wins`.
        task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
      }
    }

    auction_run_loop_ = std::make_unique<base::RunLoop>();
    auction_runner_ = AuctionRunner::CreateAndStart(
        this, interest_group_manager_.get(), std::move(auction_config),
        std::vector<url::Origin>{kBidder1, kBidder2},
        std::move(browser_signals), frame_origin_,
        base::BindOnce(&AuctionRunnerTest::OnAuctionComplete,
                       base::Unretained(this)));
  }

  const Result& RunAuctionAndWait(
      const GURL& seller_decision_logic_url,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders,
      const std::string& auction_signals_json,
      auction_worklet::mojom::BrowserSignalsPtr browser_signals) {
    StartAuction(seller_decision_logic_url, std::move(bidders),
                 auction_signals_json, std::move(browser_signals));
    auction_run_loop_->Run();
    return result_;
  }

  void OnAuctionComplete(AuctionRunner* auction_runner,
                         absl::optional<GURL> ad_url,
                         absl::optional<GURL> bidder_report_url,
                         absl::optional<GURL> seller_report_url,
                         std::vector<std::string> errors) {
    DCHECK(auction_run_loop_);
    DCHECK(!auction_complete_);

    auction_complete_ = true;
    result_.ad_url = std::move(ad_url);
    result_.bidder_report_url = std::move(bidder_report_url);
    result_.seller_report_url = std::move(seller_report_url);
    result_.errors = std::move(errors);
    result_.bidder1_bid_count = -1;
    result_.bidder1_prev_wins.clear();
    result_.bidder2_bid_count = -1;
    result_.bidder2_prev_wins.clear();

    // Retrieve bid count and previous wins for kBidder1 (and subsequently
    // kBidder2).
    interest_group_manager_->GetInterestGroupsForOwner(
        kBidder1, base::BindOnce(&AuctionRunnerTest::OnBidder1GroupsRetrieved,
                                 base::Unretained(this)));
  }

  void OnBidder1GroupsRetrieved(
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
          bidding_interest_groups) {
    for (auto& bidder : bidding_interest_groups) {
      if (bidder->group->owner == kBidder1 &&
          bidder->group->name == kBidder1Name) {
        result_.bidder1_bid_count = bidder->signals->bid_count;
        result_.bidder1_prev_wins = std::move(bidder->signals->prev_wins);
        SortPrevWins(result_.bidder1_prev_wins);
        break;
      }
    }
    interest_group_manager_->GetInterestGroupsForOwner(
        kBidder2, base::BindOnce(&AuctionRunnerTest::OnBidder2GroupsRetrieved,
                                 base::Unretained(this)));
  }

  void OnBidder2GroupsRetrieved(
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
          bidding_interest_groups) {
    for (auto& bidder : bidding_interest_groups) {
      if (bidder->group->owner == kBidder2 &&
          bidder->group->name == kBidder2Name) {
        result_.bidder2_bid_count = bidder->signals->bid_count;
        result_.bidder2_prev_wins = std::move(bidder->signals->prev_wins);
        SortPrevWins(result_.bidder2_prev_wins);
        break;
      }
    }

    auction_run_loop_->Quit();
  }

  auction_worklet::mojom::BiddingInterestGroupPtr MakeInterestGroup(
      const url::Origin& owner,
      const std::string& name,
      const GURL& bidding_url,
      const absl::optional<GURL>& trusted_bidding_signals_url,
      const std::vector<std::string>& trusted_bidding_signals_keys,
      const GURL& ad_url) {
    std::vector<blink::mojom::InterestGroupAdPtr> ads;
    // Give only kBidder1 an InterestGroupAd ad with non-empty metadata, to
    // better test the `ad_metadata` output.
    if (owner == kBidder1) {
      ads.push_back(
          blink::mojom::InterestGroupAd::New(ad_url, R"({"ads": true})"));
    } else {
      ads.push_back(blink::mojom::InterestGroupAd::New(ad_url, absl::nullopt));
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

    return auction_worklet::mojom::BiddingInterestGroup::New(
        blink::mojom::InterestGroup::New(
            base::Time::Max(), owner, name, bidding_url,
            GURL() /* update_url */, trusted_bidding_signals_url,
            trusted_bidding_signals_keys, absl::nullopt, std::move(ads)),
        auction_worklet::mojom::BiddingBrowserSignals::New(
            3, 5, std::move(previous_wins)));
  }

  void StartStandardAuction() {
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;
    bidders.push_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                        kBidder1TrustedSignalsUrl, {"k1", "k2"},
                                        GURL("https://ad1.com")));
    bidders.push_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                        kBidder2TrustedSignalsUrl, {"l1", "l2"},
                                        GURL("https://ad2.com")));

    StartAuction(kSellerUrl, std::move(bidders),
                 R"({"isAuctionSignals": true})", /* auction_signals_json */
                 auction_worklet::mojom::BrowserSignals::New(
                     url::Origin::Create(GURL("https://publisher1.com")),
                     url::Origin::Create(kSellerUrl)));
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
    mock_auction_process_manager_->WaitForWorklets(2 /* num_bidders */);
  }

  // AuctionRunner::Delegate implementation:
  network::mojom::URLLoaderFactory* GetFrameURLLoaderFactory() override {
    return &url_loader_factory_;
  }
  network::mojom::URLLoaderFactory* GetTrustedURLLoaderFactory() override {
    return &url_loader_factory_;
  }

  // Enables use of a mock AuctionProcessManager when the next auction is run.
  void UseMockWorkletService() {
    auto mock_auction_process_manager =
        std::make_unique<MockAuctionProcessManager>();
    mock_auction_process_manager_ = mock_auction_process_manager.get();
    auction_process_manager_ = std::move(mock_auction_process_manager);
  }

  void CheckHistograms(AuctionRunner::AuctionResult expected_result,
                       int expected_interest_groups,
                       int expected_owners) {
    histogram_tester_->ExpectUniqueSample("Ads.InterestGroup.Auction.Result",
                                          expected_result, 1);
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NumInterestGroups", expected_interest_groups,
        1);
    histogram_tester_->ExpectUniqueSample(
        "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
        expected_owners, 1);
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

  const url::Origin frame_origin_ =
      url::Origin::Create(GURL("https://frame.origin.test"));
  const GURL kSellerUrl{"https://adstuff.publisher1.com/auction.js"};

  const GURL kBidder1Url{"https://adplatform.com/offers.js"};
  const url::Origin kBidder1 = url::Origin::Create(kBidder1Url);
  const std::string kBidder1Name{"Ad Platform"};
  const GURL kBidder1TrustedSignalsUrl{"https://adplatform.com/signals1"};

  const GURL kBidder2Url{"https://anotheradthing.com/bids.js"};
  const url::Origin kBidder2 = url::Origin::Create(kBidder2Url);
  const std::string kBidder2Name{"Another Ad Thing"};
  const GURL kBidder2TrustedSignalsUrl{"https://anotheradthing.com/signals2"};

  base::test::TaskEnvironment task_environment_;

  // RunLoop that's quit on auction completion.
  std::unique_ptr<base::RunLoop> auction_run_loop_;
  // True if the most recently started auction has completed.
  bool auction_complete_ = false;
  // Result of the most recent auction.
  Result result_;

  network::TestURLLoaderFactory url_loader_factory_;

  // This is used (and consumed) when starting an auction, if non-null. Allows
  // either using a MockAuctionProcessManager instead of a
  // SameThreadAuctionProcessManager, or using a SameThreadAuctionProcessManager
  // that has already vended processes. If nullptr, a new
  // SameThreadAuctionProcessManager() is created when an auction is started.
  std::unique_ptr<AuctionProcessManager> auction_process_manager_;

  // Set by UseMockWorkletService(). Non-owning reference to the
  // AuctionProcessManager that will be / has been passed to the
  // InterestGroupManager.
  MockAuctionProcessManager* mock_auction_process_manager_ = nullptr;

  // The InterestGroupManager is recreated and repopulated for each auction.
  std::unique_ptr<InterestGroupManager> interest_group_manager_;

  std::unique_ptr<AuctionRunner> auction_runner_;
  // This should be inspected using TakeBadMessage(), which also clears it.
  std::string bad_message_;

  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Runs the standard auction, but without adding any interest groups to the
// manager.
TEST_F(AuctionRunnerTest, NoInterestGroups) {
  RunAuctionAndWait(
      kSellerUrl,
      std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>(),
      R"({"isAuctionSignals": true})" /* auction_signals_json */,
      auction_worklet::mojom::BrowserSignals::New(
          url::Origin::Create(GURL("https://publisher1.com")),
          url::Origin::Create(kSellerUrl)));

  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(-1, result_.bidder1_bid_count);
  EXPECT_EQ(0u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kNoInterestGroups,
                  0 /* expected_interest_groups */, 0 /* expected_owners */);
}

// Runs the standard auction, but with only adding one of the two standard
// interest groups to the manager.
TEST_F(AuctionRunnerTest, OneInterestGroup) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());
  auction_worklet::AddJsonResponse(&url_loader_factory_,
                                   GURL(kBidder1TrustedSignalsUrl.spec() +
                                        "?hostname=publisher1.com&keys=k1,k2"),
                                   R"({"k1":"a", "k2": "b", "extra": "c"})");

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;
  bidders.push_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                      kBidder1TrustedSignalsUrl, {"k1", "k2"},
                                      GURL("https://ad1.com")));

  RunAuctionAndWait(kSellerUrl, std::move(bidders),
                    R"({"isAuctionSignals": true})" /* auction_signals_json */,
                    auction_worklet::mojom::BrowserSignals::New(
                        url::Origin::Create(GURL("https://publisher1.com")),
                        url::Origin::Create(kSellerUrl)));

  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_EQ(GURL("https://reporting.example.com/"), result_.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(-1, result_.bidder2_bid_count);
  EXPECT_EQ(0u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  1 /* expected_interest_groups */, 1 /* expected_owners */);
}

// An auction with two successful bids.
TEST_F(AuctionRunnerTest, Basic) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
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
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where one bid is successful, another's script 404s.
TEST_F(AuctionRunnerTest, OneBidOne404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
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
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where one bid is successful, another's script does not provide a
// bidding function.
TEST_F(AuctionRunnerTest, OneBidOneNotMade) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));

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
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
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
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
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
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where the seller script doesn't have a scoring function.
TEST_F(AuctionRunnerTest, SellerRejectsAll) {
  std::string bid_script1 =
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a");
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kBidder1Url,
                                         bid_script1);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));

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
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where seller rejects one bid when scoring.
TEST_F(AuctionRunnerTest, SellerRejectsOne) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
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
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  ASSERT_EQ(4u, res.bidder1_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            res.bidder1_prev_wins[3]->ad_json);
  EXPECT_EQ(6, res.bidder2_bid_count);
  EXPECT_EQ(3u, res.bidder2_prev_wins.size());
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where the seller script fails to load.
TEST_F(AuctionRunnerTest, NoSellerScript) {
  // Tests to make sure that if seller script fails the other fetches are
  // cancelled, too.
  url_loader_factory_.AddResponse(kSellerUrl.spec(), "", net::HTTP_NOT_FOUND);
  const Result& res = RunStandardAuction();
  EXPECT_FALSE(res.ad_url);
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);

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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where bidders don't request trusted bidding signals.
TEST_F(AuctionRunnerTest, NoTrustedBiddingSignals) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    false /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    false /* has_signals */, "l2", "b"));
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         MakeAuctionScript());

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr> bidders;
  bidders.push_back(MakeInterestGroup(kBidder1, kBidder1Name, kBidder1Url,
                                      absl::nullopt, {"k1", "k2"},
                                      GURL("https://ad1.com")));
  bidders.push_back(MakeInterestGroup(kBidder2, kBidder2Name, kBidder2Url,
                                      absl::nullopt, {"l1", "l2"},
                                      GURL("https://ad2.com")));

  const Result& res = RunAuctionAndWait(
      kSellerUrl, std::move(bidders),
      R"({"isAuctionSignals": true})", /* auction_signals_json */
      auction_worklet::mojom::BrowserSignals::New(
          url::Origin::Create(GURL("https://publisher1.com")),
          url::Origin::Create(kSellerUrl)));

  EXPECT_EQ(GURL("https://ad2.com/"), res.ad_url);
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// An auction where trusted bidding signals are requested, but the fetch 404s.
TEST_F(AuctionRunnerTest, TrustedBiddingSignals404) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    false /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    false /* has_signals */, "l2", "b"));
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
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// A successful auction where seller reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportResultUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
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
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// A successful auction where bidder reporting worklet doesn't set a URL.
TEST_F(AuctionRunnerTest, NoReportWinUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b") +
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
  EXPECT_EQ(GURL("https://reporting.example.com/"), res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// A successful auction where neither reporting worklets sets a URL.
TEST_F(AuctionRunnerTest, NeitherReportUrl) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a") +
          kReportWinNoUrl);
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b") +
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
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_FALSE(res.bidder_report_url);
  EXPECT_EQ(6, res.bidder1_bid_count);
  EXPECT_EQ(3u, res.bidder1_prev_wins.size());
  EXPECT_EQ(6, res.bidder2_bid_count);
  ASSERT_EQ(4u, res.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            res.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(res.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Test the case where the seller worklet provides no signals for the winner,
// since it has no reportResult() method. The winning bidder's reportWin()
// function should be passed null as `sellerSignals`, and should still be able
// to send a report.
TEST_F(AuctionRunnerTest, NoReportResult) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b") +
          kReportWinExpectNullAuctionSignals);
  auction_worklet::AddJavascriptResponse(&url_loader_factory_, kSellerUrl,
                                         kCheckingAuctionScript);
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
  EXPECT_FALSE(res.seller_report_url);
  EXPECT_EQ(GURL("https://seller.signals.were.null.test/"),
            res.bidder_report_url);
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
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Test the case where the ProcessManager delays the auction.
TEST_F(AuctionRunnerTest, ProcessManagerDelaysAuction) {
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder1Url,
      MakeBidScript("1", "https://ad1.com/", kBidder1, kBidder1Name,
                    true /* has_signals */, "k1", "a"));
  auction_worklet::AddJavascriptResponse(
      &url_loader_factory_, kBidder2Url,
      MakeBidScript("2", "https://ad2.com/", kBidder2, kBidder2Name,
                    true /* has_signals */, "l2", "b"));
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

  // Create AuctionProcessManager in advance of starting the auction so can
  // create seller worklets before the auction starts.
  auction_process_manager_ =
      std::make_unique<SameThreadAuctionProcessManager>();

  AuctionProcessManager* auction_process_manager =
      auction_process_manager_.get();

  // Make kMaxActiveSellerWorklets seller worklet requests for kSellerUrl's
  // origin.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> sellers;
  for (size_t i = 0; i < AuctionProcessManager::kMaxActiveSellerWorklets; ++i) {
    sellers.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    EXPECT_TRUE(auction_process_manager->RequestWorkletService(
        AuctionProcessManager::WorkletType::kSeller,
        url::Origin::Create(kSellerUrl), &*sellers.back(), base::BindOnce([]() {
          ADD_FAILURE() << "This should not be called";
        })));
  }

  StartStandardAuction();

  // The auction should be waiting for a seller worklet slot.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, auction_process_manager->GetPendingSellerRequestsForTesting());
  EXPECT_EQ(0u, auction_process_manager->GetPendingBidderRequestsForTesting());
  EXPECT_FALSE(auction_complete_);

  // Make kMaxBidderProcesses bidder worklet requests different bidder origins.
  // Do this after starting the auction, so the auction will incorrectly
  // complete once a seller worklet slot is freed if the auction already
  // requested bidder worklet processes.
  std::list<std::unique_ptr<AuctionProcessManager::ProcessHandle>> bidders;
  for (size_t i = 0; i < AuctionProcessManager::kMaxBidderProcesses; ++i) {
    bidders.push_back(std::make_unique<AuctionProcessManager::ProcessHandle>());
    url::Origin origin = url::Origin::Create(
        GURL(base::StringPrintf("https://blocking.bidder.%zu.test", i)));
    EXPECT_TRUE(auction_process_manager->RequestWorkletService(
        AuctionProcessManager::WorkletType::kBidder, origin, &*bidders.back(),
        base::BindOnce(
            []() { ADD_FAILURE() << "This should not be called"; })));
  }

  // Free up a seller slot. Auction should now be blocked waiting for bidder
  // slots.
  sellers.pop_front();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, auction_process_manager->GetPendingSellerRequestsForTesting());
  EXPECT_EQ(2u, auction_process_manager->GetPendingBidderRequestsForTesting());
  EXPECT_FALSE(auction_complete_);

  // Free up a single bidder slot. Auction should still be blocked waiting for a
  // bidder slot.
  bidders.pop_front();
  EXPECT_EQ(0u, auction_process_manager->GetPendingSellerRequestsForTesting());
  EXPECT_EQ(1u, auction_process_manager->GetPendingBidderRequestsForTesting());
  EXPECT_FALSE(auction_complete_);

  // Free up other bidder slot. Auction should complete.
  bidders.pop_front();
  auction_run_loop_->Run();
  EXPECT_TRUE(auction_complete_);

  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_EQ(GURL("https://reporting.example.com/"), result_.seller_report_url);
  EXPECT_EQ(GURL("https://buyer-reporting.example.com/"),
            result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

TEST_F(AuctionRunnerTest, AllBiddersCrashBeforeBidding) {
  for (bool seller_worklet_loads_first : {false, true}) {
    SCOPED_TRACE(seller_worklet_loads_first);

    StartStandardAuctionWithMockService();
    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    if (seller_worklet_loads_first) {
      seller_worklet->CompleteLoading();
      mock_auction_process_manager_->Flush();
    }
    EXPECT_FALSE(auction_complete_);

    // Have to keep the callbacks around, since the AuctionWorkletService is
    // still live. Closing it here causes more issues than its worth (seller
    // worklet can't complete loading if it hasn't already). In production,
    // there would be multiple service processes in this case, with different
    // pipes.
    auto bidder1_callback = bidder1_worklet->TakeLoadCallback();
    auto bidder2_callback = bidder2_worklet->TakeLoadCallback();
    bidder1_worklet.reset();
    bidder2_worklet.reset();

    base::RunLoop().RunUntilIdle();
    // The auction isn't failed until the seller worklet has completed loading.
    if (!seller_worklet_loads_first)
      seller_worklet->CompleteLoading();

    auction_run_loop_->Run();

    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
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

    // Need to close the AuctionWorkletService pipes so callbacks can be
    // destroyed without DCHECKing.
    mock_auction_process_manager_->ClosePipes();
    CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                    2 /* expected_interest_groups */, 2 /* expected_owners */);
  }
}

// Test the case a single bidder worklet crashes before bidding. The auction
// should continue, without that bidder's bid.
TEST_F(AuctionRunnerTest, BidderCrashBeforeBidding) {
  for (bool other_bidder_finishes_first : {false, true}) {
    SCOPED_TRACE(other_bidder_finishes_first);
    for (bool seller_worklet_loads_first : {false, true}) {
      SCOPED_TRACE(seller_worklet_loads_first);
      StartStandardAuctionWithMockService();
      auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
      ASSERT_TRUE(seller_worklet);
      auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          kBidder1, kBidder1Name);
      ASSERT_TRUE(bidder1_worklet);
      auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
          kBidder2, kBidder2Name);
      ASSERT_TRUE(bidder2_worklet);

      ASSERT_FALSE(auction_complete_);
      if (seller_worklet_loads_first)
        seller_worklet->CompleteLoading();
      if (other_bidder_finishes_first) {
        bidder2_worklet->CompleteLoadingAndBid(7 /* bid */,
                                               GURL("https://ad2.com/"));
      }
      mock_auction_process_manager_->Flush();

      ASSERT_FALSE(auction_complete_);

      // Close Bidder1's pipe, keeping its callback alive to avoid a DCHECK.
      auto bidder1_callback = bidder1_worklet->TakeLoadCallback();
      bidder1_worklet.reset();
      // Can't flush the closed pipe without reaching into AuctionRunner, so use
      // RunUntilIdle() instead.
      base::RunLoop().RunUntilIdle();

      if (!seller_worklet_loads_first)
        seller_worklet->CompleteLoading();
      if (!other_bidder_finishes_first) {
        bidder2_worklet->CompleteLoadingAndBid(7 /* bid */,
                                               GURL("https://ad2.com/"));
      }
      mock_auction_process_manager_->Flush();

      // The auction should be scored without waiting on the crashed kBidder1.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
      EXPECT_EQ(7, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(11);

      // Finish the auction.
      seller_worklet->WaitForReportResult();
      seller_worklet->InvokeReportResultCallback();
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();

      // Bidder2 won, Bidder1 crashed.
      auction_run_loop_->Run();
      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_FALSE(result_.seller_report_url);
      EXPECT_FALSE(result_.bidder_report_url);
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
                      2 /* expected_interest_groups */,
                      2 /* expected_owners */);

      // Need to close the AuctionWorkletService pipes so callbacks can be
      // destroyed without DCHECKing.
      mock_auction_process_manager_->ClosePipes();
    }
  }
}

// If a losing bidder crashes while scoring, the auction should succeed.
TEST_F(AuctionRunnerTest, LosingBidderCrashWhileScoring) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  bidder1_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad2.com/"));

  // Bidder1 crashes while the seller scores its bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  bidder1_worklet.reset();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder2 won.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  // Since Bidder1 crashed after bidding, don't report anything.
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// If the winning bidder crashes while scoring, the auction should fail.
TEST_F(AuctionRunnerTest, WinningBidderCrashWhileScoring) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  bidder1_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad2.com/"));

  // Bidder1 crashes while scoring its bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  bidder1_worklet.reset();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  // AuctionRunner discovered Bidder1 crashed before calling its ReportWin
  // method.
  auction_run_loop_->Run();

  // No bidder won, Bidder1 crashed.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors,
              testing::ElementsAre(base::StringPrintf(
                  "%s crashed while idle.", kBidder1Url.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kWinningBidderWorkletCrashed,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// If the winning bidder crashes while coming up with the reporting URL, the
// auction should fail.
TEST_F(AuctionRunnerTest, WinningBidderCrashWhileReporting) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  bidder1_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad2.com/"));

  // Bidder1 crashes while scoring its bid.
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Score Bidder2's bid.
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet.reset();
  auction_run_loop_->Run();

  // No bidder won, Bidder1 crashed.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                  "%s crashed while trying to run reportWin().",
                                  kBidder1Url.spec().c_str())));
  CheckHistograms(AuctionRunner::AuctionResult::kWinningBidderWorkletCrashed,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// If the seller crashes at any point in the auction, the auction fails.
TEST_F(AuctionRunnerTest, SellerCrash) {
  enum class CrashPhase {
    kLoad,
    kLoadAfterBiddersLoaded,
    kScoreBid,
    kReportResult,
  };
  for (CrashPhase crash_phase :
       {CrashPhase::kLoad, CrashPhase::kLoadAfterBiddersLoaded,
        CrashPhase::kScoreBid, CrashPhase::kReportResult}) {
    SCOPED_TRACE(static_cast<int>(crash_phase));

    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    // While loop to allow breaking when the crash stage is reached.
    while (true) {
      if (crash_phase == CrashPhase::kLoad) {
        // Need to close the AuctionWorkletService pipes so callbacks can be
        // destroyed without DCHECKing.
        mock_auction_process_manager_->ClosePipes();
        seller_worklet.reset();
        break;
      }

      bidder1_worklet->CompleteLoadingAndBid(5 /* bid */,
                                             GURL("https://ad1.com/"));
      bidder2_worklet->CompleteLoadingAndBid(7 /* bid */,
                                             GURL("https://ad2.com/"));
      // Wait for bids to be received.
      base::RunLoop().RunUntilIdle();

      if (crash_phase == CrashPhase::kLoadAfterBiddersLoaded) {
        // Need to close the AuctionWorkletService pipes so callbacks can be
        // destroyed without DCHECKing.
        mock_auction_process_manager_->ClosePipes();
        seller_worklet.reset();
        break;
      }

      seller_worklet->CompleteLoading();

      // Wait for Bidder1's bid.
      auto score_ad_params = seller_worklet->WaitForScoreAd();
      if (crash_phase == CrashPhase::kScoreBid) {
        seller_worklet.reset();
        break;
      }
      // Score Bidder1's bid.
      bidder1_worklet.reset();
      EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
      EXPECT_EQ(5, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(10 /* score */);

      // Score Bidder2's bid.
      score_ad_params = seller_worklet->WaitForScoreAd();
      EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
      EXPECT_EQ(7, score_ad_params->bid);
      seller_worklet->InvokeScoreAdCallback(11 /* score */);

      seller_worklet->WaitForReportResult();
      DCHECK_EQ(CrashPhase::kReportResult, crash_phase);
      seller_worklet.reset();
      break;
    }

    // Wait for auction to complete.
    auction_run_loop_->Run();

    // No bidder won, seller crashed.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
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
                    2 /* expected_interest_groups */, 2 /* expected_owners */);
    EXPECT_THAT(result_.errors, testing::ElementsAre(base::StringPrintf(
                                    "%s crashed.", kSellerUrl.spec().c_str())));
  }
}

// Test cases where a bad bid is received over Mojo. Bad bids should be rejected
// in the Mojo process, so these are treated as security errors.
TEST_F(AuctionRunnerTest, BadBid) {
  const struct TestCase {
    const char* expected_error_message;
    double bid;
    GURL render_url;
    base::TimeDelta duration;
  } kTestCases[] = {
      // Bids that aren't positive integers.
      {
          "Invalid bid value",
          -10,
          GURL("https://ad1.com"),
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          0,
          GURL("https://ad1.com"),
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::infinity(),
          GURL("https://ad1.com"),
          base::TimeDelta(),
      },
      {
          "Invalid bid value",
          std::numeric_limits<double>::quiet_NaN(),
          GURL("https://ad1.com"),
          base::TimeDelta(),
      },

      // Invalid URL.
      {
          "Invalid bid render URL",
          1,
          GURL(":"),
          base::TimeDelta(),
      },

      // Non-HTTPS URLs.
      {
          "Invalid bid render URL",
          1,
          GURL("data:,foo"),
          base::TimeDelta(),
      },
      {
          "Invalid bid render URL",
          1,
          GURL("http://ad1.com"),
          base::TimeDelta(),
      },

      // HTTPS URL that's not in the list of allowed renderUrls.
      {
          "Bid render URL must be an ad URL",
          1,
          GURL("https://ad2.com"),
          base::TimeDelta(),
      },

      // Negative time.
      {
          "Invalid bid duration",
          1,
          GURL("https://ad2.com"),
          base::TimeDelta::FromMilliseconds(-1),
      },
  };

  for (const auto& test_case : kTestCases) {
    StartStandardAuctionWithMockService();

    auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
    ASSERT_TRUE(seller_worklet);
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    seller_worklet->CompleteLoading();
    bidder1_worklet->CompleteLoadingAndBid(test_case.bid, test_case.render_url,
                                           test_case.duration);
    // Bidder 2 doesn't bid..
    bidder2_worklet->CompleteLoadingWithoutBid();

    // Since there's no acceptable bid, the seller worklet is never asked to
    // score a bid.
    auction_run_loop_->Run();

    EXPECT_EQ(test_case.expected_error_message, TakeBadMessage());

    // No bidder won.
    EXPECT_FALSE(result_.ad_url);
    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
    EXPECT_EQ(5, result_.bidder1_bid_count);
    EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
    EXPECT_EQ(5, result_.bidder2_bid_count);
    EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    CheckHistograms(AuctionRunner::AuctionResult::kNoBids,
                    2 /* expected_interest_groups */, 2 /* expected_owners */);
  }
}

// Test cases where bad a report URL is received over Mojo from the bidder
// worklet. Bad report URLs should be rejected in the Mojo process, so these are
// treated as security errors.
TEST_F(AuctionRunnerTest, BadSellerReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingWithoutBid();

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  // Bidder1 never gets to report anything, since the seller providing a bad
  // report URL aborts the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(GURL("http://not.https.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid seller report URL", TakeBadMessage());

  // No bidder won.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kBadMojoMessage,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Test cases where bad a report URL is received over Mojo from the seller
// worklet. Bad report URLs should be rejected in the Mojo process, so these are
// treated as security errors.
TEST_F(AuctionRunnerTest, BadBidderReportUrl) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();
  // Only Bidder1 bids, to keep things simple.
  bidder1_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad1.com/"));
  bidder2_worklet->CompleteLoadingWithoutBid();

  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);

  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback(
      GURL("https://valid.url.that.is.thrown.out.test/"));
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback(GURL("http://not.https.test/"));
  auction_run_loop_->Run();

  EXPECT_EQ("Invalid bidder report URL", TakeBadMessage());

  // No bidder won.
  EXPECT_FALSE(result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(5, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kBadMojoMessage,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Make sure that requesting unexpected URLs is blocked.
TEST_F(AuctionRunnerTest, UrlRequestProtection) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  // It should be possible to request the seller URL from the seller's
  // URLLoaderFactory.
  network::ResourceRequest request;
  request.url = kSellerUrl;
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                            "application/javascript");
  mojo::PendingRemote<network::mojom::URLLoader> receiver;
  mojo::PendingReceiver<network::mojom::URLLoaderClient> client;
  seller_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  seller_worklet->url_loader_factory().FlushForTesting();
  EXPECT_TRUE(seller_worklet->url_loader_factory().is_connected());
  ASSERT_EQ(1u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ(kSellerUrl,
            (*url_loader_factory_.pending_requests())[0].request.url);
  receiver.reset();
  client.reset();

  // A bidder's URLLoaderFactory should reject the seller URL, closing the Mojo
  // pipe.
  bidder1_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  bidder1_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(bidder1_worklet->url_loader_factory().is_connected());
  EXPECT_EQ(1u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());
  receiver.reset();
  client.reset();

  // A bidder's URLLoaderFactory should allow the bidder's URL to be requested.
  request.url = kBidder2Url;
  bidder2_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  bidder2_worklet->url_loader_factory().FlushForTesting();
  EXPECT_TRUE(bidder2_worklet->url_loader_factory().is_connected());
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ(kBidder2Url,
            (*url_loader_factory_.pending_requests())[1].request.url);
  receiver.reset();
  client.reset();

  // The seller's URLLoaderFactory should reject a bidder worklet's URL, closing
  // the Mojo pipe.
  seller_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  seller_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(seller_worklet->url_loader_factory().is_connected());
  EXPECT_EQ(2u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());
  receiver.reset();
  client.reset();

  // A bidder's URLLoaderFactory should also reject the URL from another bidder.
  request.url = kBidder1Url;
  bidder2_worklet->url_loader_factory()->CreateLoaderAndStart(
      receiver.InitWithNewPipeAndPassReceiver(), 0 /* request_id_ */,
      0 /* options */, request, client.InitWithNewPipeAndPassRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  bidder2_worklet->url_loader_factory().FlushForTesting();
  EXPECT_FALSE(bidder2_worklet->url_loader_factory().is_connected());
  ASSERT_EQ(2u, url_loader_factory_.pending_requests()->size());
  EXPECT_EQ("Unexpected request", TakeBadMessage());

  // Need to close the AuctionWorkletService pipes so callbacks can be destroyed
  // without DCHECKing.
  mock_auction_process_manager_->ClosePipes();
}

// Check that BidderWorklets that don't make a bid are destroyed immediately.
TEST_F(AuctionRunnerTest, DestroyBidderWorkletWithoutBid) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();

  bidder1_worklet->CompleteLoadingWithoutBid();
  // Need to flush the service pipe to make sure the AuctionRunner has received
  // the bid.
  mock_auction_process_manager_->Flush();
  // The AuctionRunner should have closed the pipe.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());

  // Bidder2 returns a bid, which is then scored.
  bidder2_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad2.com/"));
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder2 won.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(5, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Check that BidderWorklets are destroyed as soon as there's a higher bid. The
// first BidderWorklet to have its bid scored is the one that loses the auction.
TEST_F(AuctionRunnerTest, DestroyLosingBidderWorkletFirstBidderLoses) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();

  // Bidder1 returns a bid, which is then scored.
  bidder1_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad1.com/"));
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);
  // Need to flush the service pipe to make sure the AuctionRunner has received
  // the score.
  mock_auction_process_manager_->Flush();
  // The AuctionRunner should not have closed the bidder pipe, since it could
  // still be the winning bid.
  EXPECT_FALSE(bidder1_worklet->PipeIsClosed());

  // Bidder2 returns a bid, which is then scored.
  bidder2_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad2.com/"));
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);
  // Need to flush the service pipe to make sure the AuctionRunner has received
  // the score.
  seller_worklet->Flush();
  // The losing bidder should now be destroyed.
  EXPECT_TRUE(bidder1_worklet->PipeIsClosed());

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  bidder2_worklet->WaitForReportWin();
  bidder2_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder2 won.
  EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
            result_.bidder2_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
}

// Check that BidderWorklets are destroyed as soon as there's a higher bid. The
// last BidderWorklet to have its bid scored is the one that loses the auction.
TEST_F(AuctionRunnerTest, DestroyLosingBidderWorkletLastBidderLoses) {
  StartStandardAuctionWithMockService();

  auto seller_worklet = mock_auction_process_manager_->TakeSellerWorklet();
  ASSERT_TRUE(seller_worklet);
  auto bidder1_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder1, kBidder1Name);
  ASSERT_TRUE(bidder1_worklet);
  auto bidder2_worklet =
      mock_auction_process_manager_->TakeBidderWorklet(kBidder2, kBidder2Name);
  ASSERT_TRUE(bidder2_worklet);

  seller_worklet->CompleteLoading();

  // Bidder1 returns a bid, which is then scored.
  bidder1_worklet->CompleteLoadingAndBid(5 /* bid */, GURL("https://ad1.com/"));
  auto score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
  EXPECT_EQ(5, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(11 /* score */);

  // Bidder2 returns a bid, which is then scored.
  bidder2_worklet->CompleteLoadingAndBid(7 /* bid */, GURL("https://ad2.com/"));
  score_ad_params = seller_worklet->WaitForScoreAd();
  EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
  EXPECT_EQ(7, score_ad_params->bid);
  seller_worklet->InvokeScoreAdCallback(10 /* score */);
  // Need to flush the service pipe to make sure the AuctionRunner has received
  // the score.
  seller_worklet->Flush();
  // The losing bidder should now be destroyed.
  EXPECT_TRUE(bidder2_worklet->PipeIsClosed());

  // Finish the auction.
  seller_worklet->WaitForReportResult();
  seller_worklet->InvokeReportResultCallback();
  bidder1_worklet->WaitForReportWin();
  bidder1_worklet->InvokeReportWinCallback();
  auction_run_loop_->Run();

  // Bidder1 won.
  EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
  EXPECT_FALSE(result_.seller_report_url);
  EXPECT_FALSE(result_.bidder_report_url);
  EXPECT_EQ(6, result_.bidder1_bid_count);
  ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
  EXPECT_EQ(6, result_.bidder2_bid_count);
  EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
  EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
            result_.bidder1_prev_wins[3]->ad_json);
  EXPECT_THAT(result_.errors, testing::ElementsAre());
  CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                  2 /* expected_interest_groups */, 2 /* expected_owners */);
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
    auto bidder1_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder1, kBidder1Name);
    ASSERT_TRUE(bidder1_worklet);
    auto bidder2_worklet = mock_auction_process_manager_->TakeBidderWorklet(
        kBidder2, kBidder2Name);
    ASSERT_TRUE(bidder2_worklet);

    seller_worklet->CompleteLoading();

    // Bidder1 returns a bid, which is then scored.
    bidder1_worklet->CompleteLoadingAndBid(5 /* bid */,
                                           GURL("https://ad1.com/"));
    auto score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder1, score_ad_params->interest_group_owner);
    EXPECT_EQ(5, score_ad_params->bid);
    seller_worklet->InvokeScoreAdCallback(10 /* score */);

    // Bidder2 returns a bid, which is then scored.
    bidder2_worklet->CompleteLoadingAndBid(5 /* bid */,
                                           GURL("https://ad2.com/"));
    score_ad_params = seller_worklet->WaitForScoreAd();
    EXPECT_EQ(kBidder2, score_ad_params->interest_group_owner);
    EXPECT_EQ(5, score_ad_params->bid);
    seller_worklet->InvokeScoreAdCallback(10 /* score */);
    // Need to flush the service pipe to make sure the AuctionRunner has
    // received the score.
    seller_worklet->Flush();

    seller_worklet->WaitForReportResult();
    seller_worklet->InvokeReportResultCallback();

    // The bidder without the closed pipe is the one that was picked to win the
    // auction.
    if (bidder2_worklet->PipeIsClosed()) {
      seen_bidder1_win = true;

      EXPECT_FALSE(bidder1_worklet->PipeIsClosed());
      bidder1_worklet->WaitForReportWin();
      bidder1_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad1.com/"), result_.ad_url);
      ASSERT_EQ(4u, result_.bidder1_prev_wins.size());
      EXPECT_EQ(3u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad1.com/","metadata":{"ads": true}})",
                result_.bidder1_prev_wins[3]->ad_json);
    } else {
      seen_bidder2_win = true;

      EXPECT_TRUE(bidder1_worklet->PipeIsClosed());
      bidder2_worklet->WaitForReportWin();
      bidder2_worklet->InvokeReportWinCallback();
      auction_run_loop_->Run();

      EXPECT_EQ(GURL("https://ad2.com/"), result_.ad_url);
      EXPECT_EQ(3u, result_.bidder1_prev_wins.size());
      ASSERT_EQ(4u, result_.bidder2_prev_wins.size());
      EXPECT_EQ(R"({"render_url":"https://ad2.com/"})",
                result_.bidder2_prev_wins[3]->ad_json);
    }

    EXPECT_FALSE(result_.seller_report_url);
    EXPECT_FALSE(result_.bidder_report_url);
    EXPECT_EQ(6, result_.bidder1_bid_count);
    EXPECT_EQ(6, result_.bidder2_bid_count);
    EXPECT_THAT(result_.errors, testing::ElementsAre());
    CheckHistograms(AuctionRunner::AuctionResult::kSuccess,
                    2 /* expected_interest_groups */, 2 /* expected_owners */);
  }
}

}  // namespace
}  // namespace content
