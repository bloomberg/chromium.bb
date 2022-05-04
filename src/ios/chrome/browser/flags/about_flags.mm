// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of about_flags for iOS that sets flags based on experimental
// settings.

#include "ios/chrome/browser/flags/about_flags.h"

#import <UIKit/UIKit.h>
#include <stddef.h>
#include <stdint.h>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#import "base/mac/foundation_util.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/system/sys_info.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/ios/browser/autofill_switches.h"
#include "components/breadcrumbs/core/features.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/flag_descriptions.h"
#include "components/content_settings/core/common/features.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feed/feed_feature_list.h"
#include "components/flags_ui/feature_entry.h"
#include "components/flags_ui/feature_entry_macros.h"
#include "components/flags_ui/flags_storage.h"
#include "components/flags_ui/flags_ui_switches.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/ntp_tiles/switches.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/payments/core/features.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/policy_constants.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/security_state/core/features.h"
#import "components/send_tab_to_self/features.h"
#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/ios/browser/features.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/command_line_switches.h"
#include "components/sync/base/features.h"
#include "components/sync/base/pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "ios/chrome/browser/browsing_data/browsing_data_features.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/chrome/browser/crash_report/features.h"
#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"
#include "ios/chrome/browser/policy/cloud/user_policy_switch.h"
#include "ios/chrome/browser/policy/policy_features.h"
#include "ios/chrome/browser/policy/policy_util.h"
#include "ios/chrome/browser/screen_time/screen_time_buildflags.h"
#import "ios/chrome/browser/sessions/session_features.h"
#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/ui/autofill/features.h"
#import "ios/chrome/browser/ui/bubble/bubble_features.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/default_promo/default_browser_utils.h"
#import "ios/chrome/browser/ui/download/features.h"
#include "ios/chrome/browser/ui/first_run/fre_field_trial.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_features.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_features.h"
#import "ios/chrome/browser/ui/popup_menu/overflow_menu/feature_flags.h"
#import "ios/chrome/browser/ui/start_surface/start_surface_features.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/features.h"
#import "ios/chrome/browser/ui/toolbar_container/toolbar_container_features.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ios/chrome/browser/ui/util/features.h"
#include "ios/chrome/browser/web/features.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"
#include "ios/web/common/features.h"
#include "ios/web/common/user_agent.h"
#include "ios/web/common/web_view_creation_util.h"

#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
#include "ios/chrome/browser/screen_time/features.h"
#endif

#if !defined(OFFICIAL_BUILD)
#include "components/variations/variations_switches.h"
#endif

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using flags_ui::FeatureEntry;

namespace {

const FeatureEntry::Choice kAutofillIOSDelayBetweenFieldsChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"0", autofill::switches::kAutofillIOSDelayBetweenFields, "0"},
    {"10", autofill::switches::kAutofillIOSDelayBetweenFields, "10"},
    {"20", autofill::switches::kAutofillIOSDelayBetweenFields, "20"},
    {"50", autofill::switches::kAutofillIOSDelayBetweenFields, "50"},
    {"100", autofill::switches::kAutofillIOSDelayBetweenFields, "100"},
    {"200", autofill::switches::kAutofillIOSDelayBetweenFields, "200"},
    {"500", autofill::switches::kAutofillIOSDelayBetweenFields, "500"},
    {"1000", autofill::switches::kAutofillIOSDelayBetweenFields, "1000"},
};

const FeatureEntry::Choice kDelayThresholdMinutesToUpdateGaiaCookieChoices[] = {
    {flags_ui::kGenericExperimentChoiceDefault, "", ""},
    {"0", signin::kDelayThresholdMinutesToUpdateGaiaCookie, "0"},
    {"10", signin::kDelayThresholdMinutesToUpdateGaiaCookie, "10"},
    {"60", signin::kDelayThresholdMinutesToUpdateGaiaCookie, "60"},
};

const FeatureEntry::Choice
    kWaitThresholdMillisecondsForCapabilitiesApiChoices[] = {
        {flags_ui::kGenericExperimentChoiceDefault, "", ""},
        {"200", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "200"},
        {"500", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "500"},
        {"5000", signin::kWaitThresholdMillisecondsForCapabilitiesApi, "5000"},
};

const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches3[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "3"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches4[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "4"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches5[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "5"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches6[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "6"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches8[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "8"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches10[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "10"}};
const FeatureEntry::FeatureParam kOmniboxUIMaxAutocompleteMatches12[] = {
    {OmniboxFieldTrial::kUIMaxAutocompleteMatchesParam, "12"}};

const FeatureEntry::FeatureVariation
    kOmniboxUIMaxAutocompleteMatchesVariations[] = {
        {"3 matches", kOmniboxUIMaxAutocompleteMatches3,
         std::size(kOmniboxUIMaxAutocompleteMatches3), nullptr},
        {"4 matches", kOmniboxUIMaxAutocompleteMatches4,
         std::size(kOmniboxUIMaxAutocompleteMatches4), nullptr},
        {"5 matches", kOmniboxUIMaxAutocompleteMatches5,
         std::size(kOmniboxUIMaxAutocompleteMatches5), nullptr},
        {"6 matches", kOmniboxUIMaxAutocompleteMatches6,
         std::size(kOmniboxUIMaxAutocompleteMatches6), nullptr},
        {"8 matches", kOmniboxUIMaxAutocompleteMatches8,
         std::size(kOmniboxUIMaxAutocompleteMatches8), nullptr},
        {"10 matches", kOmniboxUIMaxAutocompleteMatches10,
         std::size(kOmniboxUIMaxAutocompleteMatches10), nullptr},
        {"12 matches", kOmniboxUIMaxAutocompleteMatches12,
         std::size(kOmniboxUIMaxAutocompleteMatches12), nullptr}};

const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowAll[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowAll}};
const FeatureEntry::FeatureParam
    kAutofillUseMobileLabelDisambiguationShowOne[] = {
        {autofill::features::kAutofillUseMobileLabelDisambiguationParameterName,
         autofill::features::
             kAutofillUseMobileLabelDisambiguationParameterShowOne}};

const FeatureEntry::FeatureVariation
    kAutofillUseMobileLabelDisambiguationVariations[] = {
        {"(show all)", kAutofillUseMobileLabelDisambiguationShowAll,
         std::size(kAutofillUseMobileLabelDisambiguationShowAll), nullptr},
        {"(show one)", kAutofillUseMobileLabelDisambiguationShowOne,
         std::size(kAutofillUseMobileLabelDisambiguationShowOne), nullptr}};

const FeatureEntry::FeatureParam
    kDefaultBrowserFullscreenPromoExperimentRemindMeLater[] = {
        {kDefaultBrowserFullscreenPromoExperimentRemindMeGroupParam, "true"}};
const FeatureEntry::FeatureVariation
    kDefaultBrowserFullscreenPromoExperimentVariations[] = {
        {"Remind me later",
         kDefaultBrowserFullscreenPromoExperimentRemindMeLater,
         std::size(kDefaultBrowserFullscreenPromoExperimentRemindMeLater),
         nullptr}};

const FeatureEntry::FeatureParam kDiscoverFeedInNtpEnableNativeUI[] = {
    {kDiscoverFeedIsNativeUIEnabled, "true"}};
const FeatureEntry::FeatureVariation kDiscoverFeedInNtpVariations[] = {
    {"Native UI", kDiscoverFeedInNtpEnableNativeUI,
     std::size(kDiscoverFeedInNtpEnableNativeUI), nullptr}};

const FeatureEntry::FeatureParam kDiscoverFeedSRSReconstructedTemplates[] = {
    {kDiscoverFeedSRSReconstructedTemplatesEnabled, "true"}};
const FeatureEntry::FeatureParam kDiscoverFeedSRSPreloadTemplates[] = {
    {kDiscoverFeedSRSPreloadTemplatesEnabled, "true"}};
const FeatureEntry::FeatureVariation
    kEnableDiscoverFeedStaticResourceServingVariations[] = {
        {"Reconstruct Templates", kDiscoverFeedSRSReconstructedTemplates,
         std::size(kDiscoverFeedSRSReconstructedTemplates), nullptr},
        {"Preload Templates", kDiscoverFeedSRSPreloadTemplates,
         std::size(kDiscoverFeedSRSPreloadTemplates), nullptr},
};

const FeatureEntry::FeatureParam kiOSOmniboxUpdatedPopupUIVersion1[] = {
    {kIOSOmniboxUpdatedPopupUIVariationName,
     kIOSOmniboxUpdatedPopupUIVariation1}};
const FeatureEntry::FeatureParam kiOSOmniboxUpdatedPopupUIVersion2[] = {
    {kIOSOmniboxUpdatedPopupUIVariationName,
     kIOSOmniboxUpdatedPopupUIVariation2}};
const FeatureEntry::FeatureVariation kiOSOmniboxUpdatedPopupUIVariations[] = {
    {"Version 1", kiOSOmniboxUpdatedPopupUIVersion1,
     std::size(kiOSOmniboxUpdatedPopupUIVersion1), nullptr},
    {"Version 2", kiOSOmniboxUpdatedPopupUIVersion2,
     std::size(kiOSOmniboxUpdatedPopupUIVersion2), nullptr}};

const FeatureEntry::FeatureParam kStartSurfaceTenSecondsShrinkLogo[] = {
    {kStartSurfaceShrinkLogoParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceTenSecondsHideShortcuts[] = {
    {kStartSurfaceHideShortcutsParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceTenSecondsReturnToRecentTab[] = {
    {kStartSurfaceReturnToRecentTabParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam
    kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab[] = {
        {kStartSurfaceShrinkLogoParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam
    kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab[] = {
        {kStartSurfaceHideShortcutsParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "10"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHourShrinkLogo[] = {
    {kStartSurfaceShrinkLogoParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHourHideShortcuts[] = {
    {kStartSurfaceHideShortcutsParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam kStartSurfaceOneHourReturnToRecentTab[] = {
    {kStartSurfaceReturnToRecentTabParam, "true"},
    {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam
    kStartSurfaceOneHourShrinkLogoReturnToRecentTab[] = {
        {kStartSurfaceShrinkLogoParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};
const FeatureEntry::FeatureParam
    kStartSurfaceOneHourHideShortcutsReturnToRecentTab[] = {
        {kStartSurfaceHideShortcutsParam, "true"},
        {kStartSurfaceReturnToRecentTabParam, "true"},
        {kReturnToStartSurfaceInactiveDurationInSeconds, "3600"}};

const FeatureEntry::FeatureVariation kStartSurfaceVariations[] = {
    {"10s:Show Return to Recent Tab tile",
     kStartSurfaceTenSecondsReturnToRecentTab,
     std::size(kStartSurfaceTenSecondsReturnToRecentTab), nullptr},
    {"10s:Shrink Logo", kStartSurfaceTenSecondsShrinkLogo,
     std::size(kStartSurfaceTenSecondsShrinkLogo), nullptr},
    {"10s:Hide Shortcuts", kStartSurfaceTenSecondsHideShortcuts,
     std::size(kStartSurfaceTenSecondsHideShortcuts), nullptr},
    {"10s:Shrink Logo and show Return to Recent Tab tile",
     kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab,
     std::size(kStartSurfaceTenSecondsShrinkLogoReturnToRecentTab), nullptr},
    {"10s:Hide Shortcuts and show Return to Recent Tab tile",
     kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab,
     std::size(kStartSurfaceTenSecondsHideShortcutsReturnToRecentTab), nullptr},
    {"1h:Show Return to Recent Tab tile", kStartSurfaceOneHourReturnToRecentTab,
     std::size(kStartSurfaceOneHourReturnToRecentTab), nullptr},
    {"1h:Shrink Logo", kStartSurfaceOneHourShrinkLogo,
     std::size(kStartSurfaceOneHourShrinkLogo), nullptr},
    {"1h:Hide Shortcuts", kStartSurfaceOneHourHideShortcuts,
     std::size(kStartSurfaceOneHourHideShortcuts), nullptr},
    {"1h:Shrink Logo and show Return to Recent Tab tile",
     kStartSurfaceOneHourShrinkLogoReturnToRecentTab,
     std::size(kStartSurfaceOneHourShrinkLogoReturnToRecentTab), nullptr},
    {"1h:Hide Shortcuts and show Return to Recent Tab tile",
     kStartSurfaceOneHourHideShortcutsReturnToRecentTab,
     std::size(kStartSurfaceOneHourHideShortcutsReturnToRecentTab), nullptr},
};

const FeatureEntry::FeatureParam kFREDefaultPromoTestingDefaultDelay[] = {
    {kFREDefaultPromoTestingDefaultDelayParam, "true"}};
const FeatureEntry::FeatureParam kFREDefaultPromoTestingOnly[] = {
    {kFREDefaultPromoTestingOnlyParam, "true"}};
const FeatureEntry::FeatureParam kFREDefaultPromoTestingShortDelay[] = {
    {kFREDefaultPromoTestingShortDelayParam, "true"}};
const FeatureEntry::FeatureVariation kFREDefaultPromoTestingVariations[] = {
    {"Wait 14 days after FRE default browser promo",
     kFREDefaultPromoTestingDefaultDelay,
     std::size(kFREDefaultPromoTestingDefaultDelay), nullptr},
    {"FRE default browser promo only", kFREDefaultPromoTestingOnly,
     std::size(kFREDefaultPromoTestingOnly), nullptr},
    {"Wait 3 days after FRE default browser promo",
     kFREDefaultPromoTestingShortDelay,
     std::size(kFREDefaultPromoTestingShortDelay), nullptr},
};

const FeatureEntry::FeatureVariation kEnableFREUIModuleIOSVariations[] = {
    {"TOP | OLD",
     (FeatureEntry::FeatureParam[]){
         {kFREUIIdentitySwitcherPositionParam, "top"},
         {kFREUIStringsSetParam, "old"}},
     2, nullptr},
    {"BOTTOM | OLD",
     (FeatureEntry::FeatureParam[]){
         {kFREUIIdentitySwitcherPositionParam, "bottom"},
         {kFREUIStringsSetParam, "old"}},
     2, nullptr},
    {"TOP | NEW",
     (FeatureEntry::FeatureParam[]){
         {kFREUIIdentitySwitcherPositionParam, "top"},
         {kFREUIStringsSetParam, "new"}},
     2, nullptr},
    {"BOTTOM | NEW",
     (FeatureEntry::FeatureParam[]){
         {kFREUIIdentitySwitcherPositionParam, "bottom"},
         {kFREUIStringsSetParam, "new"}},
     2, nullptr}};

const FeatureEntry::FeatureParam kNewMICEFREWithUMADialog[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamUMADialog}};
const FeatureEntry::FeatureParam kNewMICEFREWithThreeSteps[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamThreeSteps}};
const FeatureEntry::FeatureParam kNewMICEFREWithTwoSteps[] = {
    {kNewMobileIdentityConsistencyFREParam,
     kNewMobileIdentityConsistencyFREParamTwoSteps}};
const FeatureEntry::FeatureVariation
    kNewMobileIdentityConsistencyFREVariations[] = {
        {"New FRE with UMA dialog", kNewMICEFREWithUMADialog,
         std::size(kNewMICEFREWithUMADialog), nullptr},
        {"new FRE with 3 steps", kNewMICEFREWithThreeSteps,
         std::size(kNewMICEFREWithThreeSteps), nullptr},
        {"new FRE with 2 steps", kNewMICEFREWithTwoSteps,
         std::size(kNewMICEFREWithTwoSteps), nullptr}};

const FeatureEntry::FeatureParam kBubbleRichIPHTargetHighlight[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterTargetHighlight}};
const FeatureEntry::FeatureParam kBubbleRichIPHExplicitDismissal[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterExplicitDismissal}};
const FeatureEntry::FeatureParam kBubbleRichIPHRich[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterRich}};
const FeatureEntry::FeatureParam kBubbleRichIPHRichWithSnooze[] = {
    {kBubbleRichIPHParameterName, kBubbleRichIPHParameterRichWithSnooze}};
const FeatureEntry::FeatureVariation kBubbleRichIPHVariations[] = {
    {"Target Highlight", kBubbleRichIPHTargetHighlight,
     std::size(kBubbleRichIPHTargetHighlight), nullptr},
    {"Explicit dismissal", kBubbleRichIPHExplicitDismissal,
     std::size(kBubbleRichIPHExplicitDismissal), nullptr},
    {"Dismissal and rich content", kBubbleRichIPHRich,
     std::size(kBubbleRichIPHRich), nullptr},
    {"Dismissal, rich content, and snooze", kBubbleRichIPHRichWithSnooze,
     std::size(kBubbleRichIPHRichWithSnooze), nullptr},
};

// To add a new entry, add to the end of kFeatureEntries. There are four
// distinct types of entries:
// . ENABLE_DISABLE_VALUE: entry is either enabled, disabled, or uses the
//   default value for this feature. Use the ENABLE_DISABLE_VALUE_TYPE
//   macro for this type supplying the command line to the macro.
// . MULTI_VALUE: a list of choices, the first of which should correspond to a
//   deactivated state for this lab (i.e. no command line option).  To specify
//   this type of entry use the macro MULTI_VALUE_TYPE supplying it the
//   array of choices.
// . FEATURE_VALUE: entry is associated with a base::Feature instance. Entry is
//   either enabled, disabled, or uses the default value of the associated
//   base::Feature instance. To specify this type of entry use the macro
//   FEATURE_VALUE_TYPE supplying it the base::Feature instance.
// . FEATURE_WITH_PARAM_VALUES: a list of choices associated with a
//   base::Feature instance. Choices corresponding to the default state, a
//   universally enabled state, and a universally disabled state are
//   automatically included. To specify this type of entry use the macro
//   FEATURE_WITH_PARAMS_VALUE_TYPE supplying it the base::Feature instance and
//   the array of choices.
//
// See the documentation of FeatureEntry for details on the fields.
//
// When adding a new choice, add it to the end of the list.
const flags_ui::FeatureEntry kFeatureEntries[] = {
    {"in-product-help-demo-mode-choice",
     flag_descriptions::kInProductHelpDemoModeName,
     flag_descriptions::kInProductHelpDemoModeDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         feature_engagement::kIPHDemoMode,
         feature_engagement::kIPHDemoModeChoiceVariations,
         "IPH_DemoMode")},
    {"enable-autofill-credit-card-upload",
     flag_descriptions::kAutofillCreditCardUploadName,
     flag_descriptions::kAutofillCreditCardUploadDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillUpstream)},
    {"enable-discover-feed-preview",
     flag_descriptions::kEnableDiscoverFeedPreviewName,
     flag_descriptions::kEnableDiscoverFeedPreviewDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableDiscoverFeedPreview)},
    {"use-sync-sandbox", flag_descriptions::kSyncSandboxName,
     flag_descriptions::kSyncSandboxDescription, flags_ui::kOsIos,
     SINGLE_VALUE_TYPE_AND_VALUE(
         syncer::kSyncServiceURL,
         "https://chrome-sync.sandbox.google.com/chrome-sync/alpha")},
    {"wallet-service-use-sandbox",
     flag_descriptions::kWalletServiceUseSandboxName,
     flag_descriptions::kWalletServiceUseSandboxDescription, flags_ui::kOsIos,
     ENABLE_DISABLE_VALUE_TYPE_AND_VALUE(
         autofill::switches::kWalletServiceUseSandbox,
         "1",
         autofill::switches::kWalletServiceUseSandbox,
         "0")},
    {"show-autofill-type-predictions",
     flag_descriptions::kShowAutofillTypePredictionsName,
     flag_descriptions::kShowAutofillTypePredictionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillShowTypePredictions)},
    {"autofill-ios-delay-between-fields",
     flag_descriptions::kAutofillIOSDelayBetweenFieldsName,
     flag_descriptions::kAutofillIOSDelayBetweenFieldsDescription,
     flags_ui::kOsIos, MULTI_VALUE_TYPE(kAutofillIOSDelayBetweenFieldsChoices)},
    {"fullscreen-viewport-adjustment-experiment",
     flag_descriptions::kFullscreenSmoothScrollingName,
     flag_descriptions::kFullscreenSmoothScrollingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(fullscreen::features::kSmoothScrollingDefault)},
    {"webpage-default-zoom-from-dynamic-type",
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeName,
     flag_descriptions::kWebPageDefaultZoomFromDynamicTypeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kWebPageDefaultZoomFromDynamicType)},
    {"webpage-alternative-text-zoom",
     flag_descriptions::kWebPageAlternativeTextZoomName,
     flag_descriptions::kWebPageAlternativeTextZoomDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(web::kWebPageAlternativeTextZoom)},
    {"toolbar-container", flag_descriptions::kToolbarContainerName,
     flag_descriptions::kToolbarContainerDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(toolbar_container::kToolbarContainerEnabled)},
    {"omnibox-ui-max-autocomplete-matches",
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesName,
     flag_descriptions::kOmniboxUIMaxAutocompleteMatchesDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         omnibox::kUIExperimentMaxAutocompleteMatches,
         kOmniboxUIMaxAutocompleteMatchesVariations,
         "OmniboxUIMaxAutocompleteVariations")},
    {"omnibox-local-history-zero-suggest",
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestName,
     flag_descriptions::kOmniboxLocalHistoryZeroSuggestDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(omnibox::kLocalHistoryZeroSuggest)},
#if defined(DCHECK_IS_CONFIGURABLE)
    {"dcheck-is-fatal", flag_descriptions::kDcheckIsFatalName,
     flag_descriptions::kDcheckIsFatalDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(base::kDCheckIsFatalFeature)},
#endif  // defined(DCHECK_IS_CONFIGURABLE)
    {"autofill-use-mobile-label-disambiguation",
     flag_descriptions::kAutofillUseMobileLabelDisambiguationName,
     flag_descriptions::kAutofillUseMobileLabelDisambiguationDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         autofill::features::kAutofillUseMobileLabelDisambiguation,
         kAutofillUseMobileLabelDisambiguationVariations,
         "AutofillUseMobileLabelDisambiguation")},
    {"metrickit-crash-reports", flag_descriptions::kMetrickitCrashReportName,
     flag_descriptions::kMetrickitCrashReportDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kMetrickitCrashReport)},
    {"ios-breadcrumbs", flag_descriptions::kLogBreadcrumbsName,
     flag_descriptions::kLogBreadcrumbsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(breadcrumbs::kLogBreadcrumbs)},
    {"ios-synthetic-crash-reports",
     flag_descriptions::kSyntheticCrashReportsForUteName,
     flag_descriptions::kSyntheticCrashReportsForUteDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSyntheticCrashReportsForUte)},
    {"force-startup-signin-promo",
     flag_descriptions::kForceStartupSigninPromoName,
     flag_descriptions::kForceStartupSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(switches::kForceStartupSigninPromo)},
    {"restore-session-from-cache",
     flag_descriptions::kRestoreSessionFromCacheName,
     flag_descriptions::kRestoreSessionFromCacheDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::kRestoreSessionFromCache)},
    {"autofill-save-card-dismiss-on-navigation",
     flag_descriptions::kAutofillSaveCardDismissOnNavigationName,
     flag_descriptions::kAutofillSaveCardDismissOnNavigationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillSaveCardDismissOnNavigation)},
    {"new-content-suggestions-feed", flag_descriptions::kDiscoverFeedInNtpName,
     flag_descriptions::kDiscoverFeedInNtpDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kDiscoverFeedInNtp,
                                    kDiscoverFeedInNtpVariations,
                                    "IOSDiscoverFeed")},
    {"expanded-tab-strip", flag_descriptions::kExpandedTabStripName,
     flag_descriptions::kExpandedTabStripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kExpandedTabStrip)},
    {"shared-highlighting-ios", flag_descriptions::kSharedHighlightingIOSName,
     flag_descriptions::kSharedHighlightingIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSharedHighlightingIOS)},
    {"enable-fre-ui-module-ios-with-options",
     flag_descriptions::kEnableFREUIModuleIOSName,
     flag_descriptions::kEnableFREUIModuleIOSDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFREUIModuleIOS,
                                    kEnableFREUIModuleIOSVariations,
                                    "EnableFREUIModuleIOS")},
    {"new-mobile-identity-consistency-fre",
     flag_descriptions::kNewMobileIdentityConsistencyFREName,
     flag_descriptions::kNewMobileIdentityConsistencyFREDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(signin::kNewMobileIdentityConsistencyFRE,
                                    kNewMobileIdentityConsistencyFREVariations,
                                    "NewMobileIdentityConsistencyFRE")},
    {"enable-long-message-duration",
     flag_descriptions::kEnableLongMessageDurationName,
     flag_descriptions::kEnableLongMessageDurationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableLongMessageDuration)},
#if BUILDFLAG(IOS_SCREEN_TIME_ENABLED)
    {"screen-time-integration-ios",
     flag_descriptions::kScreenTimeIntegrationName,
     flag_descriptions::kScreenTimeIntegrationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kScreenTimeIntegration)},
#endif
    {"modern-tab-strip", flag_descriptions::kModernTabStripName,
     flag_descriptions::kModernTabStripDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kModernTabStrip)},
    {"minutes-delay-to-restore-gaia-cookies-if-deleted",
     flag_descriptions::kDelayThresholdMinutesToUpdateGaiaCookieName,
     flag_descriptions::kDelayThresholdMinutesToUpdateGaiaCookieDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kDelayThresholdMinutesToUpdateGaiaCookieChoices)},
    {"add-passwords-in-settings",
     flag_descriptions::kAddPasswordsInSettingsName,
     flag_descriptions::kAddPasswordsInSettingsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kSupportForAddPasswordsInSettings)},
    {"record-snapshot-size", flag_descriptions::kRecordSnapshotSizeName,
     flag_descriptions::kRecordSnapshotSizeDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kRecordSnapshotSize)},
    {"default-browser-fullscreen-promo-experiment",
     flag_descriptions::kDefaultBrowserFullscreenPromoExperimentName,
     flag_descriptions::kDefaultBrowserFullscreenPromoExperimentDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kDefaultBrowserFullscreenPromoExperiment,
         kDefaultBrowserFullscreenPromoExperimentVariations,
         "IOSDefaultBrowserFullscreenPromoExperiment")},
    {"ios-shared-highlighting-color-change",
     flag_descriptions::kIOSSharedHighlightingColorChangeName,
     flag_descriptions::kIOSSharedHighlightingColorChangeDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kIOSSharedHighlightingColorChange)},
    {"ios-shared-highlighting-v2",
     flag_descriptions::kIOSSharedHighlightingV2Name,
     flag_descriptions::kIOSSharedHighlightingV2Description, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(shared_highlighting::kIOSSharedHighlightingV2)},
    {"omnibox-new-textfield-implementation",
     flag_descriptions::kOmniboxNewImplementationName,
     flag_descriptions::kOmniboxNewImplementationDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIOSNewOmniboxImplementation)},
    {"omnibox-new-popup-ui", flag_descriptions::kIOSOmniboxUpdatedPopupUIName,
     flag_descriptions::kIOSOmniboxUpdatedPopupUIDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kIOSOmniboxUpdatedPopupUI,
                                    kiOSOmniboxUpdatedPopupUIVariations,
                                    "kIOSOmniboxUpdatedPopupUI")},
    {"start-surface", flag_descriptions::kStartSurfaceName,
     flag_descriptions::kStartSurfaceDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kStartSurface,
                                    kStartSurfaceVariations,
                                    "StartSurface")},
    {"ios-crashpad", flag_descriptions::kCrashpadIOSName,
     flag_descriptions::kCrashpadIOSDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kCrashpadIOS)},
    {"detect-form-submission-on-form-clear",
     flag_descriptions::kDetectFormSubmissionOnFormClearIOSName,
     flag_descriptions::kDetectFormSubmissionOnFormClearIOSDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kDetectFormSubmissionOnFormClear)},
    {"autofill-address-verification-in-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptAddressVerificationName,
     flag_descriptions::
         kEnableAutofillAddressSavePromptAddressVerificationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::
             kAutofillAddressProfileSavePromptAddressVerificationSupport)},
    {"autofill-address-save-prompt",
     flag_descriptions::kEnableAutofillAddressSavePromptName,
     flag_descriptions::kEnableAutofillAddressSavePromptDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(autofill::features::kAutofillAddressProfileSavePrompt)},
    {"filling-across-affiliated-websites",
     flag_descriptions::kFillingAcrossAffiliatedWebsitesName,
     flag_descriptions::kFillingAcrossAffiliatedWebsitesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kFillingAcrossAffiliatedWebsites)},
    {"autofill-parse-merchant-promo-code-fields",
     flag_descriptions::kAutofillParseMerchantPromoCodeFieldsName,
     flag_descriptions::kAutofillParseMerchantPromoCodeFieldsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillParseMerchantPromoCodeFields)},
    {"interest-feed-v2-clicks-and-views-cond-upload",
     flag_descriptions::kInterestFeedV2ClickAndViewActionsConditionalUploadName,
     flag_descriptions::
         kInterestFeedV2ClickAndViewActionsConditionalUploadDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(feed::kInterestFeedV2ClicksAndViewsConditionalUpload)},
    {"tabs-search-ios", flag_descriptions::kTabsSearchName,
     flag_descriptions::kTabsSearchDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabsSearch)},
    {"tabs-search-regular-results-suggested-actions-ios",
     flag_descriptions::kTabsSearchRegularResultsSuggestedActionsName,
     flag_descriptions::kTabsSearchRegularResultsSuggestedActionsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kTabsSearchRegularResultsSuggestedActions)},
    {"incognito-brand-consistency-for-ios",
     flag_descriptions::kIncognitoBrandConsistencyForIOSName,
     flag_descriptions::kIncognitoBrandConsistencyForIOSDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kIncognitoBrandConsistencyForIOS)},
    {"incognito-ntp-revamp", flag_descriptions::kIncognitoNtpRevampName,
     flag_descriptions::kIncognitoNtpRevampDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kIncognitoNtpRevamp)},
    {"update-history-entry-points-in-incognito",
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoName,
     flag_descriptions::kUpdateHistoryEntryPointsInIncognitoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUpdateHistoryEntryPointsInIncognito)},
    {"sync-trusted-vault-passphrase-ios-rpc",
     flag_descriptions::kSyncTrustedVaultPassphraseiOSRPCName,
     flag_descriptions::kSyncTrustedVaultPassphraseiOSRPCDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kSyncTrustedVaultPassphraseiOSRPC)},
    {"sync-trusted-vault-passphrase-promo",
     flag_descriptions::kSyncTrustedVaultPassphrasePromoName,
     flag_descriptions::kSyncTrustedVaultPassphrasePromoDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kSyncTrustedVaultPassphrasePromo)},
    {"sync-trusted-vault-passphrase-recovery",
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryName,
     flag_descriptions::kSyncTrustedVaultPassphraseRecoveryDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kSyncTrustedVaultPassphraseRecovery)},
    {"wait-threshold-seconds-for-capabilities-api",
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiName,
     flag_descriptions::kWaitThresholdMillisecondsForCapabilitiesApiDescription,
     flags_ui::kOsIos,
     MULTI_VALUE_TYPE(kWaitThresholdMillisecondsForCapabilitiesApiChoices)},
    {"autofill-fill-merchant-promo-code-fields",
     flag_descriptions::kAutofillFillMerchantPromoCodeFieldsName,
     flag_descriptions::kAutofillFillMerchantPromoCodeFieldsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillFillMerchantPromoCodeFields)},
    {"context-menu-phase2",
     flag_descriptions::kWebViewNativeContextMenuPhase2Name,
     flag_descriptions::kWebViewNativeContextMenuPhase2Description,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kWebViewNativeContextMenuPhase2)},
    {"default-wkwebview-context-menu",
     flag_descriptions::kDefaultWebViewContextMenuName,
     flag_descriptions::kDefaultWebViewContextMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kDefaultWebViewContextMenu)},
    {"new-overflow-menu", flag_descriptions::kNewOverflowMenuName,
     flag_descriptions::kNewOverflowMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNewOverflowMenu)},
    {"enable-new-download-api", flag_descriptions::kEnableNewDownloadAPIName,
     flag_descriptions::kEnableNewDownloadAPIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableNewDownloadAPI)},
    {"use-lens-to-search-for-image",
     flag_descriptions::kUseLensToSearchForImageName,
     flag_descriptions::kUseLensToSearchForImageDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseLensToSearchForImage)},
    {"use-load-simulated-request-for-error-page-navigation",
     flag_descriptions::kUseLoadSimulatedRequestForErrorPageNavigationName,
     flag_descriptions::
         kUseLoadSimulatedRequestForErrorPageNavigationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         web::features::kUseLoadSimulatedRequestForErrorPageNavigation)},
    {"enable-discover-feed-static-resource-serving",
     flag_descriptions::kEnableDiscoverFeedStaticResourceServingName,
     flag_descriptions::kEnableDiscoverFeedStaticResourceServingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(
         kEnableDiscoverFeedStaticResourceServing,
         kEnableDiscoverFeedStaticResourceServingVariations,
         "IOSDiscoverFeedStaticResourceServing")},
    {"enable-disco-feed-endpoint",
     flag_descriptions::kEnableDiscoverFeedDiscoFeedEndpointName,
     flag_descriptions::kEnableDiscoverFeedDiscoFeedEndpointDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableDiscoverFeedDiscoFeedEndpoint)},
    {"enable-fre-default-browser-screen-testing",
     flag_descriptions::kEnableFREDefaultBrowserScreenTestingName,
     flag_descriptions::kEnableFREDefaultBrowserScreenTestingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kEnableFREDefaultBrowserScreenTesting,
                                    kFREDefaultPromoTestingVariations,
                                    "EnableFREDefaultBrowserScreenTesting")},
    {"enable-discover-feed-shorter-cache",
     flag_descriptions::kEnableDiscoverFeedShorterCacheName,
     flag_descriptions::kEnableDiscoverFeedShorterCacheDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kEnableDiscoverFeedShorterCache)},
    {"shared-highlighting-amp",
     flag_descriptions::kIOSSharedHighlightingAmpName,
     flag_descriptions::kIOSSharedHighlightingAmpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(shared_highlighting::kSharedHighlightingAmp)},
    {"enable-commerce-price-tracking",
     commerce::flag_descriptions::kCommercePriceTrackingName,
     commerce::flag_descriptions::kCommercePriceTrackingDescription,
     flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(commerce::kCommercePriceTracking,
                                    commerce::kCommercePriceTrackingVariations,
                                    "CommercePriceTracking")},
    {"web-feed-ios", flag_descriptions::kEnableWebChannelsName,
     flag_descriptions::kEnableWebChannelsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableWebChannels)},
    {"ntp-view-hierarchy-repair",
     flag_descriptions::kNTPViewHierarchyRepairName,
     flag_descriptions::kNTPViewHierarchyRepairDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kNTPViewHierarchyRepair)},
    {"single-ntp", flag_descriptions::kSingleNtpName,
     flag_descriptions::kSingleNtpDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kSingleNtp)},
    {"synthesized-restore-session",
     flag_descriptions::kSynthesizedRestoreSessionName,
     flag_descriptions::kSynthesizedRestoreSessionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kSynthesizedRestoreSession)},
    {"remove-extra-ntps", flag_descriptions::kRemoveExcessNTPsExperimentName,
     flag_descriptions::kRemoveExcessNTPsExperimentDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kRemoveExcessNTPs)},
    {"lazily-create-web-state-on-restoration",
     flag_descriptions::kLazilyCreateWebStateOnRestorationName,
     flag_descriptions::kLazilyCreateWebStateOnRestorationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableUnrealizedWebStates)},
    {"enable-shortened-password-auto-fill-instruction",
     flag_descriptions::kEnableShortenedPasswordAutoFillInstructionName,
     flag_descriptions::kEnableShortenedPasswordAutoFillInstructionDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kEnableShortenedPasswordAutoFillInstruction)},
    {"enable-password-manager-branding-update",
     flag_descriptions::kIOSEnablePasswordManagerBrandingUpdateName,
     flag_descriptions::kIOSEnablePasswordManagerBrandingUpdateDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kIOSEnablePasswordManagerBrandingUpdate)},
    {"default-mode-ua", flag_descriptions::kAddSettingForDefaultPageModeName,
     flag_descriptions::kAddSettingForDefaultPageModeDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kAddSettingForDefaultPageMode)},
    {"ios-media-permissions-control",
     flag_descriptions::kMediaPermissionsControlName,
     flag_descriptions::kMediaPermissionsControlDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kMediaPermissionsControl)},
    {"enable-save-session-tabs-in-separate-files",
     flag_descriptions::kSaveSessionTabsToSeparateFilesName,
     flag_descriptions::kSaveSessionTabsToSeparateFilesDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(sessions::kSaveSessionTabsToSeparateFiles)},
    {"use-sf-symbols-samples", flag_descriptions::kUseSFSymbolsSamplesName,
     flag_descriptions::kUseSFSymbolsSamplesDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseSFSymbolsSamples)},
    {"use-new-popup-menu", flag_descriptions::kUseUIKitPopupMenuName,
     flag_descriptions::kUseUIKitPopupMenuDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUseUIKitPopupMenu)},
    {"enable-unicorn-account-support",
     flag_descriptions::kEnableUnicornAccountSupportName,
     flag_descriptions::kEnableUnicornAccountSupportDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(signin::kEnableUnicornAccountSupport)},
    {"single-cell-content-suggestions",
     flag_descriptions::kSingleCellContentSuggestionsName,
     flag_descriptions::kSingleCellContentSuggestionsDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kSingleCellContentSuggestions)},
    {"content-suggestions-header-migration",
     flag_descriptions::kContentSuggestionsHeaderMigrationName,
     flag_descriptions::kContentSuggestionsHeaderMigrationDescription,
     flags_ui::kOsIos, FEATURE_VALUE_TYPE(kContentSuggestionsHeaderMigration)},
    {"leak-detection-unauthenticated",
     flag_descriptions::kLeakDetectionUnauthenticatedName,
     flag_descriptions::kLeakDetectionUnauthenticatedDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kLeakDetectionUnauthenticated)},
    {"mute-compromised-passwords",
     flag_descriptions::kMuteCompromisedPasswordsName,
     flag_descriptions::kMuteCompromisedPasswordsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(password_manager::features::kMuteCompromisedPasswords)},
    {"enable-favicon-passwords",
     flag_descriptions::kEnableFaviconForPasswordsName,
     flag_descriptions::kEnableFaviconForPasswordsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         password_manager::features::kEnableFaviconForPasswords)},
    {"autofill-enable-sending-bcn-in-get-upload-details",
     flag_descriptions::kAutofillEnableSendingBcnInGetUploadDetailsName,
     flag_descriptions::kAutofillEnableSendingBcnInGetUploadDetailsDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableSendingBcnInGetUploadDetails)},
    {"enable-fullscreen-api", flag_descriptions::kEnableFullscreenAPIName,
     flag_descriptions::kEnableFullscreenAPIDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(web::features::kEnableFullscreenAPI)},
    {"enable-enhanced-safe-browsing",
     flag_descriptions::kEnhancedProtectionName,
     flag_descriptions::kEnhancedProtectionDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(safe_browsing::kEnhancedProtection)},
    {"context-menu-phase2-screenshot",
     flag_descriptions::kWebViewNativeContextMenuPhase2ScreenshotName,
     flag_descriptions::kWebViewNativeContextMenuPhase2ScreenshotDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         web::features::kWebViewNativeContextMenuPhase2Screenshot)},
    {"autofill-enable-unmask-card-request-set-instrument-id",
     flag_descriptions::kAutofillEnableUnmaskCardRequestSetInstrumentIdName,
     flag_descriptions::
         kAutofillEnableUnmaskCardRequestSetInstrumentIdDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnableUnmaskCardRequestSetInstrumentId)},
    {"send-tab-to-self-signin-promo",
     flag_descriptions::kSendTabToSelfSigninPromoName,
     flag_descriptions::kSendTabToSelfSigninPromoDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(send_tab_to_self::kSendTabToSelfSigninPromo)},
    {"content-suggestions-uiviewcontroller-migration",
     flag_descriptions::kContentSuggestionsUIViewControllerMigrationName,
     flag_descriptions::kContentSuggestionsUIViewControllerMigrationDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kContentSuggestionsUIViewControllerMigration)},
    {"autofill-password-rich-iph",
     flag_descriptions::kAutofillPasswordRichIPHName,
     flag_descriptions::kAutofillPasswordRichIPHDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kAutofillPasswordRichIPH)},
    {"bubble-rich-iph", flag_descriptions::kBubbleRichIPHName,
     flag_descriptions::kBubbleRichIPHDescription, flags_ui::kOsIos,
     FEATURE_WITH_PARAMS_VALUE_TYPE(kBubbleRichIPH,
                                    kBubbleRichIPHVariations,
                                    "BubbleRichIPH")},
    {"autofill-enforce-delays-in-strike-database",
     flag_descriptions::kAutofillEnforceDelaysInStrikeDatabaseName,
     flag_descriptions::kAutofillEnforceDelaysInStrikeDatabaseDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(
         autofill::features::kAutofillEnforceDelaysInStrikeDatabase)},
    {"download-calendar", flag_descriptions::kDownloadCalendarName,
     flag_descriptions::kDownloadCalendarDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kDownloadCalendar)},
    {"uiview-window-observing", flag_descriptions::kUIViewWindowObservingName,
     flag_descriptions::kUIViewWindowObservingDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(kUIViewWindowObserving)},
    {"sync-standalone-invalidations", flag_descriptions::kSyncInvalidationsName,
     flag_descriptions::kSyncInvalidationsDescription, flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kUseSyncInvalidations)},
    {"sync-standalone-invalidations-wallet-and-offer",
     flag_descriptions::kSyncInvalidationsWalletAndOfferName,
     flag_descriptions::kSyncInvalidationsWalletAndOfferDescription,
     flags_ui::kOsIos,
     FEATURE_VALUE_TYPE(::syncer::kUseSyncInvalidationsForWalletAndOffer)},
};

bool SkipConditionalFeatureEntry(const flags_ui::FeatureEntry& entry) {
  return false;
}

flags_ui::FlagsState& GetGlobalFlagsState() {
  static base::NoDestructor<flags_ui::FlagsState> flags_state(kFeatureEntries,
                                                              nullptr);
  return *flags_state;
}
// Creates the experimental test policies map, used by AsyncPolicyLoader and
// PolicyLoaderIOS to locally enable policies.
NSMutableDictionary* CreateExperimentalTestingPolicies() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Shared variables for all enterprise experimental flags.
  NSMutableDictionary* testing_policies = [[NSMutableDictionary alloc] init];
  NSMutableArray* allowed_experimental_policies = [[NSMutableArray alloc] init];

  // Set some sample policy values for testing if EnableSamplePolicies is set to
  // true.
  if ([defaults boolForKey:@"EnableSamplePolicies"]) {
    [testing_policies addEntriesFromDictionary:@{
      base::SysUTF8ToNSString(policy::key::kAutofillAddressEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kAutofillCreditCardEnabled) : @NO,

      // 2 = Disable all variations
      base::SysUTF8ToNSString(policy::key::kChromeVariations) : @2,

      // 2 = Do not allow any site to show popups
      base::SysUTF8ToNSString(policy::key::kDefaultPopupsSetting) : @2,

      // Set default search engine.
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderEnabled) :
          @YES,
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderSearchURL) :
          @"http://www.google.com/search?q={searchTerms}",
      base::SysUTF8ToNSString(policy::key::kDefaultSearchProviderName) :
          @"TestEngine",

      base::SysUTF8ToNSString(policy::key::kEditBookmarksEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kNTPContentSuggestionsEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kPasswordManagerEnabled) : @NO,

      base::SysUTF8ToNSString(policy::key::kTranslateEnabled) : @NO,

      // 2 = Enhanced safe browsing protection
      base::SysUTF8ToNSString(policy::key::kSafeBrowsingProtectionLevel) : @2,

      base::SysUTF8ToNSString(policy::key::kSearchSuggestEnabled) : @YES,
    }];
  }

  if ([defaults boolForKey:@"EnableSyncDisabledPolicy"]) {
    NSString* sync_policy_key =
        base::SysUTF8ToNSString(policy::key::kSyncDisabled);
    [testing_policies addEntriesFromDictionary:@{sync_policy_key : @YES}];
  }

  // SyncTypesListDisabled policy.
  NSString* Sync_types_list_disabled_key =
      base::SysUTF8ToNSString(policy::key::kSyncTypesListDisabled);
  NSMutableArray* Sync_types_list_disabled_values =
      [[NSMutableArray alloc] init];
  if ([defaults boolForKey:@"SyncTypesListBookmarks"]) {
    [Sync_types_list_disabled_values addObject:@"bookmarks"];
  }
  if ([defaults boolForKey:@"SyncTypesListReadingList"]) {
    [Sync_types_list_disabled_values addObject:@"readingList"];
  }
  if ([defaults boolForKey:@"SyncTypesListPreferences"]) {
    [Sync_types_list_disabled_values addObject:@"preferences"];
  }
  if ([defaults boolForKey:@"SyncTypesListPasswords"]) {
    [Sync_types_list_disabled_values addObject:@"passwords"];
  }
  if ([defaults boolForKey:@"SyncTypesListAutofill"]) {
    [Sync_types_list_disabled_values addObject:@"autofill"];
  }
  if ([defaults boolForKey:@"SyncTypesListTypedUrls"]) {
    [Sync_types_list_disabled_values addObject:@"typedUrls"];
  }
  if ([defaults boolForKey:@"SyncTypesListTabs"]) {
    [Sync_types_list_disabled_values addObject:@"tabs"];
  }
  if ([Sync_types_list_disabled_values count]) {
    [testing_policies addEntriesFromDictionary:@{
      Sync_types_list_disabled_key : Sync_types_list_disabled_values
    }];
  }

  // If an incognito mode availability is set, set the value.
  NSString* incognito_policy_key =
      base::SysUTF8ToNSString(policy::key::kIncognitoModeAvailability);
  NSInteger incognito_mode_availability =
      [defaults integerForKey:incognito_policy_key];
  if (incognito_mode_availability) {
    [testing_policies addEntriesFromDictionary:@{
      incognito_policy_key : @(incognito_mode_availability),
    }];
  }

  NSString* restriction_pattern =
      [defaults stringForKey:@"RestrictAccountsToPatterns"];
  if ([restriction_pattern length] > 0) {
    NSString* restrict_key =
        base::SysUTF8ToNSString(policy::key::kRestrictAccountsToPatterns);
    [testing_policies addEntriesFromDictionary:@{
      restrict_key : @[ restriction_pattern ]
    }];
  }

  // If the sign-in policy is set (not "None"), add the policy key to the list
  // of enabled experimental policies, and set the value.
  NSString* const kSigninPolicyKey = @"BrowserSignin";
  NSInteger signin_policy_mode = [defaults integerForKey:kSigninPolicyKey];
  if (signin_policy_mode) {
    // Remove the mode offset that was used to represent the unset policy.
    --signin_policy_mode;
    DCHECK(signin_policy_mode >= 0);

    [testing_policies addEntriesFromDictionary:@{
      kSigninPolicyKey : @(signin_policy_mode),
    }];
  }

  // If the New Tab Page URL is set (not empty) add the value to the list of
  // test policies.
  NSString* ntp_location = [defaults stringForKey:@"NTPLocation"];
  if ([ntp_location length] > 0) {
    NSString* ntp_location_key =
        base::SysUTF8ToNSString(policy::key::kNewTabPageLocation);
    [testing_policies
        addEntriesFromDictionary:@{ntp_location_key : ntp_location}];
    [allowed_experimental_policies addObject:ntp_location_key];
  }

  if ([defaults boolForKey:@"DisallowChromeDataInBackups"]) {
    NSString* allow_backups_key =
        base::SysUTF8ToNSString(policy::key::kAllowChromeDataInBackups);
    [testing_policies addEntriesFromDictionary:@{allow_backups_key : @NO}];
    [allowed_experimental_policies addObject:allow_backups_key];
  }

  // If any experimental policy was allowed, set the EnableExperimentalPolicies
  // policy.
  if ([allowed_experimental_policies count] > 0) {
    [testing_policies setValue:allowed_experimental_policies
                        forKey:base::SysUTF8ToNSString(
                                   policy::key::kEnableExperimentalPolicies)];
  }

  // Warning: Add new flags to TestingPoliciesHash() below.

  return testing_policies;
}

// Generates a unique NSString based on currently monitored policies from
// NSUserDefaults standardUserDefaults.
NSString* TestingPoliciesHash() {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  return [NSString
      stringWithFormat:@"%d|%d|%d|%d|%@|%d|%d|%d|%d|%d|%d|%d",
                       [defaults boolForKey:@"DisallowChromeDataInBackups"],
                       [defaults boolForKey:@"EnableSyncDisabledPolicy"],
                       [defaults boolForKey:@"EnableSamplePolicies"],
                       (int)[defaults
                           integerForKey:@"IncognitoModeAvailability"],
                       [defaults stringForKey:@"RestrictAccountsToPatterns"],
                       [defaults boolForKey:@"SyncTypesListBookmarks"],
                       [defaults boolForKey:@"SyncTypesListReadingList"],
                       [defaults boolForKey:@"SyncTypesListPreferences"],
                       [defaults boolForKey:@"SyncTypesListPasswords"],
                       [defaults boolForKey:@"SyncTypesListAutofill"],
                       [defaults boolForKey:@"SyncTypesListTypedUrls"],
                       [defaults boolForKey:@"SyncTypesListTabs"]];
}
}  // namespace

// Add all switches from experimental flags to |command_line|.
void AppendSwitchesFromExperimentalSettings(base::CommandLine* command_line) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  // Set the UA flag if UseMobileSafariUA is enabled.
  if ([defaults boolForKey:@"UseMobileSafariUA"]) {
    // Safari uses "Vesion/", followed by the OS version excluding bugfix, where
    // Chrome puts its product token.
    int32_t major = 0;
    int32_t minor = 0;
    int32_t bugfix = 0;
    base::SysInfo::OperatingSystemVersionNumbers(&major, &minor, &bugfix);
    std::string product = base::StringPrintf("Version/%d.%d", major, minor);

    command_line->AppendSwitchASCII(switches::kUserAgent,
                                    web::BuildMobileUserAgent(product));
  }

  // Shared variables for all enterprise experimental flags.
  NSMutableDictionary* testing_policies = CreateExperimentalTestingPolicies();

  // If a CBCM enrollment token is provided, force Chrome Browser Cloud
  // Management to enabled and add the token to the list of policies.
  NSString* token_key =
      base::SysUTF8ToNSString(policy::key::kCloudManagementEnrollmentToken);
  NSString* token = [defaults stringForKey:token_key];

  if ([token length] > 0) {
    command_line->AppendSwitch(switches::kEnableChromeBrowserCloudManagement);
    [testing_policies setValue:token forKey:token_key];
  }

  if ([defaults boolForKey:@"EnableUserPolicy"]) {
    policy::EnableUserPolicy();
  }

  // If some policies were set, commit them to the app's registration defaults.
  if ([testing_policies count] > 0) {
    NSDictionary* registration_defaults =
        @{kPolicyLoaderIOSConfigurationKey : testing_policies};
    [defaults registerDefaults:registration_defaults];
  }

  // Freeform commandline flags.  These are added last, so that any flags added
  // earlier in this function take precedence.
  if ([defaults boolForKey:@"EnableFreeformCommandLineFlags"]) {
    base::CommandLine::StringVector flags;
    // Append an empty "program" argument.
    flags.push_back("");

    // The number of flags corresponds to the number of text fields in
    // Experimental.plist.
    const int kNumFreeformFlags = 5;
    for (int i = 1; i <= kNumFreeformFlags; ++i) {
      NSString* key =
          [NSString stringWithFormat:@"FreeformCommandLineFlag%d", i];
      NSString* flag = [defaults stringForKey:key];
      if ([flag length]) {
        // iOS keyboard replaces -- with —, so undo that.
        flag = [flag stringByReplacingOccurrencesOfString:@"—"
                                               withString:@"--"
                                                  options:0
                                                    range:NSMakeRange(0, 1)];
        // To make things easier, allow flags with no dashes by prepending them
        // here. This also allows for flags that just have one dash if they
        // exist.
        if (![flag hasPrefix:@"-"]) {
          flag = [@"--" stringByAppendingString:flag];
        }
        flags.push_back(base::SysNSStringToUTF8(flag));
      }
    }

    base::CommandLine temp_command_line(flags);
    command_line->AppendArguments(temp_command_line, false);
  }

  // Populate command line flag for 3rd party keyboard omnibox workaround.
  NSString* enableThirdPartyKeyboardWorkaround =
      [defaults stringForKey:@"EnableThirdPartyKeyboardWorkaround"];
  if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Enabled"]) {
    command_line->AppendSwitch(switches::kEnableThirdPartyKeyboardWorkaround);
  } else if ([enableThirdPartyKeyboardWorkaround isEqualToString:@"Disabled"]) {
    command_line->AppendSwitch(switches::kDisableThirdPartyKeyboardWorkaround);
  }

  ios::provider::AppendSwitchesFromExperimentalSettings(defaults, command_line);
}

void MonitorExperimentalSettingsChanges() {
  // Startup values for settings to be observed.
  __block NSString* hash = TestingPoliciesHash();
  static std::atomic_bool pending_check(false);

  auto monitor = ^(NSNotification* notification) {
    bool has_pending_check = pending_check.exchange(true);
    if (has_pending_check)
      return;

    // Can be called from any thread from where the notification was sent,
    // but since it may change standardUserDefaults, and that has to be on main
    // thread, dispatch to main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      // Check if observed settings have changed. Since source and destination
      // are both user defaults, this is required to avoid cycling back here.
      NSString* newHash = TestingPoliciesHash();
      if (![newHash isEqualToString:hash]) {
        hash = newHash;

        // Publish update.
        NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
        NSMutableDictionary* testing_policies =
            CreateExperimentalTestingPolicies();
        NSDictionary* registration_defaults =
            @{kPolicyLoaderIOSConfigurationKey : testing_policies};
        [defaults registerDefaults:registration_defaults];
      }

      pending_check.store(false);
    });
  };

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
  [center addObserverForName:NSUserDefaultsDidChangeNotification
                      object:nil
                       queue:nil
                  usingBlock:monitor];
}

void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line) {
  GetGlobalFlagsState().ConvertFlagsToSwitches(
      flags_storage, command_line, flags_ui::kAddSentinels,
      switches::kEnableFeatures, switches::kDisableFeatures);
}

std::vector<std::string> RegisterAllFeatureVariationParameters(
    flags_ui::FlagsStorage* flags_storage,
    base::FeatureList* feature_list) {
  // Occasionally DCHECK crashes on canary can be very distuptive.  An
  // experimental flag was added to aid in temporarily disabling this for
  // canary testers.
#if defined(DCHECK_IS_CONFIGURABLE)
  if (experimental_flags::AreDCHECKCrashesDisabled()) {
    std::vector<base::FeatureList::FeatureOverrideInfo> overrides;
    overrides.push_back(
        {std::cref(base::kDCheckIsFatalFeature),
         base::FeatureList::OverrideState::OVERRIDE_DISABLE_FEATURE});
    feature_list->RegisterExtraFeatureOverrides(std::move(overrides));
  }
#endif  // defined(DCHECK_IS_CONFIGURABLE)

  return GetGlobalFlagsState().RegisterAllFeatureVariationParameters(
      flags_storage, feature_list);
}

void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           flags_ui::FlagAccess access,
                           base::Value::ListStorage& supported_entries,
                           base::Value::ListStorage& unsupported_entries) {
  GetGlobalFlagsState().GetFlagFeatureEntries(
      flags_storage, access, supported_entries, unsupported_entries,
      base::BindRepeating(&SkipConditionalFeatureEntry));
}

void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable) {
  GetGlobalFlagsState().SetFeatureEntryEnabled(flags_storage, internal_name,
                                               enable);
}

void ResetAllFlags(flags_ui::FlagsStorage* flags_storage) {
  GetGlobalFlagsState().ResetAllFlags(flags_storage);
}

namespace testing {

base::span<const flags_ui::FeatureEntry> GetFeatureEntries() {
  return base::span<const flags_ui::FeatureEntry>(kFeatureEntries,
                                                  std::size(kFeatureEntries));
}

}  // namespace testing
