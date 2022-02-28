// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_ids_provider.h"

#include <algorithm>

#include "base/base64.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_features.h"

namespace variations {
namespace {

// Range of low entropy source values (8000) as variation ids for the
// X-Client-Data header. This range is reserved in cl/333331461 (internal CL).
const int kLowEntropySourceVariationIdRangeMin = 3320978;
const int kLowEntropySourceVariationIdRangeMax = 3328977;

VariationsIdsProvider* g_instance = nullptr;

base::Lock& GetInstanceLock() {
  static base::NoDestructor<base::Lock> lock;
  return *lock;
}

}  // namespace

bool VariationsHeaderKey::operator<(const VariationsHeaderKey& other) const {
  if (is_signed_in != other.is_signed_in)
    return is_signed_in < other.is_signed_in;
  return web_visibility < other.web_visibility;
}

// Adding/removing headers is implemented by request consumers, and how it is
// implemented depends on the request type.
// There are three cases:
// 1. Subresources request in renderer, it is implemented by
// WebURLLoaderImpl::Context::Start() by adding a VariationsURLLoaderThrottle
// to a content::URLLoaderThrottle vector.
// 2. Navigations/Downloads request in browser, it is implemented in
// ChromeContentBrowserClient::CreateURLLoaderThrottles() which calls
// CreateContentBrowserURLLoaderThrottles which also adds a
// VariationsURLLoaderThrottle to a content::URLLoaderThrottle vector.
// 3. SimpleURLLoader in browser, it is implemented in a SimpleURLLoader wrapper
// function variations::CreateSimpleURLLoaderWithVariationsHeader().

// static
VariationsIdsProvider* VariationsIdsProvider::Create(Mode mode) {
  base::AutoLock lock(GetInstanceLock());
  DCHECK(!g_instance);
  g_instance = new VariationsIdsProvider(mode);
  return g_instance;
}

// static
VariationsIdsProvider* VariationsIdsProvider::GetInstance() {
  base::AutoLock lock(GetInstanceLock());
  DCHECK(g_instance);
  return g_instance;
}

variations::mojom::VariationsHeadersPtr
VariationsIdsProvider::GetClientDataHeaders(bool is_signed_in) {
  // Lazily initialize the header, if not already done, before attempting to
  // transmit it.
  InitVariationIDsCacheIfNeeded();

  if (mode_ == Mode::kIgnoreSignedInState)
    is_signed_in = true;

  std::string first_party_header_copy;
  std::string any_context_header_copy;
  {
    base::AutoLock lock(lock_);
    first_party_header_copy = GetClientDataHeaderWhileLocked(
        is_signed_in, Study_GoogleWebVisibility_FIRST_PARTY);
    any_context_header_copy = GetClientDataHeaderWhileLocked(
        is_signed_in, Study_GoogleWebVisibility_ANY);
  }

  if (first_party_header_copy.empty() && any_context_header_copy.empty())
    return nullptr;

  base::flat_map<variations::mojom::GoogleWebVisibility, std::string> headers =
      {{variations::mojom::GoogleWebVisibility::FIRST_PARTY,
        first_party_header_copy},
       {variations::mojom::GoogleWebVisibility::ANY, any_context_header_copy}};

  return variations::mojom::VariationsHeaders::New(headers);
}

std::string VariationsIdsProvider::GetVariationsString(
    const std::set<IDCollectionKey>& keys) {
  // Construct a space-separated string with leading and trailing spaces from
  // the VariationIDs set. The IDs in the string are in sorted order as per the
  // std::set contract.
  std::string ids_string = " ";

  for (const VariationID& id : GetVariationsVector(keys)) {
    ids_string.append(base::NumberToString(id));
    ids_string.push_back(' ');
  }

  return ids_string;
}

std::string VariationsIdsProvider::GetGoogleAppVariationsString() {
  return GetVariationsString({GOOGLE_APP});
}

std::string VariationsIdsProvider::GetVariationsString() {
  return GetVariationsString(
      {GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, GOOGLE_WEB_PROPERTIES_FIRST_PARTY});
}

std::vector<VariationID> VariationsIdsProvider::GetVariationsVector(
    const std::set<IDCollectionKey>& keys) {
  return GetVariationsVectorImpl(keys);
}

std::vector<VariationID>
VariationsIdsProvider::GetVariationsVectorForWebPropertiesKeys() {
  const std::set<IDCollectionKey> web_properties_keys{
      GOOGLE_WEB_PROPERTIES_ANY_CONTEXT,
      GOOGLE_WEB_PROPERTIES_FIRST_PARTY,
      GOOGLE_WEB_PROPERTIES_SIGNED_IN,
      GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT,
      GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY,
  };
  return GetVariationsVectorImpl(web_properties_keys);
}

void VariationsIdsProvider::SetLowEntropySourceValue(
    absl::optional<int> low_entropy_source_value) {
  // The low entropy source value is an integer that is between 0 and 7999,
  // inclusive. See components/metrics/metrics_state_manager.cc for the logic to
  // generate it.
  if (low_entropy_source_value) {
    DCHECK_GE(low_entropy_source_value.value(), 0);
    DCHECK_LE(low_entropy_source_value.value(), 7999);
  }
  low_entropy_source_value_ = low_entropy_source_value;
}

VariationsIdsProvider::ForceIdsResult VariationsIdsProvider::ForceVariationIds(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids) {
  default_variation_ids_set_.clear();

  if (!AddVariationIdsToSet(variation_ids, &default_variation_ids_set_))
    return ForceIdsResult::INVALID_VECTOR_ENTRY;

  if (!ParseVariationIdsParameter(command_line_variation_ids,
                                  &default_variation_ids_set_)) {
    return ForceIdsResult::INVALID_SWITCH_ENTRY;
  }
  if (variation_ids_cache_initialized_) {
    // Update the cached variation ids header value after cache initialization,
    // otherwise the change won't be in the cache.
    base::AutoLock scoped_lock(lock_);
    UpdateVariationIDsHeaderValue();
  }
  return ForceIdsResult::SUCCESS;
}

bool VariationsIdsProvider::ForceDisableVariationIds(
    const std::string& command_line_variation_ids) {
  force_disabled_ids_set_.clear();
  if (!ParseVariationIdsParameter(command_line_variation_ids,
                                  &force_disabled_ids_set_)) {
    return false;
  }
  if (variation_ids_cache_initialized_) {
    // Update the cached variation ids header value after cache initialization,
    // otherwise the change won't be in the cache.
    base::AutoLock scoped_lock(lock_);
    UpdateVariationIDsHeaderValue();
  }
  return true;
}

void VariationsIdsProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VariationsIdsProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VariationsIdsProvider::ResetForTesting() {
  base::AutoLock scoped_lock(lock_);

  // Stop observing field trials so that it can be restarted when this is
  // re-inited. Note: This is a no-op if this is not currently observing.
  base::FieldTrialList::RemoveObserver(this);
  variation_ids_cache_initialized_ = false;
  variation_ids_set_.clear();
  default_variation_ids_set_.clear();
  synthetic_variation_ids_set_.clear();
  force_disabled_ids_set_.clear();
  variations_headers_map_.clear();
}

VariationsIdsProvider::VariationsIdsProvider(Mode mode)
    : mode_(mode), variation_ids_cache_initialized_(false) {}

VariationsIdsProvider::~VariationsIdsProvider() {
  base::FieldTrialList::RemoveObserver(this);
}

// static
void VariationsIdsProvider::CreateInstanceForTesting(Mode mode) {
  base::AutoLock lock(GetInstanceLock());
  delete g_instance;
  g_instance = new VariationsIdsProvider(mode);
}

// static
void VariationsIdsProvider::DestroyInstanceForTesting() {
  base::AutoLock lock(GetInstanceLock());
  delete g_instance;
  g_instance = nullptr;
}

void VariationsIdsProvider::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  base::AutoLock scoped_lock(lock_);
  const size_t old_size = variation_ids_set_.size();
  CacheVariationsId(trial_name, group_name);
  if (variation_ids_set_.size() != old_size)
    UpdateVariationIDsHeaderValue();
}

void VariationsIdsProvider::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& groups) {
  base::AutoLock scoped_lock(lock_);

  synthetic_variation_ids_set_.clear();
  for (const SyntheticTrialGroup& group : groups) {
    VariationID id = GetGoogleVariationIDFromHashes(
        GOOGLE_WEB_PROPERTIES_ANY_CONTEXT, group.id);
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES_ANY_CONTEXT));
    }
    id = GetGoogleVariationIDFromHashes(GOOGLE_WEB_PROPERTIES_SIGNED_IN,
                                        group.id);
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES_SIGNED_IN));
    }
    // Google App IDs omitted because they should never be defined
    // synthetically.
  }
  UpdateVariationIDsHeaderValue();
}

void VariationsIdsProvider::InitVariationIDsCacheIfNeeded() {
  base::AutoLock scoped_lock(lock_);
  if (variation_ids_cache_initialized_)
    return;

  // Query the kRestrictGoogleWebVisibility feature to activate the
  // associated field trial, if any, so that querying it in
  // OnFieldTrialGroupFinalized() does not result in deadlock.
  //
  // Note: Must be done before the AddObserver() call below.
  base::FeatureList::IsEnabled(internal::kRestrictGoogleWebVisibility);

  // Register for additional cache updates. This is done before initializing the
  // cache to avoid a race that could cause registered FieldTrials to be missed.
  bool success = base::FieldTrialList::AddObserver(this);
  DCHECK(success);

  base::FieldTrial::ActiveGroups initial_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&initial_groups);

  for (const auto& entry : initial_groups) {
    CacheVariationsId(entry.trial_name, entry.group_name);
  }
  UpdateVariationIDsHeaderValue();

  variation_ids_cache_initialized_ = true;
}

void VariationsIdsProvider::CacheVariationsId(const std::string& trial_name,
                                              const std::string& group_name) {
  for (int i = 0; i < ID_COLLECTION_COUNT; ++i) {
    IDCollectionKey key = static_cast<IDCollectionKey>(i);
    const VariationID id = GetGoogleVariationID(key, trial_name, group_name);
    if (id != EMPTY_ID)
      variation_ids_set_.insert(VariationIDEntry(id, key));
  }
}

void VariationsIdsProvider::UpdateVariationIDsHeaderValue() {
  lock_.AssertAcquired();

  variations_headers_map_.clear();

  // Note that the list of IDs and the header could be temporarily out of sync
  // if IDs are added as the header is recreated. The receiving servers are OK
  // with such discrepancies.
  variations_headers_map_[VariationsHeaderKey{/*is_signed_in=*/false,
                                              Study_GoogleWebVisibility_ANY}] =
      GenerateBase64EncodedProto(/*is_signed_in=*/false,
                                 /*is_first_party_context=*/false);

  variations_headers_map_[VariationsHeaderKey{
      /*is_signed_in=*/false, Study_GoogleWebVisibility_FIRST_PARTY}] =
      GenerateBase64EncodedProto(/*is_signed_in=*/false,
                                 /*is_first_party_context=*/true);

  variations_headers_map_[VariationsHeaderKey{/*is_signed_in=*/true,
                                              Study_GoogleWebVisibility_ANY}] =
      GenerateBase64EncodedProto(/*is_signed_in=*/true,
                                 /*is_first_party_context=*/false);

  variations_headers_map_[VariationsHeaderKey{
      /*is_signed_in=*/true, Study_GoogleWebVisibility_FIRST_PARTY}] =
      GenerateBase64EncodedProto(/*is_signed_in=*/true,
                                 /*is_first_party_context=*/true);

  for (auto& observer : observer_list_) {
    observer.VariationIdsHeaderUpdated();
  }
}

std::string VariationsIdsProvider::GenerateBase64EncodedProto(
    bool is_signed_in,
    bool is_first_party_context) {
  std::set<VariationIDEntry> all_variation_ids_set = GetAllVariationIds();

  ClientVariations proto;
  for (const VariationIDEntry& entry : all_variation_ids_set) {
    switch (entry.second) {
      case GOOGLE_WEB_PROPERTIES_SIGNED_IN:
        if (is_signed_in)
          proto.add_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_ANY_CONTEXT:
        proto.add_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_FIRST_PARTY:
        if (base::FeatureList::IsEnabled(
                internal::kRestrictGoogleWebVisibility)) {
          if (is_first_party_context)
            proto.add_variation_id(entry.first);
        } else {
          // When the feature is not enabled, treat VariationIDs associated with
          // GOOGLE_WEB_PROPERTIES_FIRST_PARTY in the same way as those
          // associated with GOOGLE_WEB_PROPERTIES_ANY_CONTEXT.
          proto.add_variation_id(entry.first);
        }
        break;
      case GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT:
        proto.add_trigger_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY:
        if (base::FeatureList::IsEnabled(
                internal::kRestrictGoogleWebVisibility)) {
          if (is_first_party_context)
            proto.add_trigger_variation_id(entry.first);
        } else {
          // When the feature is not enabled, treat VariationIDs associated with
          // GOOGLE_WEB_PROPERTIES_TRIGGER_FIRST_PARTY in the same way as those
          // associated with GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT.
          proto.add_trigger_variation_id(entry.first);
        }
        break;
      case GOOGLE_APP:
        // These IDs should not be added into Google Web headers.
        break;
      case ID_COLLECTION_COUNT:
        // This case included to get full enum coverage for switch, so that
        // new enums introduce compiler warnings. Nothing to do for this.
        break;
    }
  }

  const size_t total_id_count =
      proto.variation_id_size() + proto.trigger_variation_id_size();

  if (total_id_count == 0)
    return std::string();

  // This is the bottleneck for the creation of the header, so validate the size
  // here. Force a hard maximum on the ID count in case the Variations server
  // returns too many IDs and DOSs receiving servers with large requests.
  DCHECK_LE(total_id_count, 50U);
  UMA_HISTOGRAM_COUNTS_100("Variations.Headers.ExperimentCount",
                           total_id_count);
  if (total_id_count > 75)
    return std::string();

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string hashed;
  base::Base64Encode(serialized, &hashed);
  return hashed;
}

// static
bool VariationsIdsProvider::AddVariationIdsToSet(
    const std::vector<std::string>& variation_ids,
    std::set<VariationIDEntry>* target_set) {
  for (const std::string& entry : variation_ids) {
    if (entry.empty()) {
      target_set->clear();
      return false;
    }
    bool trigger_id =
        base::StartsWith(entry, "t", base::CompareCase::SENSITIVE);
    // Remove the "t" prefix if it's there.
    std::string trimmed_entry = trigger_id ? entry.substr(1) : entry;

    int variation_id = 0;
    if (!base::StringToInt(trimmed_entry, &variation_id)) {
      target_set->clear();
      return false;
    }
    target_set->insert(VariationIDEntry(
        variation_id, trigger_id ? GOOGLE_WEB_PROPERTIES_TRIGGER_ANY_CONTEXT
                                 : GOOGLE_WEB_PROPERTIES_ANY_CONTEXT));
  }
  return true;
}

// static
bool VariationsIdsProvider::ParseVariationIdsParameter(
    const std::string& command_line_variation_ids,
    std::set<VariationIDEntry>* target_set) {
  if (command_line_variation_ids.empty())
    return true;

  std::vector<std::string> variation_ids_from_command_line =
      base::SplitString(command_line_variation_ids, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  return AddVariationIdsToSet(variation_ids_from_command_line, target_set);
}

std::string VariationsIdsProvider::GetClientDataHeaderWhileLocked(
    bool is_signed_in,
    Study_GoogleWebVisibility web_visibility) {
  lock_.AssertAcquired();

  auto it = variations_headers_map_.find(
      VariationsHeaderKey{is_signed_in, web_visibility});

  if (it == variations_headers_map_.end())
    return "";
  // Deliberately return a copy.
  return it->second;
}

std::set<VariationsIdsProvider::VariationIDEntry>
VariationsIdsProvider::GetAllVariationIds() {
  lock_.AssertAcquired();

  std::set<VariationIDEntry> all_variation_ids_set = default_variation_ids_set_;
  for (const VariationIDEntry& entry : variation_ids_set_) {
    all_variation_ids_set.insert(entry);
  }
  for (const VariationIDEntry& entry : synthetic_variation_ids_set_) {
    all_variation_ids_set.insert(entry);
  }
  for (const VariationIDEntry& entry : force_disabled_ids_set_) {
    all_variation_ids_set.erase(entry);
  }

  // Add the low entropy source value, if it exists, which has one of
  // 8000 possible values (between kLowEntropySourceVariationIdRange[Min/Max],
  // ~13 bits). This is the value that has been used for deriving the variation
  // ids included in the X-Client-Data header and therefore does not reveal
  // additional information about the client when there are more than 13
  // variations. A typical Chrome client has more than 13 variation ids
  // reported.
  //
  // The entropy source value is used for retrospective A/A tests to validate
  // that there's no existing bias between two randomized groups of clients for
  // a later A/B study.
  if (low_entropy_source_value_) {
    int source_value = low_entropy_source_value_.value() +
                       kLowEntropySourceVariationIdRangeMin;
    DCHECK_GE(source_value, kLowEntropySourceVariationIdRangeMin);
    DCHECK_LE(source_value, kLowEntropySourceVariationIdRangeMax);
    all_variation_ids_set.insert(
        VariationIDEntry(source_value, GOOGLE_WEB_PROPERTIES_FIRST_PARTY));
  }

  return all_variation_ids_set;
}

std::vector<VariationID> VariationsIdsProvider::GetVariationsVectorImpl(
    const std::set<IDCollectionKey>& keys) {
  InitVariationIDsCacheIfNeeded();

  // Get all the active variation ids while holding the lock.
  std::set<VariationIDEntry> all_variation_ids;
  {
    base::AutoLock scoped_lock(lock_);
    all_variation_ids = GetAllVariationIds();
  }

  // Copy the requested variations to the output vector.
  std::vector<VariationID> result;
  result.reserve(all_variation_ids.size());
  for (const VariationIDEntry& entry : all_variation_ids) {
    if (keys.find(entry.second) != keys.end())
      result.push_back(entry.first);
  }

  // Make sure each entry is unique. As a side effect, the output is sorted.
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

}  // namespace variations
