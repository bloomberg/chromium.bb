// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_

#include <deque>
#include <list>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_profile_save_strike_database.h"
#include "components/autofill/core/browser/autofill_profile_update_strike_database.h"
#include "components/autofill/core/browser/autofill_profile_validator.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/alternative_state_name_map_updater.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager_cleaner.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/strike_database_base.h"
#include "components/autofill/core/browser/sync_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/webdata/common/web_data_service_consumer.h"

class Profile;
class PrefService;
class RemoveAutofillTester;

namespace autofill {
class AutofillImageFetcher;
class AutofillInteractiveTest;
struct CreditCardArtImage;
class PersonalDataManagerObserver;
class PersonalDataManagerFactory;
class PersonalDatabaseHelper;
}  // namespace autofill

namespace autofill_helper {
void SetProfiles(int, std::vector<autofill::AutofillProfile>*);
void SetCreditCards(int, std::vector<autofill::CreditCard>*);
}  // namespace autofill_helper

namespace syncer {
class SyncService;
}  // namespace syncer

namespace autofill {

// Handles loading and saving Autofill profile information to the web database.
// This class also stores the profiles loaded from the database for use during
// Autofill.
class PersonalDataManager : public KeyedService,
                            public WebDataServiceConsumer,
                            public AutofillWebDataServiceObserverOnUISequence,
                            public history::HistoryServiceObserver,
                            public syncer::SyncServiceObserver,
                            public signin::IdentityManager::Observer,
                            public AccountInfoGetter {
 public:
  explicit PersonalDataManager(const std::string& app_locale);
  PersonalDataManager(const std::string& app_locale,
                      const std::string& country_code);
  PersonalDataManager(const PersonalDataManager&) = delete;
  PersonalDataManager& operator=(const PersonalDataManager&) = delete;
  ~PersonalDataManager() override;

  // Kicks off asynchronous loading of profiles and credit cards.
  // |profile_database| is a profile-scoped database that will be used to save
  // local cards. |account_database| is scoped to the currently signed-in
  // account, and is wiped on signout and browser exit. This can be a nullptr
  // if personal_data_manager should use |profile_database| for all data.
  // If passed in, the |account_database| is used by default for server cards.
  // |pref_service| must outlive this instance. |image_fetcher| is to fetch the
  // customized images for autofill data. |is_off_the_record| informs this
  // instance whether the user is currently operating in an off-the-record
  // context.
  void Init(scoped_refptr<AutofillWebDataService> profile_database,
            scoped_refptr<AutofillWebDataService> account_database,
            PrefService* pref_service,
            PrefService* local_state,
            signin::IdentityManager* identity_manager,
            AutofillProfileValidator* client_profile_validator,
            history::HistoryService* history_service,
            StrikeDatabaseBase* strike_database,
            AutofillImageFetcher* image_fetcher,
            bool is_off_the_record);

  // KeyedService:
  void Shutdown() override;

  // Wires the circular sync service dependency. |sync_service| is either null
  // (sync disabled by CLI) or outlives this object. This method is called once
  // in production code, but may be called again in tests to replace the real
  // service with a stub. |sync_service| may not have started yet but its
  // preferences can already be queried.
  virtual void OnSyncServiceInitialized(syncer::SyncService* sync_service);

  // history::HistoryServiceObserver
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // AutofillWebDataServiceObserverOnUISequence:
  void AutofillMultipleChangedBySync() override;
  void AutofillAddressConversionCompleted() override;
  void SyncStarted(syncer::ModelType model_type) override;

  // SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // AccountInfoGetter:
  CoreAccountInfo GetAccountInfoForPaymentsServer() const override;
  bool IsSyncFeatureEnabled() const override;

  // signin::IdentityManager::Observer:
  void OnAccountsCookieDeletedByUserAction() override;

  // Returns the current sync status.
  virtual AutofillSyncSigninState GetSyncSigninState() const;

  // Adds a listener to be notified of PersonalDataManager events.
  virtual void AddObserver(PersonalDataManagerObserver* observer);

  // Removes |observer| as an observer of this PersonalDataManager.
  virtual void RemoveObserver(PersonalDataManagerObserver* observer);

  // Notifies test observers that an address or credit card could not be
  // imported from a form.
  void MarkObserversInsufficientFormDataForImport();

  // Called to indicate |profile_or_credit_card| was used (to fill in a form).
  // Updates the database accordingly.
  virtual void RecordUseOf(
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card);

  // Saves |imported_profile| to the WebDB if it exists. Returns the guid of
  // the new or updated profile, or the empty string if no profile was saved.
  virtual std::string SaveImportedProfile(
      const AutofillProfile& imported_profile);

  // Called when the user accepts the prompt to save the credit card locally.
  // Records some metrics and attempts to save the imported card. Returns the
  // guid of the new or updated card, or the empty string if no card was saved.
  std::string OnAcceptedLocalCreditCardSave(
      const CreditCard& imported_credit_card);

  // Triggered when the user accepts saving a UPI ID. Stores the |upi_id| to
  // the database.
  virtual void AddUpiId(const std::string& upi_id);

  // Returns all the stored UPI IDs.
  virtual std::vector<std::string> GetUpiIds();

  // Adds |profile| to the web database.
  virtual void AddProfile(const AutofillProfile& profile);

  // Updates |profile| which already exists in the web database.
  virtual void UpdateProfile(const AutofillProfile& profile);

  // Removes the profile or credit card represented by |guid|.
  virtual void RemoveByGUID(const std::string& guid);

  // Returns the profile with the specified |guid|, or nullptr if there is no
  // profile with the specified |guid|.
  virtual AutofillProfile* GetProfileByGUID(const std::string& guid);

  // Returns the profile with the specified |guid| from the given |profiles|, or
  // nullptr if there is no profile with the specified |guid|.
  static AutofillProfile* GetProfileFromProfilesByGUID(
      const std::string& guid,
      const std::vector<AutofillProfile*>& profiles);

  // Adds |credit_card| to the web database as a local card.
  virtual void AddCreditCard(const CreditCard& credit_card);

  // Delete list of provided credit cards.
  virtual void DeleteLocalCreditCards(const std::vector<CreditCard>& cards);

  // Updates |credit_card| which already exists in the web database. This
  // can only be used on local credit cards.
  virtual void UpdateCreditCard(const CreditCard& credit_card);

  // Adds |credit_card| to the web database as a full server card.
  virtual void AddFullServerCreditCard(const CreditCard& credit_card);

  // Update a server card. Only the full number and masked/unmasked
  // status can be changed. Looks up the card by server ID.
  virtual void UpdateServerCreditCard(const CreditCard& credit_card);

  // Updates the use stats and billing address id for the server |credit_cards|.
  // Looks up the cards by server_id.
  virtual void UpdateServerCardsMetadata(
      const std::vector<CreditCard>& credit_cards);

  // Resets the card for |guid| to the masked state.
  void ResetFullServerCard(const std::string& guid);

  // Resets all unmasked cards to the masked state.
  void ResetFullServerCards();

  // Deletes all server profiles and cards (both masked and unmasked).
  void ClearAllServerData();

  // Deletes all local profiles and cards.
  virtual void ClearAllLocalData();

  // Sets a server credit card for test.
  void AddServerCreditCardForTest(std::unique_ptr<CreditCard> credit_card);

  // Returns whether server credit cards are stored in account (i.e. ephemeral)
  // storage.
  bool IsUsingAccountStorageForServerDataForTest() const;

  // Adds the offer data to local cache for tests. This does not affect data in
  // the real database.
  void AddOfferDataForTest(std::unique_ptr<AutofillOfferData> offer_data);

  // Returns the credit card with the specified |guid|, or nullptr if there is
  // no credit card with the specified |guid|.
  virtual CreditCard* GetCreditCardByGUID(const std::string& guid);

  // Returns the credit card with the specified |number|, or nullptr if there is
  // no credit card with the specified |number|.
  virtual CreditCard* GetCreditCardByNumber(const std::string& number);

  // Returns the credit card with the specified |instrument_id|, or nullptr if
  // there is no credit card with the specified |instrument_id|.
  CreditCard* GetCreditCardByInstrumentId(int64_t instrument_id);

  // Gets the field types available in the stored address and credit card data.
  void GetNonEmptyTypes(ServerFieldTypeSet* non_empty_types) const;

  // Returns whether the personal data has been loaded from the web database.
  virtual bool IsDataLoaded() const;

  // This PersonalDataManager owns these profiles and credit cards.  Their
  // lifetime is until the web database is updated with new profile and credit
  // card information, respectively.
  virtual std::vector<AutofillProfile*> GetProfiles() const;
  // Returns just SERVER_PROFILES.
  virtual std::vector<AutofillProfile*> GetServerProfiles() const;
  // Returns just LOCAL_CARD cards.
  virtual std::vector<CreditCard*> GetLocalCreditCards() const;
  // Returns just server cards.
  virtual std::vector<CreditCard*> GetServerCreditCards() const;
  // Returns all credit cards, server and local.
  virtual std::vector<CreditCard*> GetCreditCards() const;

  // Returns the Payments customer data. Returns nullptr if no data is present.
  virtual PaymentsCustomerData* GetPaymentsCustomerData() const;

  // Returns the credit card cloud token data.
  virtual std::vector<CreditCardCloudTokenData*> GetCreditCardCloudTokenData()
      const;

  // Returns autofill offer data, including card-linked and promo code offers.
  virtual std::vector<AutofillOfferData*> GetAutofillOffers() const;

  // Returns autofill offer data, but only promo code offers that are not
  // expired and that are for the given |origin|.
  std::vector<const AutofillOfferData*>
  GetActiveAutofillPromoCodeOffersForOrigin(GURL origin) const;

  // Returns the customized credit card art image for the |card_art_url|.
  virtual gfx::Image* GetCreditCardArtImageForUrl(
      const GURL& card_art_url) const;

  // Updates the validity states of |profiles| according to server validity map.
  void UpdateProfilesServerValidityMapsIfNeeded(
      const std::vector<AutofillProfile*>& profiles);

  // Requests an update for the validity states of the |profiles| according to
  // client side validation API: |client_profile_validator_|.
  void UpdateClientValidityStates(
      const std::vector<AutofillProfile*>& profiles);

  // Requests an update for the validity states of the |profile| according to
  // the client side validation API: |client_profile_validator_|. Returns true
  // if the validation was requested.
  bool UpdateClientValidityStates(const AutofillProfile& profile);

  // Returns the profiles to suggest to the user, ordered by frecency.
  std::vector<AutofillProfile*> GetProfilesToSuggest() const;

  // Returns Suggestions corresponding to the focused field's |type| and
  // |field_contents|, i.e. what the user has typed. |field_is_autofilled| is
  // true if the field has already been autofilled, and |field_types| stores the
  // types of all the form's input fields, including the field with which the
  // user is interacting.
  // TODO(crbug.com/1210112): Move profile suggestion creation to
  // suggestion_generator.
  std::vector<Suggestion> GetProfileSuggestions(
      const AutofillType& type,
      const std::u16string& field_contents,
      bool field_is_autofilled,
      const std::vector<ServerFieldType>& field_types);

  // Returns the credit cards to suggest to the user. Those have been deduped
  // and ordered by frecency with the expired cards put at the end of the
  // vector. If |include_server_cards| is false, server side cards should not
  // be included.
  const std::vector<CreditCard*> GetCreditCardsToSuggest(
      bool include_server_cards) const;

  // Re-loads profiles and credit cards from the WebDatabase asynchronously.
  // In the general case, this is a no-op and will re-create the same
  // in-memory model as existed prior to the call.  If any change occurred to
  // profiles in the WebDatabase directly, as is the case if the browser sync
  // engine processed a change from the cloud, we will learn of these as a
  // result of this call.
  //
  // Also see SetProfile for more details.
  virtual void Refresh();

  // Returns the |app_locale_| that was provided during construction.
  const std::string& app_locale() const { return app_locale_; }

#ifdef UNIT_TEST
  // Returns the country code that was provided from the variations service
  // during construction.
  const std::string& variations_country_code_for_testing() const {
    return variations_country_code_;
  }

  // Sets the country code from the variations service.
  void set_variations_country_code_for_testing(std::string country_code) {
    variations_country_code_ = country_code;
  }

  // Returns the raw pointer to PersonalDataManagerCleaner used for testing
  // purposes.
  PersonalDataManagerCleaner* personal_data_manager_cleaner_for_testing()
      const {
    DCHECK(personal_data_manager_cleaner_);
    return personal_data_manager_cleaner_.get();
  }
#endif

  // Returns our best guess for the country a user is likely to use when
  // inputting a new address. The value is calculated once and cached, so it
  // will only update when Chrome is restarted.
  virtual const std::string& GetDefaultCountryCodeForNewAddress() const;

  // Returns our best guess for the country a user is in, for experiment group
  // purposes. The value is calculated once and cached, so it will only update
  // when Chrome is restarted.
  virtual const std::string& GetCountryCodeForExperimentGroup() const;

  // De-dupe credit card to suggest. Full server cards are preferred over their
  // local duplicates, and local cards are preferred over their masked server
  // card duplicate.
  static void DedupeCreditCardToSuggest(
      std::list<CreditCard*>* cards_to_suggest);

  // Cancels any pending queries to the server web database.
  void CancelPendingServerQueries();

  // Returns if there are any pending queries to the web database.
  bool HasPendingQueriesForTesting();

  // This function assumes |credit_card| contains the full PAN. Returns |true|
  // if the card number of |credit_card| is equal to any local card or any
  // unmasked server card known by the browser, or |TypeAndLastFourDigits| of
  // |credit_card| is equal to any masked server card known by the browser.
  bool IsKnownCard(const CreditCard& credit_card) const;

  // Check whether a card is a server card or has a duplicated server card.
  bool IsServerCard(const CreditCard* credit_card) const;

  // Sets the value that can skip the checks to see if we are syncing in a test.
  void SetSyncingForTest(bool is_syncing_for_test) {
    is_syncing_for_test_ = is_syncing_for_test;
  }

  // Returns whether a row to give the option of showing cards from the user's
  // account should be shown in the dropdown.
  bool ShouldShowCardsFromAccountOption() const;

  // Triggered when a user selects the option to see cards from their account.
  // Records the sync transport consent.
  void OnUserAcceptedCardsFromAccountOption();

  // Logs the fact that the server card link was clicked including information
  // about the current sync state.
  void LogServerCardLinkClicked() const;

  // Records the sync transport consent if the user is in sync transport mode.
  virtual void OnUserAcceptedUpstreamOffer();

  // Notifies observers that the waiting should be stopped.
  void NotifyPersonalDataObserver();

  // Called when at least one (can be multiple) card was saved. |is_local_card|
  // indicates if the card is saved to local storage.
  void OnCreditCardSaved(bool is_local_card);

  // Returns true if either Profile or CreditCard Autofill is enabled.
  virtual bool IsAutofillEnabled() const;

  // Returns the value of the AutofillProfileEnabled pref.
  virtual bool IsAutofillProfileEnabled() const;

  // Returns the value of the AutofillCreditCardEnabled pref.
  virtual bool IsAutofillCreditCardEnabled() const;

  // Returns the value of the AutofillWalletImportEnabled pref.
  virtual bool IsAutofillWalletImportEnabled() const;

  void set_client_profile_validator_for_test(
      AutofillProfileValidator* validator) {
    client_profile_validator_ = validator;
  }

  // Returns true if the PDM is in the off-the-record mode.
  bool IsOffTheRecord() { return is_off_the_record_; }

  // Sets |web_profiles_| to the contents of |profiles| and updates the web
  // database by adding, updating and removing profiles. |web_profiles_| need to
  // be updated at the end of the function, since some tasks cannot tolerate
  // database delays.
  virtual void SetProfiles(std::vector<AutofillProfile>* profiles);

  // Returns true if the import of new profiles should be blocked on `url`.
  // Returns false if the strike database is not available, the `url` is not
  // valid or has no host.
  bool IsNewProfileImportBlockedForDomain(const GURL& url) const;

  // Add a strike for blocking the import of new profiles on `url`.
  // Does nothing if the strike database is not available, the `url` is not
  // valid or has no host.
  void AddStrikeToBlockNewProfileImportForDomain(const GURL& url);

  // Removes potential strikes for the import of new profiles from `url`.
  // Does nothing if the strike database is not available, the `url` is not
  // valid or has no host.
  void RemoveStrikesToBlockNewProfileImportForDomain(const GURL& url);

  // Returns true if a profile identified by its `guid` is blocked for updates.
  // Returns false if the database is not available.
  bool IsProfileUpdateBlocked(const std::string& guid) const;

  // Adds a strike to block a profile identified by its `guid` for updates.
  // Does nothing if the strike database is not available.
  void AddStrikeToBlockProfileUpdate(const std::string& guid);

  // Removes potential strikes to block a profile identified by its `guid` for
  // updates. Does nothing if the strike database is not available.
  void RemoveStrikesToBlockProfileUpdate(const std::string& guid);

  // Used to automatically import addresses without a prompt. Should only be
  // set to true in tests.
  void set_auto_accept_address_imports_for_testing(bool auto_accept) {
    auto_accept_address_imports_for_testing_ = auto_accept;
  }
  bool auto_accept_address_imports_for_testing() {
    return auto_accept_address_imports_for_testing_;
  }

 protected:
  // Only PersonalDataManagerFactory and certain tests can create instances of
  // PersonalDataManager.
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddProfile_CrazyCharacters);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddProfile_Invalid);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           AddCreditCard_CrazyCharacters);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, AddCreditCard_Invalid);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           AddAndGetCreditCardArtImage);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DedupeProfiles_ProfilesToDelete);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DedupeProfiles_GuidsMergeMap);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           UpdateCardsBillingAddressReference);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_CardsBillingAddressIdUpdated);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MergedProfileValues);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileFirst);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_VerifiedProfileLast);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleVerifiedProfiles);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_FeatureDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_NopIfZeroProfiles);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_NopIfOneProfile);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_OncePerVersion);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           ApplyDedupingRoutine_MultipleDedupes);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_NewProfile);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_MergedProfile);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_NewCrd_AddressAlreadyConverted);  // NOLINT
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_AlreadyConverted);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      ConvertWalletAddressesAndUpdateWalletCards_MultipleSimilarWalletAddresses);  // NOLINT
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DoNotConvertWalletAddressesInEphemeralStorage);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           DeleteDisusedCreditCards_DoNothingWhenDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      DeleteDisusedCreditCards_OnlyDeleteExpiredDisusedLocalCards);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_ProfileAutofillDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_NoProfilesLoadedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_NoProfilesAddedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardsToSuggest_CreditCardAutofillDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetCreditCardsToSuggest_NoCardsLoadedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(
      PersonalDataManagerTest,
      GetCreditCardsToSuggest_NoCreditCardsAddedIfDisabled);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           RequestProfileServerValidity);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest,
                           GetProfileSuggestions_Validity);
  FRIEND_TEST_ALL_PREFIXES(PersonalDataManagerTest, LogStoredCreditCardMetrics);

  friend class autofill::AutofillInteractiveTest;
  friend class autofill::PersonalDataManagerCleaner;
  friend class autofill::PersonalDataManagerFactory;
  friend class AutofillMetricsTest;
  friend class FormDataImporterTest;
  friend class PersonalDataManagerTest;
  friend class PersonalDataManagerTestBase;
  friend class PersonalDataManagerHelper;
  friend class PersonalDataManagerMockTest;
  friend class SaveImportedProfileTest;
  friend class ::RemoveAutofillTester;
  friend std::default_delete<PersonalDataManager>;
  friend void autofill_helper::SetProfiles(
      int,
      std::vector<autofill::AutofillProfile>*);
  friend void autofill_helper::SetCreditCards(
      int,
      std::vector<autofill::CreditCard>*);
  friend void SetTestProfiles(Profile* base_profile,
                              std::vector<AutofillProfile>* profiles);

  // Used to get a pointer to the strike database for importing new profiles.
  // Note, the result can be a nullptr.
  AutofillProfileSaveStrikeDatabase* GetProfileSaveStrikeDatabase();
  virtual const AutofillProfileSaveStrikeDatabase*
  GetProfileSaveStrikeDatabase() const;

  // Used to get a pointer to the strike database for updating existing
  // profiles. Note, the result can be a nullptr.
  AutofillProfileUpdateStrikeDatabase* GetProfileUpdateStrikeDatabase();
  virtual const AutofillProfileUpdateStrikeDatabase*
  GetProfileUpdateStrikeDatabase() const;

  // Sets |credit_cards_| to the contents of |credit_cards| and updates the web
  // database by adding, updating and removing credit cards.
  void SetCreditCards(std::vector<CreditCard>* credit_cards);

  // Loads the saved profiles from the web database.
  virtual void LoadProfiles();

  // Loads the saved credit cards from the web database.
  virtual void LoadCreditCards();

  // Loads the saved credit card cloud token data from the web database.
  virtual void LoadCreditCardCloudTokenData();

  // Loads the payments customer data from the web database.
  virtual void LoadPaymentsCustomerData();

  // Loads the saved UPI IDs from the web database.
  virtual void LoadUpiIds();

  // Loads the autofill offer data from the web database.
  virtual void LoadAutofillOffers();

  // Cancels a pending query to the local web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingLocalQuery(WebDataServiceBase::Handle* handle);

  // Cancels a pending query to the server web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingServerQuery(WebDataServiceBase::Handle* handle);

  // The first time this is called, logs a UMA metrics about the user's autofill
  // addresses. On subsequent calls, does nothing.
  void LogStoredProfileMetrics() const;

  // The first time this is called, logs an UMA metric about the user's autofill
  // credit cards. On subsequent calls, does nothing.
  void LogStoredCreditCardMetrics() const;

  // The first time this is called, logs UMA metrics about the users's autofill
  // offer data. On subsequent calls, does nothing.
  void LogStoredOfferMetrics() const;

  // Whether the server cards are enabled and should be suggested to the user.
  virtual bool ShouldSuggestServerCards() const;

  // Overrideable for testing.
  virtual std::string CountryCodeForCurrentTimezone() const;

  // Sets which PrefService to use and observe. |pref_service| is not owned by
  // this class and must outlive |this|.
  void SetPrefService(PrefService* pref_service);

  // Called when the |profile| is validated by the AutofillProfileValidator,
  // updates the profiles on the |ongoing_profile_changes_| and the DB.
  virtual void OnValidated(const AutofillProfile* profile);

  // Get the profiles fields validity map by |guid|.
  const ProfileValidityMap& GetProfileValidityByGUID(const std::string& guid);

  // Asks AutofillImageFetcher to fetch images.
  virtual void FetchImagesForUrls(const std::vector<GURL>& updated_urls) const;

  // Decides which database type to use for server and local cards.
  std::unique_ptr<PersonalDatabaseHelper> database_helper_;

  // True if personal data has been loaded from the web database.
  bool is_data_loaded_ = false;

  // The loaded web profiles. These are constructed from entries on web pages
  // and from manually editing in the settings.
  std::vector<std::unique_ptr<AutofillProfile>> web_profiles_;

  // Profiles read from the user's account stored on the server.
  std::vector<std::unique_ptr<AutofillProfile>> server_profiles_;

  // Stores the PaymentsCustomerData obtained from the database.
  std::unique_ptr<PaymentsCustomerData> payments_customer_data_;

  // Cached versions of the local and server credit cards.
  std::vector<std::unique_ptr<CreditCard>> local_credit_cards_;
  std::vector<std::unique_ptr<CreditCard>> server_credit_cards_;

  // Cached UPI IDs.
  std::vector<std::string> upi_ids_;

  // Cached version of the CreditCardCloudTokenData obtained from the database.
  std::vector<std::unique_ptr<CreditCardCloudTokenData>>
      server_credit_card_cloud_token_data_;

  // Autofill offer data, including card-linked offers for the user's credit
  // cards as well as promo code offers.
  std::vector<std::unique_ptr<AutofillOfferData>> autofill_offer_data_;

  // The customized card art images for the URL.
  std::map<GURL, std::unique_ptr<gfx::Image>> credit_card_art_images_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another sequence, we record the query handle until we
  // get called back.  We store handles for both profile and credit card queries
  // so they can be loaded at the same time.
  WebDataServiceBase::Handle pending_profiles_query_ = 0;
  WebDataServiceBase::Handle pending_server_profiles_query_ = 0;
  WebDataServiceBase::Handle pending_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcard_cloud_token_data_query_ =
      0;
  WebDataServiceBase::Handle pending_customer_data_query_ = 0;
  WebDataServiceBase::Handle pending_upi_ids_query_ = 0;
  WebDataServiceBase::Handle pending_offer_data_query_ = 0;

  // The observers.
  base::ObserverList<PersonalDataManagerObserver>::Unchecked observers_;

  // Used to populate AlternativeStateNameMap with the geographical state data
  // (including their abbreviations and localized names).
  std::unique_ptr<AlternativeStateNameMapUpdater>
      alternative_state_name_map_updater_;

  // |profile_valditiies_need_update| whenever the profile validities are out of
  bool profiles_server_validities_need_update_ = true;

 private:
  // Saves |imported_credit_card| to the WebDB if it exists. Returns the guid of
  // the new or updated card, or the empty string if no card was saved.
  virtual std::string SaveImportedCreditCard(
      const CreditCard& imported_credit_card);

  // Finds the country code that occurs most frequently among all profiles.
  // Prefers verified profiles over unverified ones.
  std::string MostCommonCountryCodeFromProfiles() const;

  // Called when the value of prefs::kAutofillWalletImportEnabled changes.
  void EnableWalletIntegrationPrefChanged();

  // Called when the value of prefs::kAutofillCreditCardEnabled or
  // prefs::kAutofillProfileEnabled changes.
  void EnableAutofillPrefChanged();

  // Converts the Wallet addresses to local autofill profiles. This should be
  // called after all the syncable data has been processed (local cards and
  // profiles, Wallet data and metadata). Also updates Wallet cards' billing
  // address id to point to the local profiles.
  void ConvertWalletAddressesAndUpdateWalletCards();

  // Converts the Wallet addresses into local profiles either by merging with an
  // existing |local_profiles| of by adding a new one. Populates the
  // |server_id_profiles_map| to be used when updating cards where the address
  // was already converted. Also populates the |guids_merge_map| to keep the
  // link between the Wallet address and the equivalent local profile (from
  // merge or creation).
  bool ConvertWalletAddressesToLocalProfiles(
      std::vector<AutofillProfile>* local_profiles,
      std::unordered_map<std::string, AutofillProfile*>* server_id_profiles_map,
      std::unordered_map<std::string, std::string>* guids_merge_map);

  // Goes through the Wallet cards to find cards where the billing address is a
  // Wallet address which was already converted in a previous pass. Looks for a
  // matching local profile and updates the |guids_merge_map| to make the card
  // refer to it.
  bool UpdateWalletCardsAlreadyConvertedBillingAddresses(
      const std::vector<AutofillProfile>& local_profiles,
      const std::unordered_map<std::string, AutofillProfile*>&
          server_id_profiles_map,
      std::unordered_map<std::string, std::string>* guids_merge_map) const;

  // Removes profile from web database according to |guid| and resets credit
  // card's billing address if that address is used by any credit cards.
  // The method does not refresh, this allows multiple removal with one
  // refreshing in the end.
  void RemoveAutofillProfileByGUIDAndBlankCreditCardReference(
      const std::string& guid);

  // Resets |synced_profile_validity_|.
  void ResetProfileValidity();

  // Add/Update/Remove |profile| on DB. |enforced| should be true when the
  // add/update should happen regardless of an existing/equal profile.
  void AddProfileToDB(const AutofillProfile& profile, bool enforced = false);
  void UpdateProfileInDB(const AutofillProfile& profile, bool enforced = false);
  void RemoveProfileFromDB(const std::string& guid);

  // Triggered when a profile is added/updated/removed on db.
  void OnAutofillProfileChanged(const AutofillProfileDeepChange& change);

  // Triggered when all the card art image fetches have been completed,
  // regardless of whether all of them succeeded.
  void OnCardArtImagesFetched(
      std::vector<std::unique_ptr<CreditCardArtImage>> art_images);

  // Look at the next profile change for profile with guid = |guid|, and handle
  // it.
  void HandleNextProfileChange(const std::string& guid);
  // returns true if there is any profile change that's still ongoing.
  bool ProfileChangesAreOngoing();
  // returns true if there is any ongoing change for profile with guid = |guid|
  // that's still ongoing.
  bool ProfileChangesAreOngoing(const std::string& guid);
  // Remove the change from the |ongoing_profile_changes_|, handle next task or
  // Refresh.
  void OnProfileChangeDone(const std::string& guid);
  // Clear |ongoing_profile_changes_|.
  void ClearOnGoingProfileChanges();

  // Returns if there are any pending queries to the web database.
  bool HasPendingQueries();

  // Migrates the user opted in to wallet sync transport. This is needed while
  // migrating from using email to Gaia ID as th account identifier.
  void MigrateUserOptedInWalletSyncTransportIfNeeded();

  // Returns true if the sync is enabled for |model_type|.
  bool IsSyncEnabledFor(syncer::ModelType model_type);

  // Returns the database that is used for storing local data.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase();

  // Invoked when server credit card cache is refreshed.
  void OnServerCreditCardsRefreshed();

  // Checks whether any virtual card metadata for server cards is new and makes
  // corresponding changes.
  void ProcessVirtualCardMetadataChanges();

  // Returns the number of server credit cards that have a valid credit card art
  // image.
  size_t GetServerCardWithArtImageCount() const;

  // Stores the |app_locale| supplied on construction.
  const std::string app_locale_;

  // Stores the country code that was provided from the variations service
  // during construction.
  std::string variations_country_code_;

  // If true, new addresses imports are automatically accepted without a prompt.
  // Only to be used for testing.
  bool auto_accept_address_imports_for_testing_ = false;

  // The default country code for new addresses.
  mutable std::string default_country_code_;

  // The determined country code for experiment group purposes. Uses
  // |variations_country_code_| if it exists but falls back to other methods if
  // necessary to ensure it always has a value.
  mutable std::string experiment_country_code_;

  // The PrefService that this instance uses. Must outlive this instance.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // The HistoryService to be observed by the personal data manager. Must
  // outlive this instance. This unowned pointer is retained so the PDM can
  // remove itself from the history service's observer list on shutdown.
  raw_ptr<history::HistoryService> history_service_ = nullptr;

  // Pref registrar for managing the change observers.
  PrefChangeRegistrar pref_registrar_;

  // Profiles validity read from the prefs. They are kept updated by
  // observing changes in pref_services. We need to set the
  // |profile_validities_need_update_| whenever this is changed.
  std::unique_ptr<UserProfileValidityMap> synced_profile_validity_;

  // PersonalDataManagerCleaner is used to apply various address and credit
  // card fixes/cleanups one time at browser startup or when the sync starts.
  // PersonalDataManagerCleaner is declared as a friend class.
  std::unique_ptr<PersonalDataManagerCleaner> personal_data_manager_cleaner_;

  // A timely ordered list of ongoing changes for each profile.
  std::unordered_map<std::string, std::deque<AutofillProfileDeepChange>>
      ongoing_profile_changes_;

  // The client side profile validator.
  raw_ptr<AutofillProfileValidator> client_profile_validator_ = nullptr;

  // The identity manager that this instance uses. Must outlive this instance.
  raw_ptr<signin::IdentityManager> identity_manager_ = nullptr;

  // The sync service this instances uses. Must outlive this instance.
  raw_ptr<syncer::SyncService> sync_service_ = nullptr;

  // The image fetcher to fetch customized images for Autofill data.
  raw_ptr<AutofillImageFetcher> image_fetcher_ = nullptr;

  // Whether the user is currently operating in an off-the-record context.
  // Default value is false.
  bool is_off_the_record_ = false;

  // Whether we have already logged the stored profile metrics this session.
  mutable bool has_logged_stored_profile_metrics_ = false;

  // Whether we have already logged the stored credit card metrics this session.
  mutable bool has_logged_stored_credit_card_metrics_ = false;

  // Whether we have already logged the stored offer metrics this session.
  mutable bool has_logged_stored_offer_metrics_ = false;

  // An observer to listen for changes to prefs::kAutofillCreditCardEnabled.
  std::unique_ptr<BooleanPrefMember> credit_card_enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillProfileEnabled.
  std::unique_ptr<BooleanPrefMember> profile_enabled_pref_;

  // An observer to listen for changes to prefs::kAutofillWalletImportEnabled.
  std::unique_ptr<BooleanPrefMember> wallet_enabled_pref_;

  // The database that is used to count domain-keyed strikes to suppress the
  // import of new profiles.
  std::unique_ptr<AutofillProfileSaveStrikeDatabase>
      profile_save_strike_database_;

  // The database that is used to count guid-keyed strikes to suppress updates
  // of existing profiles.
  std::unique_ptr<AutofillProfileUpdateStrikeDatabase>
      profile_update_strike_database_;

  // Whether sync should be considered on in a test.
  bool is_syncing_for_test_ = false;

  base::ScopedObservation<history::HistoryService, HistoryServiceObserver>
      history_service_observation_{this};

  base::WeakPtrFactory<PersonalDataManager> weak_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PERSONAL_DATA_MANAGER_H_
