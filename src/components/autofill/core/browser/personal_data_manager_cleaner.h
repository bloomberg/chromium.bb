// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_CLEANER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_CLEANER_H_

#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/test_data_creator.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/sync/base/model_type.h"

class PrefService;

namespace autofill {

class PersonalDataManager;

// PersonalDataManagerCleaner is responsible for applying address and credit
// card cleanups once on browser startup provided that the sync is enabled or
// when the sync starts.
class PersonalDataManagerCleaner {
 public:
  PersonalDataManagerCleaner(
      PersonalDataManager* personal_data_manager,
      AlternativeStateNameMapUpdater* alternative_state_name_map_updater,
      PrefService* pref_service);
  ~PersonalDataManagerCleaner();
  PersonalDataManagerCleaner(const PersonalDataManagerCleaner&) = delete;
  PersonalDataManagerCleaner& operator=(const PersonalDataManagerCleaner&) =
      delete;

  // Applies address and credit card fixes and cleanups if the sync is enabled.
  // Also, logs address, credit card and offer startup metrics.
  void CleanupDataAndNotifyPersonalDataObservers();

  // Applies address/credit card fixes and cleanups depending on the
  // |model_type|.
  void SyncStarted(syncer::ModelType model_type);

#if defined(UNIT_TEST)
  // A wrapper around |ApplyDedupingRoutine()| used for testing purposes.
  bool ApplyDedupingRoutineForTesting() { return ApplyDedupingRoutine(); }

  // A wrapper around |DedupeProfiles()| used for testing purposes.
  void DedupeProfilesForTesting(
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      std::unordered_set<std::string>* profile_guids_to_delete,
      std::unordered_map<std::string, std::string>* guids_merge_map) const {
    DedupeProfiles(existing_profiles, profile_guids_to_delete, guids_merge_map);
  }

  // A wrapper around |UpdateCardsBillingAddressReference()| used for testing
  // purposes.
  void UpdateCardsBillingAddressReferenceForTesting(
      const std::unordered_map<std::string, std::string>& guids_merge_map) {
    UpdateCardsBillingAddressReference(guids_merge_map);
  }

  // A wrapper around |DeleteDisusedAddresses()| used for testing purposes.
  bool DeleteDisusedAddressesForTesting() { return DeleteDisusedAddresses(); }

  // A wrapper around |DeleteDisusedCreditCards()| used for testing purposes.
  bool DeleteDisusedCreditCardsForTesting() {
    return DeleteDisusedCreditCards();
  }

  // A wrapper around |ClearProfileNonSettingsOrigins()| used for testing
  // purposes.
  void ClearProfileNonSettingsOriginsForTesting() {
    ClearProfileNonSettingsOrigins();
  }

  // A wrapper around |ClearCreditCardNonSettingsOrigins()| used for testing
  // purposes.
  void ClearCreditCardNonSettingsOriginsForTesting() {
    ClearCreditCardNonSettingsOrigins();
  }
#endif  // defined(UNIT_TEST)

 private:
  // Applies various fixes and cleanups on autofill addresses.
  void ApplyAddressFixesAndCleanups();

  // Applies various fixes and cleanups on autofill credit cards.
  void ApplyCardFixesAndCleanups();

  // Runs the routine that removes the orphan rows in the autofill tables if
  // it's never been done.
  void RemoveOrphanAutofillTableRows();

  // Applies the deduping routine once per major version if the feature is
  // enabled. Calls DedupeProfiles with the content of
  // |PersonalDataManager::GetProfiles()| as a parameter. Removes the profiles
  // to delete from the database and updates the others. Also updates the credit
  // cards billing address references. Returns true if the routine was run.
  bool ApplyDedupingRoutine();

  // Goes through all the |existing_profiles| and merges all similar unverified
  // profiles together. Also discards unverified profiles that are similar to a
  // verified profile. All the profiles except the results of the merges will be
  // added to |profile_guids_to_delete|. This routine should be run once per
  // major version. Records all the merges into the |guids_merge_map|.
  //
  // This method should only be called by ApplyDedupingRoutine. It is split for
  // testing purposes.
  void DedupeProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
      std::unordered_set<std::string>* profile_guids_to_delete,
      std::unordered_map<std::string, std::string>* guids_merge_map) const;

  // Updates the credit cards billing address references based on the merges
  // that happened during the dedupe, as defined in |guids_merge_map|. Also
  // updates the cards entries in the database.
  void UpdateCardsBillingAddressReference(
      const std::unordered_map<std::string, std::string>& guids_merge_map);

  // Tries to delete disused addresses once per major version if the
  // feature is enabled.
  bool DeleteDisusedAddresses();

  // Tries to delete disused credit cards once per major version if the
  // feature is enabled.
  bool DeleteDisusedCreditCards();

  // Clears the value of the origin field of the autofill profiles or cards that
  // were not created from the settings page.
  void ClearProfileNonSettingsOrigins();
  void ClearCreditCardNonSettingsOrigins();

  // Used for adding test data by |test_data_creator_|.
  void AddProfileForTest(const AutofillProfile& profile);
  void AddCreditCardForTest(const CreditCard& credit_card);

  // True if autofill profile cleanup needs to be performed.
  bool is_autofill_profile_cleanup_pending_ = false;

  // Used to create test data. If the AutofillCreateDataForTest feature is
  // enabled, this helper creates autofill profiles and credit card data that
  // would otherwise be difficult to create manually using the UI.
  TestDataCreator test_data_creator_;

  // The personal data manager, used to load and update the personal data
  // from/to the web database.
  const raw_ptr<PersonalDataManager> personal_data_manager_ = nullptr;

  // The PrefService used by this instance.
  const raw_ptr<PrefService> pref_service_ = nullptr;

  // The AlternativeStateNameMapUpdater, used to populate
  // AlternativeStateNameMap with the geographical state data.
  const raw_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_ = nullptr;

  // base::WeakPtr ensures that the callback bound to the object is canceled
  // when that object is destroyed.
  base::WeakPtrFactory<PersonalDataManagerCleaner> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_CLEANER_H_
