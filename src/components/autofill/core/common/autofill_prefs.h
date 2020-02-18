// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_

#include <string>

#include "google_apis/gaia/core_account_id.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace autofill {
namespace prefs {

// Alphabetical list of preference names specific to the Autofill
// component. Keep alphabetized, and document each in the .cc file.
extern const char kAutofillAcceptSaveCreditCardPromptState[];
// Do not get/set the value of this pref directly. Use provided getter/setter.
extern const char kAutofillCreditCardEnabled[];
extern const char kAutofillCreditCardFidoAuthEnabled[];
extern const char kAutofillCreditCardFidoAuthOfferCheckboxState[];
extern const char kAutofillCreditCardSigninPromoImpressionCount[];
// Please use kAutofillCreditCardEnabled and kAutofillProfileEnabled instead.
extern const char kAutofillEnabledDeprecated[];
extern const char kAutofillJapanCityFieldMigratedDeprecated[];
extern const char kAutofillLastVersionDeduped[];
extern const char kAutofillLastVersionValidated[];
extern const char kAutofillLastVersionDisusedAddressesDeleted[];
extern const char kAutofillLastVersionDisusedCreditCardsDeleted[];
extern const char kAutofillOrphanRowsRemoved[];
// Do not get/set the value of this pref directly. Use provided getter/setter.
extern const char kAutofillProfileEnabled[];
extern const char kAutofillProfileValidity[];
extern const char kAutofillSyncTransportOptIn[];
extern const char kAutofillUploadEncodingSeed[];
extern const char kAutofillUploadEvents[];
extern const char kAutofillUploadEventsLastResetTimestamp[];
extern const char kAutofillWalletImportEnabled[];
extern const char kAutofillWalletImportStorageCheckboxState[];
extern const char kAutocompleteLastVersionRetentionPolicy[];

namespace sync_transport_opt_in {
enum Flags {
  kWallet = 1 << 0,
};
}  // namespace sync_transport_opt_in

// Possible values for previous user decision when we displayed a save credit
// card prompt.
enum PreviousSaveCreditCardPromptUserDecision {
  PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_NONE,
  PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_ACCEPTED,
  PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISION_DENIED,
  NUM_PREVIOUS_SAVE_CREDIT_CARD_PROMPT_USER_DECISIONS
};

// Registers Autofill prefs.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Migrates deprecated Autofill prefs values.
void MigrateDeprecatedAutofillPrefs(PrefService* prefs);

bool IsAutocompleteEnabled(const PrefService* prefs);

bool IsCreditCardFIDOAuthEnabled(PrefService* prefs);

void SetCreditCardFIDOAuthEnabled(PrefService* prefs, bool enabled);

bool IsAutofillCreditCardEnabled(const PrefService* prefs);

void SetAutofillCreditCardEnabled(PrefService* prefs, bool enabled);

bool IsAutofillManaged(const PrefService* prefs);

bool IsAutofillProfileManaged(const PrefService* prefs);

bool IsAutofillCreditCardManaged(const PrefService* prefs);

bool IsAutofillProfileEnabled(const PrefService* prefs);

void SetAutofillProfileEnabled(PrefService* prefs, bool enabled);

bool IsPaymentsIntegrationEnabled(const PrefService* prefs);

void SetPaymentsIntegrationEnabled(PrefService* prefs, bool enabled);

std::string GetAllProfilesValidityMapsEncodedString(const PrefService* prefs);

void SetUserOptedInWalletSyncTransport(PrefService* prefs,
                                       const CoreAccountId& account_id,
                                       bool opted_in);

bool IsUserOptedInWalletSyncTransport(const PrefService* prefs,
                                      const CoreAccountId& account_id);

void ClearSyncTransportOptIns(PrefService* prefs);

}  // namespace prefs
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PREFS_H_
