// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_import_process.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill {

namespace {

using UserDecision = AutofillClient::SaveAddressProfileOfferUserDecision;

// Returns a unique import id.
AutofillProfileImportId GetImportId() {
  static AutofillProfileImportId next_import_id(0);
  next_import_id.value()++;
  return next_import_id;
}

}  // namespace

ProfileImportProcess::ProfileImportProcess(
    const AutofillProfile& observed_profile,
    const std::string& app_locale,
    const GURL& form_source_url,
    const PersonalDataManager* personal_data_manager,
    bool allow_only_silent_updates)
    : import_id_(GetImportId()),
      observed_profile_(observed_profile),
      app_locale_(app_locale),
      form_source_url_(form_source_url),
      personal_data_manager_(personal_data_manager),
      allow_only_silent_updates_(allow_only_silent_updates) {
  DetermineProfileImportType();
}

ProfileImportProcess::ProfileImportProcess(const ProfileImportProcess&) =
    default;

ProfileImportProcess& ProfileImportProcess::operator=(
    const ProfileImportProcess& other) = default;

ProfileImportProcess::~ProfileImportProcess() = default;

bool ProfileImportProcess::prompt_shown() const {
  return prompt_shown_;
}

bool ProfileImportProcess::UserDeclined() const {
  return user_decision_ == UserDecision::kDeclined ||
         user_decision_ == UserDecision::kEditDeclined ||
         user_decision_ == UserDecision::kMessageDeclined;
}

bool ProfileImportProcess::UserAccepted() const {
  return user_decision_ == UserDecision::kAccepted ||
         user_decision_ == UserDecision::kEditAccepted;
}

void ProfileImportProcess::DetermineProfileImportType() {
  AutofillProfileComparator comparator(app_locale_);
  bool is_mergeable_with_existing_profile = false;

  new_profiles_suppressed_for_domain_ =
      personal_data_manager_
          ? personal_data_manager_->IsNewProfileImportBlockedForDomain(
                form_source_url_)
          : false;

  int number_of_unchanged_profiles = 0;

  const std::vector<AutofillProfile*> existing_profiles =
      personal_data_manager_->GetProfiles();

  for (const auto* existing_profile : existing_profiles) {
    // If the existing profile is not mergeable with the observed profile, the
    // existing profile is not altered by this import.
    if (!comparator.AreMergeable(*existing_profile, observed_profile_)) {
      ++number_of_unchanged_profiles;
      continue;
    }

    // The observed profile is mergeable with an existing profile.
    // This information is used to determine if the observed profile classifies
    // as an import of a new profile or the import of a duplicate profile.
    is_mergeable_with_existing_profile = true;

    // Make a copy of the existing profile and merge it with the observation.
    // The return value of |MergeDataFrom()| indicates if the existing profile
    // was changed at all during that merge.
    AutofillProfile merged_profile = *existing_profile;
    if (!merged_profile.MergeDataFrom(observed_profile_, app_locale_)) {
      ++number_of_unchanged_profiles;
      continue;
    }

    // At this point, the observed profile was merged with the existing profile
    // which changed in some way.
    // Now, determine if the merge alters any settings-visible value, or if the
    // merge can  be considered as a silent update that does not need to get
    // user confirmation.
    if (AutofillProfileComparator::ProfilesHaveDifferentSettingsVisibleValues(
            *existing_profile, merged_profile)) {
      if (allow_only_silent_updates_) {
        ++number_of_unchanged_profiles;
        continue;
      }

      // Determine if the existing profile is blocked for updates.
      // If the personal data manager is not available the profile is considered
      // as not blocked.
      bool is_blocked_for_update =
          personal_data_manager_
              ? personal_data_manager_->IsProfileUpdateBlocked(
                    existing_profile->guid())
              : false;
      if (is_blocked_for_update) {
        ++number_of_blocked_profile_updates_;
      }

      // If a settings-visible value changed, the existing profile is the merge
      // candidate if no other merge candidate has already been found and if the
      // existing profile is not blocked for updates.
      if (!merge_candidate_.has_value() && !is_blocked_for_update) {
        merge_candidate_ = *existing_profile;
        import_candidate_ = merged_profile;
      } else {
        // If there is already a merge candidate, the existing profile is not
        // supposed to be changed.
        ++number_of_unchanged_profiles;
      }
      continue;
    }
    // If the profile changed but all settings-visible values are maintained,
    // the profile can be updated silently.
    merged_profile.set_modification_date(AutofillClock::Now());
    updated_profiles_.emplace_back(merged_profile);
  }

  // If the profile is not mergeable with an existing profile, the import
  // corresponds to a new profile.
  if (!is_mergeable_with_existing_profile) {
    if (!allow_only_silent_updates_) {
      // There should be no import candidate yet.
      DCHECK(!import_candidate_.has_value());
      if (new_profiles_suppressed_for_domain_) {
        import_type_ = AutofillProfileImportType::kSuppressedNewProfile;
      } else {
        import_type_ = AutofillProfileImportType::kNewProfile;
        import_candidate_ = observed_profile();
      }
    } else {
      import_type_ = AutofillProfileImportType::kUnusableIncompleteProfile;
    }
  } else {
    bool silent_updates_present = updated_profiles_.size() > 0;

    if (merge_candidate_.has_value()) {
      import_type_ =
          silent_updates_present
              ? AutofillProfileImportType::kConfirmableMergeAndSilentUpdate
              : AutofillProfileImportType::kConfirmableMerge;
    } else if (number_of_blocked_profile_updates_ > 0) {
      import_type_ =
          silent_updates_present
              ? AutofillProfileImportType::
                    kSuppressedConfirmableMergeAndSilentUpdate
              : AutofillProfileImportType::kSuppressedConfirmableMerge;
    } else if (allow_only_silent_updates_) {
      import_type_ =
          silent_updates_present
              ? AutofillProfileImportType::kSilentUpdateForIncompleteProfile
              : AutofillProfileImportType::kUnusableIncompleteProfile;
    } else {
      import_type_ = silent_updates_present
                         ? AutofillProfileImportType::kSilentUpdate
                         : AutofillProfileImportType::kDuplicateImport;
    }
  }

  if (import_candidate_.has_value()) {
    import_candidate_->set_modification_date(AutofillClock::Now());
  }

  // At this point, all existing profiles are either unchanged, updated and/or
  // one is the merge candidate.
  DCHECK_EQ(existing_profiles.size(),
            number_of_unchanged_profiles + updated_profiles_.size() +
                (merge_candidate_.has_value() ? 1 : 0));
  DCHECK_NE(import_type_, AutofillProfileImportType::kImportTypeUnspecified);
}

std::vector<AutofillProfile> ProfileImportProcess::GetResultingProfiles() {
  // At this point, a user decision must have been supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);

  std::vector<AutofillProfile> resulting_profiles;
  std::set<std::string> guids_of_changed_profiles;

  // Add all updated profiles.
  for (const auto& updated_profile : updated_profiles_) {
    resulting_profiles.push_back(updated_profile);
    guids_of_changed_profiles.insert(updated_profile.guid());
  }

  // If there is a confirmed import candidate, add it.
  if (confirmed_import_candidate_.has_value()) {
    resulting_profiles.emplace_back(confirmed_import_candidate_.value());
    guids_of_changed_profiles.insert(confirmed_import_candidate_->guid());
  }

  // Add all other profiles that are currently available in the personal data
  // manager.
  for (const auto* unchanged_profile : personal_data_manager_->GetProfiles()) {
    if (guids_of_changed_profiles.count(unchanged_profile->guid()) == 0) {
      resulting_profiles.push_back(*unchanged_profile);
    }
  }

  return resulting_profiles;
}

void ProfileImportProcess::SetUserDecision(
    UserDecision decision,
    absl::optional<AutofillProfile> edited_profile) {
  // A user decision should only be supplied once.
  DCHECK_EQ(user_decision_, UserDecision::kUndefined);
  DCHECK(!confirmed_import_candidate_.has_value());

  user_decision_ = decision;
  switch (user_decision_) {
    // If the import was accepted either with or without a prompt, the import
    // candidate gets confired.
    case UserDecision::kUserNotAsked:
    case UserDecision::kAccepted:
      confirmed_import_candidate_ = import_candidate_;
      break;

    case UserDecision::kEditAccepted:
      // If the import candidate is supplied, the 'edited_profile' must be
      // supplied.
      DCHECK(edited_profile.has_value());

      // Make sure the verification status of all settings-visible non-empty
      // fields in the edited profile are set to kUserVerified.
      for (auto type : GetUserVisibleTypes()) {
        std::u16string value = edited_profile->GetRawInfo(type);
        if (!value.empty() &&
            edited_profile->GetVerificationStatus(type) ==
                structured_address::VerificationStatus::kNoStatus) {
          edited_profile->SetRawInfoWithVerificationStatus(
              type, value,
              structured_address::VerificationStatus::kUserVerified);
        }
      }

      edited_profile->FinalizeAfterImport();
      edited_profile->set_modification_date(AutofillClock::Now());
      // The `edited_profile` has to have the same `guid` as the original import
      // candidate.
      DCHECK_EQ(import_candidate_.value().guid(), edited_profile->guid());
      confirmed_import_candidate_ = std::move(edited_profile);
      break;

    // If the confirmable merge was declided or ignored, the original merge
    // candidate should be maintined. Note that the decline/ignore does not mean
    // that silent updates are not performed.
    case UserDecision::kDeclined:
    case UserDecision::kEditDeclined:
    case UserDecision::kMessageDeclined:
    case UserDecision::kMessageTimeout:
    case UserDecision::kIgnored:
    case UserDecision::kAutoDeclined:
      confirmed_import_candidate_ = merge_candidate_;
      break;

    case UserDecision::kNever:
      break;

    case UserDecision::kUndefined:
      NOTREACHED();
      break;
  }
}

void ProfileImportProcess::AcceptWithoutPrompt() {
  SetUserDecision(UserDecision::kUserNotAsked);
}

void ProfileImportProcess::AcceptWithoutEdits() {
  SetUserDecision(UserDecision::kAccepted);
}

void ProfileImportProcess::AcceptWithEdits(AutofillProfile edited_profile) {
  SetUserDecision(UserDecision::kEditAccepted,
                  absl::make_optional(edited_profile));
}

void ProfileImportProcess::Declined() {
  SetUserDecision(UserDecision::kDeclined);
}

void ProfileImportProcess::Ignore() {
  SetUserDecision(UserDecision::kIgnored);
}

bool ProfileImportProcess::ProfilesChanged() const {
  // At this point, a user decision must have been supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);

  // If there are any updated profiles, return true.
  if (updated_profiles_.size() > 0) {
    return true;
  }

  // If there is no confirmed import candidate there are no changes.
  if (!confirmed_import_candidate_.has_value()) {
    return false;
  }

  // If the import was accepted, return true.
  if (user_decision_ == UserDecision::kAccepted ||
      user_decision_ == UserDecision::kEditAccepted ||
      user_decision_ == UserDecision::kUserNotAsked) {
    return true;
  }

  return false;
}

void ProfileImportProcess::set_prompt_was_shown() {
  prompt_shown_ = true;
}

void ProfileImportProcess::CollectMetrics() const {
  // Metrics should only be recorded after a user decision was supplied.
  DCHECK_NE(user_decision_, UserDecision::kUndefined);

  if (allow_only_silent_updates_) {
    // Record the import type for the silent updates.
    AutofillMetrics::LogSilentUpdatesProfileImportType(import_type_);
    return;
  }

  // For any finished import process record the type of the import.
  AutofillMetrics::LogProfileImportType(import_type_);

  // For an import process that involves prompting the user, record the
  // decision.
  if (import_type_ == AutofillProfileImportType::kNewProfile) {
    AutofillMetrics::LogNewProfileImportDecision(user_decision_);
  } else if (import_type_ == AutofillProfileImportType::kConfirmableMerge ||
             import_type_ ==
                 AutofillProfileImportType::kConfirmableMergeAndSilentUpdate) {
    AutofillMetrics::LogProfileUpdateImportDecision(user_decision_);

    DCHECK(merge_candidate_.has_value() && import_candidate_.has_value());

    // For all update prompts, log the field types and total number of fields
    // that would change due to the update. Note that this does not include
    // additional manual edits the user can perform in the storage dialog.
    // Those are covered separately below.
    const std::vector<ProfileValueDifference> merge_difference =
        AutofillProfileComparator::GetSettingsVisibleProfileDifference(
            import_candidate_.value(), merge_candidate_.value(), app_locale_);

    for (const auto& difference : merge_difference) {
      AutofillMetrics::LogProfileUpdateAffectedType(difference.type,
                                                    user_decision_);
    }

    AutofillMetrics::LogUpdateProfileNumberOfAffectedFields(
        merge_difference.size(), user_decision_);
  }

  // If the profile was edited by the user, record a histogram of edited types.
  if (user_decision_ == UserDecision::kEditAccepted) {
    const std::vector<ProfileValueDifference> edit_difference =
        AutofillProfileComparator::GetSettingsVisibleProfileDifference(
            import_candidate_.value(), confirmed_import_candidate_.value(),
            app_locale_);
    for (const auto& difference : edit_difference) {
      if (import_type_ == AutofillProfileImportType::kNewProfile) {
        AutofillMetrics::LogNewProfileEditedType(difference.type);
      } else {
        AutofillMetrics::LogProfileUpdateEditedType(difference.type);
      }
    }
    if (import_type_ == AutofillProfileImportType::kNewProfile) {
      AutofillMetrics::LogNewProfileNumberOfEditedFields(
          edit_difference.size());
    } else {
      AutofillMetrics::LogUpdateProfileNumberOfEditedFields(
          edit_difference.size());
    }
  }
}

}  // namespace autofill
