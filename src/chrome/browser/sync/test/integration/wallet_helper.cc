// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/wallet_helper.h"

#include <stddef.h>

#include <map>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/sync/driver/sync_driver_switches.h"

using autofill::AutofillProfile;
using autofill::AutofillTable;
using autofill::AutofillWebDataService;
using autofill::CreditCard;
using autofill::PaymentsCustomerData;
using autofill::PersonalDataManager;
using autofill::data_util::TruncateUTF8;
using sync_datatype_helper::test;

namespace {

// Constants for the credit card.
const int kDefaultCardExpMonth = 8;
const int kDefaultCardExpYear = 2087;
const char kDefaultCardLastFour[] = "1234";
const char kDefaultCardName[] = "Patrick Valenzuela";
const sync_pb::WalletMaskedCreditCard_WalletCardType kDefaultCardType =
    sync_pb::WalletMaskedCreditCard::AMEX;

// Constants for the address.
const char kDefaultAddressName[] = "John S. Doe";
const char kDefaultCompanyName[] = "The Company";
const char kDefaultStreetAddress[] = "1234 Fake Street\nApp 2";
const char kDefaultCity[] = "Cityville";
const char kDefaultState[] = "Stateful";
const char kDefaultCountry[] = "US";
const char kDefaultZip[] = "90011";
const char kDefaultPhone[] = "1.800.555.1234";
const char kDefaultSortingCode[] = "CEDEX";
const char kDefaultDependentLocality[] = "DepLoc";
const char kDefaultLanguageCode[] = "en";

template <class Item>
bool ListsMatch(int profile_a,
                const std::vector<Item*>& list_a,
                int profile_b,
                const std::vector<Item*>& list_b) {
  std::map<std::string, Item> list_a_map;
  for (Item* item : list_a) {
    list_a_map[item->server_id()] = *item;
  }

  // This seems to be a transient state that will eventually be rectified by
  // model type logic. We don't need to check b for duplicates directly because
  // after the first is erased from |autofill_profiles_a_map| the second will
  // not be found.
  if (list_a.size() != list_a_map.size()) {
    DVLOG(1) << "Profile " << profile_a << " contains duplicate server_ids.";
    return false;
  }

  for (Item* item : list_b) {
    if (!list_a_map.count(item->server_id())) {
      DVLOG(1) << "GUID " << item->server_id() << " not found in profile "
               << profile_b << ".";
      return false;
    }
    Item* expected_item = &list_a_map[item->server_id()];
    if (expected_item->Compare(*item) != 0 ||
        expected_item->use_count() != item->use_count()) {
      DVLOG(1) << "Mismatch in profile with server_id " << item->server_id()
               << ".";
      return false;
    }
    list_a_map.erase(item->server_id());
  }

  if (list_a_map.size()) {
    DVLOG(1) << "Entries present in profile " << profile_a
             << " but not in profile" << profile_b << ".";
    return false;
  }
  return true;
}

template <class Item>
void LogLists(const std::vector<Item*>& list_a,
              const std::vector<Item*>& list_b) {
  int x = 0;
  for (Item* item : list_a) {
    DVLOG(1) << "A#" << x++ << " " << *item;
  }
  x = 0;
  for (Item* item : list_b) {
    DVLOG(1) << "B#" << x++ << " " << *item;
  }
}

bool WalletDataAndMetadataMatchAndAddressesHaveConverted(
    int profile_a,
    const std::vector<CreditCard*>& server_cards_a,
    const std::vector<AutofillProfile*>& server_profiles_a,
    int profile_b,
    const std::vector<CreditCard*>& server_cards_b,
    const std::vector<AutofillProfile*>& server_profiles_b) {
  if (!ListsMatch(profile_a, server_cards_a, profile_b, server_cards_b)) {
    LogLists(server_cards_a, server_cards_b);
    return false;
  }
  if (!ListsMatch(profile_a, server_profiles_a, profile_b, server_profiles_b)) {
    LogLists(server_profiles_a, server_profiles_b);
    return false;
  }
  // Check that all server profiles have converted to local ones.
  for (AutofillProfile* profile : server_profiles_a) {
    if (!profile->has_converted()) {
      return false;
    }
  }
  return true;
}

void WaitForCurrentTasksToComplete(base::SequencedTaskRunner* task_runner) {
  base::RunLoop loop;
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&base::RunLoop::Quit, base::Unretained(&loop)));
  loop.Run();
}

void WaitForPDMToRefresh(int profile) {
  PersonalDataManager* pdm = wallet_helper::GetPersonalDataManager(profile);
  // Get the latest values from the DB: (1) post tasks on the DB sequence; (2)
  // wait for the DB sequence tasks to finish; (3) wait for the UI sequence
  // tasks to finish (that update the in-memory cache in PDM).
  pdm->Refresh();
  WaitForCurrentTasksToComplete(
      wallet_helper::GetProfileWebDataService(profile)->GetDBTaskRunner());
  base::RunLoop().RunUntilIdle();
}

void SetServerCardsOnDBSequence(AutofillWebDataService* wds,
                                const std::vector<CreditCard>& credit_cards) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetServerCreditCards(credit_cards);
}

void SetServerProfilesOnDBSequence(
    AutofillWebDataService* wds,
    const std::vector<AutofillProfile>& profiles) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetServerProfiles(profiles);
}

void SetPaymentsCustomerDataOnDBSequence(
    AutofillWebDataService* wds,
    const PaymentsCustomerData& customer_data) {
  DCHECK(wds->GetDBTaskRunner()->RunsTasksInCurrentSequence());
  AutofillTable::FromWebDatabase(wds->GetDatabase())
      ->SetPaymentsCustomerData(&customer_data);
}

}  // namespace

namespace wallet_helper {

const char kDefaultCardID[] = "wallet card ID";
const char kDefaultAddressID[] = "wallet address ID";
const char kDefaultCustomerID[] = "deadbeef";
const char kDefaultBillingAddressID[] = "billing address entity ID";

PersonalDataManager* GetPersonalDataManager(int index) {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      test()->GetProfile(index));
}

scoped_refptr<AutofillWebDataService> GetProfileWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForProfile(
      test()->GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS);
}

scoped_refptr<AutofillWebDataService> GetAccountWebDataService(int index) {
  return WebDataServiceFactory::GetAutofillWebDataForAccount(
      test()->GetProfile(index), ServiceAccessType::EXPLICIT_ACCESS);
}

void SetServerCreditCards(
    int profile,
    const std::vector<autofill::CreditCard>& credit_cards) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetServerCardsOnDBSequence,
                                base::Unretained(wds.get()), credit_cards));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void SetServerProfiles(int profile,
                       const std::vector<autofill::AutofillProfile>& profiles) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetServerProfilesOnDBSequence,
                                base::Unretained(wds.get()), profiles));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void SetPaymentsCustomerData(
    int profile,
    const autofill::PaymentsCustomerData& customer_data) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->GetDBTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&SetPaymentsCustomerDataOnDBSequence,
                                base::Unretained(wds.get()), customer_data));
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void UpdateServerCardMetadata(int profile, const CreditCard& credit_card) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->UpdateServerCardMetadata(credit_card);
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

void UpdateServerAddressMetadata(int profile,
                                 const AutofillProfile& server_address) {
  scoped_refptr<AutofillWebDataService> wds = GetProfileWebDataService(profile);
  wds->UpdateServerAddressMetadata(server_address);
  WaitForCurrentTasksToComplete(wds->GetDBTaskRunner());
}

sync_pb::SyncEntity CreateDefaultSyncWalletCard() {
  return CreateSyncWalletCard(kDefaultCardID, kDefaultCardLastFour,
                              kDefaultBillingAddressID);
}

sync_pb::SyncEntity CreateSyncWalletCard(
    const std::string& name,
    const std::string& last_four,
    const std::string& billing_address_id) {
  sync_pb::SyncEntity entity;
  entity.set_name(name);
  entity.set_id_string(name);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* credit_card =
      wallet_specifics->mutable_masked_card();
  credit_card->set_id(name);
  credit_card->set_exp_month(kDefaultCardExpMonth);
  credit_card->set_exp_year(kDefaultCardExpYear);
  credit_card->set_last_four(last_four);
  credit_card->set_name_on_card(kDefaultCardName);
  credit_card->set_status(sync_pb::WalletMaskedCreditCard::VALID);
  credit_card->set_type(kDefaultCardType);
  if (!billing_address_id.empty()) {
    credit_card->set_billing_address_id(billing_address_id);
  }
  return entity;
}

sync_pb::SyncEntity CreateSyncPaymentsCustomerData(
    const std::string& customer_id) {
  sync_pb::SyncEntity entity;
  entity.set_name(customer_id);
  entity.set_id_string(customer_id);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA);

  sync_pb::PaymentsCustomerData* customer_data =
      wallet_specifics->mutable_customer_data();
  customer_data->set_id(customer_id);
  return entity;
}

sync_pb::SyncEntity CreateDefaultSyncPaymentsCustomerData() {
  return CreateSyncPaymentsCustomerData(kDefaultCustomerID);
}

CreditCard GetDefaultCreditCard() {
  return GetCreditCard(kDefaultCardID, kDefaultCardLastFour);
}

autofill::CreditCard GetCreditCard(const std::string& name,
                                   const std::string& last_four) {
  CreditCard card(CreditCard::MASKED_SERVER_CARD, name);
  card.SetExpirationMonth(kDefaultCardExpMonth);
  card.SetExpirationYear(kDefaultCardExpYear);
  card.SetNumber(base::UTF8ToUTF16(last_four));
  card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                  base::UTF8ToUTF16(kDefaultCardName));
  card.SetServerStatus(CreditCard::OK);
  card.SetNetworkForMaskedCard(autofill::kAmericanExpressCard);
  card.set_card_type(CreditCard::CARD_TYPE_CREDIT);
  card.set_billing_address_id(kDefaultBillingAddressID);
  return card;
}

sync_pb::SyncEntity CreateDefaultSyncWalletAddress() {
  sync_pb::SyncEntity entity;
  entity.set_name(kDefaultAddressID);
  entity.set_id_string(kDefaultAddressID);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);

  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* wallet_address =
      wallet_specifics->mutable_address();
  wallet_address->set_id(kDefaultAddressID);
  wallet_address->set_recipient_name(kDefaultAddressName);
  wallet_address->set_company_name(kDefaultCompanyName);
  wallet_address->add_street_address(kDefaultStreetAddress);
  wallet_address->set_address_1(kDefaultState);
  wallet_address->set_address_2(kDefaultCity);
  wallet_address->set_address_3(kDefaultDependentLocality);
  wallet_address->set_postal_code(kDefaultZip);
  wallet_address->set_country_code(kDefaultCountry);
  wallet_address->set_phone_number(kDefaultPhone);
  wallet_address->set_sorting_code(kDefaultSortingCode);
  wallet_address->set_language_code(kDefaultLanguageCode);

  return entity;
}

sync_pb::SyncEntity CreateSyncWalletAddress(const std::string& name,
                                            const std::string& company) {
  sync_pb::SyncEntity result = CreateDefaultSyncWalletAddress();
  result.set_name(name);
  result.set_id_string(name);
  sync_pb::WalletPostalAddress* wallet_address =
      result.mutable_specifics()->mutable_autofill_wallet()->mutable_address();
  wallet_address->set_id(name);
  wallet_address->set_company_name(company);
  return result;
}

void ExpectDefaultCreditCardValues(const CreditCard& card) {
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
  EXPECT_EQ(kDefaultCardID, card.server_id());
  EXPECT_EQ(base::UTF8ToUTF16(kDefaultCardLastFour), card.LastFourDigits());
  EXPECT_EQ(autofill::kAmericanExpressCard, card.network());
  EXPECT_EQ(kDefaultCardExpMonth, card.expiration_month());
  EXPECT_EQ(kDefaultCardExpYear, card.expiration_year());
  EXPECT_EQ(base::UTF8ToUTF16(kDefaultCardName),
            card.GetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(kDefaultBillingAddressID, card.billing_address_id());
}

void ExpectDefaultProfileValues(const AutofillProfile& profile) {
  EXPECT_EQ(kDefaultLanguageCode, profile.language_code());
  EXPECT_EQ(
      kDefaultAddressName,
      TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(autofill::NAME_FULL))));
  EXPECT_EQ(kDefaultCompanyName,
            TruncateUTF8(
                base::UTF16ToUTF8(profile.GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultStreetAddress,
            TruncateUTF8(base::UTF16ToUTF8(
                profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS))));
  EXPECT_EQ(kDefaultCity, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                              autofill::ADDRESS_HOME_CITY))));
  EXPECT_EQ(kDefaultState, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                               autofill::ADDRESS_HOME_STATE))));
  EXPECT_EQ(kDefaultZip, TruncateUTF8(base::UTF16ToUTF8(
                             profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP))));
  EXPECT_EQ(kDefaultCountry, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                                 autofill::ADDRESS_HOME_COUNTRY))));
  EXPECT_EQ(kDefaultPhone, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                               autofill::PHONE_HOME_WHOLE_NUMBER))));
  EXPECT_EQ(kDefaultSortingCode,
            TruncateUTF8(base::UTF16ToUTF8(
                profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE))));
  EXPECT_EQ(kDefaultDependentLocality,
            TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                autofill::ADDRESS_HOME_DEPENDENT_LOCALITY))));
}

std::vector<AutofillProfile*> GetServerProfiles(int profile) {
  WaitForPDMToRefresh(profile);
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  return pdm->GetServerProfiles();
}

std::vector<AutofillProfile*> GetLocalProfiles(int profile) {
  WaitForPDMToRefresh(profile);
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  return pdm->GetProfiles();
}

std::vector<CreditCard*> GetServerCreditCards(int profile) {
  WaitForPDMToRefresh(profile);
  PersonalDataManager* pdm = GetPersonalDataManager(profile);
  return pdm->GetServerCreditCards();
}

}  // namespace wallet_helper

AutofillWalletChecker::AutofillWalletChecker(int profile_a, int profile_b)
    : profile_a_(profile_a), profile_b_(profile_b) {
  wallet_helper::GetPersonalDataManager(profile_a_)->AddObserver(this);
  wallet_helper::GetPersonalDataManager(profile_b_)->AddObserver(this);
}

AutofillWalletChecker::~AutofillWalletChecker() {
  wallet_helper::GetPersonalDataManager(profile_a_)->RemoveObserver(this);
  wallet_helper::GetPersonalDataManager(profile_b_)->RemoveObserver(this);
}

bool AutofillWalletChecker::Wait() {
  // We need to make sure we are not reading before any locally instigated async
  // writes. This is run exactly one time before the first
  // IsExitConditionSatisfied() is called.
  WaitForPDMToRefresh(profile_a_);
  WaitForPDMToRefresh(profile_b_);
  return StatusChangeChecker::Wait();
}

bool AutofillWalletChecker::IsExitConditionSatisfied() {
  autofill::PersonalDataManager* pdm_a =
      wallet_helper::GetPersonalDataManager(profile_a_);
  autofill::PersonalDataManager* pdm_b =
      wallet_helper::GetPersonalDataManager(profile_b_);
  return WalletDataAndMetadataMatchAndAddressesHaveConverted(
      profile_a_, pdm_a->GetServerCreditCards(), pdm_a->GetServerProfiles(),
      profile_b_, pdm_b->GetServerCreditCards(), pdm_b->GetServerProfiles());
}

std::string AutofillWalletChecker::GetDebugMessage() const {
  return "Waiting for matching autofill wallet cards and addresses";
}

void AutofillWalletChecker::OnPersonalDataChanged() {
  CheckExitCondition();
}

UssWalletSwitchToggler::UssWalletSwitchToggler() {}

void UssWalletSwitchToggler::InitWithDefaultFeatures() {
  InitWithFeatures({}, {});
}

void UssWalletSwitchToggler::InitWithFeatures(
    std::vector<base::Feature> enabled_features,
    std::vector<base::Feature> disabled_features) {
  if (GetParam()) {
    enabled_features.push_back(switches::kSyncUSSAutofillWalletData);
  } else {
    disabled_features.push_back(switches::kSyncUSSAutofillWalletData);
  }

  override_features_.InitWithFeatures(enabled_features, disabled_features);
}
