// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_payments_client.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace autofill {
namespace payments {

TestPaymentsClient::TestPaymentsClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_,
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    PersonalDataManager* personal_data_manager)
    : PaymentsClient(url_loader_factory_,
                     pref_service,
                     identity_manager,
                     personal_data_manager) {}

TestPaymentsClient::~TestPaymentsClient() {}

void TestPaymentsClient::GetUploadDetails(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<const char*>& active_experiments,
    const std::string& app_locale,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const base::string16&,
                            std::unique_ptr<base::DictionaryValue>)> callback,
    const int billable_service_number,
    PaymentsClient::MigrationSource migration_source) {
  upload_details_addresses_ = addresses;
  detected_values_ = detected_values;
  active_experiments_ = active_experiments;
  migration_source_ = migration_source;
  std::move(callback).Run(app_locale == "en-US"
                              ? AutofillClient::SUCCESS
                              : AutofillClient::PERMANENT_FAILURE,
                          base::ASCIIToUTF16("this is a context token"),
                          std::unique_ptr<base::DictionaryValue>(nullptr));
}

void TestPaymentsClient::UploadCard(
    const payments::PaymentsClient::UploadRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  upload_card_addresses_ = request_details.profiles;
  active_experiments_ = request_details.active_experiments;
  std::move(callback).Run(AutofillClient::SUCCESS, server_id_);
}

void TestPaymentsClient::MigrateCards(
    const MigrationRequestDetails& details,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrateCardsCallback callback) {
  std::move(callback).Run(AutofillClient::SUCCESS, std::move(save_result_),
                          "this is display text");
}

void TestPaymentsClient::SetServerIdForCardUpload(std::string server_id) {
  server_id_ = server_id;
}

void TestPaymentsClient::SetSaveResultForCardsMigration(
    std::unique_ptr<std::unordered_map<std::string, std::string>> save_result) {
  save_result_ = std::move(save_result);
}

}  // namespace payments
}  // namespace autofill
