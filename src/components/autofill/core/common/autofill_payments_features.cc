// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_payments_features.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {
namespace features {

const base::Feature kAutofillCreditCardAblationExperiment{
    "AutofillCreditCardAblationExperiment", base::FEATURE_DISABLED_BY_DEFAULT};

// Enables the use of platform authenticators through WebAuthn to retrieve
// credit cards from Google payments.
const base::Feature kAutofillCreditCardAuthentication{
    "AutofillCreditCardAuthentication", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillCreditCardLocalCardMigration{
    "AutofillCreditCardLocalCardMigration", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kAutofillDoNotUploadSaveUnsupportedCards{
    "AutofillDoNotUploadSaveUnsupportedCards",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the credit card downstream keyboard accessory shows
// the Google Pay logo animation on iOS.
const base::Feature kAutofillDownstreamUseGooglePayBrandingOniOS{
    "AutofillDownstreamUseGooglePayBrandingOniOS",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, enable local card migration flow for user who has signed in but
// has not turned on sync.
const base::Feature kAutofillEnableLocalCardMigrationForNonSyncUser{
    "AutofillEnableLocalCardMigrationForNonSyncUser",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, Autofill data related icons will be shown in the status
// chip in toolbar along with the avatar toolbar button.
const base::Feature kAutofillEnableToolbarStatusChip{
    "AutofillEnableToolbarStatusChip", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, autofill can import credit cards from dynamic change form.
const base::Feature kAutofillImportDynamicForms{
    "AutofillImportDynamicForms", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, a credit card form that is hidden after receiving input can
// import the card.
const base::Feature kAutofillImportNonFocusableCreditCardForms{
    "AutofillImportNonFocusableCreditCardForms",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, the local card migration dialog will show the progress
// and result of the migration after starting the migration. When disabled,
// there is no feedback for the migration.
const base::Feature kAutofillLocalCardMigrationShowFeedback{
    "AutofillLocalCardMigrationShowFeedback", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether offering to migrate cards will consider data from the
// Autofill strike database (new version).
const base::Feature kAutofillLocalCardMigrationUsesStrikeSystemV2{
    "AutofillLocalCardMigrationUsesStrikeSystemV2",
    base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, will remove the option to save unmasked server cards as
// FULL_SERVER_CARDs upon successful unmask.
const base::Feature kAutofillNoLocalSaveOnUnmaskSuccess{
    "AutofillNoLocalSaveOnUnmaskSuccess", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, no local copy of server card will be saved when upload
// succeeds.
const base::Feature kAutofillNoLocalSaveOnUploadSuccess{
    "AutofillNoLocalSaveOnUploadSuccess", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, local and upload credit card save dialogs will be updated to
// new M72 guidelines, including a [No thanks] cancel button and an extended
// title string.
const base::Feature kAutofillSaveCardImprovedUserConsent{
    "AutofillSaveCardImprovedUserConsent", base::FEATURE_DISABLED_BY_DEFAULT};

// When enabled, a sign in promo will show up right after the user
// saves a card locally. This also introduces a "Manage Cards" bubble.
const base::Feature kAutofillSaveCardSignInAfterLocalSave{
    "AutofillSaveCardSignInAfterLocalSave", base::FEATURE_ENABLED_BY_DEFAULT};

// Controls whether offering to save cards will consider data from the Autofill
// strike database.
const base::Feature kAutofillSaveCreditCardUsesStrikeSystem{
    "AutofillSaveCreditCardUsesStrikeSystem",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether offering to save cards will consider data from the Autofill
// strike database (new version).
const base::Feature kAutofillSaveCreditCardUsesStrikeSystemV2{
    "AutofillSaveCreditCardUsesStrikeSystemV2",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether experiment ids should be sent through
// Google Payments RPCs or not.
const base::Feature kAutofillSendExperimentIdsInPaymentsRPCs{
    "AutofillSendExperimentIdsInPaymentsRPCs",
    base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, only countries of recently-used addresses are sent in the
// GetUploadDetails call to Payments. If disabled, whole recently-used addresses
// are sent.
const base::Feature kAutofillSendOnlyCountryInGetUploadDetails{
    "AutofillSendOnlyCountryInGetUploadDetails",
    base::FEATURE_ENABLED_BY_DEFAULT};

// Controls offering credit card upload to Google Payments. Cannot ever be
// ENABLED_BY_DEFAULT because it's a country-specific whitelist. There are
// countries we simply can't turn this on for, and they change over time, so
// it's important that we can flip a switch and be done instead of having old
// versions of Chrome forever do the wrong thing. Enabling it by default would
// mean that any first-run client without a Finch config won't get the
// overriding command to NOT turn it on, which becomes an issue.
const base::Feature kAutofillUpstream{"AutofillUpstream",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamAllowAllEmailDomains{
    "AutofillUpstreamAllowAllEmailDomains", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamAlwaysRequestCardholderName{
    "AutofillUpstreamAlwaysRequestCardholderName",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamBlankCardholderNameField{
    "AutofillUpstreamBlankCardholderNameField",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether ELO cards should be uploaded to Google Payments.
const base::Feature kAutofillUpstreamDisallowElo{
    "AutofillUpstreamDisallowElo", base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether JCB cards should be uploaded to Google Payments.
const base::Feature kAutofillUpstreamDisallowJcb{
    "AutofillUpstreamDisallowJcb", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamEditableCardholderName{
    "AutofillUpstreamEditableCardholderName",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kAutofillUpstreamEditableExpirationDate{
    "AutofillUpstreamEditableExpirationDate",
    base::FEATURE_DISABLED_BY_DEFAULT};

// Controls whether the PaymentsCustomerData is used to make requests to
// Google Payments.
const base::Feature kAutofillUsePaymentsCustomerData{
    "AutofillUsePaymentsCustomerData", base::FEATURE_ENABLED_BY_DEFAULT};

const char kAutofillCreditCardLocalCardMigrationParameterName[] = "variant";

const char kAutofillCreditCardLocalCardMigrationParameterWithoutSettingsPage[] =
    "without-settings-page";

LocalCardMigrationExperimentalFlag GetLocalCardMigrationExperimentalFlag() {
  if (!base::FeatureList::IsEnabled(kAutofillCreditCardLocalCardMigration))
    return LocalCardMigrationExperimentalFlag::kMigrationDisabled;

  std::string param = base::GetFieldTrialParamValueByFeature(
      kAutofillCreditCardLocalCardMigration,
      kAutofillCreditCardLocalCardMigrationParameterName);

  if (param ==
      kAutofillCreditCardLocalCardMigrationParameterWithoutSettingsPage) {
    return LocalCardMigrationExperimentalFlag::kMigrationWithoutSettingsPage;
  }
  return LocalCardMigrationExperimentalFlag::kMigrationIncludeSettingsPage;
}

bool IsAutofillUpstreamAlwaysRequestCardholderNameExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillUpstreamAlwaysRequestCardholderName);
}

bool IsAutofillUpstreamBlankCardholderNameFieldExperimentEnabled() {
  return base::FeatureList::IsEnabled(
      features::kAutofillUpstreamBlankCardholderNameField);
}

bool IsAutofillUpstreamEditableCardholderNameExperimentEnabled() {
  return base::FeatureList::IsEnabled(kAutofillUpstreamEditableCardholderName);
}

}  // namespace features
}  // namespace autofill
