// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/personal_data_manager.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/timezone.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill-inl.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile_comparator.h"
#include "components/autofill/core/browser/country_data.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/suggestion_selection.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/history/core/browser/history_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_auth_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/version_info/version_info.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_formatter.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/source.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/storage.h"

namespace autofill {

namespace {

using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::GetStreetAddressLinesAsSingleLine;
using ::i18n::addressinput::STREET_ADDRESS;

// The length of a local profile GUID.
const int LOCAL_GUID_LENGTH = 36;

template <typename T>
class FormGroupMatchesByGUIDFunctor {
 public:
  explicit FormGroupMatchesByGUIDFunctor(const std::string& guid)
      : guid_(guid) {}

  bool operator()(const T& form_group) { return form_group.guid() == guid_; }

  bool operator()(const T* form_group) { return form_group->guid() == guid_; }

  bool operator()(const std::unique_ptr<T>& form_group) {
    return form_group->guid() == guid_;
  }

 private:
  const std::string guid_;
};

template <typename T, typename C>
typename C::const_iterator FindElementByGUID(const C& container,
                                             const std::string& guid) {
  return std::find_if(container.begin(), container.end(),
                      FormGroupMatchesByGUIDFunctor<T>(guid));
}

template <typename T, typename C>
bool FindByGUID(const C& container, const std::string& guid) {
  return FindElementByGUID<T>(container, guid) != container.end();
}

template <typename T>
class IsEmptyFunctor {
 public:
  explicit IsEmptyFunctor(const std::string& app_locale)
      : app_locale_(app_locale) {}

  bool operator()(const T& form_group) {
    return form_group.IsEmpty(app_locale_);
  }

 private:
  const std::string app_locale_;
};

bool IsSyncEnabledFor(const syncer::SyncService* sync_service,
                      syncer::ModelType model_type) {
  return sync_service != nullptr && sync_service->CanSyncFeatureStart() &&
         sync_service->GetPreferredDataTypes().Has(model_type);
}

// Receives the loaded profiles from the web data service and stores them in
// |*dest|. The pending handle is the address of the pending handle
// corresponding to this request type. This function is used to save both
// server and local profiles and credit cards.
template <typename ValueType>
void ReceiveLoadedDbValues(WebDataServiceBase::Handle h,
                           WDTypedResult* result,
                           WebDataServiceBase::Handle* pending_handle,
                           std::vector<std::unique_ptr<ValueType>>* dest) {
  DCHECK_EQ(*pending_handle, h);
  *pending_handle = 0;

  *dest = std::move(
      static_cast<WDResult<std::vector<std::unique_ptr<ValueType>>>*>(result)
          ->GetValue());
}

// A helper function for finding the maximum value in a string->int map.
static bool CompareVotes(const std::pair<std::string, int>& a,
                         const std::pair<std::string, int>& b) {
  return a.second < b.second;
}
}  // namespace

// Helper class to abstract the switching between account and profile storage
// for server cards away from the rest of PersonalDataManager.
class PersonalDatabaseHelper
    : public AutofillWebDataServiceObserverOnUISequence {
 public:
  explicit PersonalDatabaseHelper(PersonalDataManager* personal_data_manager)
      : personal_data_manager_(personal_data_manager) {}

  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            scoped_refptr<AutofillWebDataService> account_database) {
    profile_database_ = profile_database;
    account_database_ = account_database;

    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }

    // Start observing the profile database. Don't observe the account database
    // until we know that we should use it.
    profile_database_->AddObserver(personal_data_manager_);

    // If we don't have an account_database , we always use the profile database
    // for server data.
    if (!account_database_) {
      server_database_ = profile_database_;
    } else {
      // Wait for the call to SetUseAccountStorageForServerData to decide
      // which database to use for server data.
      server_database_ = nullptr;
    }
  }

  ~PersonalDatabaseHelper() override {
    if (profile_database_) {
      profile_database_->RemoveObserver(personal_data_manager_);
    }

    // If we have a different server database, also remove its observer.
    if (server_database_ && server_database_ != profile_database_) {
      server_database_->RemoveObserver(personal_data_manager_);
    }
  }

  // Returns the database that should be used for storing local data.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase() {
    return profile_database_;
  }

  // Returns the database that should be used for storing server data.
  scoped_refptr<AutofillWebDataService> GetServerDatabase() {
    return server_database_;
  }

  // Whether we're currently using the ephemeral account storage for saving
  // server data.
  bool IsUsingAccountStorageForServerData() {
    return server_database_ != profile_database_;
  }

  // Set whether this should use the passed in account storage for server
  // addresses. If false, this will use the profile_storage.
  // It's an error to call this if no account storage was passed in at
  // construction time.
  void SetUseAccountStorageForServerData(
      bool use_account_storage_for_server_cards) {
    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }
    scoped_refptr<AutofillWebDataService> new_server_database =
        use_account_storage_for_server_cards ? account_database_
                                             : profile_database_;
    DCHECK(new_server_database != nullptr)
        << "SetUseAccountStorageForServerData("
        << use_account_storage_for_server_cards << "): storage not available.";

    if (new_server_database == server_database_) {
      // Nothing to do :)
      return;
    }

    if (server_database_ != nullptr) {
      if (server_database_ != profile_database_) {
        // Remove the previous observer if we had any.
        server_database_->RemoveObserver(personal_data_manager_);
      }
      personal_data_manager_->CancelPendingServerQueries();
    }
    server_database_ = new_server_database;
    // We don't need to add an observer if server_database_ is equal to
    // profile_database_, because we're already observing that.
    if (server_database_ != profile_database_) {
      server_database_->AddObserver(personal_data_manager_);
    }
    // Notify the manager that the database changed.
    personal_data_manager_->Refresh();
  }

 private:
  scoped_refptr<AutofillWebDataService> profile_database_;
  scoped_refptr<AutofillWebDataService> account_database_;

  // The database that should be used for server data. This will always be equal
  // to either profile_database_, or account_database_.
  scoped_refptr<AutofillWebDataService> server_database_;

  PersonalDataManager* personal_data_manager_;

  DISALLOW_COPY_AND_ASSIGN(PersonalDatabaseHelper);
};

PersonalDataManager::PersonalDataManager(const std::string& app_locale)
    : app_locale_(app_locale),
      test_data_creator_(kDisusedDataModelDeletionTimeDelta, app_locale_) {
  database_helper_ = std::make_unique<PersonalDatabaseHelper>(this);
}

void PersonalDataManager::Init(
    scoped_refptr<AutofillWebDataService> profile_database,
    scoped_refptr<AutofillWebDataService> account_database,
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    AutofillProfileValidator* client_profile_validator,
    history::HistoryService* history_service,
    bool is_off_the_record) {
  CountryNames::SetLocaleString(app_locale_);
  database_helper_->Init(profile_database, account_database);

  SetPrefService(pref_service);

  // Listen for the preference changes.
  pref_registrar_.Init(pref_service);
  pref_registrar_.Add(
      prefs::kAutofillProfileValidity,
      base::BindRepeating(&PersonalDataManager::ResetProfileValidity,
                          base::Unretained(this)));

  // Listen for URL deletions from browsing history.
  history_service_ = history_service;
  if (history_service_)
    history_service_->AddObserver(this);

  // Listen for account cookie deletion by the user.
  identity_manager_ = identity_manager;
  if (identity_manager_)
    identity_manager_->AddObserver(this);

  is_off_the_record_ = is_off_the_record;

  if (!is_off_the_record_)
    AutofillMetrics::LogIsAutofillEnabledAtStartup(IsAutofillEnabled());

  client_profile_validator_ = client_profile_validator;

  // WebDataService may not be available in tests.
  if (!database_helper_->GetLocalDatabase()) {
    return;
  }

  database_helper_->GetLocalDatabase()->SetAutofillProfileChangedCallback(
      base::BindRepeating(&PersonalDataManager::OnAutofillProfileChanged,
                          weak_factory_.GetWeakPtr()));

  LoadProfiles();
  LoadCreditCards();
  LoadPaymentsCustomerData();

  // Check if profile cleanup has already been performed this major version.
  is_autofill_profile_cleanup_pending_ =
      pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) >=
      CHROME_VERSION_MAJOR;
  DVLOG(1) << "Autofill profile cleanup "
           << (is_autofill_profile_cleanup_pending_ ? "needs to be"
                                                    : "has already been")
           << " performed for this version";
}

PersonalDataManager::~PersonalDataManager() {
  CancelPendingLocalQuery(&pending_profiles_query_);
  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQuery(&pending_server_profiles_query_);
  CancelPendingServerQuery(&pending_server_creditcards_query_);
  CancelPendingServerQuery(&pending_customer_data_query_);
}

void PersonalDataManager::Shutdown() {
  if (sync_service_)
    sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;

  if (history_service_)
    history_service_->RemoveObserver(this);
  history_service_ = nullptr;

  if (identity_manager_)
    identity_manager_->RemoveObserver(this);
  identity_manager_ = nullptr;
}

void PersonalDataManager::OnSyncServiceInitialized(
    syncer::SyncService* sync_service) {
  if (sync_service_ != sync_service) {
    // Before the sync service pointer gets changed, remove the observer.
    if (sync_service_)
      sync_service_->RemoveObserver(this);

    sync_service_ = sync_service;

    UMA_HISTOGRAM_BOOLEAN(
        "Autofill.ResetFullServerCards.SyncServiceNullOnInitialized",
        !sync_service_);
    if (!sync_service_) {
      ResetFullServerCards();
      return;
    }

    sync_service_->AddObserver(this);
    // Re-mask all server cards if the upload state is not active.
    bool is_upload_not_active =
        syncer::GetUploadToGoogleState(
            sync_service_, syncer::ModelType::AUTOFILL_WALLET_DATA) ==
        syncer::UploadState::NOT_ACTIVE;
    UMA_HISTOGRAM_BOOLEAN(
        "Autofill.ResetFullServerCards.SyncServiceNotActiveOnInitialized",
        is_upload_not_active);
    if (is_upload_not_active) {
      ResetFullServerCards();
    }
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillEnableAccountWalletStorage)) {
      // Use the ephemeral account storage when the user didn't enable the sync
      // feature explicitly.
      database_helper_->SetUseAccountStorageForServerData(
          !sync_service->IsSyncFeatureEnabled());
    }
  }
}

void PersonalDataManager::OnURLsDeleted(
    history::HistoryService* /* history_service */,
    const history::DeletionInfo& deletion_info) {
  if (!deletion_info.is_from_expiration() && deletion_info.IsAllHistory()) {
    AutofillDownloadManager::ClearUploadHistory(pref_service_);
  }
}

void PersonalDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(pending_profiles_query_ || pending_server_profiles_query_ ||
         pending_creditcards_query_ || pending_server_creditcards_query_ ||
         pending_customer_data_query_);

  if (!result) {
    // Error from the web database.
    if (h == pending_creditcards_query_)
      pending_creditcards_query_ = 0;
    else if (h == pending_profiles_query_)
      pending_profiles_query_ = 0;
    else if (h == pending_server_creditcards_query_)
      pending_server_creditcards_query_ = 0;
    else if (h == pending_server_profiles_query_)
      pending_server_profiles_query_ = 0;
    else if (h == pending_customer_data_query_)
      pending_customer_data_query_ = 0;
  } else {
    switch (result->GetType()) {
      case AUTOFILL_PROFILES_RESULT:
        if (h == pending_profiles_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_profiles_query_,
                                &web_profiles_);
        } else {
          DCHECK_EQ(h, pending_server_profiles_query_)
              << "received profiles from invalid request.";
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_server_profiles_query_,
                                &server_profiles_);
        }
        break;
      case AUTOFILL_CREDITCARDS_RESULT:
        if (h == pending_creditcards_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_creditcards_query_,
                                &local_credit_cards_);
        } else {
          DCHECK_EQ(h, pending_server_creditcards_query_)
              << "received creditcards from invalid request.";
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_server_creditcards_query_,
                                &server_credit_cards_);

          // If the user has a saved unmasked server card and the experiment is
          // disabled, force mask all cards back to the unsaved state.
          if (!OfferStoreUnmaskedCards(is_off_the_record_))
            ResetFullServerCards();
        }
        break;
      case AUTOFILL_CUSTOMERDATA_RESULT:
        DCHECK_EQ(h, pending_customer_data_query_)
            << "received customer data from invalid request.";
        pending_customer_data_query_ = 0;

        payments_customer_data_ =
            static_cast<WDResult<std::unique_ptr<PaymentsCustomerData>>*>(
                result.get())
                ->GetValue();
        break;
      default:
        NOTREACHED();
    }
  }

  // If all requests have responded, then all personal data is loaded.
  // We need to check if the server database is set here, because we won't have
  // the server data yet if we don't have the database.
  if (pending_profiles_query_ == 0 && pending_creditcards_query_ == 0 &&
      pending_server_profiles_query_ == 0 &&
      pending_server_creditcards_query_ == 0 &&
      pending_customer_data_query_ == 0 &&
      database_helper_->GetServerDatabase()) {
    // On initial data load, is_data_loaded_ will be false here.
    if (!is_data_loaded_) {
      // If sync is enabled for addresses, defer running cleanups until address
      // sync has started; otherwise, do it now.
      if (!IsSyncEnabledFor(sync_service_, syncer::AUTOFILL_PROFILE))
        ApplyAddressFixesAndCleanups();

      // If sync is enabled for credit cards, defer running cleanups until card
      // sync has started; otherwise, do it now.
      if (!IsSyncEnabledFor(sync_service_, syncer::AUTOFILL_WALLET_DATA))
        ApplyCardFixesAndCleanups();

      // Log address and credit card startup metrics.
      LogStoredProfileMetrics();
      LogStoredCreditCardMetrics();
    }

    is_data_loaded_ = true;
    NotifyPersonalDataChanged();
  }
}

void PersonalDataManager::AutofillMultipleChanged() {
  has_synced_new_data_ = true;
  Refresh();
}

void PersonalDataManager::SyncStarted(syncer::ModelType model_type) {
  // Run deferred autofill address profile startup code.
  // See: OnSyncServiceInitialized
  if (model_type == syncer::AUTOFILL_PROFILE)
    ApplyAddressFixesAndCleanups();

  // Run deferred credit card startup code.
  // See: OnSyncServiceInitialized
  if (model_type == syncer::AUTOFILL_WALLET_DATA)
    ApplyCardFixesAndCleanups();
}

void PersonalDataManager::OnStateChanged(syncer::SyncService* sync_service) {
  // TODO(mastiz,jkrcal): Once AUTOFILL_WALLET is migrated to USS, it shouldn't
  // be necessary anymore to implement SyncServiceObserver; instead the
  // notification should flow through the payments sync bridge.
  DCHECK_EQ(sync_service_, sync_service);
  syncer::UploadState upload_state = syncer::GetUploadToGoogleState(
      sync_service_, syncer::ModelType::AUTOFILL_WALLET_DATA);
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.ResetFullServerCards.SyncServiceStatusOnStateChanged",
      upload_state);
  if (upload_state == syncer::UploadState::NOT_ACTIVE) {
    ResetFullServerCards();
  }
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableAccountWalletStorage)) {
    // Use the ephemeral account storage when the user didn't enable the sync
    // feature explicitly.
    database_helper_->SetUseAccountStorageForServerData(
        !sync_service->IsSyncFeatureEnabled());
  }
}

void PersonalDataManager::OnSyncShutdown(syncer::SyncService* sync_service) {
  DCHECK_EQ(sync_service_, sync_service);
  sync_service_->RemoveObserver(this);
  sync_service_ = nullptr;
}

CoreAccountInfo PersonalDataManager::GetAccountInfoForPaymentsServer() const {
  // If butter is enabled or the feature to get the Payment Identity from Sync
  // is enabled, return the account of the active signed-in user irrespective of
  // whether they enabled sync or not.
  // Otherwise, return the latest cached AccountInfo of the user's primary
  // account, which is empty if the user has disabled sync.
  // In both cases, the AccountInfo will be empty if the user is not signed in.
  return ShouldUseActiveSignedInAccount() && sync_service_
             ? sync_service_->GetAuthenticatedAccountInfo()
             : identity_manager_->GetPrimaryAccountInfo();
}

// TODO(crbug.com/903914): Clean up this function so that it's more clear what
// it's checking. It should not check the database helper.
bool PersonalDataManager::IsSyncFeatureEnabled() const {
  if (!sync_service_)
    return false;

  return !sync_service_->GetAuthenticatedAccountInfo().IsEmpty() &&
         !database_helper_->IsUsingAccountStorageForServerData();
}

void PersonalDataManager::OnAccountsCookieDeletedByUserAction() {
  // Clear all the Sync Transport feature opt-ins.
  ::autofill::prefs::ClearSyncTransportOptIns(pref_service_);
}

// TODO(crbug.com/903896): Generalize this to all the possible states relavant
// to Autofill.
AutofillSyncSigninState PersonalDataManager::GetSyncSigninState() const {
  // Check if the user is signed out.
  if (!sync_service_ || !identity_manager_ ||
      syncer::DetermineAccountToUse(identity_manager_,
                                    /*allow_secondary_accounts=*/true)
          .account_info.IsEmpty()) {
    return AutofillSyncSigninState::kSignedOut;
  }

  // Check if the user has turned on sync.
  if (sync_service_->IsSyncFeatureEnabled()) {
    return AutofillSyncSigninState::kSignedInAndSyncFeature;
  }

  // Check if the feature is enabled and if Wallet data types are supported.
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableAccountWalletStorage) &&
      sync_service_->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA)) {
    return AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled;
  }

  return AutofillSyncSigninState::kSignedIn;
}

void PersonalDataManager::AddObserver(PersonalDataManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void PersonalDataManager::RemoveObserver(
    PersonalDataManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void PersonalDataManager::MarkObserversInsufficientFormDataForImport() {
  for (PersonalDataManagerObserver& observer : observers_)
    observer.OnInsufficientFormData();
}

void PersonalDataManager::RecordUseOf(const AutofillDataModel& data_model) {
  if (is_off_the_record_)
    return;

  CreditCard* credit_card = GetCreditCardByGUID(data_model.guid());
  if (credit_card) {
    credit_card->RecordAndLogUse();

    if (credit_card->record_type() == CreditCard::LOCAL_CARD) {
      // Fail silently if there's no local database, because we need to support
      // this for tests.
      if (database_helper_->GetLocalDatabase()) {
        database_helper_->GetLocalDatabase()->UpdateCreditCard(*credit_card);
      }
    } else {
      DCHECK(database_helper_->GetServerDatabase())
          << "Recording use of server card without server storage.";
      database_helper_->GetServerDatabase()->UpdateServerCardMetadata(
          *credit_card);
    }

    Refresh();
    return;
  }

  AutofillProfile* profile = GetProfileByGUID(data_model.guid());
  if (profile) {
    profile->RecordAndLogUse();

    switch (profile->record_type()) {
      case AutofillProfile::LOCAL_PROFILE:
        UpdateProfileInDB(*profile, /*enforced=*/true);
        break;
      case AutofillProfile::SERVER_PROFILE:
        DCHECK(database_helper_->GetServerDatabase())
            << "Recording use of server address without server storage.";
        database_helper_->GetServerDatabase()->UpdateServerAddressMetadata(
            *profile);
        Refresh();
        break;
    }
  }
}

void PersonalDataManager::AddProfile(const AutofillProfile& profile) {
  if (!IsAutofillProfileEnabled())
    return;

  if (is_off_the_record_)
    return;

  if (!database_helper_->GetLocalDatabase())
    return;

  AddProfileToDB(profile);
}

void PersonalDataManager::UpdateProfile(const AutofillProfile& profile) {
  if (is_off_the_record_)
    return;

  if (profile.IsEmpty(app_locale_)) {
    RemoveByGUID(profile.guid());
    return;
  }

  if (!database_helper_->GetLocalDatabase())
    return;

  UpdateProfileInDB(profile);
}

AutofillProfile* PersonalDataManager::GetProfileByGUID(
    const std::string& guid) {
  return GetProfileFromProfilesByGUID(guid, GetProfiles());
}

// static
AutofillProfile* PersonalDataManager::GetProfileFromProfilesByGUID(
    const std::string& guid,
    const std::vector<AutofillProfile*>& profiles) {
  auto iter = FindElementByGUID<AutofillProfile>(profiles, guid);
  return iter != profiles.end() ? *iter : nullptr;
}

void PersonalDataManager::AddCreditCard(const CreditCard& credit_card) {
  if (!IsAutofillCreditCardEnabled())
    return;

  if (is_off_the_record_)
    return;

  if (credit_card.IsEmpty(app_locale_))
    return;

  if (FindByGUID<CreditCard>(local_credit_cards_, credit_card.guid()))
    return;

  if (!database_helper_->GetLocalDatabase())
    return;

  // Don't add a duplicate.
  if (FindByContents(local_credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  database_helper_->GetLocalDatabase()->AddCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::DeleteLocalCreditCards(
    const std::vector<CreditCard>& cards) {
  DCHECK(database_helper_);
  DCHECK(database_helper_->GetLocalDatabase())
      << "Use of local card without local storage.";

  for (const auto& card : cards)
    database_helper_->GetLocalDatabase()->RemoveCreditCard(card.guid());

  // Refresh the database, so latest state is reflected in all consumers.
  if (!cards.empty())
    Refresh();
}

void PersonalDataManager::UpdateCreditCard(const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::LOCAL_CARD, credit_card.record_type());
  if (is_off_the_record_)
    return;

  CreditCard* existing_credit_card = GetCreditCardByGUID(credit_card.guid());
  if (!existing_credit_card)
    return;

  // Don't overwrite the origin for a credit card that is already stored.
  if (existing_credit_card->Compare(credit_card) == 0)
    return;

  if (credit_card.IsEmpty(app_locale_)) {
    RemoveByGUID(credit_card.guid());
    return;
  }

  // Update the cached version.
  *existing_credit_card = credit_card;

  if (!database_helper_->GetLocalDatabase())
    return;

  // Make the update.
  database_helper_->GetLocalDatabase()->UpdateCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::AddFullServerCreditCard(
    const CreditCard& credit_card) {
  DCHECK_EQ(CreditCard::FULL_SERVER_CARD, credit_card.record_type());
  DCHECK(!credit_card.IsEmpty(app_locale_));
  DCHECK(!credit_card.server_id().empty());

  if (is_off_the_record_)
    return;

  DCHECK(database_helper_->GetServerDatabase())
      << "Adding server card without server storage.";

  // Don't add a duplicate.
  if (FindByGUID<CreditCard>(server_credit_cards_, credit_card.guid()) ||
      FindByContents(server_credit_cards_, credit_card))
    return;

  // Add the new credit card to the web database.
  database_helper_->GetServerDatabase()->AddFullServerCreditCard(credit_card);

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::UpdateServerCreditCard(
    const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::LOCAL_CARD, credit_card.record_type());

  if (is_off_the_record_ || !database_helper_->GetServerDatabase())
    return;

  // Look up by server id, not GUID.
  const CreditCard* existing_credit_card = nullptr;
  for (const auto& server_card : server_credit_cards_) {
    if (credit_card.server_id() == server_card->server_id()) {
      existing_credit_card = server_card.get();
      break;
    }
  }
  if (!existing_credit_card)
    return;

  DCHECK_NE(existing_credit_card->record_type(), credit_card.record_type());
  DCHECK_EQ(existing_credit_card->Label(), credit_card.Label());
  if (existing_credit_card->record_type() == CreditCard::MASKED_SERVER_CARD) {
    database_helper_->GetServerDatabase()->UnmaskServerCreditCard(
        credit_card, credit_card.number());
  } else {
    database_helper_->GetServerDatabase()->MaskServerCreditCard(
        credit_card.server_id());
  }

  Refresh();
}

void PersonalDataManager::UpdateServerCardMetadata(
    const CreditCard& credit_card) {
  DCHECK_NE(CreditCard::LOCAL_CARD, credit_card.record_type());

  if (is_off_the_record_)
    return;

  DCHECK(database_helper_->GetServerDatabase())
      << "Updating server card metadata without server storage.";

  database_helper_->GetServerDatabase()->UpdateServerCardMetadata(credit_card);

  Refresh();
}

void PersonalDataManager::ResetFullServerCard(const std::string& guid) {
  for (const auto& card : server_credit_cards_) {
    if (card->guid() == guid) {
      DCHECK_EQ(card->record_type(), CreditCard::FULL_SERVER_CARD);
      CreditCard card_copy = *card;
      card_copy.set_record_type(CreditCard::MASKED_SERVER_CARD);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
      break;
    }
  }
}

void PersonalDataManager::ResetFullServerCards() {
  size_t nb_cards_reset = 0;
  for (const auto& card : server_credit_cards_) {
    if (card->record_type() == CreditCard::FULL_SERVER_CARD) {
      ++nb_cards_reset;
      CreditCard card_copy = *card;
      card_copy.set_record_type(CreditCard::MASKED_SERVER_CARD);
      card_copy.SetNumber(card->LastFourDigits());
      UpdateServerCreditCard(card_copy);
    }
  }
  UMA_HISTOGRAM_COUNTS_100("Autofill.ResetFullServerCards.NumberOfCardsReset",
                           nb_cards_reset);
}

void PersonalDataManager::ClearAllServerData() {
  // This could theoretically be called before we get the data back from the
  // database on startup, and it could get called when the wallet pref is
  // off (meaning this class won't even query for the server data) so don't
  // check the server_credit_cards_/profiles_ before posting to the DB.

  // TODO(crbug.com/864519): Move this nullcheck logic to the database helper.
  // The server database can be null for a limited amount of time before the
  // sync service gets initialized. Not clearing it does not matter in that case
  // since it will not have been created yet (nothing to clear).
  if (database_helper_->GetServerDatabase())
    database_helper_->GetServerDatabase()->ClearAllServerData();

  // The above call will eventually clear our server data by notifying us
  // that the data changed and then this class will re-fetch. Preemptively
  // clear so that tests can synchronously verify that this data was cleared.
  server_credit_cards_.clear();
  server_profiles_.clear();
  payments_customer_data_.reset();
}

void PersonalDataManager::ClearAllLocalData() {
  database_helper_->GetLocalDatabase()->ClearAllLocalData();
  local_credit_cards_.clear();
  web_profiles_.clear();
}

void PersonalDataManager::AddServerCreditCardForTest(
    std::unique_ptr<CreditCard> credit_card) {
  server_credit_cards_.push_back(std::move(credit_card));
}

bool PersonalDataManager::IsUsingAccountStorageForServerDataForTest() const {
  return database_helper_->IsUsingAccountStorageForServerData();
}

void PersonalDataManager::SetSyncServiceForTest(
    syncer::SyncService* sync_service) {
  if (sync_service_)
    sync_service_->RemoveObserver(this);

  sync_service_ = sync_service;

  if (sync_service_)
    sync_service_->AddObserver(this);
}

void PersonalDataManager::
    RemoveAutofillProfileByGUIDAndBlankCreditCardReference(
        const std::string& guid) {
  RemoveProfileFromDB(guid);

  // Reset the billing_address_id of any card that refered to this profile.
  for (CreditCard* credit_card : GetCreditCards()) {
    if (credit_card->billing_address_id() == guid) {
      credit_card->set_billing_address_id("");

      if (credit_card->record_type() == CreditCard::LOCAL_CARD) {
        database_helper_->GetLocalDatabase()->UpdateCreditCard(*credit_card);
      } else {
        DCHECK(database_helper_->GetServerDatabase())
            << "Updating metadata on null server db.";
        database_helper_->GetServerDatabase()->UpdateServerCardMetadata(
            *credit_card);
      }
    }
  }
}

void PersonalDataManager::RemoveByGUID(const std::string& guid) {
  if (is_off_the_record_)
    return;

  if (!database_helper_->GetLocalDatabase())
    return;

  bool is_credit_card = FindByGUID<CreditCard>(local_credit_cards_, guid);
  if (is_credit_card) {
    database_helper_->GetLocalDatabase()->RemoveCreditCard(guid);
    // Refresh our local cache and send notifications to observers.
    Refresh();
  } else {
    RemoveAutofillProfileByGUIDAndBlankCreditCardReference(guid);
  }
}

CreditCard* PersonalDataManager::GetCreditCardByGUID(const std::string& guid) {
  const std::vector<CreditCard*>& credit_cards = GetCreditCards();
  auto iter = FindElementByGUID<CreditCard>(credit_cards, guid);
  return iter != credit_cards.end() ? *iter : nullptr;
}

CreditCard* PersonalDataManager::GetCreditCardByNumber(
    const std::string& number) {
  CreditCard numbered_card;
  numbered_card.SetNumber(base::ASCIIToUTF16(number));
  for (CreditCard* credit_card : GetCreditCards()) {
    DCHECK(credit_card);
    if (credit_card->HasSameNumberAs(numbered_card))
      return credit_card;
  }
  return nullptr;
}

void PersonalDataManager::GetNonEmptyTypes(
    ServerFieldTypeSet* non_empty_types) const {
  for (AutofillProfile* profile : GetProfiles())
    profile->GetNonEmptyTypes(app_locale_, non_empty_types);
  for (CreditCard* card : GetCreditCards())
    card->GetNonEmptyTypes(app_locale_, non_empty_types);
}

bool PersonalDataManager::IsDataLoaded() const {
  return is_data_loaded_;
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfiles() const {
  std::vector<AutofillProfile*> result;
  result.reserve(web_profiles_.size());
  for (const auto& profile : web_profiles_)
    result.push_back(profile.get());
  return result;
}

void PersonalDataManager::UpdateProfilesServerValidityMapsIfNeeded(
    const std::vector<AutofillProfile*>& profiles) {
  if (!profiles_server_validities_need_update_)
    return;
  profiles_server_validities_need_update_ = false;
  for (auto* profile : profiles) {
    profile->UpdateServerValidityMap(GetProfileValidityByGUID(profile->guid()));
  }
}

void PersonalDataManager::UpdateClientValidityStates(
    const std::vector<AutofillProfile*>& profiles) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillProfileClientValidation))
    return;

  if (!client_profile_validator_)
    return;

  // The profiles' validity states need to be updated for each major version, to
  // keep up with the validation logic.
  bool update_validation =
      pref_service_->GetInteger(prefs::kAutofillLastVersionValidated) <
      CHROME_VERSION_MAJOR;

  DVLOG(1) << "Autofill profile client validation "
           << (update_validation ? "needs to be" : "has already been")
           << " performed for this version";

  for (const auto* profile : profiles) {
    if (!profile->is_client_validity_states_updated() || update_validation) {
      profile->set_is_client_validity_states_updated(false);
      ongoing_profile_changes_[profile->guid()].push_back(
          AutofillProfileDeepChange(AutofillProfileChange::UPDATE, *profile));
      ongoing_profile_changes_[profile->guid()].back().set_enforce_update();
      client_profile_validator_->StartProfileValidation(
          profile, base::BindOnce(&PersonalDataManager::OnValidated,
                                  weak_factory_.GetWeakPtr()));
    }
  }

  // Set the pref to the current major version if already not set.
  if (update_validation)
    pref_service_->SetInteger(prefs::kAutofillLastVersionValidated,
                              CHROME_VERSION_MAJOR);
}

bool PersonalDataManager::UpdateClientValidityStates(
    const AutofillProfile& profile) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillProfileClientValidation) ||
      !client_profile_validator_ ||
      profile.is_client_validity_states_updated()) {
    OnValidated(&profile);
    return false;
  }

  client_profile_validator_->StartProfileValidation(
      &profile, base::BindOnce(&PersonalDataManager::OnValidated,
                               weak_factory_.GetWeakPtr()));
  return true;
}

std::vector<AutofillProfile*> PersonalDataManager::GetServerProfiles() const {
  std::vector<AutofillProfile*> result;
  if (!IsAutofillProfileEnabled())
    return result;
  result.reserve(server_profiles_.size());
  for (const auto& profile : server_profiles_)
    result.push_back(profile.get());
  return result;
}

std::vector<CreditCard*> PersonalDataManager::GetLocalCreditCards() const {
  std::vector<CreditCard*> result;
  result.reserve(local_credit_cards_.size());
  for (const auto& card : local_credit_cards_)
    result.push_back(card.get());
  return result;
}

std::vector<CreditCard*> PersonalDataManager::GetServerCreditCards() const {
  std::vector<CreditCard*> result;
  if (!IsAutofillWalletImportEnabled())
    return result;

  result.reserve(server_credit_cards_.size());
  for (const auto& card : server_credit_cards_)
    result.push_back(card.get());
  return result;
}

std::vector<CreditCard*> PersonalDataManager::GetCreditCards() const {
  std::vector<CreditCard*> result;

  result.reserve(local_credit_cards_.size() + server_credit_cards_.size());
  for (const auto& card : local_credit_cards_)
    result.push_back(card.get());
  if (IsAutofillWalletImportEnabled()) {
    for (const auto& card : server_credit_cards_)
      result.push_back(card.get());
  }
  return result;
}

PaymentsCustomerData* PersonalDataManager::GetPaymentsCustomerData() const {
  return payments_customer_data_ ? payments_customer_data_.get() : nullptr;
}

void PersonalDataManager::Refresh() {
  LoadProfiles();
  LoadCreditCards();
  LoadPaymentsCustomerData();
}

std::vector<AutofillProfile*> PersonalDataManager::GetProfilesToSuggest()
    const {
  if (!IsAutofillProfileEnabled())
    return std::vector<AutofillProfile*>{};

  std::vector<AutofillProfile*> profiles = GetProfiles();

  bool use_server_validation = base::FeatureList::IsEnabled(
      autofill::features::kAutofillProfileServerValidation);
  bool use_client_validation = base::FeatureList::IsEnabled(
      autofill::features::kAutofillProfileClientValidation);

  // Rank the suggestions by frescocency (see AutofillDataModel for details).
  // Frescocency is frecency + validity score.
  const base::Time comparison_time = AutofillClock::Now();
  std::sort(profiles.begin(), profiles.end(),
            [comparison_time, use_client_validation, use_server_validation](
                const AutofillProfile* a, const AutofillProfile* b) {
              return a->HasGreaterFrescocencyThan(b, comparison_time,
                                                  use_client_validation,
                                                  use_server_validation);
            });

  return profiles;
}

std::vector<Suggestion> PersonalDataManager::GetProfileSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    bool field_is_autofilled,
    const std::vector<ServerFieldType>& field_types) {
  if (IsInAutofillSuggestionsDisabledExperiment())
    return std::vector<Suggestion>();

  AutofillProfileComparator comparator(app_locale_);
  base::string16 field_contents_canon =
      comparator.NormalizeForComparison(field_contents);

  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillProfileServerValidation)) {
    UpdateProfilesServerValidityMapsIfNeeded(GetProfiles());
  }

  // Get the profiles to suggest, which are already sorted.
  std::vector<AutofillProfile*> sorted_profiles = GetProfilesToSuggest();

  // When suggesting with no prefix to match, consider suppressing disused
  // address suggestions as well as those based on invalid profile data.
  if (field_contents_canon.empty()) {
    if (base::FeatureList::IsEnabled(
            features::kAutofillSuppressDisusedAddresses)) {
      const base::Time min_last_used =
          AutofillClock::Now() - kDisusedDataModelTimeDelta;
      suggestion_selection::RemoveProfilesNotUsedSinceTimestamp(
          min_last_used, &sorted_profiles);
    }
  }

  std::vector<AutofillProfile*> matched_profiles;
  std::vector<Suggestion> suggestions =
      suggestion_selection::GetPrefixMatchedSuggestions(
          type, field_contents_canon, comparator, sorted_profiles,
          &matched_profiles);

  // Don't show two suggestions if one is a subset of the other.
  std::vector<AutofillProfile*> unique_matched_profiles;
  std::vector<Suggestion> unique_suggestions =
      suggestion_selection::GetUniqueSuggestions(field_types, app_locale_,
                                                 matched_profiles, suggestions,
                                                 &unique_matched_profiles);

  // Generate disambiguating labels based on the list of matches.
  std::vector<base::string16> labels;
  AutofillProfile::CreateInferredLabels(unique_matched_profiles, &field_types,
                                        type.GetStorableType(), 1, app_locale_,
                                        &labels);
  DCHECK_EQ(unique_suggestions.size(), labels.size());
  for (size_t i = 0; i < labels.size(); i++) {
    // A suggestion's label has one line of disambiguating information to show
    // to the user. However, when the two-line suggestion display experiment is
    // enabled on desktop, label is replaced by additional label.
    unique_suggestions[i].label = labels[i];
    unique_suggestions[i].additional_label = labels[i];
  }

  return unique_suggestions;
}

// TODO(crbug.com/613187): Investigate if it would be more efficient to dedupe
// with a vector instead of a list.
const std::vector<CreditCard*> PersonalDataManager::GetCreditCardsToSuggest(
    bool include_server_cards) const {
  if (!IsAutofillCreditCardEnabled())
    return std::vector<CreditCard*>{};

  std::vector<CreditCard*> credit_cards;
  if (include_server_cards && ShouldSuggestServerCards()) {
    credit_cards = GetCreditCards();
  } else {
    credit_cards = GetLocalCreditCards();
  }

  std::list<CreditCard*> cards_to_dedupe(credit_cards.begin(),
                                         credit_cards.end());

  DedupeCreditCardToSuggest(&cards_to_dedupe);

  std::vector<CreditCard*> cards_to_suggest(
      std::make_move_iterator(std::begin(cards_to_dedupe)),
      std::make_move_iterator(std::end(cards_to_dedupe)));

  // Rank the cards by frecency (see AutofillDataModel for details). All expired
  // cards should be suggested last, also by frecency.
  base::Time comparison_time = AutofillClock::Now();
  std::stable_sort(cards_to_suggest.begin(), cards_to_suggest.end(),
                   [comparison_time](const CreditCard* a, const CreditCard* b) {
                     bool a_is_expired = a->IsExpired(comparison_time);
                     if (a_is_expired != b->IsExpired(comparison_time))
                       return !a_is_expired;

                     return a->HasGreaterFrecencyThan(b, comparison_time);
                   });

  return cards_to_suggest;
}

// static
void PersonalDataManager::RemoveExpiredCreditCardsNotUsedSinceTimestamp(
    base::Time comparison_time,
    base::Time min_last_used,
    std::vector<CreditCard*>* cards) {
  const size_t original_size = cards->size();
  // Split the vector into [unexpired-or-expired-but-after-timestamp,
  // expired-and-before-timestamp], then delete the latter.
  cards->erase(std::stable_partition(
                   cards->begin(), cards->end(),
                   [comparison_time, min_last_used](const CreditCard* c) {
                     return !c->IsExpired(comparison_time) ||
                            c->use_date() > min_last_used;
                   }),
               cards->end());
  const size_t num_cards_supressed = original_size - cards->size();
  AutofillMetrics::LogNumberOfCreditCardsSuppressedForDisuse(
      num_cards_supressed);
}

std::vector<Suggestion> PersonalDataManager::GetCreditCardSuggestions(
    const AutofillType& type,
    const base::string16& field_contents,
    bool include_server_cards) {
  if (IsInAutofillSuggestionsDisabledExperiment())
    return std::vector<Suggestion>();
  std::vector<CreditCard*> cards =
      GetCreditCardsToSuggest(include_server_cards);
  // If enabled, suppress disused address profiles when triggered from an empty
  // field.
  if (field_contents.empty() &&
      base::FeatureList::IsEnabled(
          features::kAutofillSuppressDisusedCreditCards)) {
    const base::Time min_last_used =
        AutofillClock::Now() - kDisusedDataModelTimeDelta;
    RemoveExpiredCreditCardsNotUsedSinceTimestamp(AutofillClock::Now(),
                                                  min_last_used, &cards);
  }

  return GetSuggestionsForCards(type, field_contents, cards);
}

bool PersonalDataManager::IsAutofillEnabled() const {
  return ::autofill::prefs::IsAutofillEnabled(pref_service_);
}

bool PersonalDataManager::IsAutofillProfileEnabled() const {
  return ::autofill::prefs::IsProfileAutofillEnabled(pref_service_);
}

bool PersonalDataManager::IsAutofillCreditCardEnabled() const {
  return ::autofill::prefs::IsCreditCardAutofillEnabled(pref_service_);
}

bool PersonalDataManager::IsAutofillWalletImportEnabled() const {
  return ::autofill::prefs::IsPaymentsIntegrationEnabled(pref_service_);
}

bool PersonalDataManager::ShouldSuggestServerCards() const {
  if (!IsAutofillWalletImportEnabled())
    return false;

  if (is_syncing_for_test_)
    return true;

  if (!sync_service_)
    return false;

  // Check if the user is in sync transport mode for wallet data.
  if (!sync_service_->IsSyncFeatureEnabled() &&
      base::FeatureList::IsEnabled(
          features::kAutofillEnableAccountWalletStorage)) {
    // For SyncTransport, only show server cards if the user has opted in to
    // seeing them in the dropdown, or if the feature to always show server
    // cards is enabled.
    if (!base::FeatureList::IsEnabled(
            features::kAutofillAlwaysShowServerCardsInSyncTransport) &&
        !prefs::IsUserOptedInWalletSyncTransport(
            pref_service_,
            sync_service_->GetAuthenticatedAccountInfo().account_id)) {
      return false;
    }
  }

  // Server cards should be suggested if the sync service is active.
  // We check for persistent auth errors, because we don't want to offer server
  // cards when the user is in the "sync paused" state.
  return sync_service_->GetActiveDataTypes().Has(
             syncer::AUTOFILL_WALLET_DATA) &&
         !sync_service_->GetAuthError().IsPersistentError();
}

std::string PersonalDataManager::CountryCodeForCurrentTimezone() const {
  return base::CountryCodeForCurrentTimezone();
}

void PersonalDataManager::SetPrefService(PrefService* pref_service) {
  wallet_enabled_pref_ = std::make_unique<BooleanPrefMember>();
  profile_enabled_pref_ = std::make_unique<BooleanPrefMember>();
  credit_card_enabled_pref_ = std::make_unique<BooleanPrefMember>();
  pref_service_ = pref_service;
  // |pref_service_| can be nullptr in tests. Using base::Unretained(this) is
  // safe because observer instances are destroyed once |this| is destroyed.
  if (pref_service_) {
    credit_card_enabled_pref_->Init(
        prefs::kAutofillCreditCardEnabled, pref_service_,
        base::BindRepeating(&PersonalDataManager::EnableAutofillPrefChanged,
                            base::Unretained(this)));
    profile_enabled_pref_->Init(
        prefs::kAutofillProfileEnabled, pref_service_,
        base::BindRepeating(&PersonalDataManager::EnableAutofillPrefChanged,
                            base::Unretained(this)));
    wallet_enabled_pref_->Init(
        prefs::kAutofillWalletImportEnabled, pref_service_,
        base::BindRepeating(
            &PersonalDataManager::EnableWalletIntegrationPrefChanged,
            base::Unretained(this)));
  }
}

void PersonalDataManager::ClearProfileNonSettingsOrigins() {
  for (AutofillProfile* profile : GetProfiles()) {
    if (profile->origin() != kSettingsOrigin && !profile->origin().empty()) {
      profile->set_origin(std::string());
      UpdateProfileInDB(*profile, /*enforced=*/true);
    }
  }

}

void PersonalDataManager::ClearCreditCardNonSettingsOrigins() {
  bool has_updated = false;

  for (CreditCard* card : GetLocalCreditCards()) {
    if (card->origin() != kSettingsOrigin && !card->origin().empty()) {
      card->set_origin(std::string());
      database_helper_->GetLocalDatabase()->UpdateCreditCard(*card);
      has_updated = true;
    }
  }

  // Refresh the local cache and send notifications to observers if a changed
  // was made.
  if (has_updated)
    Refresh();
}

void PersonalDataManager::MoveJapanCityToStreetAddress() {
  if (!database_helper_->GetLocalDatabase())
    return;

  // Don't run if the migration has already been performed.
  if (pref_service_->GetBoolean(prefs::kAutofillJapanCityFieldMigrated))
    return;

  base::string16 japan_country_code = base::ASCIIToUTF16("JP");
  base::string16 line_separator = base::ASCIIToUTF16("\n");
  for (AutofillProfile* profile : GetProfiles()) {
    base::string16 country_code = profile->GetRawInfo(ADDRESS_HOME_COUNTRY);
    base::string16 city = profile->GetRawInfo(ADDRESS_HOME_CITY);
    if (country_code == japan_country_code && !city.empty()) {
      base::string16 street_address =
          profile->GetRawInfo(ADDRESS_HOME_STREET_ADDRESS);
      street_address = street_address.empty()
                           ? city
                           : street_address + line_separator + city;
      profile->SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, street_address);
      profile->SetRawInfo(ADDRESS_HOME_CITY, base::string16());
      UpdateProfileInDB(*profile, /*enforced=*/true);
    }
  }

  // Set the pref so that this migration is never run again.
  pref_service_->SetBoolean(prefs::kAutofillJapanCityFieldMigrated, true);
}

void PersonalDataManager::OnValidated(const AutofillProfile* profile) {
  if (!profile)
    return;

  if (!ProfileChangesAreOnGoing(profile->guid()))
    return;

  // Set the validity states updated, only when the validation has occurred. If
  // the rules were not loaded for any reason, don't set the flag.
  bool validity_updated =
      (profile->GetValidityState(ServerFieldType::ADDRESS_HOME_COUNTRY,
                                 AutofillProfile::CLIENT) !=
       AutofillProfile::UNVALIDATED);

  // For every relevant profile change on the ongoing_profile_changes_, mark the
  // change to show that the validation is done, and set the validity of the
  // profile if the validity was updated.
  for (const auto& change : ongoing_profile_changes_[profile->guid()]) {
    if (!profile->EqualsForClientValidationPurpose(*(change.profile())))
      continue;

    change.validation_effort_made();

    if (validity_updated) {
      change.profile()->set_is_client_validity_states_updated(true);
      change.profile()->SetClientValidityFromBitfieldValue(
          profile->GetClientValidityBitfieldValue());
    }
  }

  HandleNextProfileChange(profile->guid());
}

const ProfileValidityMap& PersonalDataManager::GetProfileValidityByGUID(
    const std::string& guid) {
  static const ProfileValidityMap& empty_validity_map = ProfileValidityMap();
  if (!synced_profile_validity_) {
    profiles_server_validities_need_update_ = true;
    synced_profile_validity_ = std::make_unique<UserProfileValidityMap>();
    if (!synced_profile_validity_->ParseFromString(
            ::autofill::prefs::GetAllProfilesValidityMapsEncodedString(
                pref_service_))) {
      return empty_validity_map;
    }
  }

  auto it = synced_profile_validity_->profile_validity().find(guid);
  if (it != synced_profile_validity_->profile_validity().end()) {
    return it->second;
  }

  return empty_validity_map;
}

// static
std::string PersonalDataManager::MergeProfile(
    const AutofillProfile& new_profile,
    std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
    const std::string& app_locale,
    std::vector<AutofillProfile>* merged_profiles) {
  merged_profiles->clear();

  // Sort the existing profiles in decreasing order of frecency, so the "best"
  // profiles are checked first. Put the verified profiles last so the non
  // verified profiles get deduped among themselves before reaching the verified
  // profiles.
  // TODO(crbug.com/620521): Remove the check for verified from the sort.
  base::Time comparison_time = AutofillClock::Now();
  std::sort(existing_profiles->begin(), existing_profiles->end(),
            [comparison_time](const std::unique_ptr<AutofillProfile>& a,
                              const std::unique_ptr<AutofillProfile>& b) {
              if (a->IsVerified() != b->IsVerified())
                return !a->IsVerified();
              return a->HasGreaterFrecencyThan(b.get(), comparison_time);
            });

  // Set to true if |existing_profiles| already contains an equivalent profile.
  bool matching_profile_found = false;
  std::string guid = new_profile.guid();

  // If we have already saved this address, merge in any missing values.
  // Only merge with the first match. Merging the new profile into the existing
  // one preserves the validity of credit card's billing address reference.
  AutofillProfileComparator comparator(app_locale);
  for (const auto& existing_profile : *existing_profiles) {
    if (!matching_profile_found &&
        comparator.AreMergeable(new_profile, *existing_profile) &&
        existing_profile->SaveAdditionalInfo(new_profile, app_locale)) {
      // Unverified profiles should always be updated with the newer data,
      // whereas verified profiles should only ever be overwritten by verified
      // data.  If an automatically aggregated profile would overwrite a
      // verified profile, just drop it.
      matching_profile_found = true;
      guid = existing_profile->guid();

      // We set the modification date so that immediate requests for profiles
      // will properly reflect the fact that this profile has been modified
      // recently. After writing to the database and refreshing the local copies
      // the profile will have a very slightly newer time reflecting what's
      // actually stored in the database.
      existing_profile->set_modification_date(AutofillClock::Now());
    }
    merged_profiles->push_back(*existing_profile);
  }

  // If the new profile was not merged with an existing one, add it to the list.
  if (!matching_profile_found) {
    merged_profiles->push_back(new_profile);
    // Similar to updating merged profiles above, set the modification date on
    // new profiles.
    merged_profiles->back().set_modification_date(AutofillClock::Now());
    AutofillMetrics::LogProfileActionOnFormSubmitted(
        AutofillMetrics::NEW_PROFILE_CREATED);
  }

  return guid;
}

bool PersonalDataManager::IsCountryOfInterest(
    const std::string& country_code) const {
  DCHECK_EQ(2U, country_code.size());

  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  std::list<std::string> country_codes;
  for (size_t i = 0; i < profiles.size(); ++i) {
    country_codes.push_back(base::ToLowerASCII(
        base::UTF16ToASCII(profiles[i]->GetRawInfo(ADDRESS_HOME_COUNTRY))));
  }

  std::string timezone_country = CountryCodeForCurrentTimezone();
  if (!timezone_country.empty())
    country_codes.push_back(base::ToLowerASCII(timezone_country));

  // Only take the locale into consideration if all else fails.
  if (country_codes.empty()) {
    country_codes.push_back(base::ToLowerASCII(
        AutofillCountry::CountryCodeForLocale(app_locale())));
  }

  return base::ContainsValue(country_codes, base::ToLowerASCII(country_code));
}

const std::string& PersonalDataManager::GetDefaultCountryCodeForNewAddress()
    const {
  if (default_country_code_.empty())
    default_country_code_ = MostCommonCountryCodeFromProfiles();

  // Failing that, guess based on system timezone.
  if (default_country_code_.empty())
    default_country_code_ = CountryCodeForCurrentTimezone();

  // Failing that, guess based on locale.
  if (default_country_code_.empty())
    default_country_code_ = AutofillCountry::CountryCodeForLocale(app_locale());

  return default_country_code_;
}

// static
void PersonalDataManager::DedupeCreditCardToSuggest(
    std::list<CreditCard*>* cards_to_suggest) {
  for (auto outer_it = cards_to_suggest->begin();
       outer_it != cards_to_suggest->end(); ++outer_it) {
    // If considering a full server card, look for local cards that are
    // duplicates of it and remove them.
    if ((*outer_it)->record_type() == CreditCard::FULL_SERVER_CARD) {
      for (auto inner_it = cards_to_suggest->begin();
           inner_it != cards_to_suggest->end();) {
        auto inner_it_copy = inner_it++;
        if ((*inner_it_copy)->IsLocalDuplicateOfServerCard(**outer_it))
          cards_to_suggest->erase(inner_it_copy);
      }
      // If considering a local card, look for masked server cards that are
      // duplicates of it and remove them.
    } else if ((*outer_it)->record_type() == CreditCard::LOCAL_CARD) {
      for (auto inner_it = cards_to_suggest->begin();
           inner_it != cards_to_suggest->end();) {
        auto inner_it_copy = inner_it++;
        if ((*inner_it_copy)->record_type() == CreditCard::MASKED_SERVER_CARD &&
            (*outer_it)->IsLocalDuplicateOfServerCard(**inner_it_copy)) {
          cards_to_suggest->erase(inner_it_copy);
        }
      }
    }
  }
}

void PersonalDataManager::SetProfiles(std::vector<AutofillProfile>* profiles) {
  if (is_off_the_record_)
    return;
  if (!database_helper_->GetLocalDatabase())
    return;

  ClearOnGoingProfileChanges();

  // Any profiles that are not in the new profile list should be removed from
  // the web database
  for (const auto& it : web_profiles_) {
    if (!FindByGUID<AutofillProfile>(*profiles, it->guid()))
      RemoveProfileFromDB(it->guid());
  }

  // Update the web database with the new and existing profiles.
  for (const AutofillProfile& it : *profiles) {
    if (FindByGUID<AutofillProfile>(web_profiles_, it.guid())) {
      UpdateProfileInDB(it);
    } else {
      AddProfileToDB(it);
    }
  }

  // Copy in the new profiles.
  web_profiles_.clear();
  for (const AutofillProfile& it : *profiles) {
    web_profiles_.push_back(std::make_unique<AutofillProfile>(it));
  }
}

void PersonalDataManager::SetCreditCards(
    std::vector<CreditCard>* credit_cards) {
  if (is_off_the_record_)
    return;

  // Remove empty credit cards from input.
  base::EraseIf(*credit_cards, IsEmptyFunctor<CreditCard>(app_locale_));

  if (!database_helper_->GetLocalDatabase())
    return;

  // Any credit cards that are not in the new credit card list should be
  // removed.
  for (const auto& card : local_credit_cards_) {
    if (!FindByGUID<CreditCard>(*credit_cards, card->guid()))
      database_helper_->GetLocalDatabase()->RemoveCreditCard(card->guid());
  }

  // Update the web database with the existing credit cards.
  for (const CreditCard& card : *credit_cards) {
    if (FindByGUID<CreditCard>(local_credit_cards_, card.guid()))
      database_helper_->GetLocalDatabase()->UpdateCreditCard(card);
  }

  // Add the new credit cards to the web database.  Don't add a duplicate.
  for (const CreditCard& card : *credit_cards) {
    if (!FindByGUID<CreditCard>(local_credit_cards_, card.guid()) &&
        !FindByContents(local_credit_cards_, card))
      database_helper_->GetLocalDatabase()->AddCreditCard(card);
  }

  // Copy in the new credit cards.
  local_credit_cards_.clear();
  for (const CreditCard& card : *credit_cards)
    local_credit_cards_.push_back(std::make_unique<CreditCard>(card));

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

void PersonalDataManager::LoadProfiles() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_profiles_query_);
  CancelPendingServerQuery(&pending_server_profiles_query_);

  pending_profiles_query_ =
      database_helper_->GetLocalDatabase()->GetAutofillProfiles(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_profiles_query_ =
        database_helper_->GetServerDatabase()->GetServerProfiles(this);
  }
}

void PersonalDataManager::LoadCreditCards() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQuery(&pending_server_creditcards_query_);

  pending_creditcards_query_ =
      database_helper_->GetLocalDatabase()->GetCreditCards(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_creditcards_query_ =
        database_helper_->GetServerDatabase()->GetServerCreditCards(this);
  }
}

void PersonalDataManager::CancelPendingLocalQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetLocalDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetLocalDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PersonalDataManager::CancelPendingServerQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetServerDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetServerDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PersonalDataManager::CancelPendingServerQueries() {
  if (pending_server_profiles_query_) {
    CancelPendingServerQuery(&pending_server_profiles_query_);
  }
  if (pending_server_creditcards_query_) {
    CancelPendingServerQuery(&pending_server_creditcards_query_);
  }
  if (pending_customer_data_query_) {
    CancelPendingServerQuery(&pending_customer_data_query_);
  }
}

void PersonalDataManager::LoadPaymentsCustomerData() {
  if (!database_helper_->GetServerDatabase())
    return;

  CancelPendingServerQuery(&pending_customer_data_query_);

  pending_customer_data_query_ =
      database_helper_->GetServerDatabase()->GetPaymentsCustomerData(this);
}

std::string PersonalDataManager::SaveImportedProfile(
    const AutofillProfile& imported_profile) {
  if (is_off_the_record_)
    return std::string();

  std::vector<AutofillProfile> profiles;
  std::string guid =
      MergeProfile(imported_profile, &web_profiles_, app_locale_, &profiles);
  SetProfiles(&profiles);
  return guid;
}

void PersonalDataManager::NotifyPersonalDataChanged() {
  bool profile_changes_are_on_going = ProfileChangesAreOnGoing();
  for (PersonalDataManagerObserver& observer : observers_) {
    observer.OnPersonalDataChanged();
    if (!profile_changes_are_on_going) {
      observer.OnPersonalDataFinishedProfileTasks();
    }
  }

  // If new data was synced, try to convert new server profiles and update
  // server cards.
  if (has_synced_new_data_) {
    has_synced_new_data_ = false;
    ConvertWalletAddressesAndUpdateWalletCards();
  }
}

std::string PersonalDataManager::OnAcceptedLocalCreditCardSave(
    const CreditCard& imported_card) {
  DCHECK(!imported_card.number().empty());
  if (is_off_the_record_)
    return std::string();

  // Log that local credit card save reached the PersonalDataManager. This is a
  // temporary metric to measure the impact, if any, of CreditCardSaveManager's
  // destruction before its callbacks are executed.
  // TODO(crbug.com/892299): Remove this once the overall problem is fixed.
  AutofillMetrics::LogSaveCardReachedPersonalDataManager(/*is_local=*/true);

  return SaveImportedCreditCard(imported_card);
}

std::string PersonalDataManager::SaveImportedCreditCard(
    const CreditCard& imported_card) {
  // Set to true if |imported_card| is merged into the credit card list.
  bool merged = false;

  std::string guid = imported_card.guid();
  std::vector<CreditCard> credit_cards;
  for (auto& card : local_credit_cards_) {
    // If |imported_card| has not yet been merged, check whether it should be
    // with the current |card|.
    if (!merged && card->UpdateFromImportedCard(imported_card, app_locale_)) {
      guid = card->guid();
      merged = true;
    }

    credit_cards.push_back(*card);
  }

  if (!merged)
    credit_cards.push_back(imported_card);

  SetCreditCards(&credit_cards);
  return guid;
}

void PersonalDataManager::LogStoredProfileMetrics() const {
  if (!has_logged_stored_profile_metrics_) {
    // Update the histogram of how many addresses the user has stored.
    AutofillMetrics::LogStoredProfileCount(web_profiles_.size());

    // If the user has stored addresses, log the distribution of days since
    // their last use and how many would be considered disused.
    if (!web_profiles_.empty()) {
      size_t num_disused_profiles = 0;
      const base::Time now = AutofillClock::Now();
      for (const std::unique_ptr<AutofillProfile>& profile : web_profiles_) {
        const base::TimeDelta time_since_last_use = now - profile->use_date();
        AutofillMetrics::LogStoredProfileDaysSinceLastUse(
            time_since_last_use.InDays());
        if (time_since_last_use > kDisusedDataModelTimeDelta)
          ++num_disused_profiles;
      }
      AutofillMetrics::LogStoredProfileDisusedCount(num_disused_profiles);
    }

    // Only log this info once per chrome user profile load.
    has_logged_stored_profile_metrics_ = true;
  }
}

void PersonalDataManager::LogStoredCreditCardMetrics() const {
  if (!has_logged_stored_credit_card_metrics_) {
    AutofillMetrics::LogStoredCreditCardMetrics(
        local_credit_cards_, server_credit_cards_, kDisusedDataModelTimeDelta);

    // Only log this info once per chrome user profile load.
    has_logged_stored_credit_card_metrics_ = true;
  }
}

std::string PersonalDataManager::MostCommonCountryCodeFromProfiles() const {
  if (!IsAutofillEnabled())
    return std::string();

  // Count up country codes from existing profiles.
  std::map<std::string, int> votes;
  // TODO(estade): can we make this GetProfiles() instead? It seems to cause
  // errors in tests on mac trybots. See http://crbug.com/57221
  const std::vector<AutofillProfile*>& profiles = GetProfiles();
  const std::vector<std::string>& country_codes =
      CountryDataMap::GetInstance()->country_codes();
  for (size_t i = 0; i < profiles.size(); ++i) {
    std::string country_code = base::ToUpperASCII(
        base::UTF16ToASCII(profiles[i]->GetRawInfo(ADDRESS_HOME_COUNTRY)));

    if (base::ContainsValue(country_codes, country_code)) {
      // Verified profiles count 100x more than unverified ones.
      votes[country_code] += profiles[i]->IsVerified() ? 100 : 1;
    }
  }

  // Take the most common country code.
  if (!votes.empty()) {
    auto iter = std::max_element(votes.begin(), votes.end(), CompareVotes);
    return iter->first;
  }

  return std::string();
}

void PersonalDataManager::EnableWalletIntegrationPrefChanged() {
  if (!prefs::IsPaymentsIntegrationEnabled(pref_service_)) {
    // Re-mask all server cards when the user turns off wallet card
    // integration.
    ResetFullServerCards();
    NotifyPersonalDataChanged();
  }
}

void PersonalDataManager::EnableAutofillPrefChanged() {
  default_country_code_.clear();

  // Refresh our local cache and send notifications to observers.
  Refresh();
}

bool PersonalDataManager::IsKnownCard(const CreditCard& credit_card) const {
  const auto stripped_pan = CreditCard::StripSeparators(credit_card.number());
  for (const auto& card : local_credit_cards_) {
    if (stripped_pan == CreditCard::StripSeparators(card->number()))
      return true;
  }

  const auto masked_info = credit_card.NetworkAndLastFourDigits();
  for (const auto& card : server_credit_cards_) {
    switch (card->record_type()) {
      case CreditCard::FULL_SERVER_CARD:
        if (stripped_pan == CreditCard::StripSeparators(card->number()))
          return true;
        break;
      case CreditCard::MASKED_SERVER_CARD:
        if (masked_info == card->NetworkAndLastFourDigits())
          return true;
        break;
      default:
        NOTREACHED();
    }
  }

  return false;
}

bool PersonalDataManager::IsServerCard(const CreditCard* credit_card) const {
  // Check whether the current card itself is a server card.
  if (credit_card->record_type() != autofill::CreditCard::LOCAL_CARD)
    return true;

  std::vector<CreditCard*> server_credit_cards = GetServerCreditCards();
  // Check whether the current card is already uploaded.
  for (const CreditCard* server_card : server_credit_cards) {
    if (credit_card->HasSameNumberAs(*server_card))
      return true;
  }
  return false;
}

bool PersonalDataManager::ShouldShowCardsFromAccountOption() const {
// The feature is only for Linux, Windows and Mac.
#if (!defined(OS_LINUX) && !defined(OS_WIN) && !defined(OS_MACOSX)) || \
    defined(OS_CHROMEOS)
  return false;
#endif  // (!defined(OS_LINUX) && !defined(OS_WIN) && !defined(OS_MACOSX)) ||
        // defined(OS_CHROMEOS)

  // This option should only be shown for users that have not enabled the Sync
  // Feature and that have server credit cards available.
  if (!sync_service_ || sync_service_->IsSyncFeatureEnabled() ||
      GetServerCreditCards().empty()) {
    return false;
  }

  // If we have not returned yet, it should mean that the user is in Sync
  // Transport mode for Wallet data (Sync Feature disabled but has server
  // cards). This should only happen if that feature is enabled.
  DCHECK(base::FeatureList::IsEnabled(
      features::kAutofillEnableAccountWalletStorage));

  // If the feature to always show the server cards in sync transport mode is
  // enabled, don't show the option.
  if (base::FeatureList::IsEnabled(
          features::kAutofillAlwaysShowServerCardsInSyncTransport)) {
    return false;
  }

  bool is_opted_in = prefs::IsUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAuthenticatedAccountInfo().account_id);

  AutofillMetrics::LogWalletSyncTransportCardsOptIn(is_opted_in);

  // The option should only be shown if the user has not already opted-in.
  return !is_opted_in;
}

void PersonalDataManager::OnUserAcceptedCardsFromAccountOption() {
  DCHECK_EQ(AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled,
            GetSyncSigninState());
  prefs::SetUserOptedInWalletSyncTransport(
      pref_service_, sync_service_->GetAuthenticatedAccountInfo().account_id,
      /*opted_in=*/true);
}

void PersonalDataManager::OnAutofillProfileChanged(
    const AutofillProfileDeepChange& change) {
  const auto& guid = change.key();
  const auto& change_type = change.type();
  const auto& profile = *(change.profile());
  DCHECK(guid == profile.guid());
  // Happens only in tests.
  if (!ProfileChangesAreOnGoing(guid)) {
    DVLOG(1) << "Received an unexpected response from database.";
    return;
  }

  const auto* existing_profile = GetProfileByGUID(guid);
  const bool profile_exists = (existing_profile != nullptr);
  switch (change_type) {
    case AutofillProfileChange::ADD:
      profiles_server_validities_need_update_ = true;
      if (!profile_exists && !FindByContents(web_profiles_, profile)) {
        web_profiles_.push_back(std::make_unique<AutofillProfile>(profile));
      }
      break;
    case AutofillProfileChange::UPDATE:
      profiles_server_validities_need_update_ = true;
      if (profile_exists &&
          (change.enforce_update() ||
           !existing_profile->EqualsForUpdatePurposes(profile))) {
        web_profiles_.erase(
            FindElementByGUID<AutofillProfile>(web_profiles_, guid));
        web_profiles_.push_back(std::make_unique<AutofillProfile>(profile));
      }
      break;
    case AutofillProfileChange::REMOVE:
      if (profile_exists) {
        web_profiles_.erase(
            FindElementByGUID<AutofillProfile>(web_profiles_, guid));
      }
      break;
    default:
      NOTREACHED();
  }

  OnProfileChangeDone(guid);
}

void PersonalDataManager::LogServerCardLinkClicked() const {
  AutofillMetrics::LogServerCardLinkClicked(GetSyncSigninState());
}

void PersonalDataManager::OnUserAcceptedUpstreamOffer() {
  // If the user is in sync transport mode for Wallet, record an opt-in.
  if (GetSyncSigninState() ==
      AutofillSyncSigninState::kSignedInAndWalletSyncTransportEnabled) {
    prefs::SetUserOptedInWalletSyncTransport(
        pref_service_, sync_service_->GetAuthenticatedAccountInfo().account_id,
        /*opted_in=*/true);
  }
}

std::vector<Suggestion> PersonalDataManager::GetSuggestionsForCards(
    const AutofillType& type,
    const base::string16& field_contents,
    const std::vector<CreditCard*>& cards_to_suggest) const {
  std::vector<Suggestion> suggestions;
  base::string16 field_contents_lower = base::i18n::ToLower(field_contents);
  for (const CreditCard* credit_card : cards_to_suggest) {
    // The value of the stored data for this field type in the |credit_card|.
    base::string16 creditcard_field_value =
        credit_card->GetInfo(type, app_locale_);
    if (creditcard_field_value.empty())
      continue;
    base::string16 creditcard_field_lower =
        base::i18n::ToLower(creditcard_field_value);

    bool prefix_matched_suggestion;
    if (suggestion_selection::IsValidSuggestionForFieldContents(
            creditcard_field_lower, field_contents_lower, type,
            credit_card->record_type() == CreditCard::MASKED_SERVER_CARD,
            &prefix_matched_suggestion)) {
      // Make a new suggestion.
      suggestions.push_back(Suggestion());
      Suggestion* suggestion = &suggestions.back();

      suggestion->value = credit_card->GetInfo(type, app_locale_);
      suggestion->icon = credit_card->network();
      suggestion->backend_id = credit_card->guid();
      suggestion->match = prefix_matched_suggestion
                              ? Suggestion::PREFIX_MATCH
                              : Suggestion::SUBSTRING_MATCH;

      // If the value is the card number, the label is the expiration date.
      // Otherwise the label is the card number, or if that is empty the
      // cardholder name. The label should never repeat the value.
      if (type.GetStorableType() == CREDIT_CARD_NUMBER) {
        suggestion->value = credit_card->NetworkOrBankNameAndLastFourDigits();
        suggestion->label = credit_card->GetInfo(
            AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR), app_locale_);
        // The additional label will be used if two-line display is enabled.
        suggestion->additional_label =
            credit_card->DescriptiveExpiration(app_locale_);
      } else if (credit_card->number().empty()) {
        if (type.GetStorableType() != CREDIT_CARD_NAME_FULL) {
          suggestion->label = credit_card->GetInfo(
              AutofillType(CREDIT_CARD_NAME_FULL), app_locale_);
        }
      } else {
#if defined(OS_ANDROID)
        // Since Android places the label on its own row, there's more
        // horizontal space to work with. Show "Amex - 1234" rather than
        // desktop's "****1234".
        suggestion->label = credit_card->NetworkOrBankNameAndLastFourDigits();
#else
        suggestion->label = credit_card->ObfuscatedLastFourDigits();
        // Add the card number with expiry information in the additional
        // label portion so that we an show it when two-line display is
        // enabled.
        suggestion->additional_label =
            credit_card
                ->NetworkOrBankNameLastFourDigitsAndDescriptiveExpiration(
                    app_locale_);
#endif
      }
    }
  }

  // Prefix matches should precede other token matches.
  if (IsFeatureSubstringMatchEnabled()) {
    std::stable_sort(suggestions.begin(), suggestions.end(),
                     [](const Suggestion& a, const Suggestion& b) {
                       return a.match < b.match;
                     });
  }

  return suggestions;
}

void PersonalDataManager::RemoveOrphanAutofillTableRows() {
  // Don't run if the fix has already been applied.
  if (pref_service_->GetBoolean(prefs::kAutofillOrphanRowsRemoved))
    return;

  if (!database_helper_->GetLocalDatabase())
    return;

  database_helper_->GetLocalDatabase()->RemoveOrphanAutofillTableRows();

  // Set the pref so that this fix is never run again.
  pref_service_->SetBoolean(prefs::kAutofillOrphanRowsRemoved, true);
}

bool PersonalDataManager::ApplyDedupingRoutine() {
  if (!is_autofill_profile_cleanup_pending_)
    return false;

  is_autofill_profile_cleanup_pending_ = false;

  // No need to de-duplicate if there are less than two profiles.
  if (web_profiles_.size() < 2) {
    DVLOG(1) << "Autofill profile de-duplication not needed.";
    return false;
  }

  // Check if de-duplication has already been performed this major version.
  if (pref_service_->GetInteger(prefs::kAutofillLastVersionDeduped) >=
      CHROME_VERSION_MAJOR) {
    DVLOG(1)
        << "Autofill profile de-duplication already performed for this version";
    return false;
  }

  DVLOG(1) << "Starting autofill profile de-duplication.";
  std::unordered_set<std::string> profiles_to_delete;
  profiles_to_delete.reserve(web_profiles_.size());

  // Create the map used to update credit card's billing addresses after the
  // dedupe.
  std::unordered_map<std::string, std::string> guids_merge_map;

  // The changes can't happen directly on the web_profiles_, but need to be
  // updated in the database at first, and then updated on the web_profiles_.
  // Therefore, we need a copy of web_profiles_ to keep track of the changes.
  std::vector<std::unique_ptr<AutofillProfile>> new_profiles;
  for (const auto& it : web_profiles_) {
    new_profiles.push_back(std::make_unique<AutofillProfile>(*(it.get())));
  }

  DedupeProfiles(&new_profiles, &profiles_to_delete, &guids_merge_map);

  // Apply the profile changes to the database.
  for (const auto& profile : new_profiles) {
    // If the profile was set to be deleted, remove it from the database,
    // otherwise update it.
    if (profiles_to_delete.count(profile->guid())) {
      RemoveProfileFromDB(profile->guid());
    } else {
      UpdateProfileInDB(*(profile.get()));
    }
  }

  UpdateCardsBillingAddressReference(guids_merge_map);

  // Set the pref to the current major version.
  pref_service_->SetInteger(prefs::kAutofillLastVersionDeduped,
                            CHROME_VERSION_MAJOR);
  return true;
}

void PersonalDataManager::DedupeProfiles(
    std::vector<std::unique_ptr<AutofillProfile>>* existing_profiles,
    std::unordered_set<std::string>* profiles_to_delete,
    std::unordered_map<std::string, std::string>* guids_merge_map) const {
  AutofillMetrics::LogNumberOfProfilesConsideredForDedupe(
      existing_profiles->size());

  // Sort the profiles by frecency with all the verified profiles at the end.
  // That way the most relevant profiles will get merged into the less relevant
  // profiles, which keeps the syntax of the most relevant profiles data.
  // Verified profiles are put at the end because they do not merge into other
  // profiles, so the loop can be stopped when we reach those. However they need
  // to be in the vector because an unverified profile trying to merge into a
  // similar verified profile will be discarded.
  base::Time comparison_time = AutofillClock::Now();
  std::sort(existing_profiles->begin(), existing_profiles->end(),
            [comparison_time](const std::unique_ptr<AutofillProfile>& a,
                              const std::unique_ptr<AutofillProfile>& b) {
              if (a->IsVerified() != b->IsVerified())
                return !a->IsVerified();
              return a->HasGreaterFrecencyThan(b.get(), comparison_time);
            });

  AutofillProfileComparator comparator(app_locale_);

  for (size_t i = 0; i < existing_profiles->size(); ++i) {
    AutofillProfile* profile_to_merge = (*existing_profiles)[i].get();

    // If the profile was set to be deleted, skip it. It has already been
    // merged into another profile.
    if (profiles_to_delete->count(profile_to_merge->guid()))
      continue;

    // If we have reached the verified profiles, stop trying to merge. Verified
    // profiles do not get merged.
    if (profile_to_merge->IsVerified())
      break;

    // If we have not reached the last profile, try to merge |profile_to_merge|
    // with all the less relevant |existing_profiles|.
    for (size_t j = i + 1; j < existing_profiles->size(); ++j) {
      AutofillProfile* existing_profile = (*existing_profiles)[j].get();

      // Don't try to merge a profile that was already set for deletion.
      if (profiles_to_delete->count(existing_profile->guid()))
        continue;

      // Move on if the profiles are not mergeable.
      if (!comparator.AreMergeable(*existing_profile, *profile_to_merge))
        continue;

      // The profiles are found to be mergeable. Attempt to update the existing
      // profile. This returns true if the merge was successful, or if the
      // merge would have been successful but the existing profile IsVerified()
      // and will not accept updates from profile_to_merge.
      if (existing_profile->SaveAdditionalInfo(*profile_to_merge,
                                               app_locale_)) {
        // Keep track that a credit card using |profile_to_merge|'s GUID as its
        // billing address id should replace it by |existing_profile|'s GUID.
        guids_merge_map->emplace(profile_to_merge->guid(),
                                 existing_profile->guid());

        // Since |profile_to_merge| was a duplicate of |existing_profile|
        // and was merged successfully, it can now be deleted.
        profiles_to_delete->insert(profile_to_merge->guid());

        // Now try to merge the new resulting profile with the rest of the
        // existing profiles.
        profile_to_merge = existing_profile;

        // Verified profiles do not get merged. Save some time by not
        // trying.
        if (profile_to_merge->IsVerified())
          break;
      }
    }
  }
  AutofillMetrics::LogNumberOfProfilesRemovedDuringDedupe(
      profiles_to_delete->size());
}

void PersonalDataManager::UpdateCardsBillingAddressReference(
    const std::unordered_map<std::string, std::string>& guids_merge_map) {
  /*  Here is an example of what the graph might look like.

      A -> B
             \
               -> E
             /
      C -> D
  */

  for (auto* credit_card : GetCreditCards()) {
    // If the credit card is not associated with a billing address, skip it.
    if (credit_card->billing_address_id().empty())
      break;

    // If the billing address profile associated with the card has been merged,
    // replace it by the id of the profile in which it was merged. Repeat the
    // process until the billing address has not been merged into another one.
    std::unordered_map<std::string, std::string>::size_type nb_guid_changes = 0;
    bool was_modified = false;
    auto it = guids_merge_map.find(credit_card->billing_address_id());
    while (it != guids_merge_map.end()) {
      was_modified = true;
      credit_card->set_billing_address_id(it->second);
      it = guids_merge_map.find(credit_card->billing_address_id());

      // Out of abundance of caution.
      if (nb_guid_changes > guids_merge_map.size()) {
        NOTREACHED();
        // Cancel the changes for that card.
        was_modified = false;
        break;
      }
    }

    // If the card was modified, apply the changes to the database.
    if (was_modified) {
      if (credit_card->record_type() == CreditCard::LOCAL_CARD)
        database_helper_->GetLocalDatabase()->UpdateCreditCard(*credit_card);
      else
        database_helper_->GetServerDatabase()->UpdateServerCardMetadata(
            *credit_card);
    }
  }
  Refresh();
}

void PersonalDataManager::ConvertWalletAddressesAndUpdateWalletCards() {
  // Copy the local profiles into a vector<AutofillProfile>. Theses are the
  // existing profiles. Get them sorted in decreasing order of frecency, so the
  // "best" profiles are checked first. Put the verified profiles last so the
  // server addresses have a chance to merge into the non-verified local
  // profiles.
  std::vector<AutofillProfile> local_profiles;
  for (AutofillProfile* existing_profile : GetProfiles()) {
    local_profiles.push_back(*existing_profile);
  }

  // Since we are already iterating on all the server profiles to convert Wallet
  // addresses and we will need to access them by guid later to update the
  // Wallet cards, create a map here.
  std::unordered_map<std::string, AutofillProfile*> server_id_profiles_map;

  // Create the map used to update credit card's billing addresses after the
  // conversion/merge.
  std::unordered_map<std::string, std::string> guids_merge_map;

  bool has_converted_addresses = ConvertWalletAddressesToLocalProfiles(
      &local_profiles, &server_id_profiles_map, &guids_merge_map);
  bool should_update_cards = UpdateWalletCardsAlreadyConvertedBillingAddresses(
      local_profiles, server_id_profiles_map, &guids_merge_map);

  if (has_converted_addresses) {
    // Save the local profiles to the DB.
    SetProfiles(&local_profiles);
  }

  if (should_update_cards || has_converted_addresses) {
    // Update the credit cards billing address relationship.
    UpdateCardsBillingAddressReference(guids_merge_map);

    // Force a reload of the profiles and cards.
  }
}

bool PersonalDataManager::ConvertWalletAddressesToLocalProfiles(
    std::vector<AutofillProfile>* local_profiles,
    std::unordered_map<std::string, AutofillProfile*>* server_id_profiles_map,
    std::unordered_map<std::string, std::string>* guids_merge_map) {
  // If the full Sync feature isn't enabled, then do NOT convert any Wallet
  // addresses to local ones.
  if (!IsSyncFeatureEnabled()) {
    return false;
  }

  bool has_converted_addresses = false;
  for (std::unique_ptr<AutofillProfile>& wallet_address : server_profiles_) {
    // Add the profile to the map.
    server_id_profiles_map->emplace(wallet_address->server_id(),
                                    wallet_address.get());

    // If the address has not been converted yet, convert it.
    if (!wallet_address->has_converted()) {
      // Try to merge the server address into a similar local profile, or create
      // a new local profile if no similar profile is found.
      std::string address_guid = MergeServerAddressesIntoProfiles(
          *wallet_address, local_profiles, app_locale_,
          GetAccountInfoForPaymentsServer().email);

      // Update the map to transfer the billing address relationship from the
      // server address to the converted/merged local profile.
      guids_merge_map->emplace(wallet_address->server_id(), address_guid);

      // Update the wallet addresses metadata to record the conversion.
      wallet_address->set_has_converted(true);
      database_helper_->GetServerDatabase()->UpdateServerAddressMetadata(
          *wallet_address);

      has_converted_addresses = true;
    }
  }

  return has_converted_addresses;
}

bool PersonalDataManager::UpdateWalletCardsAlreadyConvertedBillingAddresses(
    const std::vector<AutofillProfile>& local_profiles,
    const std::unordered_map<std::string, AutofillProfile*>&
        server_id_profiles_map,
    std::unordered_map<std::string, std::string>* guids_merge_map) const {
  // Look for server cards that still refer to server addresses but for which
  // there is no mapping. This can happen if it's a new card for which the
  // billing address has already been converted. This should be a no-op for most
  // situations. Otherwise, it should affect only one Wallet card, sinces users
  // do not add a lot of credit cards.
  AutofillProfileComparator comparator(app_locale_);
  bool should_update_cards = false;
  for (const std::unique_ptr<CreditCard>& wallet_card : server_credit_cards_) {
    std::string billing_address_id = wallet_card->billing_address_id();

    // If billing address refers to a server id and that id is not a key in the
    // |guids_merge_map|, it means that the card is new but the address was
    // already converted. Look for the matching converted profile.
    if (!billing_address_id.empty() &&
        billing_address_id.length() != LOCAL_GUID_LENGTH &&
        guids_merge_map->find(billing_address_id) == guids_merge_map->end()) {
      // Get the profile.
      auto it = server_id_profiles_map.find(billing_address_id);
      if (it != server_id_profiles_map.end()) {
        const AutofillProfile* billing_address = it->second;

        // Look for a matching local profile (DO NOT MERGE).
        for (const auto& local_profile : local_profiles) {
          if (comparator.AreMergeable(*billing_address, local_profile)) {
            // The Wallet address matches this local profile. Add this to the
            // merge mapping.
            guids_merge_map->emplace(billing_address_id, local_profile.guid());
            should_update_cards = true;
            break;
          }
        }
      }
    }
  }

  return should_update_cards;
}

// TODO(crbug.com/687975): Reuse MergeProfile in this function.
// static
std::string PersonalDataManager::MergeServerAddressesIntoProfiles(
    const AutofillProfile& server_address,
    std::vector<AutofillProfile>* existing_profiles,
    const std::string& app_locale,
    const std::string& primary_account_email) {
  // If there is already a local profile that is very similar, merge in any
  // missing values. Only merge with the first match.
  AutofillProfileComparator comparator(app_locale);
  for (auto& local_profile : *existing_profiles) {
    if (comparator.AreMergeable(server_address, local_profile) &&
        local_profile.SaveAdditionalInfo(server_address, app_locale)) {
      local_profile.set_modification_date(AutofillClock::Now());
      AutofillMetrics::LogWalletAddressConversionType(
          AutofillMetrics::CONVERTED_ADDRESS_MERGED);
      return local_profile.guid();
    }
  }

  // If the server address was not merged with a local profile, add it to the
  // list.
  existing_profiles->push_back(server_address);
  // Set the profile as being local.
  existing_profiles->back().set_record_type(AutofillProfile::LOCAL_PROFILE);
  existing_profiles->back().set_modification_date(AutofillClock::Now());

  // Wallet addresses don't have an email address, use the one from the
  // currently signed-in account.
  base::string16 email = base::UTF8ToUTF16(primary_account_email);
  if (!email.empty())
    existing_profiles->back().SetRawInfo(EMAIL_ADDRESS, email);

  AutofillMetrics::LogWalletAddressConversionType(
      AutofillMetrics::CONVERTED_ADDRESS_ADDED);

  return server_address.guid();
}

bool PersonalDataManager::DeleteDisusedCreditCards() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillDeleteDisusedCreditCards)) {
    return false;
  }

  // Only delete local cards, as server cards are managed by Payments.
  auto cards = GetLocalCreditCards();

  // Early exit when there is no local cards.
  if (cards.empty()) {
    return true;
  }

  std::vector<std::string> guid_to_delete;
  for (CreditCard* card : cards) {
    if (card->IsDeletable()) {
      guid_to_delete.push_back(card->guid());
    }
  }

  size_t num_deleted_cards = guid_to_delete.size();

  for (auto const guid : guid_to_delete) {
    database_helper_->GetLocalDatabase()->RemoveCreditCard(guid);
  }

  if (num_deleted_cards > 0) {
    Refresh();
  }

  AutofillMetrics::LogNumberOfCreditCardsDeletedForDisuse(num_deleted_cards);

  return true;
}

bool PersonalDataManager::DeleteDisusedAddresses() {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillDeleteDisusedAddresses)) {
    DVLOG(1) << "Deletion is disabled";
    return false;
  }

  const std::vector<AutofillProfile*>& profiles = GetProfiles();

  // Early exit when there are no profiles.
  if (profiles.empty()) {
    DVLOG(1) << "There are no profiles";
    return true;
  }

  std::unordered_set<std::string> used_billing_address_guids;
  for (CreditCard* card : GetCreditCards()) {
    if (!card->IsDeletable()) {
      used_billing_address_guids.insert(card->billing_address_id());
    }
  }

  std::vector<std::string> guids_to_delete;
  for (AutofillProfile* profile : profiles) {
    if (profile->IsDeletable() &&
        !used_billing_address_guids.count(profile->guid())) {
      guids_to_delete.push_back(profile->guid());
    }
  }

  size_t num_deleted_addresses = guids_to_delete.size();

  for (auto const guid : guids_to_delete) {
    RemoveAutofillProfileByGUIDAndBlankCreditCardReference(guid);
  }

  if (num_deleted_addresses > 0) {
    Refresh();
  }

  AutofillMetrics::LogNumberOfAddressesDeletedForDisuse(num_deleted_addresses);

  return true;
}

void PersonalDataManager::ApplyAddressFixesAndCleanups() {
  // Validate profiles once per major.
  UpdateClientValidityStates(GetProfiles());

  // One-time fix, otherwise NOP.
  RemoveOrphanAutofillTableRows();

  // Once per major version, otherwise NOP.
  ApplyDedupingRoutine();

  DeleteDisusedAddresses();

  // If feature AutofillCreateDataForTest is enabled, and once per user profile
  // startup.
  test_data_creator_.MaybeAddTestProfiles(base::BindRepeating(
      &PersonalDataManager::AddProfile, base::Unretained(this)));

  // Ran everytime it is called.
  ClearProfileNonSettingsOrigins();

  // One-time fix, otherwise NOP.
  MoveJapanCityToStreetAddress();
}

void PersonalDataManager::ApplyCardFixesAndCleanups() {
  DeleteDisusedCreditCards();

  // If feature AutofillCreateDataForTest is enabled, and once per user profile
  // startup.
  test_data_creator_.MaybeAddTestCreditCards(base::BindRepeating(
      &PersonalDataManager::AddCreditCard, base::Unretained(this)));

  // Ran everytime it is called.
  ClearCreditCardNonSettingsOrigins();
}

void PersonalDataManager::ResetProfileValidity() {
  synced_profile_validity_.reset();
  profiles_server_validities_need_update_ = true;
}

void PersonalDataManager::AddProfileToDB(const AutofillProfile& profile) {
  if (profile.IsEmpty(app_locale_)) {
    NotifyPersonalDataChanged();
    return;
  }

  if (!ProfileChangesAreOnGoing(profile.guid())) {
    if (FindByGUID<AutofillProfile>(web_profiles_, profile.guid()) ||
        FindByContents(web_profiles_, profile)) {
      NotifyPersonalDataChanged();
      return;
    }
  }
  ongoing_profile_changes_[profile.guid()].push_back(
      AutofillProfileDeepChange(AutofillProfileChange::ADD, profile));
  UpdateClientValidityStates(profile);
}

void PersonalDataManager::UpdateProfileInDB(const AutofillProfile& profile,
                                            bool enforced) {
  // if the update is enforced, don't check if a similar profile already exists
  // or not. Otherwise, check if updating the profile makes sense.
  if (!enforced && !ProfileChangesAreOnGoing(profile.guid())) {
    const auto* existing_profile = GetProfileByGUID(profile.guid());
    bool profile_exists = (existing_profile != nullptr);
    if (!profile_exists || existing_profile->EqualsForUpdatePurposes(profile)) {
      NotifyPersonalDataChanged();
      return;
    }
  }

  ongoing_profile_changes_[profile.guid()].push_back(
      AutofillProfileDeepChange(AutofillProfileChange::UPDATE, profile));
  if (enforced)
    ongoing_profile_changes_[profile.guid()].back().set_enforce_update();
  UpdateClientValidityStates(profile);
}

void PersonalDataManager::RemoveProfileFromDB(const std::string& guid) {
  bool profile_exists = FindByGUID<AutofillProfile>(web_profiles_, guid);
  if (!profile_exists && !ProfileChangesAreOnGoing(guid)) {
    NotifyPersonalDataChanged();
    return;
  }
  AutofillProfileDeepChange change(AutofillProfileChange::REMOVE, guid);
  if (!ProfileChangesAreOnGoing(guid)) {
    database_helper_->GetLocalDatabase()->RemoveAutofillProfile(guid);
    change.set_is_ongoing_on_background();
  }
  ongoing_profile_changes_[guid].push_back(std::move(change));
}

void PersonalDataManager::HandleNextProfileChange(const std::string& guid) {
  if (!ProfileChangesAreOnGoing(guid))
    return;

  const auto& change = ongoing_profile_changes_[guid].front();
  if (change.is_ongoing_on_background())
    return;

  const auto& change_type = change.type();
  const auto* existing_profile = GetProfileByGUID(guid);
  const bool profile_exists = (existing_profile != nullptr);
  const auto& profile = *(ongoing_profile_changes_[guid].front().profile());

  DCHECK(guid == profile.guid());

  if (change_type == AutofillProfileChange::REMOVE) {
    if (!profile_exists) {
      OnProfileChangeDone(guid);
      return;
    }
    database_helper_->GetLocalDatabase()->RemoveAutofillProfile(guid);
    change.set_is_ongoing_on_background();
    return;
  }

  if (!change.has_validation_effort_made())
    return;

  if (change_type == AutofillProfileChange::ADD) {
    if (profile_exists || FindByContents(web_profiles_, profile)) {
      OnProfileChangeDone(guid);
      return;
    }
    database_helper_->GetLocalDatabase()->AddAutofillProfile(profile);
    change.set_is_ongoing_on_background();
    return;
  }

  if (profile_exists && (change.enforce_update() ||
                         !existing_profile->EqualsForUpdatePurposes(profile))) {
    database_helper_->GetLocalDatabase()->UpdateAutofillProfile(profile);
    change.set_is_ongoing_on_background();
  } else {
    OnProfileChangeDone(guid);
  }
}

bool PersonalDataManager::ProfileChangesAreOnGoing(const std::string& guid) {
  return ongoing_profile_changes_.find(guid) !=
             ongoing_profile_changes_.end() &&
         !ongoing_profile_changes_[guid].empty();
}

bool PersonalDataManager::ProfileChangesAreOnGoing() {
  for (const auto& task : ongoing_profile_changes_) {
    if (ProfileChangesAreOnGoing(task.first)) {
      return true;
    }
  }
  return false;
}

void PersonalDataManager::OnProfileChangeDone(const std::string& guid) {
  ongoing_profile_changes_[guid].pop_front();

  if (!ProfileChangesAreOnGoing()) {
    Refresh();
  } else {
    NotifyPersonalDataChanged();
    HandleNextProfileChange(guid);
  }
}

void PersonalDataManager::ClearOnGoingProfileChanges() {
  ongoing_profile_changes_.clear();
}

}  // namespace autofill
