// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_profile_save_manager.h"

#include "base/stl_util.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

using UserDecision = AutofillClient::SaveAddressProfileOfferUserDecision;

AddressProfileSaveManager::AddressProfileSaveManager(
    AutofillClient* client,
    PersonalDataManager* personal_data_manager)
    : client_(client), personal_data_manager_(personal_data_manager) {}

AddressProfileSaveManager::~AddressProfileSaveManager() = default;

void AddressProfileSaveManager::ImportProfileFromForm(
    const AutofillProfile& observed_profile,
    const std::string& app_locale,
    const GURL& url,
    bool allow_only_silent_updates) {
  // Without a personal data manager, profile storage is not possible.
  if (!personal_data_manager_)
    return;

  // If the explicit save prompts are not enabled, revert back to the legacy
  // behavior and directly import the observed profile without recording any
  // additional metrics. However, if only silent updates are allowed, proceed
  // with the profile import process.
  if (!base::FeatureList::IsEnabled(
          features::kAutofillAddressProfileSavePrompt) &&
      !allow_only_silent_updates) {
    personal_data_manager_->SaveImportedProfile(observed_profile);
    return;
  }

  auto process_ptr = std::make_unique<ProfileImportProcess>(
      observed_profile, app_locale, url, personal_data_manager_,
      allow_only_silent_updates);

  MaybeOfferSavePrompt(std::move(process_ptr));
}

void AddressProfileSaveManager::MaybeOfferSavePrompt(
    std::unique_ptr<ProfileImportProcess> import_process) {
  switch (import_process->import_type()) {
    // If the import was a duplicate, only results in silent updates or if the
    // import of a new profile or a profile update is blocked, finish the
    // process without initiating a user prompt
    case AutofillProfileImportType::kDuplicateImport:
    case AutofillProfileImportType::kSilentUpdate:
    case AutofillProfileImportType::kSilentUpdateForIncompleteProfile:
    case AutofillProfileImportType::kSuppressedNewProfile:
    case AutofillProfileImportType::kSuppressedConfirmableMergeAndSilentUpdate:
    case AutofillProfileImportType::kSuppressedConfirmableMerge:
    case AutofillProfileImportType::kUnusableIncompleteProfile:
      import_process->AcceptWithoutPrompt();
      FinalizeProfileImport(std::move(import_process));
      return;

    // Both the import of a new profile, or a merge with an existing profile
    // that changes a settings-visible value of an existing profile triggers a
    // user prompt.
    case AutofillProfileImportType::kNewProfile:
    case AutofillProfileImportType::kConfirmableMerge:
    case AutofillProfileImportType::kConfirmableMergeAndSilentUpdate:
      // Emulates manually accepting new profiles and profile updates for
      // testing purposes. This should only be applied in tests.
      if (personal_data_manager_->auto_accept_address_imports_for_testing()) {
        import_process->AcceptWithoutEdits();
        FinalizeProfileImport(std::move(import_process));
        return;
      }
      OfferSavePrompt(std::move(import_process));
      return;

    case AutofillProfileImportType::kImportTypeUnspecified:
      NOTREACHED();
      return;
  }
}

void AddressProfileSaveManager::OfferSavePrompt(
    std::unique_ptr<ProfileImportProcess> import_process) {
  // The prompt should not have been shown yet.
  DCHECK(!import_process->prompt_shown());

  // TODO(crbug.com/1175693): Pass the correct SaveAddressProfilePromptOptions
  // below.

  // TODO(crbug.com/1175693): Check import_process->set_prompt_was_shown() is
  // always correct even in cases where it conflicts with
  // SaveAddressProfilePromptOptions

  // Initiate the prompt and mark it as shown.
  // The import process that carries to state of the current import process is
  // attached to the callback.
  import_process->set_prompt_was_shown();
  ProfileImportProcess* process_ptr = import_process.get();
  client_->ConfirmSaveAddressProfile(
      process_ptr->import_candidate().value(),
      base::OptionalOrNullptr(process_ptr->merge_candidate()),
      AutofillClient::SaveAddressProfilePromptOptions{.show_prompt = true},
      base::BindOnce(&AddressProfileSaveManager::OnUserDecision,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(import_process)));
}

void AddressProfileSaveManager::OnUserDecision(
    std::unique_ptr<ProfileImportProcess> import_process,
    UserDecision decision,
    AutofillProfile edited_profile) {
  DCHECK(import_process->prompt_shown());

  import_process->SetUserDecision(decision, edited_profile);
  FinalizeProfileImport(std::move(import_process));
}

void AddressProfileSaveManager::FinalizeProfileImport(
    std::unique_ptr<ProfileImportProcess> import_process) {
  DCHECK(personal_data_manager_);

  // If the profiles changed at all, reset the full list of AutofillProfiles in
  // the personal data manager.
  if (import_process->ProfilesChanged()) {
    std::vector<AutofillProfile> resulting_profiles =
        import_process->GetResultingProfiles();
    personal_data_manager_->SetProfiles(&resulting_profiles);
  }

  AutofillProfileImportType import_type = import_process->import_type();

  // If the import of a new profile was declined, add a strike for this source
  // url. If it was accepted, reset the potentially existing strikes.
  if (import_type == AutofillProfileImportType::kNewProfile) {
    if (import_process->UserDeclined()) {
      personal_data_manager_->AddStrikeToBlockNewProfileImportForDomain(
          import_process->form_source_url());
    } else if (import_process->UserAccepted()) {
      personal_data_manager_->RemoveStrikesToBlockNewProfileImportForDomain(
          import_process->form_source_url());
    }
  } else if (import_type == AutofillProfileImportType::kConfirmableMerge ||
             import_type ==
                 AutofillProfileImportType::kConfirmableMergeAndSilentUpdate) {
    DCHECK(import_process->merge_candidate().has_value());
    if (import_process->UserDeclined()) {
      personal_data_manager_->AddStrikeToBlockProfileUpdate(
          import_process->merge_candidate()->guid());
    } else if (import_process->UserAccepted()) {
      personal_data_manager_->RemoveStrikesToBlockProfileUpdate(
          import_process->merge_candidate()->guid());
    }
  }

  import_process->CollectMetrics();
  ClearPendingImport(std::move(import_process));
}

void AddressProfileSaveManager::ClearPendingImport(
    std::unique_ptr<ProfileImportProcess> import_process) {}

}  // namespace autofill
