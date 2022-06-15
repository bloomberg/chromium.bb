// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
#define IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_

#include "Availability.h"

// Please add names and descriptions in alphabetical order.

namespace flag_descriptions {

// Title and description for the flag to enable
// kSupportForAddPasswordsInSettings flag on iOS.
extern const char kAddPasswordsInSettingsName[];
extern const char kAddPasswordsInSettingsDescription[];

// Title and description for the flag to control upstreaming credit cards.
extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

// Title and description for the flag to control the new autofill suggestion
// ranking formula.
extern const char kAutofillEnableRankingFormulaName[];
extern const char kAutofillEnableRankingFormulaDescription[];

// Title and description for the flag enable sending billing customer number in
// GetUploadDetails preflight call.
extern const char kAutofillEnableSendingBcnInGetUploadDetailsName[];
extern const char kAutofillEnableSendingBcnInGetUploadDetailsDescription[];

// Title and description for flag to enable sending non-legacy instrument ID in
// UnmaskCardRequest.
extern const char kAutofillEnableUnmaskCardRequestSetInstrumentIdName[];
extern const char kAutofillEnableUnmaskCardRequestSetInstrumentIdDescription[];

// Title and description for flag to enforce delays between offering Autofill
// opportunities.
extern const char kAutofillEnforceDelaysInStrikeDatabaseName[];
extern const char kAutofillEnforceDelaysInStrikeDatabaseDescription[];

// Title and description for the flag to fill promo code fields with Autofill.
extern const char kAutofillFillMerchantPromoCodeFieldsName[];
extern const char kAutofillFillMerchantPromoCodeFieldsDescription[];

// Title and description for the flag to control the autofill delay.
extern const char kAutofillIOSDelayBetweenFieldsName[];
extern const char kAutofillIOSDelayBetweenFieldsDescription[];

// Title and description for the flag to parse promo code fields in Autofill.
extern const char kAutofillParseMerchantPromoCodeFieldsName[];
extern const char kAutofillParseMerchantPromoCodeFieldsDescription[];

// Title and description for the flag that controls whether the maximum number
// of Autofill suggestions shown is pruned.
extern const char kAutofillPruneSuggestionsName[];
extern const char kAutofillPruneSuggestionsDescription[];

// Title and description for the flag to control removing card expiration date
// from the downstream suggestions.
extern const char kAutofillRemoveCardExpiryFromDownstreamSuggestionName[];
extern const char
    kAutofillRemoveCardExpiryFromDownstreamSuggestionDescription[];

// Title and description for the flag to control dismissing the Save Card
// Infobar on Navigation.
extern const char kAutofillSaveCardDismissOnNavigationName[];
extern const char kAutofillSaveCardDismissOnNavigationDescription[];

// Title and description for the flag to control allowing credit card upload
// save for accounts from common email providers.
extern const char kAutofillUpstreamAllowAdditionalEmailDomainsName[];
extern const char kAutofillUpstreamAllowAdditionalEmailDomainsDescription[];

// Title and description for the flag to control allowing credit card upload
// save for all accounts, regardless of the email domain.
extern const char kAutofillUpstreamAllowAllEmailDomainsName[];
extern const char kAutofillUpstreamAllowAllEmailDomainsDescription[];

// Title and description for the flag that controls whether Autofill's
// suggestions' labels are formatting with a mobile-friendly approach.
extern const char kAutofillUseMobileLabelDisambiguationName[];
extern const char kAutofillUseMobileLabelDisambiguationDescription[];

// Title and description for the flag that controls whether Autofill's
// logic is using numeric unique renderer IDs instead of string IDs for
// form and field elements.
extern const char kAutofillUseRendererIDsName[];
extern const char kAutofillUseRendererIDsDescription[];

// Title and description for the flag to control if initial uploading of crash
// reports is delayed.
extern const char kBreakpadNoDelayInitialUploadName[];
extern const char kBreakpadNoDelayInitialUploadDescription[];

// Title and description for the flag to enable the rich iph experiment on
// bubble views.
extern const char kBubbleRichIPHName[];
extern const char kBubbleRichIPHDescription[];

// Title and description for the flag to change the string of the "Bookmark"
// action in the overflow menu.
extern const char kBookmarkStringName[];
extern const char kBookmarkStringDescription[];

// Title and description for the flag to enable the use of apple calendar in
// experience kit calendar.
extern const char kEnableExpKitAppleCalendarName[];
extern const char kEnableExpKitAppleCalendarDescription[];

// Title and description for the flag to enable experience kit calendar events.
extern const char kCalendarExperienceKitName[];
extern const char kCalendarExperienceKitDescription[];

// Title and description for the flag that moves the Content Suggestions header
// to the Discover feed ScrollView.
extern const char kContentSuggestionsHeaderMigrationName[];
extern const char kContentSuggestionsHeaderMigrationDescription[];

// Title and description for the flag that updates the Content Suggestions to a
// new module design.
extern const char kContentSuggestionsUIModuleRefreshName[];
extern const char kContentSuggestionsUIModuleRefreshDescription[];

// Title and description for the flag that moves the Content Suggestions to a
// UIViewController.
extern const char kContentSuggestionsUIViewControllerMigrationName[];
extern const char kContentSuggestionsUIViewControllerMigrationDescription[];

// Title and description for the flag to control which crash generation tool
// is used.
extern const char kCrashpadIOSName[];
extern const char kCrashpadIOSDescription[];

#if defined(DCHECK_IS_CONFIGURABLE)
// Title and description for the flag to enable configurable DCHECKs.
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

// Title and description for the flag to show a modified fullscreen modal promo
// with a button that would send the users in the Settings.app to update the
// default browser.
extern const char kDefaultBrowserFullscreenPromoExperimentName[];
extern const char kDefaultBrowserFullscreenPromoExperimentDescription[];

// Title and description for the flag to show the default browser tutorial from
// an external app.
extern const char kDefaultBrowserIntentsShowSettingsName[];
extern const char kDefaultBrowserIntentsShowSettingsDescription[];

// Title and description for the flag that is used to let the user choose the
// default mode (Mobile/Desktop) they would like to use when requesting a page.
extern const char kAddSettingForDefaultPageModeName[];
extern const char kAddSettingForDefaultPageModeDescription[];

// Title and description for the flag to use default WebKit context menu in web
// content.
extern const char kDefaultWebViewContextMenuName[];
extern const char kDefaultWebViewContextMenuDescription[];

// Title and description for the flag to control the delay (in minutes) for
// polling for the existence of Gaia cookies for google.com.
extern const char kDelayThresholdMinutesToUpdateGaiaCookieName[];
extern const char kDelayThresholdMinutesToUpdateGaiaCookieDescription[];

// Title and description for the flag to control if a crash report is generated
// on main thread freeze.
extern const char kDetectMainThreadFreezeName[];
extern const char kDetectMainThreadFreezeDescription[];

// Title and description for the flag to replace the Zine feed with the
// Discover feed in the Bling NTP.
extern const char kDiscoverFeedInNtpName[];
extern const char kDiscoverFeedInNtpDescription[];

// Title and description for the flag to enable the sync promotion on top of the
// discover feed.
extern const char kEnableDiscoverFeedTopSyncPromoName[];
extern const char kEnableDiscoverFeedTopSyncPromoDescription[];

// Title and description for the flag to enable DMToken deletion.
extern const char kDmTokenDeletionName[];
extern const char kDmTokenDeletionDescription[];

// Title and description for the flag to enable Calendar support.
extern const char kDownloadCalendarName[];
extern const char kDownloadCalendarDescription[];

// Title and description for the flag to enable kEditPasswordsInSettings flag on
// iOS.
extern const char kEditPasswordsInSettingsName[];
extern const char kEditPasswordsInSettingsDescription[];

// Title and description for the flag to enable kEnhancedProtection flag on
// iOS.
extern const char kEnhancedProtectionName[];
extern const char kEnhancedProtectionDescription[];

// Title and description for the flag to enable kEnhancedProtectionPhase2
// flag on iOS.
extern const char kEnhancedProtectionPhase2Name[];
extern const char kEnhancedProtectionPhase2Description[];

// Title and description for the flag to enable address verification support in
// autofill address save prompts.
extern const char kEnableAutofillAddressSavePromptAddressVerificationName[];
extern const char
    kEnableAutofillAddressSavePromptAddressVerificationDescription[];

// Title and description for the flag to enable autofill address save prompts.
extern const char kEnableAutofillAddressSavePromptName[];
extern const char kEnableAutofillAddressSavePromptDescription[];

// Title and description for the flag to enable the discover feed discofeed
// endpoint.
extern const char kEnableDiscoverFeedDiscoFeedEndpointName[];
extern const char kEnableDiscoverFeedDiscoFeedEndpointDescription[];

// Title and description for the flag to enable the discover feed live preview
// in long-press feed context menu.
extern const char kEnableDiscoverFeedPreviewName[];
extern const char kEnableDiscoverFeedPreviewDescription[];

// Title and description for the flag to show ghost cards when refreshing the
// discover feed.
extern const char kEnableDiscoverFeedGhostCardsName[];
extern const char kEnableDiscoverFeedGhostCardsDescription[];

// Title and description for the flag to shorten the cache.
extern const char kEnableDiscoverFeedShorterCacheName[];
extern const char kEnableDiscoverFeedShorterCacheDescription[];

// Title and description for the flag to enable the discover feed static
// resource serving.
extern const char kEnableDiscoverFeedStaticResourceServingName[];
extern const char kEnableDiscoverFeedStaticResourceServingDescription[];

// Title and description for the flag to enable favicon for the Password
// Manager.
extern const char kEnableFaviconForPasswordsName[];
extern const char kEnableFaviconForPasswordsDescription[];

// Title and description for the flag to remove the Feed from the NTP.
extern const char kEnableFeedAblationName[];
extern const char kEnableFeedAblationDescription[];

// Title and description for the flag to test the FRE default browser promo
// experiment.
extern const char kEnableFREDefaultBrowserPromoScreenName[];
extern const char kEnableFREDefaultBrowserPromoScreenDescription[];

// Title and description for the flag to enable FRE UI module.
extern const char kEnableFREUIModuleIOSName[];
extern const char kEnableFREUIModuleIOSDescription[];

// Title and description for the flag to enable the Fullscreen API.
extern const char kEnableFullscreenAPIName[];
extern const char kEnableFullscreenAPIDescription[];

// Title and description for the flag to enable long message duration.
extern const char kEnableLongMessageDurationName[];
extern const char kEnableLongMessageDurationDescription[];

// Title and description for the flag to enable the new download API.
extern const char kEnableNewDownloadAPIName[];
extern const char kEnableNewDownloadAPIDescription[];

// Title and description for the flag to enable shorted password auto-fill
// instructions and button.
extern const char kEnableShortenedPasswordAutoFillInstructionName[];
extern const char kEnableShortenedPasswordAutoFillInstructionDescription[];

// Title and description for the flag to enable omnibox suggestions scrolling on
// iPad.
extern const char kEnableSuggestionsScrollingOnIPadName[];
extern const char kEnableSuggestionsScrollingOnIPadDescription[];

// Title and description for the flag to introduce following web channels on
// Chrome iOS.
extern const char kEnableWebChannelsName[];
extern const char kEnableWebChannelsDescription[];

// Title and description for the flag to enable an expanded tab strip.
extern const char kExpandedTabStripName[];
extern const char kExpandedTabStripDescription[];

// Title and description for the flag to enable filling across affiliated
// websites.
extern const char kFillingAcrossAffiliatedWebsitesName[];
extern const char kFillingAcrossAffiliatedWebsitesDescription[];

// Title and description for the flag to trigger the startup sign-in promo.
extern const char kForceStartupSigninPromoName[];
extern const char kForceStartupSigninPromoDescription[];

// Title and description for the flag to enable sign-in with a Unicorn account.
extern const char kEnableUnicornAccountSupportName[];
extern const char kEnableUnicornAccountSupportDescription[];

// Title and description for the command line switch used to determine the
// active fullscreen viewport adjustment mode.
extern const char kFullscreenSmoothScrollingName[];
extern const char kFullscreenSmoothScrollingDescription[];

// Title and description for the flag to enable HTTPS-Only Mode setting.
extern const char kHttpsOnlyModeName[];
extern const char kHttpsOnlyModeDescription[];

// Title and description for the flag to enable dark mode colors while in
// Incognito mode.
extern const char kIncognitoBrandConsistencyForIOSName[];
extern const char kIncognitoBrandConsistencyForIOSDescription[];

// Title and description for the flag to enable revamped Incognito NTP page.
extern const char kIncognitoNtpRevampName[];
extern const char kIncognitoNtpRevampDescription[];

// Title and description for the flag that conditionally uploads clicks and view
// actions in the feed (e.g., the user needs to view X cards).
extern const char kInterestFeedV2ClickAndViewActionsConditionalUploadName[];
extern const char
    kInterestFeedV2ClickAndViewActionsConditionalUploadDescription[];

// Title and description for the flag to enable feature_engagement::Tracker
// demo mode.
extern const char kInProductHelpDemoModeName[];
extern const char kInProductHelpDemoModeDescription[];

// Title and description for the flag to enable third-party intents in
// Incognito.
extern const char kIOS3PIntentsInIncognitoName[];
extern const char kIOS3PIntentsInIncognitoDescription[];

// Title and description for the flag to enable updated password manager
// branding.
extern const char kIOSEnablePasswordManagerBrandingUpdateName[];
extern const char kIOSEnablePasswordManagerBrandingUpdateDescription[];

// Title and description for the flag to enable Shared Highlighting color
// change in iOS.
extern const char kIOSSharedHighlightingColorChangeName[];
extern const char kIOSSharedHighlightingColorChangeDescription[];

// Title and description for the flag to enable Shared Highlighting on AMP pages
// in iOS.
extern const char kIOSSharedHighlightingAmpName[];
extern const char kIOSSharedHighlightingAmpDescription[];

// Title and description for the flag to enable browser-layer improvements to
// the text fragments UI.
extern const char kIOSSharedHighlightingV2Name[];
extern const char kIOSSharedHighlightingV2Description[];

// Title and description for the flag to enable unrealized WebStates.
extern const char kLazilyCreateWebStateOnRestorationName[];
extern const char kLazilyCreateWebStateOnRestorationDescription[];

// Title and description for the flag to enable leak detection for signed out
// users.
extern const char kLeakDetectionUnauthenticatedName[];
extern const char kLeakDetectionUnauthenticatedDescription[];

// Title and description for the flag to lock the bottom toolbar into place.
extern const char kLockBottomToolbarName[];
extern const char kLockBottomToolbarDescription[];

// Title and description for the flag that controls whether event breadcrumbs
// are captured.
extern const char kLogBreadcrumbsName[];
extern const char kLogBreadcrumbsDescription[];

// Title and description for the flag to control camera and/or microphone access
// for a specific site through site settings during its lifespan.
extern const char kMediaPermissionsControlName[];
extern const char kMediaPermissionsControlDescription[];

// Title and description for the flag that controls sending metrickit crash
// reports.
extern const char kMetrickitCrashReportName[];
extern const char kMetrickitCrashReportDescription[];

// TODO(crbug.com/1128242): Remove this flag after the refactoring work is
// finished.
// Title and description for the flag used to test the newly
// implemented tabstrip.
extern const char kModernTabStripName[];
extern const char kModernTabStripDescription[];

// Title and description for the flag to enable muting/unmuting compromised
// passwords in bulk leak check.
extern const char kMuteCompromisedPasswordsName[];
extern const char kMuteCompromisedPasswordsDescription[];

// Title and description for the flag to use the new MICe FRE.
extern const char kNewMobileIdentityConsistencyFREName[];
extern const char kNewMobileIdentityConsistencyFREDescription[];

// Title and description for the flag to add a Clear Browsing Data action to
// the new overflow menu.
extern const char kNewOverflowMenuCBDActionName[];
extern const char kNewOverflowMenuCBDActionDescription[];

// Title and description for the flag to enable the new overflow menu.
extern const char kNewOverflowMenuName[];
extern const char kNewOverflowMenuDescription[];

// Title and description for the flag to add a Settings action to the new
// overflow menu.
extern const char kNewOverflowMenuSettingsActionName[];
extern const char kNewOverflowMenuSettingsActionDescription[];

// Title and description for the flag to use simple icons for the Destinations
// in the new overflow menu.
extern const char kNewOverflowMenuSimpleDestinationIconsName[];
extern const char kNewOverflowMenuSimpleDestinationIconsDescription[];

// Title and description for temporary bug fix to broken NTP view hierarhy.
// TODO(crbug.com/1262536): Remove this when fixed.
extern const char kNTPViewHierarchyRepairName[];
extern const char kNTPViewHierarchyRepairDescription[];

// Title and description for the flag to change the max number of autocomplete
// matches in the omnibox popup.
extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

// Title and description for the flag to change the max number of ZPS
// matches in the omnibox popup.
extern const char kOmniboxMaxZPSMatchesName[];
extern const char kOmniboxMaxZPSMatchesDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[];

// Title and description for the flag to enable Omnibox On Device Head
// suggestions (non incognito).
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[];
extern const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[];

// Title and description for the flag to control Omnibox on-focus suggestions.
extern const char kOmniboxOnFocusSuggestionsName[];
extern const char kOmniboxOnFocusSuggestionsDescription[];

// Title and description for the flag to control Omnibox Local zero-prefix
// suggestions.
extern const char kOmniboxLocalHistoryZeroSuggestName[];
extern const char kOmniboxLocalHistoryZeroSuggestDescription[];

// Title and description for the flag to swap Omnibox Textfield implementation
// to a new experimental one.
extern const char kOmniboxNewImplementationName[];
extern const char kOmniboxNewImplementationDescription[];

// Title and description for the flag to enable the SwiftUI omnibox popup
// implementation.
extern const char kIOSOmniboxUpdatedPopupUIName[];
extern const char kIOSOmniboxUpdatedPopupUIDescription[];

// Title and description for the flag to enable TFLite model downloading.
extern const char kOptimizationGuideModelDownloadingName[];
extern const char kOptimizationGuideModelDownloadingDescription[];

// Title and description for the flag to enable the prediction of optimization
// targets.
extern const char kOptimizationTargetPredictionDescription[];
extern const char kOptimizationTargetPredictionName[];

// Title and description for the flag to enable PhishGuard password reuse
// detection.
extern const char kPasswordReuseDetectionName[];
extern const char kPasswordReuseDetectionDescription[];

// Title and description for the flag to native restore web states.
extern const char kRestoreSessionFromCacheName[];
extern const char kRestoreSessionFromCacheDescription[];

extern const char kRecordSnapshotSizeName[];
extern const char kRecordSnapshotSizeDescription[];

// Title and description for the flag to remove excess NTP tabs that don't have
// navigation history.
extern const char kRemoveExcessNTPsExperimentName[];
extern const char kRemoveExcessNTPsExperimentDescription[];

// Title and description for the flag that makes Safe Browsing available.
extern const char kSafeBrowsingAvailableName[];
extern const char kSafeBrowsingAvailableDescription[];

// Title and description for the flag to enable real-time Safe Browsing lookups.
extern const char kSafeBrowsingRealTimeLookupName[];
extern const char kSafeBrowsingRealTimeLookupDescription[];

// Title and description for the flag to enable saving one file per webstate.
extern const char kSaveSessionTabsToSeparateFilesName[];
extern const char kSaveSessionTabsToSeparateFilesDescription[];

// Title and description for the flag to enable integration with the ScreenTime
// system.
extern const char kScreenTimeIntegrationName[];
extern const char kScreenTimeIntegrationDescription[];

// Title and description for the flag to show a sign-in promo if the user tries
// to use send-tab-to-self while being signed-out.
extern const char kSendTabToSelfSigninPromoName[];
extern const char kSendTabToSelfSigninPromoDescription[];

// Title and description for the flag to send UMA data over any network.
extern const char kSendUmaOverAnyNetwork[];
extern const char kSendUmaOverAnyNetworkDescription[];

// Title and description for the flag to enable Shared Highlighting (Link to
// Text Edit Menu option).
extern const char kSharedHighlightingIOSName[];
extern const char kSharedHighlightingIOSDescription[];

// Title and description for the flag to enable annotating web forms with
// Autofill field type predictions as placeholder.
extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

// Title and description for the flag to use one cell for the Content
// Suggestions
extern const char kSingleCellContentSuggestionsName[];
extern const char kSingleCellContentSuggestionsDescription[];

// Title and description for the flag to use one NTP for all tabs in a Browser.
extern const char kSingleNtpName[];
extern const char kSingleNtpDescription[];

// Title and description for the flag to enable smart sorting the new overflow
// menu.
extern const char kSmartSortingNewOverflowMenuName[];
extern const char kSmartSortingNewOverflowMenuDescription[];

// Title and description for the flag to enable the Start Surface.
extern const char kStartSurfaceName[];
extern const char kStartSurfaceDescription[];

// Title and description for the flag to control if Chrome Sync should use the
// sandbox servers.
extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

// Title and description for the flag that controls synthetic crash reports
// generation for Unexplained Termination Events.
extern const char kSyntheticCrashReportsForUteName[];
extern const char kSyntheticCrashReportsForUteDescription[];

// Title and description for the flag to synthesize native restore web states.
extern const char kSynthesizedRestoreSessionName[];
extern const char kSynthesizedRestoreSessionDescription[];

// Title and description for the flag to control if Chrome Sync should support
// trusted vault RPC.
extern const char kSyncTrustedVaultPassphraseiOSRPCName[];
extern const char kSyncTrustedVaultPassphraseiOSRPCDescription[];

// Title and description for the flag to control if Chrome Sync should support
// trusted vault passphrase promos.
extern const char kSyncTrustedVaultPassphrasePromoName[];
extern const char kSyncTrustedVaultPassphrasePromoDescription[];

// Title and description for the flag to control if Chrome Sync should support
// trusted vault passphrase type with improved recovery.
extern const char kSyncTrustedVaultPassphraseRecoveryName[];
extern const char kSyncTrustedVaultPassphraseRecoveryDescription[];

// Title and description for the flag to enable Sync standalone invalidations.
extern const char kSyncInvalidationsName[];
extern const char kSyncInvalidationsDescription[];

// Title and description for the flag to enable Sync standalone invalidations
// for the Wallet and Offer data types.
extern const char kSyncInvalidationsWalletAndOfferName[];
extern const char kSyncInvalidationsWalletAndOfferDescription[];

// Title and description for the flag to enable tabs search feature.
extern const char kTabsSearchName[];
extern const char kTabsSearchDescription[];

// Title and description for the flag to enable tabs search regular results
// suggested actions feature.
extern const char kTabsSearchRegularResultsSuggestedActionsName[];
extern const char kTabsSearchRegularResultsSuggestedActionsDescription[];

// Title and description for the flag to enable TFLite for language detection.
extern const char kTFLiteLanguageDetectionName[];
extern const char kTFLiteLanguageDetectionDescription[];

// Title and description for the flag to enable the toolbar container
// implementation.
extern const char kToolbarContainerName[];
extern const char kToolbarContainerDescription[];

// Title and description for the flag to enable -[UIView window] observing.
extern const char kUIViewWindowObservingName[];
extern const char kUIViewWindowObservingDescription[];

// Title and description for the flag to enable removing any entry points to the
// history UI from Incognito mode.
extern const char kUpdateHistoryEntryPointsInIncognitoName[];
extern const char kUpdateHistoryEntryPointsInIncognitoDescription[];

// Title and description for the flag to enable using Lens to search for an
// image from the long press context menu.
extern const char kUseLensToSearchForImageName[];
extern const char kUseLensToSearchForImageDescription[];

// Title and description for the flag to enable using the
// loadSimulatedRequest:responseHTMLString: API for displaying error pages in
// CRWWKNavigationHandler.
extern const char kUseLoadSimulatedRequestForOfflinePageName[];
extern const char kUseLoadSimulatedRequestForOfflinePageDescription[];

// Title and description for the flag to enable the replacement of images
// by SFSymbols.
extern const char kUseSFSymbolsName[];
extern const char kUseSFSymbolsDescription[];

// Title and description for the flag to control the maximum wait time (in
// seconds) for a response from the Account Capabilities API.
extern const char kWaitThresholdMillisecondsForCapabilitiesApiName[];
extern const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[];

// Title and description for the flag to control if Google Payments API calls
// should use the sandbox servers.
extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

// Title and description for the flag to tie the default text zoom level to
// the dynamic type setting.
extern const char kWebPageDefaultZoomFromDynamicTypeName[];
extern const char kWebPageDefaultZoomFromDynamicTypeDescription[];

// Title and description for the flag to enable a different method of zooming
// web pages.
extern const char kWebPageAlternativeTextZoomName[];
extern const char kWebPageAlternativeTextZoomDescription[];

// Title and description for the flag to (re)-enable text zoom on iPad.
extern const char kWebPageTextZoomIPadName[];
extern const char kWebPageTextZoomIPadDescription[];

// Please add names and descriptions above in alphabetical order.

}  // namespace flag_descriptions

#endif  // IOS_CHROME_BROWSER_FLAGS_IOS_CHROME_FLAG_DESCRIPTIONS_H_
