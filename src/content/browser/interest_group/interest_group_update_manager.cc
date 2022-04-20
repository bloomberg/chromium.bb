// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_update_manager.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/span.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_storage.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// 10 kb update size limit. We are potentially fetching many interest group
// updates, so don't let this get too large.
constexpr size_t kMaxUpdateSize = 10 * 1024;

// The maximum amount of time that the update process can run before it gets
// cancelled for taking too long.
constexpr base::TimeDelta kMaxUpdateRoundDuration = base::Minutes(10);

// The maximum number of groups that can be updated at the same time.
constexpr int kMaxParallelUpdates = 5;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("interest_group_update_fetcher", R"(
        semantics {
          sender: "Interest group periodic update fetcher"
          description:
            "Fetches periodic updates of FLEDGE interest groups previously "
            "joined by navigator.joinAdInterestGroup(). FLEDGE allow sites to "
            "store persistent interest groups that are only accessible to "
            "special on-device ad auction worklets run via "
            "navigator.runAdAuction(). JavaScript running in the context of a "
            "frame cannot read interest groups, but it can request that all "
            "interest groups owned by the current frame's origin be updated by "
            "fetching JSON from the registered update URL for each interest "
            "group."
            "See https://github.com/WICG/turtledove/blob/main/FLEDGE.md and "
            "https://developer.chrome.com/docs/privacy-sandbox/fledge/"
          trigger:
            "Fetched upon a navigator.updateAdInterestGroups() call. Also "
            "triggered upon navigator.runAdAuction() completion for interest "
            "groups that participated in the auction."
          data: "URL registered for updating this interest group."
          destination: WEBSITE
        }
        policy {
          cookies_allowed: NO
          setting:
            "These requests are controlled by a feature flag that is off by "
            "default now. When enabled, they can be disabled by the Privacy"
            " Sandbox setting."
          policy_exception_justification:
            "These requests are triggered by a website."
        })");

// TODO(crbug.com/1186444): Report errors to devtools for the TryToCopy*().
// functions.

// Name and owner are optional in `value` (parsed server JSON response), but
// must match `name` and `owner`, respectively, if either is specified. Returns
// true if the check passes, and false otherwise.
[[nodiscard]] bool ValidateNameAndOwnerIfPresent(const url::Origin& owner,
                                                 const std::string& name,
                                                 const base::Value& value) {
  const std::string* maybe_owner = value.FindStringKey("owner");
  if (maybe_owner && url::Origin::Create(GURL(*maybe_owner)) != owner)
    return false;
  const std::string* maybe_name = value.FindStringKey("name");
  if (maybe_name && *maybe_name != name)
    return false;
  return true;
}

// Copies the trustedBiddingSignals list JSON field into
// `interest_group_update`, returns true iff the JSON is valid and the copy
// completed.
[[nodiscard]] bool TryToCopyTrustedBiddingSignalsKeys(
    blink::InterestGroup& interest_group_update,
    const base::Value& value) {
  const base::Value* maybe_update_trusted_bidding_signals_keys =
      value.FindListKey("trustedBiddingSignalsKeys");
  if (!maybe_update_trusted_bidding_signals_keys)
    return true;
  std::vector<std::string> trusted_bidding_signals_keys;
  for (const base::Value& keys_value :
       maybe_update_trusted_bidding_signals_keys->GetListDeprecated()) {
    const std::string* maybe_key = keys_value.GetIfString();
    if (!maybe_key)
      return false;
    trusted_bidding_signals_keys.push_back(*maybe_key);
  }
  interest_group_update.trusted_bidding_signals_keys =
      trusted_bidding_signals_keys;
  return true;
}

// Helper for TryToCopyAds() and TryToCopyAdComponents().
[[nodiscard]] absl::optional<std::vector<blink::InterestGroup::Ad>> ExtractAds(
    const base::Value& ads_list) {
  std::vector<blink::InterestGroup::Ad> ads;
  for (const base::Value& ads_value : ads_list.GetListDeprecated()) {
    if (!ads_value.is_dict())
      return absl::nullopt;
    const std::string* maybe_render_url = ads_value.FindStringKey("renderUrl");
    if (!maybe_render_url)
      return absl::nullopt;
    blink::InterestGroup::Ad ad;
    ad.render_url = GURL(*maybe_render_url);
    const base::Value* maybe_metadata = ads_value.FindKey("metadata");
    if (maybe_metadata) {
      std::string metadata;
      JSONStringValueSerializer serializer(&metadata);
      if (!serializer.Serialize(*maybe_metadata)) {
        // Binary blobs shouldn't be present, but it's possible we exceeded the
        // max JSON depth.
        return absl::nullopt;
      }
      ad.metadata = std::move(metadata);
    }
    ads.push_back(std::move(ad));
  }
  return ads;
}

// Copies the `ads` list JSON field into `interest_group_update`, returns true
// iff the JSON is valid and the copy completed.
[[nodiscard]] bool TryToCopyAds(blink::InterestGroup& interest_group_update,
                                const base::Value& value) {
  const base::Value* maybe_ads = value.FindListKey("ads");
  if (!maybe_ads)
    return true;
  absl::optional<std::vector<blink::InterestGroup::Ad>> maybe_extracted_ads =
      ExtractAds(*maybe_ads);
  if (!maybe_extracted_ads)
    return false;
  interest_group_update.ads = std::move(*maybe_extracted_ads);
  return true;
}

// Copies the `adComponents` list JSON field into `interest_group_update`,
// returns true iff the JSON is valid and the copy completed.
[[nodiscard]] bool TryToCopyAdComponents(
    blink::InterestGroup& interest_group_update,
    const base::Value& value) {
  const base::Value* maybe_ads = value.FindListKey("adComponents");
  if (!maybe_ads)
    return true;
  absl::optional<std::vector<blink::InterestGroup::Ad>> maybe_extracted_ads =
      ExtractAds(*maybe_ads);
  if (!maybe_extracted_ads)
    return false;
  interest_group_update.ad_components = std::move(*maybe_extracted_ads);
  return true;
}

absl::optional<blink::InterestGroup> ParseUpdateJson(
    const url::Origin& owner,
    const std::string& name,
    const data_decoder::DataDecoder::ValueOrError& result) {
  // TODO(crbug.com/1186444): Report to devtools.
  if (result.error) {
    return absl::nullopt;
  }
  const base::Value& value = *result.value;
  if (!value.is_dict()) {
    return absl::nullopt;
  }
  if (!ValidateNameAndOwnerIfPresent(owner, name, value)) {
    return absl::nullopt;
  }
  blink::InterestGroup interest_group_update;
  interest_group_update.owner = owner;
  interest_group_update.name = name;
  const base::Value* maybe_priority_value = value.FindKey("priority");
  if (maybe_priority_value) {
    // If the field is specified, it must be an integer or a double.
    if (!maybe_priority_value->is_int() && !maybe_priority_value->is_double())
      return absl::nullopt;
    interest_group_update.priority = maybe_priority_value->GetDouble();
  }
  const std::string* maybe_bidding_url = value.FindStringKey("biddingLogicUrl");
  if (maybe_bidding_url)
    interest_group_update.bidding_url = GURL(*maybe_bidding_url);
  const std::string* maybe_bidding_wasm_helper_url =
      value.FindStringKey("biddingWasmHelperUrl");
  if (maybe_bidding_wasm_helper_url) {
    interest_group_update.bidding_wasm_helper_url =
        GURL(*maybe_bidding_wasm_helper_url);
  }
  const std::string* maybe_update_trusted_bidding_signals_url =
      value.FindStringKey("trustedBiddingSignalsUrl");
  if (maybe_update_trusted_bidding_signals_url) {
    interest_group_update.trusted_bidding_signals_url =
        GURL(*maybe_update_trusted_bidding_signals_url);
  }
  if (!TryToCopyTrustedBiddingSignalsKeys(interest_group_update, value)) {
    return absl::nullopt;
  }
  if (!TryToCopyAds(interest_group_update, value)) {
    return absl::nullopt;
  }
  if (!TryToCopyAdComponents(interest_group_update, value)) {
    return absl::nullopt;
  }
  if (!interest_group_update.IsValid()) {
    return absl::nullopt;
  }
  // If not specified by the update make sure the field is not specified.
  // This must occur after the IsValid check since priority is required for a
  // valid interest group, while an update should just keep the existing value.
  if (!maybe_priority_value)
    interest_group_update.priority.reset();
  return interest_group_update;
}

}  // namespace

namespace content {

InterestGroupUpdateManager::InterestGroupUpdateManager(
    InterestGroupManagerImpl* manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : manager_(manager),
      max_update_round_duration_(kMaxUpdateRoundDuration),
      max_parallel_updates_(kMaxParallelUpdates),
      url_loader_factory_(std::move(url_loader_factory)) {}

InterestGroupUpdateManager::~InterestGroupUpdateManager() = default;

void InterestGroupUpdateManager::UpdateInterestGroupsOfOwner(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  owners_to_update_.Enqueue(owner, std::move(client_security_state));
  MaybeContinueUpdatingCurrentOwner();
}

void InterestGroupUpdateManager::UpdateInterestGroupsOfOwners(
    base::span<url::Origin> owners,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  // Shuffle the list of interest group owners for fairness.
  base::RandomShuffle(owners.begin(), owners.end());
  for (const url::Origin& owner : owners) {
    UpdateInterestGroupsOfOwner(owner, client_security_state.Clone());
  }
}

void InterestGroupUpdateManager::set_max_update_round_duration_for_testing(
    base::TimeDelta delta) {
  max_update_round_duration_ = delta;
}

void InterestGroupUpdateManager::set_max_parallel_updates_for_testing(
    int max_parallel_updates) {
  max_parallel_updates_ = max_parallel_updates;
}

InterestGroupUpdateManager::OwnersToUpdate::OwnersToUpdate() = default;

InterestGroupUpdateManager::OwnersToUpdate::~OwnersToUpdate() = default;

bool InterestGroupUpdateManager::OwnersToUpdate::Empty() const {
  return owners_to_update_.empty();
}

const url::Origin& InterestGroupUpdateManager::OwnersToUpdate::FrontOwner()
    const {
  return owners_to_update_.front();
}

network::mojom::ClientSecurityStatePtr
InterestGroupUpdateManager::OwnersToUpdate::FrontSecurityState() const {
  return security_state_map_.at(FrontOwner()).Clone();
}

bool InterestGroupUpdateManager::OwnersToUpdate::Enqueue(
    const url::Origin& owner,
    network::mojom::ClientSecurityStatePtr client_security_state) {
  if (!security_state_map_.emplace(owner, std::move(client_security_state))
           .second) {
    return false;
  }
  owners_to_update_.emplace_back(owner);
  return true;
}

void InterestGroupUpdateManager::OwnersToUpdate::PopFront() {
  security_state_map_.erase(owners_to_update_.front());
  owners_to_update_.pop_front();
}

void InterestGroupUpdateManager::OwnersToUpdate::Clear() {
  owners_to_update_.clear();
  security_state_map_.clear();
}

void InterestGroupUpdateManager::MaybeContinueUpdatingCurrentOwner() {
  if (num_in_flight_updates_ > 0 || waiting_on_db_read_)
    return;

  if (owners_to_update_.Empty()) {
    // This update round is finished, there's no more work to do.
    last_update_started_ = base::TimeTicks::Min();
    return;
  }

  if (last_update_started_ == base::TimeTicks::Min()) {
    // It appears we're staring a new update round; mark the time we started the
    // round.
    last_update_started_ = base::TimeTicks::Now();
  } else if (base::TimeTicks::Now() - last_update_started_ >
             max_update_round_duration_) {
    // We've been updating for too long; cancel all outstanding updates.
    owners_to_update_.Clear();
    last_update_started_ = base::TimeTicks::Min();
    return;
  }

  GetInterestGroupsForUpdate(
      owners_to_update_.FrontOwner(),
      base::BindOnce(
          &InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerDbLoad,
          weak_factory_.GetWeakPtr(), owners_to_update_.FrontOwner()));
}

void InterestGroupUpdateManager::GetInterestGroupsForUpdate(
    const url::Origin& owner,
    base::OnceCallback<void(std::vector<StorageInterestGroup>)> callback) {
  DCHECK_EQ(num_in_flight_updates_, 0);
  DCHECK(!waiting_on_db_read_);
  waiting_on_db_read_ = true;
  manager_->GetInterestGroupsForUpdate(
      owner, /*groups_limit=*/max_parallel_updates_, std::move(callback));
}

void InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerDbLoad(
    url::Origin owner,
    std::vector<StorageInterestGroup> storage_groups) {
  DCHECK_EQ(owner, owners_to_update_.FrontOwner());
  DCHECK_EQ(num_in_flight_updates_, 0);
  DCHECK(waiting_on_db_read_);
  DCHECK_LE(storage_groups.size(),
            static_cast<unsigned int>(max_parallel_updates_));
  waiting_on_db_read_ = false;
  if (storage_groups.empty()) {
    // All interest groups for `owner` are up to date, so we can pop it off the
    // queue.
    owners_to_update_.PopFront();
    MaybeContinueUpdatingCurrentOwner();
    return;
  }
  net::IsolationInfo per_update_isolation_info =
      net::IsolationInfo::CreateTransient();

  for (auto& storage_group : storage_groups) {
    if (!storage_group.interest_group.daily_update_url)
      continue;
    ++num_in_flight_updates_;
    base::UmaHistogramCounts100000(
        "Ads.InterestGroup.Net.RequestUrlSizeBytes.Update",
        storage_group.interest_group.daily_update_url->spec().size());
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url =
        std::move(storage_group.interest_group.daily_update_url).value();
    resource_request->redirect_mode = network::mojom::RedirectMode::kError;
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
    resource_request->request_initiator = owner;
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request->trusted_params->isolation_info =
        per_update_isolation_info;
    resource_request->trusted_params->client_security_state =
        owners_to_update_.FrontSecurityState();
    auto simple_url_loader = network::SimpleURLLoader::Create(
        std::move(resource_request), kTrafficAnnotation);
    simple_url_loader->SetTimeoutDuration(base::Seconds(30));
    auto simple_url_loader_it =
        url_loaders_.insert(url_loaders_.end(), std::move(simple_url_loader));
    (*simple_url_loader_it)
        ->DownloadToString(
            url_loader_factory_.get(),
            base::BindOnce(&InterestGroupUpdateManager::
                               DidUpdateInterestGroupsOfOwnerNetFetch,
                           weak_factory_.GetWeakPtr(), simple_url_loader_it,
                           std::move(storage_group.interest_group.owner),
                           std::move(storage_group.interest_group.name)),
            kMaxUpdateSize);
  }
}

void InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerNetFetch(
    UrlLoadersList::iterator simple_url_loader_it,
    url::Origin owner,
    std::string name,
    std::unique_ptr<std::string> fetch_body) {
  DCHECK_EQ(owner, owners_to_update_.FrontOwner());
  DCHECK_GT(num_in_flight_updates_, 0);
  DCHECK(!waiting_on_db_read_);
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      std::move(*simple_url_loader_it);
  url_loaders_.erase(simple_url_loader_it);
  // TODO(crbug.com/1186444): Report HTTP error info to devtools.
  if (!fetch_body) {
    ReportUpdateFailed(owner, name,
                       /*delay_type=*/simple_url_loader->NetError() ==
                               net::ERR_INTERNET_DISCONNECTED
                           ? UpdateDelayType::kNoInternet
                           : UpdateDelayType::kNetFailure);
    return;
  }
  base::UmaHistogramCounts100000(
      "Ads.InterestGroup.Net.ResponseSizeBytes.Update", fetch_body->size());
  data_decoder::DataDecoder::ParseJsonIsolated(
      *fetch_body,
      base::BindOnce(
          &InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerJsonParse,
          weak_factory_.GetWeakPtr(), std::move(owner), std::move(name)));
}

void InterestGroupUpdateManager::DidUpdateInterestGroupsOfOwnerJsonParse(
    url::Origin owner,
    std::string name,
    data_decoder::DataDecoder::ValueOrError result) {
  DCHECK_EQ(owner, owners_to_update_.FrontOwner());
  DCHECK_GT(num_in_flight_updates_, 0);
  DCHECK(!waiting_on_db_read_);
  absl::optional<blink::InterestGroup> interest_group_update =
      ParseUpdateJson(owner, name, result);
  if (!interest_group_update) {
    ReportUpdateFailed(owner, name, UpdateDelayType::kParseFailure);
    return;
  }
  UpdateInterestGroup(std::move(*interest_group_update));
}

void InterestGroupUpdateManager::OnOneUpdateCompleted() {
  DCHECK_GT(num_in_flight_updates_, 0);
  --num_in_flight_updates_;
  MaybeContinueUpdatingCurrentOwner();
}

void InterestGroupUpdateManager::UpdateInterestGroup(
    blink::InterestGroup group) {
  manager_->UpdateInterestGroup(std::move(group));
  OnOneUpdateCompleted();
}

void InterestGroupUpdateManager::ReportUpdateFailed(
    const url::Origin& owner,
    const std::string& name,
    UpdateDelayType delay_type) {
  if (delay_type != UpdateDelayType::kNoInternet) {
    manager_->ReportUpdateFailed(
        owner, name,
        /*parse_failure=*/delay_type == UpdateDelayType::kParseFailure);
  }

  if (delay_type == UpdateDelayType::kNoInternet) {
    // If the internet is disconnected, no more updating is possible at the
    // moment. As we are now stopping update work, and it is an invariant
    // that `owners_to_update_` only stores owners that will eventually
    // be processed, we clear `owners_to_update_` to ensure that future
    // update attempts aren't blocked.
    //
    // To avoid violating the invariant that we're always updating the front of
    // the queue, only clear we encounter this error on the last in-flight
    // update.
    if (num_in_flight_updates_ == 1)
      owners_to_update_.Clear();
  }

  OnOneUpdateCompleted();
}

}  // namespace content
