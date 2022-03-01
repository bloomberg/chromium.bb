// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_update_strike_database.h"

#include "components/autofill/core/browser/proto/strike_data.pb.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

// Limit the number of profiles for which an update os blocked.
constexpr size_t kMaxStrikeEntities = 100;

// Once the limit of profiles is reached, delete 30 to create a bit of headroom.
constexpr size_t kMaxStrikeEntitiesAfterCleanup = 70;

AutofillProfileUpdateStrikeDatabase::AutofillProfileUpdateStrikeDatabase(
    StrikeDatabaseBase* strike_database)
    : StrikeDatabaseIntegratorBase(strike_database) {
  RemoveExpiredStrikes();
}

AutofillProfileUpdateStrikeDatabase::~AutofillProfileUpdateStrikeDatabase() =
    default;

absl::optional<size_t> AutofillProfileUpdateStrikeDatabase::GetMaximumEntries()
    const {
  return absl::make_optional(kMaxStrikeEntities);
}

absl::optional<size_t>
AutofillProfileUpdateStrikeDatabase::GetMaximumEntriesAfterCleanup() const {
  return absl::make_optional(kMaxStrikeEntitiesAfterCleanup);
}

std::string AutofillProfileUpdateStrikeDatabase::GetProjectPrefix() const {
  return "AutofillProfileUpdate";
}

int AutofillProfileUpdateStrikeDatabase::GetMaxStrikesLimit() const {
  // The default limit for strikes is 3.
  return features::kAutofillAutoBlockUpdateAddressProfilePromptStrikeLimit
      .Get();
}

absl::optional<base::TimeDelta>
AutofillProfileUpdateStrikeDatabase::GetExpiryTimeDelta() const {
  // Expiry time is 180 days by default.
  return base::Days(
      features::kAutofillAutoBlockUpdateAddressProfilePromptExpirationDays
          .Get());
}

bool AutofillProfileUpdateStrikeDatabase::UniqueIdsRequired() const {
  return true;
}

}  // namespace autofill
