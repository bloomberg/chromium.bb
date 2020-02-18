// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_http_header_provider.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/variations/proto/client_variations.pb.h"

namespace variations {

// The following documents how adding/removing http headers for web content
// requests are implemented when Network Service is enabled or not enabled.
//
// When Network Service is not enabled, adding headers is implemented in
// ChromeResourceDispatcherHostDelegate::RequestBeginning() by calling
// variations::AppendVariationHeaders(), and removing headers is implemented in
// ChromeNetworkDelegate::OnBeforeRedirect() by calling
// variations::StripVariationHeaderIfNeeded().
//
// When Network Service is enabled, adding/removing headers is implemented by
// request consumers, and how it is implemented depends on the request type.
// There are three cases:
// 1. Subresources request in renderer, it is implemented
// in URLLoaderThrottleProviderImpl::CreateThrottles() by adding a
// GoogleURLLoaderThrottle to a content::URLLoaderThrottle vector.
// 2. Navigations/Downloads request in browser, it is implemented in
// ChromeContentBrowserClient::CreateURLLoaderThrottles() by also adding a
// GoogleURLLoaderThrottle to a content::URLLoaderThrottle vector.
// 3. SimpleURLLoader in browser, it is implemented in a SimpleURLLoader wrapper
// function variations::CreateSimpleURLLoaderWithVariationsHeader().

// static
VariationsHttpHeaderProvider* VariationsHttpHeaderProvider::GetInstance() {
  return base::Singleton<VariationsHttpHeaderProvider>::get();
}

std::string VariationsHttpHeaderProvider::GetClientDataHeader(
    bool is_signed_in) {
  // Lazily initialize the header, if not already done, before attempting to
  // transmit it.
  InitVariationIDsCacheIfNeeded();

  std::string variation_ids_header_copy;
  {
    base::AutoLock scoped_lock(lock_);
    variation_ids_header_copy = is_signed_in
                                    ? cached_variation_ids_header_signed_in_
                                    : cached_variation_ids_header_;
  }
  return variation_ids_header_copy;
}

std::string VariationsHttpHeaderProvider::GetVariationsString() {
  InitVariationIDsCacheIfNeeded();

  // Construct a space-separated string with leading and trailing spaces from
  // the variations set. Note: The ids in it will be in sorted order per the
  // std::set contract.
  std::string ids_string = " ";
  {
    base::AutoLock scoped_lock(lock_);
    for (const VariationIDEntry& entry : GetAllVariationIds()) {
      if (entry.second == GOOGLE_WEB_PROPERTIES) {
        ids_string.append(base::NumberToString(entry.first));
        ids_string.push_back(' ');
      }
    }
  }
  return ids_string;
}

std::vector<VariationID> VariationsHttpHeaderProvider::GetVariationsVector(
    IDCollectionKey key) {
  InitVariationIDsCacheIfNeeded();

  // Get all the active variation ids while holding the lock.
  std::set<VariationIDEntry> all_variation_ids;
  {
    base::AutoLock scoped_lock(lock_);
    all_variation_ids = GetAllVariationIds();
  }

  // Copy the requested variations to the output vector. Note that the ids will
  // be in sorted order because they're coming from a std::set.
  std::vector<VariationID> result;
  result.reserve(all_variation_ids.size());
  for (const VariationIDEntry& entry : all_variation_ids) {
    if (entry.second == key)
      result.push_back(entry.first);
  }
  return result;
}

VariationsHttpHeaderProvider::ForceIdsResult
VariationsHttpHeaderProvider::ForceVariationIds(
    const std::vector<std::string>& variation_ids,
    const std::string& command_line_variation_ids) {
  default_variation_ids_set_.clear();

  if (!AddVariationIdsToSet(variation_ids, &default_variation_ids_set_))
    return ForceIdsResult::INVALID_VECTOR_ENTRY;

  if (!ParseVariationIdsParameter(command_line_variation_ids,
                                  &default_variation_ids_set_)) {
    return ForceIdsResult::INVALID_SWITCH_ENTRY;
  }
  return ForceIdsResult::SUCCESS;
}

bool VariationsHttpHeaderProvider::ForceDisableVariationIds(
    const std::string& command_line_variation_ids) {
  force_disabled_ids_set_.clear();
  return ParseVariationIdsParameter(command_line_variation_ids,
                                    &force_disabled_ids_set_);
}

void VariationsHttpHeaderProvider::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void VariationsHttpHeaderProvider::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void VariationsHttpHeaderProvider::ResetForTesting() {
  base::AutoLock scoped_lock(lock_);

  // Stop observing field trials so that it can be restarted when this is
  // re-inited. Note: This is a no-op if this is not currently observing.
  base::FieldTrialList::RemoveObserver(this);
  variation_ids_cache_initialized_ = false;
  variation_ids_set_.clear();
  default_variation_ids_set_.clear();
  synthetic_variation_ids_set_.clear();
  force_disabled_ids_set_.clear();
  cached_variation_ids_header_.clear();
  cached_variation_ids_header_signed_in_.clear();
}

VariationsHttpHeaderProvider::VariationsHttpHeaderProvider()
    : variation_ids_cache_initialized_(false) {}

VariationsHttpHeaderProvider::~VariationsHttpHeaderProvider() {
  base::FieldTrialList::RemoveObserver(this);
}

void VariationsHttpHeaderProvider::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  base::AutoLock scoped_lock(lock_);
  const size_t old_size = variation_ids_set_.size();
  CacheVariationsId(trial_name, group_name, GOOGLE_WEB_PROPERTIES);
  CacheVariationsId(trial_name, group_name, GOOGLE_WEB_PROPERTIES_SIGNED_IN);
  CacheVariationsId(trial_name, group_name, GOOGLE_WEB_PROPERTIES_TRIGGER);
  if (variation_ids_set_.size() != old_size)
    UpdateVariationIDsHeaderValue();
}

void VariationsHttpHeaderProvider::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& groups) {
  base::AutoLock scoped_lock(lock_);

  synthetic_variation_ids_set_.clear();
  for (const SyntheticTrialGroup& group : groups) {
    VariationID id =
        GetGoogleVariationIDFromHashes(GOOGLE_WEB_PROPERTIES, group.id);
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES));
    }
    id = GetGoogleVariationIDFromHashes(GOOGLE_WEB_PROPERTIES_SIGNED_IN,
                                        group.id);
    if (id != EMPTY_ID) {
      synthetic_variation_ids_set_.insert(
          VariationIDEntry(id, GOOGLE_WEB_PROPERTIES_SIGNED_IN));
    }
  }
  UpdateVariationIDsHeaderValue();
}

void VariationsHttpHeaderProvider::InitVariationIDsCacheIfNeeded() {
  base::AutoLock scoped_lock(lock_);
  if (variation_ids_cache_initialized_)
    return;

  // Register for additional cache updates. This is done first to avoid a race
  // that could cause registered FieldTrials to be missed.
  bool success = base::FieldTrialList::AddObserver(this);
  DCHECK(success);

  base::FieldTrial::ActiveGroups initial_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&initial_groups);

  for (const auto& entry : initial_groups) {
    CacheVariationsId(entry.trial_name, entry.group_name,
                      GOOGLE_WEB_PROPERTIES);
    CacheVariationsId(entry.trial_name, entry.group_name,
                      GOOGLE_WEB_PROPERTIES_SIGNED_IN);
    CacheVariationsId(entry.trial_name, entry.group_name,
                      GOOGLE_WEB_PROPERTIES_TRIGGER);
  }
  UpdateVariationIDsHeaderValue();

  variation_ids_cache_initialized_ = true;
}

void VariationsHttpHeaderProvider::CacheVariationsId(
    const std::string& trial_name,
    const std::string& group_name,
    IDCollectionKey key) {
  const VariationID id = GetGoogleVariationID(key, trial_name, group_name);
  if (id != EMPTY_ID)
    variation_ids_set_.insert(VariationIDEntry(id, key));
}

void VariationsHttpHeaderProvider::UpdateVariationIDsHeaderValue() {
  lock_.AssertAcquired();

  // The header value is a serialized protobuffer of Variation IDs which is
  // base64 encoded before transmitting as a string.
  cached_variation_ids_header_.clear();
  cached_variation_ids_header_signed_in_.clear();

  // If successful, swap the header value with the new one.
  // Note that the list of IDs and the header could be temporarily out of sync
  // if IDs are added as the header is recreated. The receiving servers are OK
  // with such discrepancies.
  cached_variation_ids_header_ = GenerateBase64EncodedProto(false);
  cached_variation_ids_header_signed_in_ = GenerateBase64EncodedProto(true);

  for (auto& observer : observer_list_) {
    observer.VariationIdsHeaderUpdated(cached_variation_ids_header_,
                                       cached_variation_ids_header_signed_in_);
  }
}

std::string VariationsHttpHeaderProvider::GenerateBase64EncodedProto(
    bool is_signed_in) {
  std::set<VariationIDEntry> all_variation_ids_set = GetAllVariationIds();

  ClientVariations proto;
  for (const VariationIDEntry& entry : all_variation_ids_set) {
    switch (entry.second) {
      case GOOGLE_WEB_PROPERTIES_SIGNED_IN:
        if (is_signed_in)
          proto.add_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES:
        proto.add_variation_id(entry.first);
        break;
      case GOOGLE_WEB_PROPERTIES_TRIGGER:
        proto.add_trigger_variation_id(entry.first);
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
  DCHECK_LE(total_id_count, 35U);
  UMA_HISTOGRAM_COUNTS_100("Variations.Headers.ExperimentCount",
                           total_id_count);
  if (total_id_count > 50)
    return std::string();

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string hashed;
  base::Base64Encode(serialized, &hashed);
  return hashed;
}

// static
bool VariationsHttpHeaderProvider::AddVariationIdsToSet(
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
        variation_id,
        trigger_id ? GOOGLE_WEB_PROPERTIES_TRIGGER : GOOGLE_WEB_PROPERTIES));
  }
  return true;
}

// static
bool VariationsHttpHeaderProvider::ParseVariationIdsParameter(
    const std::string& command_line_variation_ids,
    std::set<VariationIDEntry>* target_set) {
  if (command_line_variation_ids.empty())
    return true;

  std::vector<std::string> variation_ids_from_command_line =
      base::SplitString(command_line_variation_ids, ",", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);
  return AddVariationIdsToSet(variation_ids_from_command_line, target_set);
}

std::set<VariationsHttpHeaderProvider::VariationIDEntry>
VariationsHttpHeaderProvider::GetAllVariationIds() {
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
  return all_variation_ids_set;
}

}  // namespace variations
