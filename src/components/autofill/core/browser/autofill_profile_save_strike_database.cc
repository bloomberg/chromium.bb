// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_save_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "url/gurl.h"

namespace autofill {

// Limit the number of domains for which the import of new profiles is disabled.
constexpr size_t kMaxStrikeEntities = 200;

// Once the limit of domains is reached, delete 50 to create a bit of headroom.
constexpr size_t kMaxStrikeEntitiesAfterCleanup = 150;

AutofillProfileSaveStrikeDatabase::AutofillProfileSaveStrikeDatabase(
    StrikeDatabaseBase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

AutofillProfileSaveStrikeDatabase::~AutofillProfileSaveStrikeDatabase() =
    default;

absl::optional<size_t> AutofillProfileSaveStrikeDatabase::GetMaximumEntries()
    const {
  return absl::make_optional(kMaxStrikeEntities);
}

absl::optional<size_t>
AutofillProfileSaveStrikeDatabase::GetMaximumEntriesAfterCleanup() const {
  return absl::make_optional(kMaxStrikeEntitiesAfterCleanup);
}

std::string AutofillProfileSaveStrikeDatabase::GetProjectPrefix() const {
  return "AutofillProfileSave";
}

int AutofillProfileSaveStrikeDatabase::GetMaxStrikesLimit() const {
  // The default limit for strikes is 3.
  return features::kAutofillAutoBlockSaveAddressProfilePromptStrikeLimit.Get();
}

absl::optional<base::TimeDelta>
AutofillProfileSaveStrikeDatabase::GetExpiryTimeDelta() const {
  // Expiry time is 180 days by default.
  return base::Days(
      features::kAutofillAutoBlockSaveAddressProfilePromptExpirationDays.Get());
}

bool AutofillProfileSaveStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

void AutofillProfileSaveStrikeDatabase::ClearStrikesByOrigin(
    const std::set<std::string>& hosts_to_delete) {
  ClearStrikesByOriginAndTimeInternal(hosts_to_delete, base::Time::Min(),
                                      base::Time::Max());
}

void AutofillProfileSaveStrikeDatabase::ClearStrikesByOriginAndTimeInternal(
    const std::set<std::string>& hosts_to_delete,
    base::Time delete_begin,
    base::Time delete_end) {
  if (delete_begin.is_null()) {
    delete_begin = base::Time::Min();
  }

  if (delete_end.is_null()) {
    delete_end = base::Time::Max();
  }

  std::vector<std::string> keys_to_delete;
  keys_to_delete.reserve(GetStrikeCache().size());

  for (auto const& entry : GetStrikeCache()) {
    std::string strike_id = GetIdFromKey(entry.first);
    if (strike_id.empty()) {
      continue;
    }

    base::Time last_update = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(entry.second.last_update_timestamp()));

    // Check if the time stamp of the record is within deletion range and if the
    // domain is deleted.
    if (last_update >= delete_begin && last_update <= delete_end &&
        hosts_to_delete.count(strike_id) != 0) {
      keys_to_delete.push_back(entry.first);
    }
  }

  ClearStrikesForKeys(keys_to_delete);
}

}  // namespace autofill
