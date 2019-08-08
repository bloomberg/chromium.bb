// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {
struct Feature;
}

namespace autofill {
namespace features {

// All features in alphabetical order.
extern const base::Feature kAutofillCreditCardAblationExperiment;
extern const base::Feature kAutofillCreditCardAuthentication;
extern const base::Feature kAutofillDoNotMigrateUnsupportedLocalCards;
extern const base::Feature kAutofillDoNotUploadSaveUnsupportedCards;
extern const base::Feature kAutofillDownstreamUseGooglePayBrandingOniOS;
extern const base::Feature kAutofillEnableLocalCardMigrationForNonSyncUser;
extern const base::Feature kAutofillEnableToolbarStatusChip;
extern const base::Feature kAutofillImportDynamicForms;
extern const base::Feature kAutofillImportNonFocusableCreditCardForms;
extern const base::Feature kAutofillLocalCardMigrationUsesStrikeSystemV2;
extern const base::Feature kAutofillNoLocalSaveOnUnmaskSuccess;
extern const base::Feature kAutofillNoLocalSaveOnUploadSuccess;
extern const base::Feature kAutofillSaveCardImprovedUserConsent;
extern const base::Feature kAutofillSaveCreditCardUsesImprovedMessaging;
extern const base::Feature kAutofillSendExperimentIdsInPaymentsRPCs;
extern const base::Feature kAutofillSendOnlyCountryInGetUploadDetails;
extern const base::Feature kAutofillUpstream;
extern const base::Feature kAutofillUpstreamAllowAllEmailDomains;
extern const base::Feature kAutofillUpstreamAlwaysRequestCardholderName;
extern const base::Feature kAutofillUpstreamBlankCardholderNameField;
extern const base::Feature kAutofillUpstreamDisallowElo;
extern const base::Feature kAutofillUpstreamDisallowJcb;
extern const base::Feature kAutofillUpstreamEditableCardholderName;
extern const base::Feature kAutofillUpstreamEditableExpirationDate;
extern const base::Feature kAutofillUsePaymentsCustomerData;

extern const char kAutofillSaveCreditCardUsesImprovedMessagingParamName[];
extern const char
    kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreCard[];
extern const char
    kAutofillSaveCreditCardUsesImprovedMessagingParamValueStoreBillingDetails[];
extern const char
    kAutofillSaveCreditCardUsesImprovedMessagingParamValueAddCard[];
extern const char
    kAutofillSaveCreditCardUsesImprovedMessagingParamValueConfirmAndSaveCard[];

// For testing purposes; not to be launched.  When enabled, Chrome Upstream
// always requests that the user enters/confirms cardholder name in the
// offer-to-save dialog, regardless of if it was present or if the user is a
// Google Payments customer.  Note that this will override the detected
// cardholder name, if one was found.
bool IsAutofillUpstreamAlwaysRequestCardholderNameExperimentEnabled();

// For experimental purposes; not to be made available in chrome://flags. When
// enabled and Chrome Upstream requests the cardholder name in the offer-to-save
// dialog, the field will be blank instead of being prefilled with the name from
// the user's Google Account.
bool IsAutofillUpstreamBlankCardholderNameFieldExperimentEnabled();

// Returns whether the experiment is enabled where Chrome Upstream can request
// the user to enter/confirm cardholder name in the offer-to-save bubble if it
// was not detected or was conflicting during the checkout flow and the user is
// NOT a Google Payments customer.
bool IsAutofillUpstreamEditableCardholderNameExperimentEnabled();

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
