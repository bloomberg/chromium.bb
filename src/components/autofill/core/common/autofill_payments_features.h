// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

namespace base {
struct Feature;
}

namespace autofill {
namespace features {

// All features in alphabetical order.
extern const base::Feature kAutofillAlwaysReturnCloudTokenizedCard;
extern const base::Feature kAutofillAutoTriggerManualFallbackForCards;
extern const base::Feature kAutofillCreditCardAuthentication;
extern const base::Feature kAutofillCreditCardUploadFeedback;
extern const base::Feature kAutofillEnableGoogleIssuedCard;
extern const base::Feature kAutofillEnableMerchantBoundVirtualCards;
extern const base::Feature kAutofillEnableOfferNotificationCrossTabTracking;
extern const base::Feature kAutofillEnableOfferNotificationForPromoCodes;
extern const base::Feature kAutofillEnableOffersInClankKeyboardAccessory;
extern const base::Feature kAutofillEnableOffersInDownstream;
extern const base::Feature kAutofillEnableStickyManualFallbackForCards;
extern const base::Feature kAutofillEnableToolbarStatusChip;
extern const base::Feature kAutofillEnableUpdateVirtualCardEnrollment;
extern const base::Feature kAutofillEnableVirtualCard;
extern const base::Feature kAutofillEnableVirtualCardsRiskBasedAuthentication;
extern const base::Feature kAutofillFillMerchantPromoCodeFields;
extern const base::Feature kAutofillFixOfferInIncognito;
extern const base::FeatureParam<int>
    kAutofillImageFetcherDiskCacheExpirationInMinutes;
extern const base::Feature kAutofillParseMerchantPromoCodeFields;
extern const base::Feature kAutofillSaveCardDismissOnNavigation;
extern const base::Feature kAutofillSaveCardInfobarEditSupport;
extern const base::Feature kAutofillShowUnmaskedCachedCardInManualFillingView;
extern const base::Feature kAutofillSortSuggestionsBasedOnOfferPresence;
extern const base::Feature kAutofillSuggestVirtualCardsOnIncompleteForm;
extern const base::Feature kAutofillUpstream;
extern const base::Feature kAutofillUpstreamAllowAllEmailDomains;

// Return whether a [No thanks] button and new messaging is shown in the save
// card bubbles. This will be called only on desktop platforms.
bool ShouldShowImprovedUserConsentForCreditCardSave();

}  // namespace features
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_PAYMENTS_FEATURES_H_
