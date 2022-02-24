// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-template.h"

namespace auction_worklet {

namespace {

// Converts `auction_config` back to JSON format, and appends to args.
// Returns true if conversion succeeded.
//
// The resulting object will look something like this (based on example from
// explainer):
//
// {
//  'seller': 'https://www.example-ssp.com/',
//  'decisionLogicUrl': 'https://www.example-ssp.com/seller.js',
//  'trustedScoringSignalsUrl': ...,
//  'interestGroupBuyers': ['https://www.example-dsp.com', 'https://buyer2.com',
//  ...], 'auctionSignals': {...}, 'sellerSignals': {...},
//  'perBuyerSignals': {'https://www.example-dsp.com': {...},
//                      'https://www.another-buyer.com': {...},
//                       ...},
//  'perBuyerTimeouts': {'https://www.example-dsp.com': 50,
//                       'https://www.another-buyer.com': 200,
//                       '*': 150,
//                       ...},
// }
bool AppendAuctionConfig(AuctionV8Helper* const v8_helper,
                         v8::Local<v8::Context> context,
                         const GURL& decision_logic_url,
                         const blink::mojom::AuctionAdConfigNonSharedParams&
                             auction_ad_config_non_shared_params,
                         std::vector<v8::Local<v8::Value>>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  v8::Local<v8::Object> auction_config_value = v8::Object::New(isolate);
  gin::Dictionary auction_config_dict(isolate, auction_config_value);
  if (!auction_config_dict.Set(
          "seller", url::Origin::Create(decision_logic_url).Serialize()) ||
      !auction_config_dict.Set("decisionLogicUrl", decision_logic_url.spec())) {
    return false;
  }

  if (auction_ad_config_non_shared_params.interest_group_buyers) {
    std::vector<v8::Local<v8::Value>> interest_group_buyers;
    for (const url::Origin& buyer :
         *auction_ad_config_non_shared_params.interest_group_buyers) {
      v8::Local<v8::String> v8_buyer;
      if (!v8_helper->CreateUtf8String(buyer.Serialize()).ToLocal(&v8_buyer))
        return false;
      interest_group_buyers.push_back(v8_buyer);
    }
    auction_config_dict.Set("interestGroupBuyers", interest_group_buyers);
  }

  if (auction_ad_config_non_shared_params.auction_signals.has_value() &&
      !v8_helper->InsertJsonValue(
          context, "auctionSignals",
          auction_ad_config_non_shared_params.auction_signals.value(),
          auction_config_value)) {
    return false;
  }

  if (auction_ad_config_non_shared_params.seller_signals.has_value() &&
      !v8_helper->InsertJsonValue(
          context, "sellerSignals",
          auction_ad_config_non_shared_params.seller_signals.value(),
          auction_config_value)) {
    return false;
  }

  if (auction_ad_config_non_shared_params.per_buyer_signals.has_value()) {
    v8::Local<v8::Object> per_buyer_value = v8::Object::New(isolate);
    for (const auto& kv :
         auction_ad_config_non_shared_params.per_buyer_signals.value()) {
      if (!v8_helper->InsertJsonValue(context, kv.first.Serialize(), kv.second,
                                      per_buyer_value)) {
        return false;
      }
    }
    auction_config_dict.Set("perBuyerSignals", per_buyer_value);
  }

  v8::Local<v8::Object> per_buyer_timeouts;
  if (auction_ad_config_non_shared_params.per_buyer_timeouts.has_value()) {
    per_buyer_timeouts = v8::Object::New(isolate);
    for (const auto& kv :
         auction_ad_config_non_shared_params.per_buyer_timeouts.value()) {
      if (!v8_helper->InsertJsonValue(
              context, kv.first.Serialize(),
              base::NumberToString(kv.second.InMilliseconds()),
              per_buyer_timeouts)) {
        return false;
      }
    }
  }
  if (auction_ad_config_non_shared_params.all_buyers_timeout.has_value()) {
    if (per_buyer_timeouts.IsEmpty())
      per_buyer_timeouts = v8::Object::New(isolate);
    if (!v8_helper->InsertJsonValue(
            context, "*",
            base::NumberToString(
                auction_ad_config_non_shared_params.all_buyers_timeout.value()
                    .InMilliseconds()),
            per_buyer_timeouts)) {
      return false;
    }
  }
  if (!per_buyer_timeouts.IsEmpty())
    auction_config_dict.Set("perBuyerTimeouts", per_buyer_timeouts);

  args->push_back(std::move(auction_config_value));
  return true;
}

}  // namespace

SellerWorklet::SellerWorklet(
    scoped_refptr<AuctionV8Helper> v8_helper,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& decision_logic_url,
    const absl::optional<GURL>& trusted_scoring_signals_url,
    const url::Origin& top_window_origin)
    : v8_runner_(v8_helper->v8_runner()),
      v8_helper_(std::move(v8_helper)),
      debug_id_(
          base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helper_.get())),
      url_loader_factory_(std::move(pending_url_loader_factory)),
      script_source_url_(decision_logic_url),
      trusted_signals_request_manager_(
          trusted_scoring_signals_url
              ? std::make_unique<TrustedSignalsRequestManager>(
                    TrustedSignalsRequestManager::Type::kScoringSignals,
                    url_loader_factory_.get(),
                    /*automatically_send_requests=*/true,
                    top_window_origin,
                    *trusted_scoring_signals_url,
                    v8_helper_.get())
              : nullptr),
      v8_state_(nullptr, base::OnTaskRunnerDeleter(v8_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  v8_state_ = std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
      new V8State(v8_helper_, debug_id_, decision_logic_url, top_window_origin,
                  weak_ptr_factory_.GetWeakPtr()),
      base::OnTaskRunnerDeleter(v8_runner_));

  paused_ = pause_for_debugger_on_start;
  if (!paused_)
    Start();
}

SellerWorklet::~SellerWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  debug_id_->AbortDebuggerPauses();
}

int SellerWorklet::context_group_id_for_testing() const {
  return debug_id_->context_group_id();
}

void SellerWorklet::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    blink::mojom::AuctionAdConfigNonSharedParamsPtr
        auction_ad_config_non_shared_params,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::vector<GURL>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    ScoreAdCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  score_ad_tasks_.emplace_front();

  auto score_ad_task = score_ad_tasks_.begin();
  score_ad_task->ad_metadata_json = ad_metadata_json;
  score_ad_task->bid = bid;
  score_ad_task->auction_ad_config_non_shared_params =
      std::move(auction_ad_config_non_shared_params);
  score_ad_task->browser_signal_interest_group_owner =
      browser_signal_interest_group_owner;
  score_ad_task->browser_signal_render_url = browser_signal_render_url;
  for (const GURL& url : browser_signal_ad_components) {
    score_ad_task->browser_signal_ad_components.emplace_back(url.spec());
  }
  score_ad_task->browser_signal_bidding_duration_msecs =
      browser_signal_bidding_duration_msecs;
  score_ad_task->callback = std::move(callback);

  // If `trusted_signals_request_manager_` exists, there's a trusted scoring
  // signals URL which needs to be fetched before the auction can be run.
  if (trusted_signals_request_manager_) {
    score_ad_task->trusted_scoring_signals_request =
        trusted_signals_request_manager_->RequestScoringSignals(
            browser_signal_render_url,
            score_ad_task->browser_signal_ad_components,
            base::BindOnce(&SellerWorklet::OnTrustedScoringSignalsDownloaded,
                           base::Unretained(this), score_ad_task));
    return;
  }

  ScoreAdIfReady(score_ad_task);
}

void SellerWorklet::SendPendingSignalsRequests() {
  if (trusted_signals_request_manager_)
    trusted_signals_request_manager_->StartBatchedTrustedSignalsRequest();
}

void SellerWorklet::ReportResult(
    blink::mojom::AuctionAdConfigNonSharedParamsPtr
        auction_ad_config_non_shared_params,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    double browser_signal_desirability,
    uint32_t scoring_signals_data_version,
    bool has_scoring_signals_data_version,
    ReportResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  report_result_tasks_.emplace_front();

  auto report_result_task = report_result_tasks_.begin();

  report_result_task->auction_ad_config_non_shared_params =
      std::move(auction_ad_config_non_shared_params);
  report_result_task->browser_signal_interest_group_owner =
      browser_signal_interest_group_owner;
  report_result_task->browser_signal_render_url = browser_signal_render_url;
  report_result_task->browser_signal_bid = browser_signal_bid;
  report_result_task->browser_signal_desirability = browser_signal_desirability;
  if (has_scoring_signals_data_version)
    report_result_task->scoring_signals_data_version =
        scoring_signals_data_version;
  report_result_task->callback = std::move(callback);

  // If not yet ready, need to wait for load to complete.
  if (!IsCodeReady())
    return;

  RunReportResult(report_result_task);
}

void SellerWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V8State::ConnectDevToolsAgent,
                     base::Unretained(v8_state_.get()), std::move(agent)));
}

SellerWorklet::ScoreAdTask::ScoreAdTask() = default;
SellerWorklet::ScoreAdTask::~ScoreAdTask() = default;

SellerWorklet::ReportResultTask::ReportResultTask() = default;
SellerWorklet::ReportResultTask::~ReportResultTask() = default;

SellerWorklet::V8State::V8State(
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    const GURL& decision_logic_url,
    const url::Origin& top_window_origin,
    base::WeakPtr<SellerWorklet> parent)
    : v8_helper_(std::move(v8_helper)),
      debug_id_(debug_id),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunnerHandle::Get()),
      decision_logic_url_(decision_logic_url),
      top_window_origin_(top_window_origin) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V8State::FinishInit, base::Unretained(this)));
}

void SellerWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = WorkletLoader::TakeScript(std::move(worklet_script));
}

void SellerWorklet::V8State::ScoreAd(
    const std::string& ad_metadata_json,
    double bid,
    blink::mojom::AuctionAdConfigNonSharedParamsPtr
        auction_ad_config_non_shared_params,
    scoped_refptr<TrustedSignals::Result> trusted_scoring_signals,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    const std::vector<std::string>& browser_signal_ad_components,
    uint32_t browser_signal_bidding_duration_msecs,
    ScoreAdCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();
  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  ForDebuggingOnlyBindings for_debugging_only_bindings(v8_helper_.get(),
                                                       global_template);

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext(global_template);
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!v8_helper_->AppendJsonValue(context, ad_metadata_json, &args)) {
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*scoring_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*errors=*/std::vector<std::string>());
    return;
  }

  args.push_back(gin::ConvertToV8(isolate, bid));

  if (!AppendAuctionConfig(v8_helper_.get(), context, decision_logic_url_,
                           *auction_ad_config_non_shared_params, &args)) {
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*scoring_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*errors=*/std::vector<std::string>());
    return;
  }

  v8::Local<v8::Value> trusted_scoring_signals_value;
  absl::optional<uint32_t> scoring_signals_data_version;
  if (trusted_scoring_signals) {
    trusted_scoring_signals_value = trusted_scoring_signals->GetScoringSignals(
        v8_helper_.get(), context, browser_signal_render_url,
        browser_signal_ad_components);
    scoring_signals_data_version = trusted_scoring_signals->GetDataVersion();
  } else {
    trusted_scoring_signals_value = v8::Null(isolate);
  }
  args.push_back(trusted_scoring_signals_value);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("biddingDurationMsec",
                                browser_signal_bidding_duration_msecs) ||
      (scoring_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 scoring_signals_data_version.value()))) {
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*scoring_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt,
        /*errors=*/std::vector<std::string>());
    return;
  }
  if (!browser_signal_ad_components.empty()) {
    if (!browser_signals_dict.Set("adComponents",
                                  browser_signal_ad_components)) {
      PostScoreAdCallbackToUserThread(
          std::move(callback), /*score=*/0,
          /*scoring_signals_data_version=*/absl::nullopt,
          /*debug_loss_report_url=*/absl::nullopt,
          /*debug_win_report_url=*/absl::nullopt,
          /*errors=*/std::vector<std::string>());
      return;
    }
  }
  args.push_back(browser_signals);

  v8::Local<v8::Value> score_ad_result;
  double score;
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeSellerWorkletScoringStart");
  if (!v8_helper_
           ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                       "scoreAd", args, /*script_timeout=*/absl::nullopt,
                       errors_out)
           .ToLocal(&score_ad_result)) {
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*scoring_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt, std::move(errors_out));
    return;
  }

  if (!gin::ConvertFromV8(isolate, score_ad_result, &score) ||
      std::isnan(score) || !std::isfinite(score)) {
    errors_out.push_back(
        base::StrCat({decision_logic_url_.spec(),
                      " scoreAd() did not return a valid number."}));

    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0,
        /*scoring_signals_data_version=*/absl::nullopt,
        /*debug_loss_report_url=*/absl::nullopt,
        /*debug_win_report_url=*/absl::nullopt, std::move(errors_out));
    return;
  }

  if (score <= 0) {
    // Keep debug report URLs because we want to send debug loss reports if
    // seller rejected all bids.
    PostScoreAdCallbackToUserThread(
        std::move(callback), /*score=*/0, scoring_signals_data_version,
        for_debugging_only_bindings.TakeLossReportUrl(),
        for_debugging_only_bindings.TakeWinReportUrl(), std::move(errors_out));
    return;
  }

  PostScoreAdCallbackToUserThread(
      std::move(callback), score, scoring_signals_data_version,
      for_debugging_only_bindings.TakeLossReportUrl(),
      for_debugging_only_bindings.TakeWinReportUrl(), std::move(errors_out));
}

void SellerWorklet::V8State::ReportResult(
    blink::mojom::AuctionAdConfigNonSharedParamsPtr
        auction_ad_config_non_shared_params,
    const url::Origin& browser_signal_interest_group_owner,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    double browser_signal_desirability,
    absl::optional<uint32_t> scoring_signals_data_version,
    ReportResultCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper_.get());
  v8::Isolate* isolate = v8_helper_->isolate();

  v8::Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate);
  ReportBindings report_bindings(v8_helper_.get(), global_template);

  // Short lived context, to avoid leaking data at global scope between either
  // repeated calls to this worklet, or to calls to any other worklet.
  v8::Local<v8::Context> context = v8_helper_->CreateContext(global_template);
  v8::Context::Scope context_scope(context);

  std::vector<v8::Local<v8::Value>> args;
  if (!AppendAuctionConfig(v8_helper_.get(), context, decision_logic_url_,
                           *auction_ad_config_non_shared_params, &args)) {
    PostReportResultCallbackToUserThread(std::move(callback),
                                         /*signals_for_winner=*/absl::nullopt,
                                         /*report_url=*/absl::nullopt,
                                         /*errors=*/std::vector<std::string>());
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          browser_signal_interest_group_owner.Serialize()) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("bid", browser_signal_bid) ||
      !browser_signals_dict.Set("desirability", browser_signal_desirability) ||
      (scoring_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 scoring_signals_data_version.value()))) {
    PostReportResultCallbackToUserThread(std::move(callback),
                                         /*signals_for_winner=*/absl::nullopt,
                                         /*report_url=*/absl::nullopt,
                                         /*errors=*/std::vector<std::string>());
    return;
  }
  args.push_back(browser_signals);

  v8::Local<v8::Value> signals_for_winner_value;
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeSellerWorkletReportingStart");
  if (!v8_helper_
           ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                       "reportResult", args, /*script_timeout=*/absl::nullopt,
                       errors_out)
           .ToLocal(&signals_for_winner_value)) {
    PostReportResultCallbackToUserThread(
        std::move(callback), /*signals_for_winner=*/absl::nullopt,
        /*report_url=*/absl::nullopt, std::move(errors_out));
    return;
  }

  // Consider lack of error but no return value type, or a return value that
  // can't be converted to JSON a valid result.
  std::string signals_for_winner;
  if (!v8_helper_->ExtractJson(context, signals_for_winner_value,
                               &signals_for_winner)) {
    signals_for_winner = "null";
  }

  PostReportResultCallbackToUserThread(
      std::move(callback), std::move(signals_for_winner),
      report_bindings.report_url(), std::move(errors_out));
}

void SellerWorklet::V8State::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->ConnectDevToolsAgent(std::move(agent), user_thread_, *debug_id_);
}

SellerWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
}

void SellerWorklet::V8State::FinishInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  debug_id_->SetResumeCallback(base::BindOnce(
      &SellerWorklet::V8State::PostResumeToUserThread, parent_, user_thread_));
}

// static
void SellerWorklet::V8State::PostResumeToUserThread(
    base::WeakPtr<SellerWorklet> parent,
    scoped_refptr<base::SequencedTaskRunner> user_thread) {
  // This is static since it's called from debugging, not SellerWorklet,
  // so the usual guarantee that SellerWorklet posts things before posting
  // V8State destruction is irrelevant.
  user_thread->PostTask(FROM_HERE,
                        base::BindOnce(&SellerWorklet::ResumeIfPaused, parent));
}

void SellerWorklet::V8State::PostScoreAdCallbackToUserThread(
    ScoreAdCallbackInternal callback,
    double score,
    absl::optional<uint32_t> scoring_signals_data_version,
    absl::optional<GURL> debug_loss_report_url,
    absl::optional<GURL> debug_win_report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), score, scoring_signals_data_version,
                     std::move(debug_loss_report_url),
                     std::move(debug_win_report_url), std::move(errors)));
}

void SellerWorklet::V8State::PostReportResultCallbackToUserThread(
    ReportResultCallbackInternal callback,
    absl::optional<std::string> signals_for_winner,
    absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), std::move(signals_for_winner),
                     std::move(report_url), std::move(errors)));
}

void SellerWorklet::ResumeIfPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!paused_)
    return;

  paused_ = false;
  Start();
}

void SellerWorklet::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(!paused_);

  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(), script_source_url_, v8_helper_, debug_id_,
      base::BindOnce(&SellerWorklet::OnDownloadComplete,
                     base::Unretained(this)));
}

void SellerWorklet::OnDownloadComplete(WorkletLoader::Result worklet_script,
                                       absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  worklet_loader_.reset();

  // On failure, delete `this`, as it can't do anything without a loaded script.
  bool success = worklet_script.success();
  if (!success) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  // The error message, if any, will be appended to all invoked ScoreAd() and
  // ReportResult() callbacks.
  load_script_error_msg_ = std::move(error_msg);

  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&SellerWorklet::V8State::SetWorkletScript,
                                      base::Unretained(v8_state_.get()),
                                      std::move(worklet_script)));

  for (auto score_ad_task = score_ad_tasks_.begin();
       score_ad_task != score_ad_tasks_.end(); ++score_ad_task) {
    ScoreAdIfReady(score_ad_task);
  }

  for (auto report_result_task = report_result_tasks_.begin();
       report_result_task != report_result_tasks_.end(); ++report_result_task) {
    RunReportResult(report_result_task);
  }
}

void SellerWorklet::OnTrustedScoringSignalsDownloaded(
    ScoreAdTaskList::iterator task,
    scoped_refptr<TrustedSignals::Result> result,
    absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->trusted_scoring_signals_error_msg = std::move(error_msg);
  task->trusted_scoring_signals_result = std::move(result);
  // Clean up single-use object, now that it has done its job.
  task->trusted_scoring_signals_request.reset();

  ScoreAdIfReady(task);
}

void SellerWorklet::ScoreAdIfReady(ScoreAdTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (task->trusted_scoring_signals_request || !IsCodeReady())
    return;

  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ScoreAd, base::Unretained(v8_state_.get()),
          task->ad_metadata_json, task->bid,
          std::move(task->auction_ad_config_non_shared_params),
          std::move(task->trusted_scoring_signals_result),
          std::move(task->browser_signal_interest_group_owner),
          std::move(task->browser_signal_render_url),
          std::move(task->browser_signal_ad_components),
          task->browser_signal_bidding_duration_msecs,
          base::BindOnce(&SellerWorklet::DeliverScoreAdCallbackOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void SellerWorklet::DeliverScoreAdCallbackOnUserThread(
    ScoreAdTaskList::iterator task,
    double score,
    absl::optional<uint32_t> scoring_signals_data_version,
    absl::optional<GURL> debug_loss_report_url,
    absl::optional<GURL> debug_win_report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (load_script_error_msg_)
    errors.insert(errors.begin(), load_script_error_msg_.value());
  if (task->trusted_scoring_signals_error_msg)
    errors.insert(errors.begin(), *task->trusted_scoring_signals_error_msg);

  std::move(task->callback)
      .Run(score, scoring_signals_data_version.value_or(0),
           scoring_signals_data_version.has_value(), debug_loss_report_url,
           debug_win_report_url, errors);
  score_ad_tasks_.erase(task);
}

void SellerWorklet::RunReportResult(ReportResultTaskList::iterator task) {
  DCHECK(IsCodeReady());

  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SellerWorklet::V8State::ReportResult,
          base::Unretained(v8_state_.get()),
          std::move(task->auction_ad_config_non_shared_params),
          std::move(task->browser_signal_interest_group_owner),
          std::move(task->browser_signal_render_url), task->browser_signal_bid,
          task->browser_signal_desirability, task->scoring_signals_data_version,
          base::BindOnce(
              &SellerWorklet::DeliverReportResultCallbackOnUserThread,
              weak_ptr_factory_.GetWeakPtr(), task)));
}

void SellerWorklet::DeliverReportResultCallbackOnUserThread(
    ReportResultTaskList::iterator task,
    const absl::optional<std::string> signals_for_winner,
    const absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  if (load_script_error_msg_)
    errors.insert(errors.begin(), load_script_error_msg_.value());

  std::move(task->callback).Run(signals_for_winner, report_url, errors);
  report_result_tasks_.erase(task);
}

bool SellerWorklet::IsCodeReady() const {
  return (!paused_ && !worklet_loader_);
}

}  // namespace auction_worklet
