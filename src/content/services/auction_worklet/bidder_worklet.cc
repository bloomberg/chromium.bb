// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/bidder_worklet.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/cxx17_backports.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/for_debugging_only_bindings.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/report_bindings.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_request_manager.h"
#include "content/services/auction_worklet/worklet_loader.h"
#include "gin/converter.h"
#include "gin/dictionary.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"
#include "v8/include/v8-wasm.h"

namespace auction_worklet {

namespace {

bool AppendJsonValueOrNull(AuctionV8Helper* const v8_helper,
                           v8::Local<v8::Context> context,
                           const absl::optional<std::string>& maybe_json,
                           std::vector<v8::Local<v8::Value>>* args) {
  v8::Isolate* isolate = v8_helper->isolate();
  if (maybe_json.has_value()) {
    if (!v8_helper->AppendJsonValue(context, maybe_json.value(), args))
      return false;
  } else {
    args->push_back(v8::Null(isolate));
  }
  return true;
}

// Creates a V8 array containing information about the passed in previous wins.
// Array is sorted by time, earliest wins first. Modifies order of `prev_wins`
// input vector. This should should be harmless, since each list of previous
// wins is only used for a single bid in a single auction, and its order is
// unspecified, anyways.
v8::MaybeLocal<v8::Value> CreatePrevWinsArray(
    AuctionV8Helper* v8_helper,
    v8::Local<v8::Context> context,
    base::Time auction_start_time,
    std::vector<mojom::PreviousWinPtr>& prev_wins) {
  std::sort(prev_wins.begin(), prev_wins.end(),
            [](const mojom::PreviousWinPtr& prev_win1,
               const mojom::PreviousWinPtr& prev_win2) {
              return prev_win1->time < prev_win2->time;
            });
  std::vector<v8::Local<v8::Value>> prev_wins_v8;
  v8::Isolate* isolate = v8_helper->isolate();
  for (const auto& prev_win : prev_wins) {
    int64_t time_delta = (auction_start_time - prev_win->time).InSeconds();
    // Don't give negative times if clock has changed since last auction win.
    if (time_delta < 0)
      time_delta = 0;
    v8::Local<v8::Value> win_values[2];
    win_values[0] = v8::Number::New(isolate, time_delta);
    if (!v8_helper->CreateValueFromJson(context, prev_win->ad_json)
             .ToLocal(&win_values[1])) {
      return v8::MaybeLocal<v8::Value>();
    }
    prev_wins_v8.push_back(
        v8::Array::New(isolate, win_values, base::size(win_values)));
  }
  return v8::Array::New(isolate, prev_wins_v8.data(), prev_wins_v8.size());
}

// Converts a vector of blink::InterestGroup::Ads into a v8 object.
bool CreateAdVector(AuctionV8Helper* v8_helper,
                    v8::Local<v8::Context> context,
                    const std::vector<blink::InterestGroup::Ad>& ads,
                    v8::Local<v8::Value>& out_value) {
  v8::Isolate* isolate = v8_helper->isolate();

  std::vector<v8::Local<v8::Value>> ads_vector;
  for (const auto& ad : ads) {
    v8::Local<v8::Object> ad_object = v8::Object::New(isolate);
    gin::Dictionary ad_dict(isolate, ad_object);
    if (!ad_dict.Set("renderUrl", ad.render_url.spec()) ||
        (ad.metadata && !v8_helper->InsertJsonValue(context, "metadata",
                                                    *ad.metadata, ad_object))) {
      return false;
    }
    ads_vector.emplace_back(std::move(ad_object));
  }
  out_value = v8::Array::New(isolate, ads_vector.data(), ads_vector.size());
  return true;
}

// Checks that `url` is a valid URL and is in `ads`. Appends an error to
// `out_errors` if not. `script_source_url` is used in output error messages
// only.
bool IsAllowedAdUrl(const GURL& url,
                    const GURL& script_source_url,
                    const char* argument_name,
                    const std::vector<blink::InterestGroup::Ad>& ads,
                    std::vector<std::string>& out_errors) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    out_errors.push_back(
        base::StrCat({script_source_url.spec(), " generateBid() returned ",
                      argument_name, " URL that isn't a valid https:// URL."}));
    return false;
  }

  for (const auto& ad : ads) {
    if (url == ad.render_url)
      return true;
  }
  out_errors.push_back(base::StrCat({script_source_url.spec(),
                                     " generateBid() returned ", argument_name,
                                     " URL that isn't one "
                                     "of the registered creative URLs."}));
  return false;
}

}  // namespace

BidderWorklet::BidderWorklet(
    scoped_refptr<AuctionV8Helper> v8_helper,
    bool pause_for_debugger_on_start,
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_url_loader_factory,
    const GURL& script_source_url,
    const absl::optional<GURL>& wasm_helper_url,
    const absl::optional<GURL>& trusted_bidding_signals_url,
    const url::Origin& top_window_origin)
    : v8_runner_(v8_helper->v8_runner()),
      v8_helper_(v8_helper),
      debug_id_(
          base::MakeRefCounted<AuctionV8Helper::DebugId>(v8_helper.get())),
      url_loader_factory_(std::move(pending_url_loader_factory)),
      script_source_url_(script_source_url),
      wasm_helper_url_(wasm_helper_url),
      trusted_signals_request_manager_(
          trusted_bidding_signals_url
              ? std::make_unique<TrustedSignalsRequestManager>(
                    TrustedSignalsRequestManager::Type::kBiddingSignals,
                    url_loader_factory_.get(),
                    /*automatically_send_requests=*/false,
                    top_window_origin,
                    *trusted_bidding_signals_url,
                    v8_helper_.get())
              : nullptr),
      top_window_origin_(top_window_origin),
      v8_state_(nullptr, base::OnTaskRunnerDeleter(v8_runner_)) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  v8_state_ = std::unique_ptr<V8State, base::OnTaskRunnerDeleter>(
      new V8State(v8_helper, debug_id_, script_source_url_, top_window_origin_,
                  weak_ptr_factory_.GetWeakPtr()),
      base::OnTaskRunnerDeleter(v8_runner_));

  paused_ = pause_for_debugger_on_start;
  if (!paused_)
    Start();
}

BidderWorklet::~BidderWorklet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  debug_id_->AbortDebuggerPauses();
}

int BidderWorklet::context_group_id_for_testing() const {
  return debug_id_->context_group_id();
}

void BidderWorklet::GenerateBid(
    mojom::BidderWorkletNonSharedParamsPtr bidder_worklet_non_shared_params,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const absl::optional<base::TimeDelta> per_buyer_timeout,
    const url::Origin& seller_origin,
    mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
    GenerateBidCallback generate_bid_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  generate_bid_tasks_.emplace_front();
  auto generate_bid_task = generate_bid_tasks_.begin();
  generate_bid_task->bidder_worklet_non_shared_params =
      std::move(bidder_worklet_non_shared_params);
  generate_bid_task->auction_signals_json = auction_signals_json;
  generate_bid_task->per_buyer_signals_json = per_buyer_signals_json;
  generate_bid_task->per_buyer_timeout = per_buyer_timeout;
  generate_bid_task->seller_origin = seller_origin;
  generate_bid_task->bidding_browser_signals =
      std::move(bidding_browser_signals);
  generate_bid_task->auction_start_time = auction_start_time;
  generate_bid_task->callback = std::move(generate_bid_callback);

  const auto& trusted_bidding_signals_keys =
      generate_bid_task->bidder_worklet_non_shared_params
          ->trusted_bidding_signals_keys;
  if (trusted_signals_request_manager_ &&
      trusted_bidding_signals_keys.has_value() &&
      !trusted_bidding_signals_keys->empty()) {
    generate_bid_task->trusted_bidding_signals_request =
        trusted_signals_request_manager_->RequestBiddingSignals(
            *trusted_bidding_signals_keys,
            base::BindOnce(&BidderWorklet::OnTrustedBiddingSignalsDownloaded,
                           base::Unretained(this), generate_bid_task));
    return;
  }

  GenerateBidIfReady(generate_bid_task);
}

void BidderWorklet::SendPendingSignalsRequests() {
  if (trusted_signals_request_manager_)
    trusted_signals_request_manager_->StartBatchedTrustedSignalsRequest();
}

void BidderWorklet::ReportWin(
    const std::string& interest_group_name,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const url::Origin& browser_signal_seller_origin,
    uint32_t bidding_signals_data_version,
    bool has_bidding_signals_data_version,
    ReportWinCallback report_win_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  report_win_tasks_.emplace_front();
  auto report_win_task = report_win_tasks_.begin();
  report_win_task->interest_group_name = interest_group_name;
  report_win_task->auction_signals_json = auction_signals_json;
  report_win_task->per_buyer_signals_json = per_buyer_signals_json;
  report_win_task->seller_signals_json = seller_signals_json;
  report_win_task->browser_signal_render_url = browser_signal_render_url;
  report_win_task->browser_signal_bid = browser_signal_bid;
  report_win_task->browser_signal_seller_origin = browser_signal_seller_origin;
  if (has_bidding_signals_data_version)
    report_win_task->bidding_signals_data_version =
        bidding_signals_data_version;
  report_win_task->callback = std::move(report_win_callback);

  // If not yet ready, need to wait for load to complete.
  if (!IsCodeReady())
    return;

  RunReportWin(report_win_task);
}

void BidderWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&V8State::ConnectDevToolsAgent,
                     base::Unretained(v8_state_.get()), std::move(agent)));
}

BidderWorklet::GenerateBidTask::GenerateBidTask() = default;
BidderWorklet::GenerateBidTask::~GenerateBidTask() = default;

BidderWorklet::ReportWinTask::ReportWinTask() = default;
BidderWorklet::ReportWinTask::~ReportWinTask() = default;

BidderWorklet::V8State::V8State(
    scoped_refptr<AuctionV8Helper> v8_helper,
    scoped_refptr<AuctionV8Helper::DebugId> debug_id,
    const GURL& script_source_url,
    const url::Origin& top_window_origin,
    base::WeakPtr<BidderWorklet> parent)
    : v8_helper_(std::move(v8_helper)),
      debug_id_(std::move(debug_id)),
      parent_(std::move(parent)),
      user_thread_(base::SequencedTaskRunnerHandle::Get()),
      script_source_url_(std::move(script_source_url)),
      top_window_origin_(top_window_origin) {
  DETACH_FROM_SEQUENCE(v8_sequence_checker_);
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce(&V8State::FinishInit, base::Unretained(this)));
}

void BidderWorklet::V8State::SetWorkletScript(
    WorkletLoader::Result worklet_script) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  worklet_script_ = WorkletLoader::TakeScript(std::move(worklet_script));
}

void BidderWorklet::V8State::SetWasmHelper(
    WorkletWasmLoader::Result wasm_helper) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  wasm_helper_ = std::move(wasm_helper);
}

void BidderWorklet::V8State::ReportWin(
    const std::string& interest_group_name,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const std::string& seller_signals_json,
    const GURL& browser_signal_render_url,
    double browser_signal_bid,
    const url::Origin& browser_signal_seller_origin,
    const absl::optional<uint32_t>& bidding_signals_data_version,
    ReportWinCallbackInternal callback) {
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
  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json,
                             &args) ||
      !v8_helper_->AppendJsonValue(context, seller_signals_json, &args)) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      absl::nullopt /* report_url */,
                                      std::vector<std::string>() /* errors */);
    return;
  }

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !browser_signals_dict.Set(
          "interestGroupOwner",
          url::Origin::Create(script_source_url_).Serialize()) ||
      !browser_signals_dict.Set("interestGroupName", interest_group_name) ||
      !browser_signals_dict.Set("renderUrl",
                                browser_signal_render_url.spec()) ||
      !browser_signals_dict.Set("bid", browser_signal_bid) ||
      !browser_signals_dict.Set("seller",
                                browser_signal_seller_origin.Serialize()) ||
      (bidding_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 bidding_signals_data_version.value()))) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      absl::nullopt /* report_url */,
                                      std::vector<std::string>() /* errors */);
    return;
  }
  args.push_back(browser_signals);

  // An empty return value indicates an exception was thrown. Any other return
  // value indicates no exception.
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletReportingStart");
  if (v8_helper_
          ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                      "reportWin", args, /*script_timeout=*/absl::nullopt,
                      errors_out)
          .IsEmpty()) {
    PostReportWinCallbackToUserThread(std::move(callback),
                                      absl::nullopt /* report_url */,
                                      std::move(errors_out));
    return;
  }

  // This covers both the case where a report URL was provided, and the case one
  // was not.
  PostReportWinCallbackToUserThread(
      std::move(callback), report_bindings.report_url(), std::move(errors_out));
}

void BidderWorklet::V8State::GenerateBid(
    mojom::BidderWorkletNonSharedParamsPtr bidder_worklet_non_shared_params,
    const absl::optional<std::string>& auction_signals_json,
    const absl::optional<std::string>& per_buyer_signals_json,
    const absl::optional<base::TimeDelta> per_buyer_timeout,
    const url::Origin& browser_signal_seller_origin,
    mojom::BiddingBrowserSignalsPtr bidding_browser_signals,
    base::Time auction_start_time,
    scoped_refptr<TrustedSignals::Result> trusted_bidding_signals_result,
    GenerateBidCallbackInternal callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);

  // Can't make a bid without any ads.
  if (!bidder_worklet_non_shared_params->ads) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }
  base::TimeTicks start = base::TimeTicks::Now();

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
  v8::Local<v8::Object> interest_group_object = v8::Object::New(isolate);
  gin::Dictionary interest_group_dict(isolate, interest_group_object);
  if (!interest_group_dict.Set(
          "owner", url::Origin::Create(script_source_url_).Serialize()) ||
      !interest_group_dict.Set("name",
                               bidder_worklet_non_shared_params->name) ||
      (bidder_worklet_non_shared_params->user_bidding_signals &&
       !v8_helper_->InsertJsonValue(
           context, "userBiddingSignals",
           *bidder_worklet_non_shared_params->user_bidding_signals,
           interest_group_object))) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  v8::Local<v8::Value> ads;
  if (!CreateAdVector(v8_helper_.get(), context,
                      *bidder_worklet_non_shared_params->ads, ads) ||
      !v8_helper_->InsertValue("ads", std::move(ads), interest_group_object)) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  if (bidder_worklet_non_shared_params->ad_components) {
    v8::Local<v8::Value> ad_components;
    if (!CreateAdVector(v8_helper_.get(), context,
                        *bidder_worklet_non_shared_params->ad_components,
                        ad_components) ||
        !v8_helper_->InsertValue("adComponents", std::move(ad_components),
                                 interest_group_object)) {
      PostErrorBidCallbackToUserThread(std::move(callback));
      return;
    }
  }

  args.push_back(std::move(interest_group_object));

  if (!AppendJsonValueOrNull(v8_helper_.get(), context, auction_signals_json,
                             &args) ||
      !AppendJsonValueOrNull(v8_helper_.get(), context, per_buyer_signals_json,
                             &args)) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  v8::Local<v8::Value> trusted_signals;
  absl::optional<uint32_t> bidding_signals_data_version;
  if (!trusted_bidding_signals_result) {
    trusted_signals = v8::Null(isolate);
  } else {
    trusted_signals = trusted_bidding_signals_result->GetBiddingSignals(
        v8_helper_.get(), context,
        *bidder_worklet_non_shared_params->trusted_bidding_signals_keys);
    bidding_signals_data_version =
        trusted_bidding_signals_result->GetDataVersion();
  }
  args.push_back(trusted_signals);

  v8::Local<v8::Object> browser_signals = v8::Object::New(isolate);
  gin::Dictionary browser_signals_dict(isolate, browser_signals);
  if (!browser_signals_dict.Set("topWindowHostname",
                                top_window_origin_.host()) ||
      !browser_signals_dict.Set("seller",
                                browser_signal_seller_origin.Serialize()) ||
      !browser_signals_dict.Set("joinCount",
                                bidding_browser_signals->join_count) ||
      !browser_signals_dict.Set("bidCount",
                                bidding_browser_signals->bid_count) ||
      (bidding_signals_data_version.has_value() &&
       !browser_signals_dict.Set("dataVersion",
                                 bidding_signals_data_version.value()))) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  if (wasm_helper_.success()) {
    v8::Local<v8::WasmModuleObject> module;
    v8::Maybe<bool> result = v8::Nothing<bool>();
    if (WorkletWasmLoader::MakeModule(wasm_helper_).ToLocal(&module)) {
      result = browser_signals->Set(
          context, gin::StringToV8(isolate, "wasmHelper"), module);
    }
    if (result.IsNothing() || !result.FromJust()) {
      PostErrorBidCallbackToUserThread(std::move(callback));
      return;
    }
  }

  v8::Local<v8::Value> prev_wins;
  if (!CreatePrevWinsArray(v8_helper_.get(), context, auction_start_time,
                           bidding_browser_signals->prev_wins)
           .ToLocal(&prev_wins)) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  v8::Maybe<bool> result = browser_signals->Set(
      context, gin::StringToV8(isolate, "prevWins"), prev_wins);
  if (result.IsNothing() || !result.FromJust()) {
    PostErrorBidCallbackToUserThread(std::move(callback));
    return;
  }

  args.push_back(browser_signals);

  v8::Local<v8::Value> generate_bid_result;
  std::vector<std::string> errors_out;
  v8_helper_->MaybeTriggerInstrumentationBreakpoint(
      *debug_id_, "beforeBidderWorkletBiddingStart");
  if (!v8_helper_
           ->RunScript(context, worklet_script_.Get(isolate), debug_id_.get(),
                       "generateBid", args, std::move(per_buyer_timeout),
                       errors_out)
           .ToLocal(&generate_bid_result)) {
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  if (!generate_bid_result->IsObject()) {
    errors_out.push_back(
        base::StrCat({script_source_url_.spec(),
                      " generateBid() return value not an object."}));
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  gin::Dictionary result_dict(isolate, generate_bid_result.As<v8::Object>());

  v8::Local<v8::Value> ad_object;
  std::string ad_json;
  double bid;
  std::string render_url_string;
  // Parse and validate values.
  if (!result_dict.Get("ad", &ad_object) ||
      !v8_helper_->ExtractJson(context, ad_object, &ad_json) ||
      !result_dict.Get("bid", &bid) ||
      !result_dict.Get("render", &render_url_string)) {
    errors_out.push_back(
        base::StrCat({script_source_url_.spec(),
                      " generateBid() return value has incorrect structure."}));
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  if (bid <= 0 || std::isnan(bid) || !std::isfinite(bid)) {
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  GURL render_url(render_url_string);
  if (!IsAllowedAdUrl(render_url, script_source_url_, "render",
                      *bidder_worklet_non_shared_params->ads, errors_out)) {
    PostErrorBidCallbackToUserThread(std::move(callback),
                                     std::move(errors_out));
    return;
  }

  absl::optional<std::vector<GURL>> ad_component_urls;
  v8::Local<v8::Value> ad_components;
  if (result_dict.Get("adComponents", &ad_components) &&
      !ad_components->IsNullOrUndefined()) {
    if (!bidder_worklet_non_shared_params->ad_components) {
      errors_out.push_back(
          base::StrCat({script_source_url_.spec(),
                        " generateBid() return value contains adComponents but "
                        "InterestGroup has no adComponents."}));
      PostErrorBidCallbackToUserThread(std::move(callback),
                                       std::move(errors_out));
      return;
    }

    if (!ad_components->IsArray()) {
      errors_out.push_back(base::StrCat(
          {script_source_url_.spec(),
           " generateBid() returned adComponents value must be an array."}));
      PostErrorBidCallbackToUserThread(std::move(callback),
                                       std::move(errors_out));
      return;
    }

    v8::Local<v8::Array> ad_components_array = ad_components.As<v8::Array>();
    if (ad_components_array->Length() > blink::kMaxAdAuctionAdComponents) {
      errors_out.push_back(base::StringPrintf(
          "%s generateBid() returned adComponents with over %zu items.",
          script_source_url_.spec().c_str(), blink::kMaxAdAuctionAdComponents));
      PostErrorBidCallbackToUserThread(std::move(callback),
                                       std::move(errors_out));
      return;
    }

    ad_component_urls.emplace();
    for (size_t i = 0; i < ad_components_array->Length(); ++i) {
      std::string url_string;
      if (!gin::ConvertFromV8(
              isolate, ad_components_array->Get(context, i).ToLocalChecked(),
              &url_string)) {
        errors_out.push_back(
            base::StrCat({script_source_url_.spec(),
                          " generateBid() returned adComponents value must be "
                          "an array of strings."}));
        PostErrorBidCallbackToUserThread(std::move(callback),
                                         std::move(errors_out));
        return;
      }

      GURL ad_component_url(url_string);
      if (!IsAllowedAdUrl(ad_component_url, script_source_url_, "adComponents",
                          *bidder_worklet_non_shared_params->ad_components,
                          errors_out)) {
        PostErrorBidCallbackToUserThread(std::move(callback),
                                         std::move(errors_out));
        return;
      }
      ad_component_urls->emplace_back(std::move(ad_component_url));
    }
  }

  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     mojom::BidderWorkletBid::New(
                         std::move(ad_json), bid, std::move(render_url),
                         std::move(ad_component_urls),
                         /*bid_duration=*/base::TimeTicks::Now() - start),
                     bidding_signals_data_version,
                     for_debugging_only_bindings.TakeLossReportUrl(),
                     for_debugging_only_bindings.TakeWinReportUrl(),
                     std::move(errors_out)));
}

void BidderWorklet::V8State::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  v8_helper_->ConnectDevToolsAgent(std::move(agent), user_thread_, *debug_id_);
}

BidderWorklet::V8State::~V8State() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
}

void BidderWorklet::V8State::FinishInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  debug_id_->SetResumeCallback(base::BindOnce(
      &BidderWorklet::V8State::PostResumeToUserThread, parent_, user_thread_));
}

// static
void BidderWorklet::V8State::PostResumeToUserThread(
    base::WeakPtr<BidderWorklet> parent,
    scoped_refptr<base::SequencedTaskRunner> user_thread) {
  // This is static since it's called from debugging, not BidderWorklet,
  // so the usual guarantee that BidderWorklet posts things before posting
  // V8State destruction is irrelevant.
  user_thread->PostTask(FROM_HERE,
                        base::BindOnce(&BidderWorklet::ResumeIfPaused, parent));
}

void BidderWorklet::V8State::PostReportWinCallbackToUserThread(
    ReportWinCallbackInternal callback,
    const absl::optional<GURL>& report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(report_url),
                                std::move(errors)));
}

void BidderWorklet::V8State::PostErrorBidCallbackToUserThread(
    GenerateBidCallbackInternal callback,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(v8_sequence_checker_);
  user_thread_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), mojom::BidderWorkletBidPtr(),
                     /*bidding_signals_data_version=*/absl::nullopt,
                     /*debug_loss_report_url=*/absl::nullopt,
                     /*debug_win_report_url=*/absl::nullopt,
                     std::move(error_msgs)));
}

void BidderWorklet::ResumeIfPaused() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (!paused_)
    return;

  paused_ = false;
  Start();
}

void BidderWorklet::Start() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  DCHECK(!paused_);

  worklet_loader_ = std::make_unique<WorkletLoader>(
      url_loader_factory_.get(), script_source_url_, v8_helper_, debug_id_,
      base::BindOnce(&BidderWorklet::OnScriptDownloaded,
                     base::Unretained(this)));

  if (wasm_helper_url_.has_value()) {
    wasm_loader_ = std::make_unique<WorkletWasmLoader>(
        url_loader_factory_.get(), wasm_helper_url_.value(), v8_helper_,
        debug_id_,
        base::BindOnce(&BidderWorklet::OnWasmDownloaded,
                       base::Unretained(this)));
  }
}

void BidderWorklet::OnScriptDownloaded(WorkletLoader::Result worklet_script,
                                       absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  worklet_loader_.reset();

  // On failure, close pipe and delete `this`, as it can't do anything without a
  // loaded script.
  if (!worklet_script.success()) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  if (error_msg.has_value())
    load_code_error_msgs_.push_back(std::move(error_msg.value()));

  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&BidderWorklet::V8State::SetWorkletScript,
                                      base::Unretained(v8_state_.get()),
                                      std::move(worklet_script)));
  RunReadyGenerateBidTasks();
  RunReportWinTasks();  // These only depends on JS, so they can be run.
}

void BidderWorklet::OnWasmDownloaded(WorkletWasmLoader::Result wasm_helper,
                                     absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  wasm_loader_.reset();

  // If the WASM helper is actually requested, delete `this` and inform the
  // browser process of the failure. ReportWin() calls would theoretically still
  // be allowed, but that adds a lot more complexity around BidderWorklet reuse.
  if (!wasm_helper.success()) {
    std::move(close_pipe_callback_)
        .Run(error_msg ? error_msg.value() : std::string());
    // `this` should be deleted at this point.
    return;
  }

  if (error_msg.has_value())
    load_code_error_msgs_.push_back(std::move(error_msg.value()));

  v8_runner_->PostTask(FROM_HERE,
                       base::BindOnce(&BidderWorklet::V8State::SetWasmHelper,
                                      base::Unretained(v8_state_.get()),
                                      std::move(wasm_helper)));
  RunReadyGenerateBidTasks();
  // ReportWin() tasks currently don't have access to WASM.
}

void BidderWorklet::RunReadyGenerateBidTasks() {
  // Run all GenerateBid() tasks that are ready. GenerateBidIfReady() does *not*
  // modify `generate_bid_tasks_` when invoked, so this is safe.
  for (auto generate_bid_task = generate_bid_tasks_.begin();
       generate_bid_task != generate_bid_tasks_.end(); ++generate_bid_task) {
    GenerateBidIfReady(generate_bid_task);
  }
}

void BidderWorklet::RunReportWinTasks() {
  // Run all ReportWin() tasks. RunReportWin() does *not* modify
  // `report_win_tasks_` when invoked, so this is safe.
  for (auto report_win_task = report_win_tasks_.begin();
       report_win_task != report_win_tasks_.end(); ++report_win_task) {
    RunReportWin(report_win_task);
  }
}

void BidderWorklet::OnTrustedBiddingSignalsDownloaded(
    GenerateBidTaskList::iterator task,
    scoped_refptr<TrustedSignals::Result> result,
    absl::optional<std::string> error_msg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  task->trusted_bidding_signals_error_msg = std::move(error_msg);
  task->trusted_bidding_signals_result = std::move(result);
  task->trusted_bidding_signals_request.reset();

  GenerateBidIfReady(task);
}

void BidderWorklet::GenerateBidIfReady(GenerateBidTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  if (task->trusted_bidding_signals_request || !IsCodeReady())
    return;

  // Other than the callback field, no fields of `task` are needed after this
  // point, so can consume them instead of copying them.
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::GenerateBid,
          base::Unretained(v8_state_.get()),
          std::move(task->bidder_worklet_non_shared_params),
          std::move(task->auction_signals_json),
          std::move(task->per_buyer_signals_json),
          std::move(task->per_buyer_timeout), std::move(task->seller_origin),
          std::move(task->bidding_browser_signals), task->auction_start_time,
          std::move(task->trusted_bidding_signals_result),
          base::BindOnce(&BidderWorklet::DeliverBidCallbackOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void BidderWorklet::RunReportWin(ReportWinTaskList::iterator task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  // Other than the callback field, no fields of `task` are needed after this
  // point, so can consume them instead of copying them.
  v8_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BidderWorklet::V8State::ReportWin, base::Unretained(v8_state_.get()),
          std::move(task->interest_group_name),
          std::move(task->auction_signals_json),
          std::move(task->per_buyer_signals_json),
          std::move(task->seller_signals_json),
          std::move(task->browser_signal_render_url),
          std::move(task->browser_signal_bid),
          std::move(task->browser_signal_seller_origin),
          std::move(task->bidding_signals_data_version),
          base::BindOnce(&BidderWorklet::DeliverReportWinOnUserThread,
                         weak_ptr_factory_.GetWeakPtr(), task)));
}

void BidderWorklet::DeliverBidCallbackOnUserThread(
    GenerateBidTaskList::iterator task,
    mojom::BidderWorkletBidPtr bid,
    absl::optional<uint32_t> bidding_signals_data_version,
    absl::optional<GURL> debug_loss_report_url,
    absl::optional<GURL> debug_win_report_url,
    std::vector<std::string> error_msgs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);

  error_msgs.insert(error_msgs.end(), load_code_error_msgs_.begin(),
                    load_code_error_msgs_.end());
  if (task->trusted_bidding_signals_error_msg) {
    error_msgs.emplace_back(
        std::move(task->trusted_bidding_signals_error_msg).value());
  }
  std::move(task->callback)
      .Run(std::move(bid), bidding_signals_data_version.value_or(0),
           bidding_signals_data_version.has_value(), debug_loss_report_url,
           debug_win_report_url, error_msgs);
  generate_bid_tasks_.erase(task);
}

void BidderWorklet::DeliverReportWinOnUserThread(
    ReportWinTaskList::iterator task,
    absl::optional<GURL> report_url,
    std::vector<std::string> errors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_sequence_checker_);
  errors.insert(errors.end(), load_code_error_msgs_.begin(),
                load_code_error_msgs_.end());
  std::move(task->callback).Run(std::move(report_url), errors);
  report_win_tasks_.erase(task);
}

bool BidderWorklet::IsCodeReady() const {
  // If `paused_`, loading hasn't started yet. Otherwise, null loaders indicate
  // the worklet script has loaded successfully, and there's no WASM helper, or
  // it has also loaded successfully.
  return !paused_ && !worklet_loader_ && !wasm_loader_;
}

}  // namespace auction_worklet
