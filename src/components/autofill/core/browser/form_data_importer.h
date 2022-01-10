// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/payments/credit_card_save_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/upi_vpa_save_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class SaveCardOfferObserver;

namespace autofill {

class AddressProfileSaveManager;

// Manages logic for importing address profiles and credit card information from
// web forms into the user's Autofill profile via the PersonalDataManager.
// Owned by BrowserAutofillManager.
class FormDataImporter {
 public:
  // Record type of the credit card imported from the form, if one exists.
  enum ImportedCreditCardRecordType {
    // No card was successfully imported from the form.
    NO_CARD,
    // The imported card is already stored locally on the device.
    LOCAL_CARD,
    // The imported card is already known to be a server card (either masked or
    // unmasked).
    SERVER_CARD,
    // The imported card is not currently stored with the browser.
    NEW_CARD,
  };

  // The parameters should outlive the FormDataImporter.
  FormDataImporter(AutofillClient* client,
                   payments::PaymentsClient* payments_client,
                   PersonalDataManager* personal_data_manager,
                   const std::string& app_locale);

  FormDataImporter(const FormDataImporter&) = delete;
  FormDataImporter& operator=(const FormDataImporter&) = delete;

  ~FormDataImporter();

  // Imports the form data, submitted by the user, into
  // |personal_data_manager_|. If a new credit card was detected and
  // |credit_card_autofill_enabled| is set to |true|, also begins the process to
  // offer local or upload credit card save.
  void ImportFormData(const FormStructure& submitted_form,
                      bool profile_autofill_enabled,
                      bool credit_card_autofill_enabled);

  // Extract credit card from the form structure. This function allows for
  // duplicated field types in the form.
  CreditCard ExtractCreditCardFromForm(const FormStructure& form);

  // Checks suitability of |profile| for adding to the user's set of profiles.
  static bool IsValidLearnableProfile(const AutofillProfile& profile,
                                      const std::string& finch_country_code,
                                      const std::string& app_locale,
                                      LogBuffer* import_log_buffer);

  // Cache the last four of the fetched virtual card so we don't offer saving
  // them.
  void CacheFetchedVirtualCard(const std::u16string& last_four);

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  LocalCardMigrationManager* local_card_migration_manager() {
    return local_card_migration_manager_.get();
  }
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

 protected:
  // Exposed for testing.
  void set_credit_card_save_manager(
      std::unique_ptr<CreditCardSaveManager> credit_card_save_manager) {
    credit_card_save_manager_ = std::move(credit_card_save_manager);
  }

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Exposed for testing.
  void set_local_card_migration_manager(
      std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager) {
    local_card_migration_manager_ = std::move(local_card_migration_manager);
  }
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

 private:
  // Defines a candidate for address profile import.
  struct AddressProfileImportCandidate {
    // The profile that was extracted from the form.
    AutofillProfile profile;
    // The URL the profile was extracted from.
    GURL url;
    // Indicates if all import requirements have been fulfilled.
    bool all_requirements_fulfilled;
    AddressProfileImportCandidate(AddressProfileImportCandidate&& other) =
        default;
    AddressProfileImportCandidate& operator=(
        AddressProfileImportCandidate&& other) = default;
  };

  // Scans the given |form| for importable Autofill data. If the form includes
  // sufficient address data for a new profile, it is immediately imported. If
  // the form includes sufficient credit card data for a new credit card and
  // |credit_card_autofill_enabled| is set to |true|, it is stored into
  // |imported_credit_card| so that we can prompt the user whether to save this
  // data. If the form contains credit card data already present in a local
  // credit card entry *and* |should_return_local_card| is true, the data is
  // stored into |imported_credit_card| so that we can prompt the user whether
  // to upload it. If the form contains UPI data and
  // |credit_card_autofill_enabled| is true, the UPI ID will be stored into
  // |imported_upi_id|. Returns |true| if sufficient address or credit card data
  // was found. Exposed for testing.
  bool ImportFormData(const FormStructure& form,
                      bool profile_autofill_enabled,
                      bool credit_card_autofill_enabled,
                      bool should_return_local_card,
                      std::unique_ptr<CreditCard>* imported_credit_card,
                      std::vector<AddressProfileImportCandidate>&
                          address_profile_import_candidates,
                      absl::optional<std::string>* imported_upi_id);

  // Go through the |form| fields and attempt to extract and import valid
  // address profiles. Returns true on extraction success of at least one
  // profile. There are many reasons that extraction may fail (see
  // implementation).  The function returns true if at least one complete
  // address profile was found.
  bool ImportAddressProfiles(
      const FormStructure& form,
      std::vector<AddressProfileImportCandidate>& import_candidates);

  // Helper method for ImportAddressProfiles which only considers the fields for
  // a specified |section|. If |section| is the empty string, the import is
  // performed on the union of all sections.
  bool ImportAddressProfileForSection(
      const FormStructure& form,
      const std::string& section,
      std::vector<AddressProfileImportCandidate>& import_candidates,
      LogBuffer* import_log_buffer);

  // Go through the |form| fields and attempt to extract a new credit card in
  // |imported_credit_card|, or update an existing card.
  // |should_return_local_card| will indicate whether |imported_credit_card| is
  // filled even if an existing card was updated. Success is defined as having
  // a new card to import, or having merged with an existing card.
  bool ImportCreditCard(const FormStructure& form,
                        bool should_return_local_card,
                        std::unique_ptr<CreditCard>* imported_credit_card);

  // Tries to initiate the saving of |imported_credit_card| if applicable.
  // |submitted_form| is the form from which the card was imported.
  // If a UPI id was found it is stored in |detected_upi_id|.
  // |credit_card_autofill_enabled| indicates if credit card filling is enabled
  // and |is_credit_card_upstream_enabled| indicates if server card storage is
  // enabled. Returns true if a save is initiated.
  bool ProcessCreditCardImportCandidate(
      const FormStructure& submitted_form,
      std::unique_ptr<CreditCard> imported_credit_card,
      absl::optional<std::string> detected_upi_id,
      bool credit_card_autofill_enabled,
      bool is_credit_card_upstream_enabled);

  // Processes the address profile import candidates.
  // |import_candidates| contains the addresses extracted from the form.
  // |allow_prompt| denotes if a prompt can be shown.
  // Returns true if the import of a complete profile is initiated.
  bool ProcessAddressProfileImportCandidates(
      const std::vector<AddressProfileImportCandidate>& import_candidates,
      bool allow_prompt = true);

  // Extracts credit card from the form structure. |hasDuplicateFieldType| will
  // be set as true if there are duplicated field types in the form.
  CreditCard ExtractCreditCardFromForm(const FormStructure& form,
                                       bool* hasDuplicateFieldType);

  // Go through the |form| fields and find a UPI ID to import. The return value
  // will be empty if no UPI ID was found.
  absl::optional<std::string> ImportUpiId(const FormStructure& form);

  // Whether a dynamic change form is imported.
  bool from_dynamic_change_form_ = false;

  // Whether the form imported has non-focusable fields after user entered
  // information into it.
  bool has_non_focusable_field_ = false;

  // The associated autofill client. Weak reference.
  raw_ptr<AutofillClient> client_;

  // Responsible for managing credit card save flows (local or upload).
  std::unique_ptr<CreditCardSaveManager> credit_card_save_manager_;

  // Responsible for managing address profiles save flows.
  std::unique_ptr<AddressProfileSaveManager> address_profile_save_manager_;

#if !defined(OS_ANDROID) && !defined(OS_IOS)
  // Responsible for migrating locally saved credit cards to Google Pay.
  std::unique_ptr<LocalCardMigrationManager> local_card_migration_manager_;

  // Responsible for managing UPI/VPA save flows.
  std::unique_ptr<UpiVpaSaveManager> upi_vpa_save_manager_;
#endif  // #if !defined(OS_ANDROID) && !defined(OS_IOS)

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the BrowserAutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  raw_ptr<PersonalDataManager> personal_data_manager_;

  // Represents the type of the imported credit card from the submitted form.
  // It will be used to determine whether to offer Upstream or card migration.
  // Will be passed to |credit_card_save_manager_| for metrics.
  ImportedCreditCardRecordType imported_credit_card_record_type_;

  std::string app_locale_;

  // Used to store the last four digits of the fetched virtual cards.
  base::flat_set<std::u16string> fetched_virtual_cards_;

  friend class AutofillMergeTest;
  friend class FormDataImporterTest;
  friend class FormDataImporterTestBase;
  friend class LocalCardMigrationBrowserTest;
  friend class SaveCardBubbleViewsFullFormBrowserTest;
  friend class SaveCardInfobarEGTestHelper;
  friend class ::SaveCardOfferObserver;
  FRIEND_TEST_ALL_PREFIXES(AutofillMergeTest, MergeProfiles);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           AllowDuplicateMaskedServerCardIfFlagEnabled);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, DontDuplicateFullServerCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, DontDuplicateMaskedServerCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_AddressesDisabledOneCreditCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_AddressCreditCardDisabled);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_HiddenCreditCardFormAfterEntered);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_FullServerCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_ImportCreditCardRecordType_LocalCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_MaskedServerCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_ImportCreditCardRecordType_NewCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_ExpiredCard_EditableExpDateOff);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NewCard_ExpiredCard_WithExpDateFixFlow);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_InvalidCardNumber);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_VirtualCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_ImportCreditCardRecordType_NoCard_NoCardOnForm);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_OneAddressCreditCardDisabled);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_OneAddressOneCreditCard);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      ImportFormData_SecondImportResetsCreditCardRecordType);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_TwoAddressesOneCreditCard);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest,
                           ImportFormData_DontSetUpiIdWhenOnlyCreditCardExists);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_FullServerCardMatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_FullServerCardMismatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMismatch);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationMonth);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationYear);
  FRIEND_TEST_ALL_PREFIXES(
      FormDataImporterTest,
      Metrics_SubmittedDifferentServerCardExpirationStatus_EmptyExpirationYear);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, ImportUpiId);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, ImportUpiIdDisabled);
  FRIEND_TEST_ALL_PREFIXES(FormDataImporterTest, ImportUpiIdIgnoreNonUpiId);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_DATA_IMPORTER_H_
