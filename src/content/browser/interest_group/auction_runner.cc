// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/auction_runner.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/browser/interest_group/auction_url_loader_factory_proxy.h"
#include "content/browser/interest_group/auction_worklet_manager.h"
#include "content/browser/interest_group/debuggable_auction_worklet.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/public/browser/content_browser_client.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "net/base/escape.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

constexpr base::TimeDelta kMaxPerBuyerTimeout = base::Milliseconds(500);

// All URLs received from worklets must be valid HTTPS URLs. It's up to callers
// to call ReportBadMessage() on invalid URLs.
bool IsUrlValid(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme);
}

// Finds InterestGroup::Ad in `ads` that matches `render_url`, if any. Returns
// nullptr if `render_url` is invalid.
const blink::InterestGroup::Ad* FindMatchingAd(
    const std::vector<blink::InterestGroup::Ad>& ads,
    const GURL& render_url) {
  // TODO(mmenke): Validate render URLs on load and make this a DCHECK just
  // before the return instead, since then `ads` will necessarily only contain
  // valid URLs at that point.
  if (!IsUrlValid(render_url))
    return nullptr;

  for (const auto& ad : ads) {
    if (ad.render_url == render_url) {
      return &ad;
    }
  }

  return nullptr;
}

}  // namespace

AuctionRunner::BidState::BidState() = default;
AuctionRunner::BidState::~BidState() = default;
AuctionRunner::BidState::BidState(BidState&&) = default;

AuctionRunner::Bid::Bid(std::string ad_metadata,
                        double bid,
                        GURL render_url,
                        absl::optional<std::vector<GURL>> ad_components,
                        base::TimeDelta bid_duration,
                        absl::optional<uint32_t> bidding_signals_data_version,
                        const blink::InterestGroup::Ad* bid_ad,
                        BidState* bid_state,
                        Auction* auction)
    : ad_metadata(std::move(ad_metadata)),
      bid(bid),
      render_url(std::move(render_url)),
      ad_components(std::move(ad_components)),
      bid_duration(bid_duration),
      bidding_signals_data_version(bidding_signals_data_version),
      interest_group(&bid_state->bidder.interest_group),
      bid_ad(bid_ad),
      bid_state(bid_state),
      auction(auction) {
  DCHECK_GT(bid, 0);
}

AuctionRunner::Bid::Bid(Bid&) = default;

AuctionRunner::Bid::~Bid() = default;

AuctionRunner::ScoredBid::ScoredBid(
    double score,
    absl::optional<uint32_t> scoring_signals_data_version,
    std::unique_ptr<Bid> bid)
    : score(score),
      scoring_signals_data_version(scoring_signals_data_version),
      bid(std::move(bid)) {
  DCHECK_GT(score, 0);
}

AuctionRunner::ScoredBid::~ScoredBid() = default;

AuctionRunner::Auction::Auction(
    blink::mojom::AuctionAdConfig* config,
    bool is_component_auction,
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    base::Time auction_start_time)
    : auction_worklet_manager_(auction_worklet_manager),
      interest_group_manager_(interest_group_manager),
      config_(config),
      is_component_auction_(is_component_auction),
      auction_start_time_(auction_start_time) {
  for (const auto& component_auction_config : config->component_auctions) {
    // Nested component auctions are not supported.
    DCHECK(!is_component_auction);
    component_auctions_.emplace_back(std::make_unique<Auction>(
        component_auction_config.get(), /*is_component_auction=*/true,
        auction_worklet_manager, interest_group_manager, auction_start_time));
  }
}

AuctionRunner::Auction::~Auction() {
  if (!final_auction_result_)
    final_auction_result_ = AuctionResult::kAborted;

  // TODO(mmenke): Record histograms for component auctions.
  if (!is_component_auction_) {
    UMA_HISTOGRAM_ENUMERATION("Ads.InterestGroup.Auction.Result",
                              *final_auction_result_);

    // Only record time of full auctions and aborts.
    switch (*final_auction_result_) {
      case AuctionResult::kAborted:
        UMA_HISTOGRAM_MEDIUM_TIMES("Ads.InterestGroup.Auction.AbortTime",
                                   base::Time::Now() - auction_start_time_);
        break;
      case AuctionResult::kNoBids:
      case AuctionResult::kAllBidsRejected:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Ads.InterestGroup.Auction.CompletedWithoutWinnerTime",
            base::Time::Now() - auction_start_time_);
        break;
      case AuctionResult::kSuccess:
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Ads.InterestGroup.Auction.AuctionWithWinnerTime",
            base::Time::Now() - auction_start_time_);
        break;
      default:
        break;
    }
  }
}

void AuctionRunner::Auction::StartLoadInterestGroupsPhase(
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    AuctionPhaseCompletionCallback load_interest_groups_phase_callback) {
  DCHECK(is_interest_group_api_allowed_callback);
  DCHECK(load_interest_groups_phase_callback);
  DCHECK(bid_states_.empty());
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK(!reporting_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK_EQ(num_pending_loads_, 0u);

  load_interest_groups_phase_callback_ =
      std::move(load_interest_groups_phase_callback);

  // If the seller can't participate in the auction, fail the auction.
  if (!is_interest_group_api_allowed_callback.Run(
          ContentBrowserClient::InterestGroupApiOperation::kSell,
          config_->seller)) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Auction::OnStartLoadInterestGroupsPhaseComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       AuctionResult::kSellerRejected));
    return;
  }

  for (auto component_auction = component_auctions_.begin();
       component_auction != component_auctions_.end(); ++component_auction) {
    (*component_auction)
        ->StartLoadInterestGroupsPhase(
            is_interest_group_api_allowed_callback,
            base::BindOnce(&Auction::OnComponentInterestGroupsRead,
                           weak_ptr_factory_.GetWeakPtr(), component_auction));
    ++num_pending_loads_;
  }

  if (config_->auction_ad_config_non_shared_params->interest_group_buyers) {
    for (const auto& buyer :
         *config_->auction_ad_config_non_shared_params->interest_group_buyers) {
      if (!is_interest_group_api_allowed_callback.Run(
              ContentBrowserClient::InterestGroupApiOperation::kBuy, buyer)) {
        continue;
      }
      interest_group_manager_->GetInterestGroupsForOwner(
          buyer, base::BindOnce(&Auction::OnInterestGroupRead,
                                weak_ptr_factory_.GetWeakPtr()));
      ++num_pending_loads_;
    }
  }

  // Fail if there are no pending loads.
  if (num_pending_loads_ == 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&Auction::OnStartLoadInterestGroupsPhaseComplete,
                       weak_ptr_factory_.GetWeakPtr(),
                       AuctionResult::kNoInterestGroups));
  }
}

void AuctionRunner::Auction::StartBiddingAndScoringPhase(
    base::OnceClosure on_seller_receiver_callback,
    AuctionPhaseCompletionCallback bidding_and_scoring_phase_callback) {
  DCHECK(bidding_and_scoring_phase_callback);
  DCHECK(!bid_states_.empty() || !component_auctions_.empty());
  DCHECK(!on_seller_receiver_callback_);
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK(!reporting_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK(!top_bid_);
  DCHECK_EQ(pending_component_seller_worklet_requests_, 0u);

  on_seller_receiver_callback_ = std::move(on_seller_receiver_callback);
  bidding_and_scoring_phase_callback_ =
      std::move(bidding_and_scoring_phase_callback);

  num_bids_not_sent_to_seller_worklet_ =
      bid_states_.size() + component_auctions_.size();
  outstanding_bids_ = num_bids_not_sent_to_seller_worklet_;

  // Need to start loading worklets before any bids can be generated or scored.

  if (component_auctions_.empty()) {
    // If there are no component auctions, request the seller worklet.
    // Otherwise, the seller worklet will be requested once all component
    // auctions have received their own seller worklets.
    RequestSellerWorklet();
  } else {
    // Since component auctions may invoke OnComponentSellerWorkletReceived()
    // synchronously, it's important to set this to the total number of
    // component auctions before invoking StartBiddingAndScoringPhase() on any
    // component auction.
    pending_component_seller_worklet_requests_ = component_auctions_.size();
    for (auto& component_auction : component_auctions_) {
      component_auction->StartBiddingAndScoringPhase(
          base::BindOnce(&Auction::OnComponentSellerWorkletReceived,
                         base::Unretained(this)),
          base::BindOnce(&Auction::OnComponentAuctionComplete,
                         base::Unretained(this), component_auction.get()));
    }
  }
  RequestBidderWorklets();
}

void AuctionRunner::Auction::StartReportingPhase(
    AuctionPhaseCompletionCallback reporting_phase_callback) {
  DCHECK(reporting_phase_callback);
  DCHECK(!load_interest_groups_phase_callback_);
  DCHECK(!bidding_and_scoring_phase_callback_);
  DCHECK(!reporting_phase_callback_);
  DCHECK(!final_auction_result_);
  DCHECK(top_bid_);

  reporting_phase_callback_ = std::move(reporting_phase_callback);

  // Component auctions unload their seller worklets on completion, so need to
  // reload the seller worklet in the case of a component auction.
  if (!seller_worklet_handle_) {
    DCHECK(is_component_auction_);
    if (!auction_worklet_manager_->RequestSellerWorklet(
            config_->decision_logic_url, config_->trusted_scoring_signals_url,
            base::BindOnce(&Auction::ReportSellerResult,
                           base::Unretained(this)),
            base::BindOnce(&Auction::OnWinningComponentSellerWorkletFatalError,
                           base::Unretained(this)),
            seller_worklet_handle_)) {
      return;
    }
  }

  ReportSellerResult();
}

void AuctionRunner::Auction::ClosePipes() {
  // This is needed in addition to closing worklet pipes since the callbacks
  // passed to Mojo aren't currently cancellable.
  weak_ptr_factory_.InvalidateWeakPtrs();

  for (BidState& bid_state : bid_states_) {
    bid_state.worklet_handle.reset();
  }
  seller_worklet_handle_.reset();

  // Close pipes for component auctions as well.
  for (auto& component_auction : component_auctions_) {
    component_auction->ClosePipes();
  }
}

void AuctionRunner::Auction::GetInterestGroupsThatBid(
    InterestGroupSet& interest_groups) const {
  if (!all_bids_scored_)
    return;

  for (const BidState& bid_state : bid_states_) {
    if (bid_state.made_bid) {
      interest_groups.emplace(std::pair(bid_state.bidder.interest_group.owner,
                                        bid_state.bidder.interest_group.name));
    }
  }

  // Retrieve data from component auctions as well.
  for (auto& component_auction : component_auctions_) {
    component_auction->GetInterestGroupsThatBid(interest_groups);
  }
}

void AuctionRunner::Auction::TakeDebugReportUrls(
    std::vector<GURL>& debug_win_report_urls,
    std::vector<GURL>& debug_loss_report_urls) {
  if (!all_bids_scored_)
    return;

  // Set `winner` to the element of `bid_states_` that won the entire auction,
  // if there is one.
  //
  // In a component auction, the highest bid may have lost the top-level
  // auction, and we want to report that as a loss. In this case, AuctionResult
  // will be kComponentLostAuction.
  //
  // Also for the top-level Auction in the case a component Auctions bid won,
  // the highest bid's BidState and its reporting URLs are stored with the
  // component auction, so the component Auction will be the one populate
  // `debug_win_report_urls`.
  BidState* winner = nullptr;
  if (final_auction_result_ == AuctionResult::kSuccess &&
      top_bid_->bid->auction == this) {
    winner = top_bid_->bid->bid_state;
  }

  for (BidState& bid_state : bid_states_) {
    if (&bid_state == winner) {
      if (winner->bidder_debug_win_report_url.has_value()) {
        debug_win_report_urls.emplace_back(
            std::move(winner->bidder_debug_win_report_url).value());
      }
      if (winner->seller_debug_win_report_url.has_value()) {
        debug_win_report_urls.emplace_back(
            std::move(winner->seller_debug_win_report_url).value());
      }
      if (winner->top_level_seller_debug_win_report_url.has_value()) {
        debug_win_report_urls.emplace_back(
            std::move(winner->top_level_seller_debug_win_report_url).value());
      }
      continue;
    }
    if (bid_state.bidder_debug_loss_report_url.has_value()) {
      debug_loss_report_urls.emplace_back(
          std::move(bid_state.bidder_debug_loss_report_url).value());
    }
    if (bid_state.seller_debug_loss_report_url.has_value()) {
      debug_loss_report_urls.emplace_back(
          std::move(bid_state.seller_debug_loss_report_url).value());
    }
    if (bid_state.top_level_seller_debug_loss_report_url.has_value()) {
      debug_loss_report_urls.emplace_back(
          std::move(bid_state.top_level_seller_debug_loss_report_url).value());
    }
  }

  // Retrieve data from component auctions as well.
  for (auto& component_auction : component_auctions_) {
    component_auction->TakeDebugReportUrls(debug_win_report_urls,
                                           debug_loss_report_urls);
  }
}

std::vector<GURL> AuctionRunner::Auction::TakeReportUrls() {
  DCHECK_EQ(*final_auction_result_, AuctionResult::kSuccess);

  // Retrieve data from winning component auction as well, if a bid from a
  // component auction won.
  if (top_bid_->bid->auction != this) {
    std::vector<GURL> nested_report_urls =
        top_bid_->bid->auction->TakeReportUrls();
    report_urls_.insert(report_urls_.begin(), nested_report_urls.begin(),
                        nested_report_urls.end());
  }
  return std::move(report_urls_);
}

std::vector<std::string> AuctionRunner::Auction::TakeErrors() {
  for (auto& component_auction : component_auctions_) {
    std::vector<std::string> errors = component_auction->TakeErrors();
    errors_.insert(errors_.begin(), errors.begin(), errors.end());
  }
  return std::move(errors_);
}

AuctionRunner::ScoredBid* AuctionRunner::Auction::top_bid() {
  DCHECK(all_bids_scored_);
  DCHECK(top_bid_);
  return top_bid_.get();
}

void AuctionRunner::Auction::OnInterestGroupRead(
    std::vector<StorageInterestGroup> interest_groups) {
  ++num_owners_loaded_;
  if (!interest_groups.empty()) {
    for (auto bidder = std::make_move_iterator(interest_groups.begin());
         bidder != std::make_move_iterator(interest_groups.end()); ++bidder) {
      // Ignore interest groups with no bidding script or no ads.
      if (!bidder->interest_group.bidding_url)
        continue;
      if (bidder->interest_group.ads->empty())
        continue;
      bid_states_.emplace_back(BidState());
      bid_states_.back().bidder = std::move(*bidder);
    }
    ++num_owners_with_interest_groups_;
  }
  OnOneLoadCompleted();
}

void AuctionRunner::Auction::OnComponentInterestGroupsRead(
    AuctionList::iterator component_auction,
    bool success) {
  num_owners_loaded_ += (*component_auction)->num_owners_loaded_;
  num_owners_with_interest_groups_ +=
      (*component_auction)->num_owners_with_interest_groups_;

  // Erase component auctions that failed to load anything, so they won't be
  // invoked in the generate bid phase. This is not a problem in the reporting
  // phase, as the top-level auction knows which component auction, if any, won.
  if (!success)
    component_auctions_.erase(component_auction);
  OnOneLoadCompleted();
}

void AuctionRunner::Auction::OnOneLoadCompleted() {
  DCHECK_GT(num_pending_loads_, 0u);
  --num_pending_loads_;

  // Wait for more buyers to be loaded, if there are still some pending.
  if (num_pending_loads_ > 0)
    return;

  // Record histograms about the interest groups participating in the auction.
  // TODO(mmenke): Record histograms for component auctions.
  if (!is_component_auction_) {
    // Only record histograms if there were interest groups that could
    // theoretically participate in the auction.
    if (num_owners_loaded_ > 0) {
      int num_interest_groups = bid_states_.size();
      for (auto& component_auction : component_auctions_) {
        // This double-counts interest groups that are participating in multiple
        // auctions.
        num_interest_groups += component_auction->bid_states_.size();
      }

      UMA_HISTOGRAM_COUNTS_1000("Ads.InterestGroup.Auction.NumInterestGroups",
                                num_interest_groups);
      UMA_HISTOGRAM_COUNTS_100(
          "Ads.InterestGroup.Auction.NumOwnersWithInterestGroups",
          num_owners_with_interest_groups_);
    }
  }

  // If there are no bidders in this auction and no component auctions with
  // bidders, either, fail the auction.
  if (bid_states_.empty() && component_auctions_.empty()) {
    OnStartLoadInterestGroupsPhaseComplete(AuctionResult::kNoInterestGroups);
    return;
  }

  // There are bidders that can generate bids, so complete without a final
  // result.
  OnStartLoadInterestGroupsPhaseComplete(
      /*auction_result=*/AuctionResult::kSuccess);
}

void AuctionRunner::Auction::OnStartLoadInterestGroupsPhaseComplete(
    AuctionResult auction_result) {
  DCHECK(load_interest_groups_phase_callback_);
  DCHECK(!final_auction_result_);

  // `final_auction_result_` should only be set to kSuccess when the entire
  // auction is complete.
  bool success = auction_result == AuctionResult::kSuccess;
  if (!success)
    final_auction_result_ = auction_result;
  std::move(load_interest_groups_phase_callback_).Run(success);
}

void AuctionRunner::Auction::OnComponentSellerWorkletReceived() {
  DCHECK_GT(pending_component_seller_worklet_requests_, 0u);
  --pending_component_seller_worklet_requests_;
  if (pending_component_seller_worklet_requests_ == 0)
    RequestSellerWorklet();
}

void AuctionRunner::Auction::RequestSellerWorklet() {
  if (auction_worklet_manager_->RequestSellerWorklet(
          config_->decision_logic_url, config_->trusted_scoring_signals_url,
          base::BindOnce(&Auction::OnSellerWorkletReceived,
                         base::Unretained(this)),
          base::BindOnce(&Auction::OnSellerWorkletFatalError,
                         base::Unretained(this)),
          seller_worklet_handle_)) {
    OnSellerWorkletReceived();
  }
}

void AuctionRunner::Auction::OnSellerWorkletReceived() {
  DCHECK(!seller_worklet_received_);

  if (on_seller_receiver_callback_)
    std::move(on_seller_receiver_callback_).Run();

  seller_worklet_received_ = true;

  auto unscored_bids = std::move(unscored_bids_);
  for (auto& unscored_bid : unscored_bids) {
    ScoreBidIfReady(std::move(unscored_bid));
  }
  // No more unscored bids should be added, once the seller worklet has been
  // received.
  DCHECK(unscored_bids_.empty());
}

void AuctionRunner::Auction::RequestBidderWorklets() {
  // Request processes for all bidder worklets.
  for (auto& bid_state : bid_states_) {
    if (RequestBidderWorklet(
            bid_state,
            base::BindOnce(&Auction::OnBidderWorkletReceived,
                           base::Unretained(this), &bid_state),
            base::BindOnce(&Auction::OnBidderWorkletGenerateBidFatalError,
                           base::Unretained(this), &bid_state))) {
      OnBidderWorkletReceived(&bid_state);
    }
  }
}

void AuctionRunner::Auction::OnSellerWorkletFatalError(
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  AuctionResult result;
  switch (fatal_error_type) {
    case AuctionWorkletManager::FatalErrorType::kScriptLoadFailed:
      result = AuctionResult::kSellerWorkletLoadFailed;
      break;
    case AuctionWorkletManager::FatalErrorType::kWorkletCrash:
      result = AuctionResult::kSellerWorkletCrashed;
      break;
  }

  // The seller worklet can crash in either the bidding or selling phase. Call
  // the appropriate method depending on the current phase.
  if (bidding_and_scoring_phase_callback_) {
    OnBiddingAndScoringComplete(result, errors);
    return;
  }
  OnReportingPhaseComplete(result, errors);
}

void AuctionRunner::Auction::OnBidderWorkletReceived(BidState* bid_state) {
  const blink::InterestGroup& interest_group = bid_state->bidder.interest_group;
  bid_state->worklet_handle->GetBidderWorklet()->GenerateBid(
      auction_worklet::mojom::BidderWorkletNonSharedParams::New(
          interest_group.name, interest_group.trusted_bidding_signals_keys,
          interest_group.user_bidding_signals, interest_group.ads,
          interest_group.ad_components),
      config_->auction_ad_config_non_shared_params->auction_signals,
      PerBuyerSignals(bid_state), PerBuyerTimeout(bid_state), config_->seller,
      bid_state->bidder.bidding_browser_signals.Clone(), auction_start_time_,
      base::BindOnce(&Auction::OnGenerateBidComplete,
                     weak_ptr_factory_.GetWeakPtr(), bid_state));

  // Invoke SendPendingSignalsRequests() asynchronously, if necessary. Do this
  // asynchronously so that all GenerateBid() calls that share a BidderWorklet
  // will have been invoked before the first SendPendingSignalsRequests() call.
  //
  // This relies on AuctionWorkletManager::Handle invoking all the callbacks
  // listening for creation of the same BidderWorklet synchronously.
  if (interest_group.trusted_bidding_signals_keys &&
      interest_group.trusted_bidding_signals_keys->size() > 0) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&Auction::SendPendingSignalsRequestsForBidder,
                                  weak_ptr_factory_.GetWeakPtr(), bid_state));
  }
}

void AuctionRunner::Auction::SendPendingSignalsRequestsForBidder(
    BidState* bid_state) {
  // Don't invoke callback if worklet was unloaded in the meantime.
  if (bid_state->worklet_handle)
    bid_state->worklet_handle->GetBidderWorklet()->SendPendingSignalsRequests();
}

void AuctionRunner::Auction::OnBidderWorkletGenerateBidFatalError(
    BidState* bid_state,
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  if (fatal_error_type ==
      AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
    // Ignore default error message in case of crash. Instead, use a more
    // specific one.
    OnGenerateBidComplete(
        bid_state, auction_worklet::mojom::BidderWorkletBidPtr(),
        /*bidding_signals_data_version=*/0,
        /*has_bidding_signals_data_version=*/false,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        {base::StrCat({bid_state->bidder.interest_group.bidding_url->spec(),
                       " crashed while trying to run generateBid()."})});
    return;
  }

  // Otherwise, use error message from the worklet.
  OnGenerateBidComplete(bid_state,
                        auction_worklet::mojom::BidderWorkletBidPtr(),
                        /*bidding_signals_data_version=*/0,
                        /*has_bidding_signals_data_version=*/false,
                        /*debug_loss_report_url=*/absl::nullopt,
                        /*debug_win_report_url=*/absl::nullopt, errors);
}

void AuctionRunner::Auction::OnGenerateBidComplete(
    BidState* state,
    auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
    uint32_t bidding_signals_data_version,
    bool has_bidding_signals_data_version,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url,
    const std::vector<std::string>& errors) {
  DCHECK(!state->made_bid);
  DCHECK_GT(num_bids_not_sent_to_seller_worklet_, 0);
  DCHECK_GT(outstanding_bids_, 0);

  absl::optional<uint32_t> maybe_bidding_signals_data_version;
  if (has_bidding_signals_data_version)
    maybe_bidding_signals_data_version = bidding_signals_data_version;

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // Release the worklet. If it wins the auction, it will be requested again to
  // invoke its ReportWin() method.
  state->worklet_handle.reset();

  // Ignore invalid bids.
  std::unique_ptr<Bid> bid;
  std::string ad_metadata;
  // `mojo_bid` is null if the worklet doesn't bid, or if the bidder worklet
  // fails to load / crashes.
  if (mojo_bid) {
    bid = TryToCreateBid(std::move(mojo_bid), *state,
                         maybe_bidding_signals_data_version,
                         debug_loss_report_url, debug_win_report_url);
    if (bid)
      state->bidder_debug_loss_report_url = std::move(debug_loss_report_url);
  } else {
    // Bidders who do not bid are allowed to get loss report.
    state->bidder_debug_loss_report_url = std::move(debug_loss_report_url);
  }

  if (!bid) {
    OnNoBid();
    return;
  }

  state->bidder_debug_win_report_url = std::move(debug_win_report_url);
  state->made_bid = true;
  ScoreBidIfReady(std::move(bid));
}

void AuctionRunner::Auction::OnComponentAuctionComplete(
    Auction* component_auction,
    bool success) {
  if (!success) {
    OnNoBid();
    return;
  }

  // Clone the bid of the component auction.
  //
  // TODO(mmenke): When the component auction can make its own bid, need a way
  // to track both the original bid price and the modified one.
  ScoreBidIfReady(std::make_unique<Bid>(*component_auction->top_bid()->bid));
}

void AuctionRunner::Auction::OnNoBid() {
  --num_bids_not_sent_to_seller_worklet_;
  --outstanding_bids_;

  // If this is the only bid that yet to be sent to the seller worklet, and
  // the seller worklet has loaded, then tell the seller worklet to send any
  // pending scoring signals request to complete the auction more quickly.
  if (num_bids_not_sent_to_seller_worklet_ == 0 && seller_worklet_received_)
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();

  MaybeCompleteBiddingAndScoringPhase();
}

void AuctionRunner::Auction::ScoreBidIfReady(std::unique_ptr<Bid> bid) {
  DCHECK(bid);
  DCHECK_GT(num_bids_not_sent_to_seller_worklet_, 0);
  DCHECK_GT(outstanding_bids_, 0);
  DCHECK(bid->bid_state->made_bid);

  // If seller worklet hasn't been received yet, wait until it is.
  if (!seller_worklet_received_) {
    unscored_bids_.emplace_back(std::move(bid));
    return;
  }

  Bid* bid_raw = bid.get();
  seller_worklet_handle_->GetSellerWorklet()->ScoreAd(
      bid_raw->ad_metadata, bid_raw->bid,
      config_->auction_ad_config_non_shared_params.Clone(),
      bid_raw->interest_group->owner, bid_raw->render_url,
      bid_raw->ad_components ? *bid_raw->ad_components : std::vector<GURL>(),
      bid_raw->bid_duration.InMilliseconds(),
      base::BindOnce(&Auction::OnBidScored, weak_ptr_factory_.GetWeakPtr(),
                     std::move(bid)));

  // If this was the last bid that needed to be passed to ScoreAd(), tell the
  // SellerWorklet no more bids are coming, so it can send a request for any
  // needed scoring signals now, if needed.
  --num_bids_not_sent_to_seller_worklet_;
  if (num_bids_not_sent_to_seller_worklet_ == 0) {
    seller_worklet_handle_->GetSellerWorklet()->SendPendingSignalsRequests();
  }
}

void AuctionRunner::Auction::OnBidScored(
    std::unique_ptr<Bid> bid,
    double score,
    uint32_t data_version,
    bool has_data_version,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url,
    const std::vector<std::string>& errors) {
  --outstanding_bids_;

  // If `debug_loss_report_url` or `debug_win_report_url` is not a valid HTTPS
  // URL, the auction should fail because the worklet is compromised.
  if (debug_loss_report_url.has_value() &&
      !IsUrlValid(debug_loss_report_url.value())) {
    mojo::ReportBadMessage("Invalid seller debugging loss report URL");
    OnBiddingAndScoringComplete(AuctionResult::kBadMojoMessage);
    return;
  }
  if (debug_win_report_url.has_value() &&
      !IsUrlValid(debug_win_report_url.value())) {
    mojo::ReportBadMessage("Invalid seller debugging win report URL");
    OnBiddingAndScoringComplete(AuctionResult::kBadMojoMessage);
    return;
  }
  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // Use separate fields for component and top-level seller reports, so both can
  // send debug reports.
  if (bid->auction == this) {
    bid->bid_state->seller_debug_loss_report_url =
        std::move(debug_loss_report_url);
    bid->bid_state->seller_debug_win_report_url =
        std::move(debug_win_report_url);
  } else {
    bid->bid_state->top_level_seller_debug_loss_report_url =
        std::move(debug_loss_report_url);
    bid->bid_state->top_level_seller_debug_win_report_url =
        std::move(debug_win_report_url);
  }

  bool is_top_bid = false;

  // A score <= 0 means the seller rejected the bid.
  if (score > 0) {
    if (!top_bid_ || score > top_bid_->score) {
      // If there's no previous top bidder, or the bidder has the highest score,
      // need to replace the previous top bidder.
      num_top_bids_ = 1;
      is_top_bid = true;
    } else if (score == top_bid_->score) {
      // If there's a tie, replace the top-bidder with 1-in-`num_top_bidders_`
      // chance. This is the select random value from a stream with fixed
      // storage problem.
      ++num_top_bids_;
      if (1 == base::RandInt(1, num_top_bids_))
        is_top_bid = true;
    }
  }

  if (is_top_bid) {
    top_bid_ = std::make_unique<ScoredBid>(
        score, has_data_version ? data_version : absl::optional<uint32_t>(),
        std::move(bid));
  }

  MaybeCompleteBiddingAndScoringPhase();
}

absl::optional<std::string> AuctionRunner::Auction::PerBuyerSignals(
    const BidState* state) {
  const auto& per_buyer_signals =
      config_->auction_ad_config_non_shared_params->per_buyer_signals;
  if (per_buyer_signals.has_value()) {
    auto it =
        per_buyer_signals.value().find(state->bidder.interest_group.owner);
    if (it != per_buyer_signals.value().end())
      return it->second;
  }
  return absl::nullopt;
}

absl::optional<base::TimeDelta> AuctionRunner::Auction::PerBuyerTimeout(
    const BidState* state) {
  const auto& per_buyer_timeouts =
      config_->auction_ad_config_non_shared_params->per_buyer_timeouts;
  if (per_buyer_timeouts.has_value()) {
    auto it =
        per_buyer_timeouts.value().find(state->bidder.interest_group.owner);
    if (it != per_buyer_timeouts.value().end()) {
      // Any per buyer timeout higher than kMaxPerBuyerTimeout ms will be
      // clamped to kMaxPerBuyerTimeout ms.
      return std::min(it->second, kMaxPerBuyerTimeout);
    }
  }
  const auto& all_buyers_timeout =
      config_->auction_ad_config_non_shared_params->all_buyers_timeout;
  if (all_buyers_timeout.has_value())
    return std::min(all_buyers_timeout.value(), kMaxPerBuyerTimeout);
  return absl::nullopt;
}

void AuctionRunner::Auction::MaybeCompleteBiddingAndScoringPhase() {
  if (!AllBidsScored())
    return;

  // Since all bids have been scored, they also should have all been sent to the
  // SellerWorklet by this point.
  DCHECK_EQ(0, num_bids_not_sent_to_seller_worklet_);

  all_bids_scored_ = true;

  // If there's no winning bid, fail with kAllBidsRejected if there were any
  // bids. Otherwise, fail with kNoBids.
  if (!top_bid_) {
    for (BidState& bid_state : bid_states_) {
      if (bid_state.made_bid) {
        OnBiddingAndScoringComplete(AuctionResult::kAllBidsRejected);
        return;
      }
    }
    OnBiddingAndScoringComplete(AuctionResult::kNoBids);
    return;
  }

  OnBiddingAndScoringComplete(AuctionResult::kSuccess);
}

void AuctionRunner::Auction::OnBiddingAndScoringComplete(
    AuctionResult auction_result,
    const std::vector<std::string>& errors) {
  DCHECK(bidding_and_scoring_phase_callback_);
  DCHECK(!final_auction_result_);

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // If this is a component auction, have to unload the seller worklet handle to
  // avoid deadlock. Otherwise, loading the top-level seller worklet may be
  // blocked by component seller worklets taking up all the quota.
  if (is_component_auction_)
    seller_worklet_handle_.reset();

  // If the seller loaded callback hasn't been invoked yet, call it now. This is
  // needed in the case the phase ended without receiving the seller worklet
  // (e.g., in the case no bidder worklet bids).
  if (on_seller_receiver_callback_)
    std::move(on_seller_receiver_callback_).Run();

  bool success = auction_result == AuctionResult::kSuccess;
  if (!success) {
    // Close all pipes, to prevent any pending callbacks from being invoked if
    // this phase is being completed due to a fatal error, like the seller
    // worklet failing to load.
    ClosePipes();

    // `final_auction_result_` should only be set to kSuccess when the entire
    // auction is complete.
    final_auction_result_ = auction_result;
  }

  // If this is a top-level auction with component auction, update final state
  // of all successfully completed component auctions with bids that did not win
  // to reflect a loss.
  for (auto& component_auction : component_auctions_) {
    // Leave the state of the winning component auction alone, if the winning
    // bid is from a component auction.
    if (top_bid_ && top_bid_->bid->auction == component_auction.get())
      continue;
    if (component_auction->final_auction_result_)
      continue;
    component_auction->final_auction_result_ =
        AuctionResult::kComponentLostAuction;
  }

  std::move(bidding_and_scoring_phase_callback_).Run(success);
}

void AuctionRunner::Auction::ReportSellerResult() {
  DCHECK(seller_worklet_handle_);
  DCHECK(reporting_phase_callback_);

  seller_worklet_handle_->GetSellerWorklet()->ReportResult(
      config_->auction_ad_config_non_shared_params.Clone(),
      top_bid_->bid->interest_group->owner, top_bid_->bid->render_url,
      top_bid_->bid->bid, top_bid_->score,
      top_bid_->scoring_signals_data_version.value_or(0),
      top_bid_->scoring_signals_data_version.has_value(),
      base::BindOnce(&Auction::OnReportSellerResultComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::Auction::OnReportSellerResultComplete(
    const absl::optional<std::string>& signals_for_winner,
    const absl::optional<GURL>& seller_report_url,
    const std::vector<std::string>& errors) {
  // There should be no other report URLs at this point.
  DCHECK(report_urls_.empty());

  // Release the seller worklet handle. It's no longer needed, and not releasing
  // it could theoretically trigger deadlock, if holding onto it prevents the
  // winning component seller from reloading its worklet. It could also trigger
  // an error if it crashes at this point, failing the auction unnecessarily.
  seller_worklet_handle_.reset();

  if (seller_report_url) {
    if (!IsUrlValid(*seller_report_url)) {
      mojo::ReportBadMessage("Invalid seller report URL");
      OnReportingPhaseComplete(AuctionResult::kBadMojoMessage);
      return;
    }

    report_urls_.push_back(*seller_report_url);
  }

  errors_.insert(errors_.end(), errors.begin(), errors.end());

  // If a the winning bid is from a nested component auction, need to call into
  // that Auction's report logic (which will invoke both that seller's
  // ReportResult() method, and the bidder's ReportWin()).
  if (top_bid_->bid->auction != this) {
    top_bid_->bid->auction->StartReportingPhase(
        base::BindOnce(&Auction::OnComponentAuctionReportingPhaseComplete,
                       base::Unretained(this)));
    return;
  }

  LoadBidderWorkletToReportBidWin(signals_for_winner);
}

void AuctionRunner::Auction::LoadBidderWorkletToReportBidWin(
    const absl::optional<std::string>& signals_for_winner) {
  // Worklet handle should have been destroyed once the bid was generated.
  DCHECK(!top_bid_->bid->bid_state->worklet_handle);

  if (RequestBidderWorklet(
          *top_bid_->bid->bid_state,
          base::BindOnce(&Auction::ReportBidWin, base::Unretained(this),
                         signals_for_winner),
          base::BindOnce(&Auction::OnWinningBidderWorkletFatalError,
                         base::Unretained(this)))) {
    ReportBidWin(signals_for_winner);
  }
}

void AuctionRunner::Auction::ReportBidWin(
    const absl::optional<std::string>& signals_for_winner) {
  DCHECK(top_bid_);

  std::string signals_for_winner_arg;
  if (signals_for_winner) {
    signals_for_winner_arg = *signals_for_winner;
  } else {
    // `signals_for_winner_arg` is passed as JSON, so need to pass "null" when
    // it's not provided. Pass in "null" instead of making the API take an
    // optional to limit the information provided to the untrusted BidderWorklet
    // process that's not part of the FLEDGE API. Unlikely to matter, but best
    // to be safe.
    signals_for_winner_arg = "null";
  }

  top_bid_->bid->bid_state->worklet_handle->GetBidderWorklet()->ReportWin(
      top_bid_->bid->interest_group->name,
      config_->auction_ad_config_non_shared_params->auction_signals,
      PerBuyerSignals(top_bid_->bid->bid_state), signals_for_winner_arg,
      top_bid_->bid->render_url, top_bid_->bid->bid, config_->seller,
      top_bid_->bid->bidding_signals_data_version.value_or(0),
      top_bid_->bid->bidding_signals_data_version.has_value(),
      base::BindOnce(&Auction::OnReportBidWinComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AuctionRunner::Auction::OnReportBidWinComplete(
    const absl::optional<GURL>& bidder_report_url,
    const std::vector<std::string>& errors) {
  // There should be at most one other report URL at this point.
  DCHECK_LE(report_urls_.size(), 1u);

  // The winning bidder worklet is no longer needed. Unload it to prevent a
  // fatal error notification.
  top_bid_->bid->bid_state->worklet_handle.reset();

  if (bidder_report_url) {
    if (!IsUrlValid(*bidder_report_url)) {
      mojo::ReportBadMessage("Invalid bidder report URL");
      OnReportingPhaseComplete(AuctionResult::kBadMojoMessage);
      return;
    }

    report_urls_.push_back(*bidder_report_url);
  }

  errors_.insert(errors_.end(), errors.begin(), errors.end());
  OnReportingPhaseComplete(AuctionResult::kSuccess);
}

void AuctionRunner::Auction::OnWinningComponentSellerWorkletFatalError(
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  // Crashes are considered fatal errors, while load errors currently are not.
  if (fatal_error_type ==
      AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
    OnReportingPhaseComplete(
        AuctionResult::kWinningComponentSellerWorkletCrashed,
        // Ignore default error message in case of crash. Instead, use a more
        // specific one.
        {base::StrCat({config_->decision_logic_url.spec(),
                       " crashed while trying to run reportResult()."})});
  } else {
    // An error while reloading the worklet to call ReportResult() does not
    // currently fail the auction.
    OnReportSellerResultComplete(/*signals_for_winner=*/absl::nullopt,
                                 /*seller_report_url=*/absl::nullopt, errors);
  }
}

void AuctionRunner::Auction::OnWinningBidderWorkletFatalError(
    AuctionWorkletManager::FatalErrorType fatal_error_type,
    const std::vector<std::string>& errors) {
  // Crashes are considered fatal errors, while load errors currently are not.
  if (fatal_error_type ==
      AuctionWorkletManager::FatalErrorType::kWorkletCrash) {
    OnReportingPhaseComplete(
        AuctionResult::kWinningBidderWorkletCrashed,
        // Ignore default error message in case of crash. Instead, use a more
        // specific one.
        {base::StrCat({top_bid_->bid->interest_group->bidding_url->spec(),
                       " crashed while trying to run reportWin()."})});
  } else {
    // An error while reloading the worklet to call ReportWin() does not
    // currently fail the auction.
    OnReportBidWinComplete(/*bidder_report_url=*/absl::nullopt, errors);
  }
}

void AuctionRunner::Auction::OnComponentAuctionReportingPhaseComplete(
    bool success) {
  // Inherit the success or error from the nested auction.
  OnReportingPhaseComplete(*top_bid_->bid->auction->final_auction_result_);
}

void AuctionRunner::Auction::OnReportingPhaseComplete(
    AuctionResult auction_result,
    const std::vector<std::string>& errors) {
  DCHECK(reporting_phase_callback_);
  DCHECK(!final_auction_result_);
  // There should be at most two report URLs.
  DCHECK_LE(report_urls_.size(), 2u);

  errors_.insert(errors_.end(), errors.begin(), errors.end());
  final_auction_result_ = auction_result;

  // Close all pipes, as they're no longer needed.
  ClosePipes();

  std::move(reporting_phase_callback_)
      .Run(auction_result == AuctionResult::kSuccess);
}

bool AuctionRunner::Auction::RequestBidderWorklet(
    BidState& bid_state,
    base::OnceClosure worklet_available_callback,
    AuctionWorkletManager::FatalErrorCallback fatal_error_callback) {
  DCHECK(!bid_state.worklet_handle);

  const blink::InterestGroup& interest_group = bid_state.bidder.interest_group;
  return auction_worklet_manager_->RequestBidderWorklet(
      interest_group.bidding_url.value_or(GURL()),
      interest_group.bidding_wasm_helper_url,
      interest_group.trusted_bidding_signals_url,
      std::move(worklet_available_callback), std::move(fatal_error_callback),
      bid_state.worklet_handle);
}

std::unique_ptr<AuctionRunner> AuctionRunner::CreateAndStart(
    AuctionWorkletManager* auction_worklet_manager,
    InterestGroupManagerImpl* interest_group_manager,
    blink::mojom::AuctionAdConfigPtr auction_config,
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback,
    RunAuctionCallback callback) {
  std::unique_ptr<AuctionRunner> instance(
      new AuctionRunner(auction_worklet_manager, interest_group_manager,
                        std::move(auction_config), std::move(callback)));
  instance->StartAuction(is_interest_group_api_allowed_callback);
  return instance;
}

AuctionRunner::~AuctionRunner() = default;

void AuctionRunner::FailAuction() {
  DCHECK(callback_);

  // Can have loss URLs if the auction failed because the seller rejected all
  // bids.
  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);
  // Shouldn't have any win report URLs if nothing won the auction.
  DCHECK(debug_win_report_urls.empty());

  std::move(callback_).Run(
      this, /*render_url=*/absl::nullopt,
      /*ad_component_urls=*/absl::nullopt,
      /*report_urls=*/{}, std::move(debug_loss_report_urls),
      std::move(debug_win_report_urls), auction_.TakeErrors());
}

AuctionRunner::AuctionRunner(AuctionWorkletManager* auction_worklet_manager,
                             InterestGroupManagerImpl* interest_group_manager,
                             blink::mojom::AuctionAdConfigPtr auction_config,
                             RunAuctionCallback callback)
    : interest_group_manager_(interest_group_manager),
      owned_auction_config_(std::move(auction_config)),
      callback_(std::move(callback)),
      auction_(owned_auction_config_.get(),
               /*is_component_auction=*/false,
               auction_worklet_manager,
               interest_group_manager,
               /*auction_start_time=*/base::Time::Now()) {}

std::unique_ptr<AuctionRunner::Bid> AuctionRunner::Auction::TryToCreateBid(
    auction_worklet::mojom::BidderWorkletBidPtr mojo_bid,
    BidState& bid_state,
    const absl::optional<uint32_t>& bidding_signals_data_version,
    const absl::optional<GURL>& debug_loss_report_url,
    const absl::optional<GURL>& debug_win_report_url) {
  if (mojo_bid->bid <= 0 || std::isnan(mojo_bid->bid) ||
      !std::isfinite(mojo_bid->bid)) {
    mojo::ReportBadMessage("Invalid bid value");
    return nullptr;
  }

  if (mojo_bid->bid_duration.is_negative()) {
    mojo::ReportBadMessage("Invalid bid duration");
    return nullptr;
  }

  const blink::InterestGroup& interest_group = bid_state.bidder.interest_group;
  const blink::InterestGroup::Ad* matching_ad =
      FindMatchingAd(*interest_group.ads, mojo_bid->render_url);
  if (!matching_ad) {
    mojo::ReportBadMessage("Bid render URL must be a valid ad URL");
    return nullptr;
  }

  // Validate `ad_component` URLs, if present.
  if (mojo_bid->ad_components) {
    // Only InterestGroups with ad components should return bids with ad
    // components.
    if (!interest_group.ad_components) {
      mojo::ReportBadMessage("Unexpected non-null ad component list");
      return nullptr;
    }

    if (mojo_bid->ad_components->size() > blink::kMaxAdAuctionAdComponents) {
      mojo::ReportBadMessage("Too many ad component URLs");
      return nullptr;
    }

    // Validate each ad component URL is valid and appears in the interest
    // group's `ad_components` field.
    for (const GURL& ad_component_url : *mojo_bid->ad_components) {
      if (!FindMatchingAd(*interest_group.ad_components, ad_component_url)) {
        mojo::ReportBadMessage(
            "Bid ad components URL must match a valid ad component URL");
        return nullptr;
      }
    }
  }

  // Validate `debug_loss_report_url` and `debug_win_report_url`, if present.
  if (debug_loss_report_url.has_value() &&
      !IsUrlValid(debug_loss_report_url.value())) {
    mojo::ReportBadMessage("Invalid bidder debugging loss report URL");
    return nullptr;
  }
  if (debug_win_report_url.has_value() &&
      !IsUrlValid(debug_win_report_url.value())) {
    mojo::ReportBadMessage("Invalid bidder debugging win report URL");
    return nullptr;
  }

  return std::make_unique<Bid>(
      std::move(mojo_bid->ad), mojo_bid->bid, std::move(mojo_bid->render_url),
      std::move(mojo_bid->ad_components), mojo_bid->bid_duration,
      bidding_signals_data_version, matching_ad, &bid_state, this);
}

void AuctionRunner::StartAuction(
    IsInterestGroupApiAllowedCallback is_interest_group_api_allowed_callback) {
  auction_.StartLoadInterestGroupsPhase(
      is_interest_group_api_allowed_callback,
      base::BindOnce(&AuctionRunner::OnLoadInterestGroupsComplete,
                     base::Unretained(this)));
}

void AuctionRunner::OnLoadInterestGroupsComplete(bool success) {
  if (!success) {
    FailAuction();
    return;
  }

  auction_.StartBiddingAndScoringPhase(
      /*on_seller_receiver_callback=*/base::OnceClosure(),
      base::BindOnce(&AuctionRunner::OnBidsGeneratedAndScored,
                     base::Unretained(this)));
}

void AuctionRunner::OnBidsGeneratedAndScored(bool success) {
  InterestGroupSet interest_groups_that_bid;
  auction_.GetInterestGroupsThatBid(interest_groups_that_bid);
  for (const auto& interest_group : interest_groups_that_bid) {
    interest_group_manager_->RecordInterestGroupBid(
        /*owner=*/interest_group.first,
        /*name=*/interest_group.second);
  }
  if (!success) {
    FailAuction();
    return;
  }

  auction_.StartReportingPhase(base::BindOnce(
      &AuctionRunner::OnReportingPhaseComplete, base::Unretained(this)));
}

void AuctionRunner::OnReportingPhaseComplete(bool success) {
  if (!success) {
    FailAuction();
    return;
  }

  DCHECK(callback_);

  std::string ad_metadata;
  if (auction_.top_bid()->bid->bid_ad->metadata) {
    //`metadata` is already in JSON so no quotes are needed.
    ad_metadata = base::StringPrintf(
        R"({"render_url":"%s","metadata":%s})",
        auction_.top_bid()->bid->render_url.spec().c_str(),
        auction_.top_bid()->bid->bid_ad->metadata.value().c_str());
  } else {
    ad_metadata =
        base::StringPrintf(R"({"render_url":"%s"})",
                           auction_.top_bid()->bid->render_url.spec().c_str());
  }

  interest_group_manager_->RecordInterestGroupWin(
      auction_.top_bid()->bid->interest_group->owner,
      auction_.top_bid()->bid->interest_group->name, ad_metadata);

  std::vector<GURL> debug_win_report_urls;
  std::vector<GURL> debug_loss_report_urls;
  auction_.TakeDebugReportUrls(debug_win_report_urls, debug_loss_report_urls);

  std::move(callback_).Run(
      this, auction_.top_bid()->bid->render_url,
      auction_.top_bid()->bid->ad_components, auction_.TakeReportUrls(),
      std::move(debug_loss_report_urls), std::move(debug_win_report_urls),
      auction_.TakeErrors());
}

}  // namespace content
