// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/commerce/core/commerce_feature_list.h"

namespace ntp_features {

// If enabled, shows a confirm dialog before removing search suggestions from
// the New Tab page real search box ("realbox").
const base::Feature kConfirmSuggestionRemovals{
    "ConfirmNtpSuggestionRemovals", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the OneGooleBar cached response is sent back to NTP.
const base::Feature kCacheOneGoogleBar{"CacheOneGoogleBar",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, "middle slot" promos on the bottom of the NTP will show a dismiss
// UI that allows users to close them and not see them again.
const base::Feature kDismissPromos{"DismissNtpPromos",
                                   base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, queries that are frequently repeated by the user (and are
// expected to be issued again) are shown as most visited tiles.
const base::Feature kNtpRepeatableQueries{"NtpRepeatableQueries",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the NTP "realbox" will be themed like the omnibox
// (same background/text/selected/hover colors).
const base::Feature kRealboxMatchOmniboxTheme{
    "NtpRealboxMatchOmniboxTheme", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the NTP "realbox" will be themed like the searchbox (same border/
// drop shadow on hover state/rounded corners).
const base::Feature kRealboxMatchSearchboxTheme{
    "NtpRealboxMatchSearchboxTheme", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the real search box ("realbox") on the New Tab page will show a
// Google (g) icon instead of the typical magnifying glass (aka loupe).
const base::Feature kRealboxUseGoogleGIcon{"NtpRealboxUseGoogleGIcon",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, handles navigations from the Most Visited tiles explicitly and
// overrides the navigation's transition type to bookmark navigation before the
// navigation is issued.
// TODO(crbug.com/1147589): When removing this flag, also remove the workaround
// in ChromeContentBrowserClient::OverrideNavigationParams.
extern const base::Feature kNtpHandleMostVisitedNavigationExplicitly{
    "HandleMostVisitedNavigationExplicitly", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, OneGoogleBar will be shown.
const base::Feature kNtpOneGoogleBar{"NtpOneGoogleBar",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, logo will be shown.
const base::Feature kNtpLogo{"NtpLogo", base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, shortcuts will be shown.
const base::Feature kNtpShortcuts{"NtpShortcuts",
                                  base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, middle slot promo will be shown.
const base::Feature kNtpMiddleSlotPromo{"NtpMiddleSlotPromo",
                                        base::FEATURE_ENABLED_BY_DEFAULT};

// If enabled, modules will be shown.
const base::Feature kModules{"NtpModules", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, modules will be loaded even if kModules is disabled. This is
// useful to determine if a user would have seen modules in order to
// counterfactually log or trigger.
const base::Feature kNtpModulesLoad{"NtpModulesLoad",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, recipe tasks module will be shown.
const base::Feature kNtpRecipeTasksModule{"NtpRecipeTasksModule",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, shopping tasks module will be shown.
const base::Feature kNtpShoppingTasksModule{"NtpShoppingTasksModule",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, chrome cart module will be shown.
const base::Feature kNtpChromeCartModule{"NtpChromeCartModule",
                                         base::FEATURE_DISABLED_BY_DEFAULT};
#if !defined(OFFICIAL_BUILD)
// If enabled, dummy modules will be shown.
const base::Feature kNtpDummyModules{"NtpDummyModules",
                                     base::FEATURE_DISABLED_BY_DEFAULT};
#endif

// If enabled, redesigned modules will be shown.
const base::Feature kNtpModulesRedesigned{"NtpModulesRedesigned",
                                          base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a different module layout where modules are organized in rows and
// columns will be shown.
const base::Feature kNtpModulesRedesignedLayout{
    "NtpModulesRedesignedLayout", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, Google Drive module will be shown.
const base::Feature kNtpDriveModule{"NtpDriveModule",
                                    base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, Google Photos module will be shown.
const base::Feature kNtpPhotosModule{"NtpPhotosModule",
                                     base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a customized title will be shown on the opt-in card.
const base::Feature kNtpPhotosModuleCustomizedOptInTitle{
    "NtpPhotosModuleCustomizedOptInTitle", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, a customized art work will be shown on the opt-in card.
const base::Feature kNtpPhotosModuleCustomizedOptInArtWork{
    "NtpPhotosModuleCustomizedOptInArtWork", base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, Google Photos opt-in card will show a button to soft opt-out.
const base::Feature kNtpPhotosModuleSoftOptOut(
    "NtpPhotosModuleSoftOptOut",
    base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, SafeBrowsing module will be shown to a target user.
const base::Feature kNtpSafeBrowsingModule{"NtpSafeBrowsingModule",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

// If enabled, modules will be able to be reordered via dragging and dropping
const base::Feature kNtpModulesDragAndDrop{"NtpModulesDragAndDrop",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const char kNtpModulesLoadTimeoutMillisecondsParam[] =
    "NtpModulesLoadTimeoutMillisecondsParam";
const char kNtpModulesOrderParam[] = "NtpModulesOrderParam";
const char kNtpShoppingTasksModuleDataParam[] =
    "NtpShoppingTasksModuleDataParam";
const char kNtpRecipeTasksModuleDataParam[] = "NtpRecipeTasksModuleDataParam";
const char kNtpShoppingTasksModuleCacheMaxAgeSParam[] =
    "NtpShoppingTasksModuleCacheMaxAgeSParam";
const char kNtpRecipeTasksModuleCacheMaxAgeSParam[] =
    "NtpRecipeTasksModuleCacheMaxAgeSParam";
const char kNtpShoppingTasksModuleExperimentGroupParam[] =
    "NtpShoppingTasksModuleExperimentGroupParam";
const char kNtpRecipeTasksModuleExperimentGroupParam[] =
    "NtpRecipeTasksModuleExperimentGroupParam";
const char kNtpChromeCartModuleDataParam[] = "NtpChromeCartModuleDataParam";
const char kNtpChromeCartModuleAbandonedCartDiscountParam[] =
    "NtpChromeCartModuleAbandonedCartDiscountParam";
const char kNtpChromeCartModuleAbandonedCartDiscountUseUtmParam[] =
    "NtpChromeCartModuleAbandonedCartDiscountUseUtmParam";
const char kNtpChromeCartModuleHeuristicsImprovementParam[] =
    "NtpChromeCartModuleHeuristicsImprovementParam";
const char kNtpChromeCartModuleCouponParam[] = "NtpChromeCartModuleCouponParam";
const char kNtpDriveModuleDataParam[] = "NtpDriveModuleDataParam";
const char kNtpDriveModuleManagedUsersOnlyParam[] =
    "NtpDriveModuleManagedUsersOnlyParam";
const char kNtpDriveModuleCacheMaxAgeSParam[] =
    "NtpDriveModuleCacheMaxAgeSParam";
const char kNtpDriveModuleExperimentGroupParam[] =
    "NtpDriveModuleExperimentGroupParam";
const char kNtpPhotosModuleDataParam[] = "NtpPhotosModuleDataParam";
const char kNtpPhotosModuleOptInTitleParam[] = "NtpPhotosModuleOptInTitleParam";
const char kNtpPhotosModuleOptInArtWorkParam[] =
    "NtpPhotosModuleOptInArtWorkParam";
const char kNtpSafeBrowsingModuleCooldownPeriodDaysParam[] =
    "NtpSafeBrowsingModuleCooldownPeriodDaysParam";
const char kNtpSafeBrowsingModuleCountMaxParam[] =
    "NtpSafeBrowsingModuleCountMaxParam";
const char kRealboxMatchOmniboxThemeVariantParam[] =
    "RealboxMatchOmniboxThemeVariantParam";
const char kRealboxMatchSearchboxThemeParam[] =
    "RealboxMatchSearchboxThemeParam";

// Params for Discount Consent V2 in the NTP Cart module.
const char kNtpChromeCartModuleDiscountConsentNtpVariationParam[] =
    "discount-consent-ntp-variation";
const base::FeatureParam<int> kNtpChromeCartModuleDiscountConsentNtpVariation{
    &commerce::kDiscountConsentV2,
    kNtpChromeCartModuleDiscountConsentNtpVariationParam, 0};
const char kNtpChromeCartModuleDiscountConsentReshowTimeParam[] =
    "discount-consent-ntp-reshow-time";
const base::FeatureParam<base::TimeDelta>
    kNtpChromeCartModuleDiscountConsentReshowTime{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentReshowTimeParam, base::Days(28)};
const char kNtpChromeCartModuleDiscountConsentMaxDismissalCountParam[] =
    "discount-consent-ntp-max-dismiss-count";
const base::FeatureParam<int>
    kNtpChromeCartModuleDiscountConsentMaxDismissalCount{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentMaxDismissalCountParam, 1};

// String change variation params.
const char kNtpChromeCartModuleDiscountConsentStringChangeContentParam[] =
    "string-change-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentStringChangeContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentStringChangeContentParam, ""};

const char kNtpChromeCartModuleDiscountConsentInlineShowCloseButtonParam[] =
    "inline-card-show-button";
const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentInlineShowCloseButton{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentStringChangeContentParam, true};

// Discount consent v2 step 1 params.
const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContentParam[] =
        "step-one-use-static-content";
const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneUseStaticContentParam,
        false};
const char kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContentParam[] =
    "step-one-static-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneStaticContentParam, ""};
const char kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCartParam[] =
    "step-one-one-cart-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCart{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneContentOneCartParam, ""};
const char kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCartsParam[] =
    "step-one-two-carts-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCarts{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneContentTwoCartsParam, ""};
const char
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCartsParam[] =
        "step-one-three-carts-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCarts{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepOneContentThreeCartsParam,
        ""};

// Discount consent v2 step 2 params.
const char kNtpChromeCartModuleDiscountConsentNtpStepTwoContentParam[] =
    "step-two-content";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpStepTwoContent{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpStepTwoContentParam, ""};
const char
    kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColorParam[] =
        "step-two-different-color";
const base::FeatureParam<bool>
    kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColor{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentInlineStepTwoDifferentColorParam,
        false};
const char kNtpChromeCartModuleDiscountConsentNtpDialogContentTitleParam[] =
    "dialog-content-title";
const base::FeatureParam<std::string>
    kNtpChromeCartModuleDiscountConsentNtpDialogContentTitle{
        &commerce::kDiscountConsentV2,
        kNtpChromeCartModuleDiscountConsentNtpDialogContentTitleParam, ""};

base::TimeDelta GetModulesLoadTimeout() {
  std::string param_value = base::GetFieldTrialParamValueByFeature(
      kModules, kNtpModulesLoadTimeoutMillisecondsParam);
  // If the field trial param is not found or cannot be parsed to an unsigned
  // integer, return the default value.
  unsigned int param_value_as_int = 0;
  if (!base::StringToUint(param_value, &param_value_as_int)) {
    return base::Seconds(3);
  }
  return base::Milliseconds(param_value_as_int);
}

std::vector<std::string> GetModulesOrder() {
  return base::SplitString(
      base::GetFieldTrialParamValueByFeature(kModules, kNtpModulesOrderParam),
      ",:;", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);
}

}  // namespace ntp_features
