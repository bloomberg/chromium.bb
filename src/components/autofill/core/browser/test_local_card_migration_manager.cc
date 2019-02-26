// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/test_local_card_migration_manager.h"

#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/payments/test_payments_client.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace autofill {

TestLocalCardMigrationManager::TestLocalCardMigrationManager(
    AutofillDriver* driver,
    AutofillClient* client,
    payments::TestPaymentsClient* payments_client,
    PersonalDataManager* personal_data_manager)
    : LocalCardMigrationManager(client,
                                payments_client,
                                "en-US",
                                personal_data_manager) {}

TestLocalCardMigrationManager::~TestLocalCardMigrationManager() {}

bool TestLocalCardMigrationManager::IsCreditCardMigrationEnabled() {
  bool migration_experiment_enabled =
      features::GetLocalCardMigrationExperimentalFlag() !=
      features::LocalCardMigrationExperimentalFlag::kMigrationDisabled;
  bool has_google_payments_account =
      (static_cast<int64_t>(payments_client_->GetPrefService()->GetDouble(
           prefs::kAutofillBillingCustomerNumber)) != 0);
  return migration_experiment_enabled && has_google_payments_account;
}

bool TestLocalCardMigrationManager::LocalCardMigrationWasTriggered() {
  return local_card_migration_was_triggered_;
}

bool TestLocalCardMigrationManager::IntermediatePromptWasShown() {
  return intermediate_prompt_was_shown_;
}

bool TestLocalCardMigrationManager::MainPromptWasShown() {
  return main_prompt_was_shown_;
}

void TestLocalCardMigrationManager::
    OnUserAcceptedIntermediateMigrationDialog() {
  intermediate_prompt_was_shown_ = true;
  LocalCardMigrationManager::OnUserAcceptedIntermediateMigrationDialog();
}

void TestLocalCardMigrationManager::OnUserAcceptedMainMigrationDialog(
    const std::vector<std::string>& selected_cards) {
  main_prompt_was_shown_ = true;
  LocalCardMigrationManager::OnUserAcceptedMainMigrationDialog(selected_cards);
}

void TestLocalCardMigrationManager::OnDidGetUploadDetails(
    bool is_from_settings_page,
    AutofillClient::PaymentsRpcResult result,
    const base::string16& context_token,
    std::unique_ptr<base::DictionaryValue> legal_message) {
  if (result == AutofillClient::SUCCESS) {
    local_card_migration_was_triggered_ = true;
    LocalCardMigrationManager::OnDidGetUploadDetails(
        is_from_settings_page, result, context_token, std::move(legal_message));
  }
}

}  // namespace autofill
