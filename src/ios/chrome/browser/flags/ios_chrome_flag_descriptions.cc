// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/flags/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kAddPasswordsInSettingsName[] = "Add passwords in settings";
const char kAddPasswordsInSettingsDescription[] =
    "Enables manually creating password credentials from the settings";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillEnableOffersInDownstreamName[] =
    "Enable Autofill offers in downstream";
const char kAutofillEnableOffersInDownstreamDescription[] =
    "When enabled, offer data will be retrieved during downstream and shown in "
    "the dropdown list.";

const char kAutofillFillMerchantPromoCodeFieldsName[] =
    "Enable Autofill of promo code fields in forms";
const char kAutofillFillMerchantPromoCodeFieldsDescription[] =
    "When enabled, Autofill will attempt to fill merchant promo/coupon/gift "
    "code fields when data is available.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillParseMerchantPromoCodeFieldsName[] =
    "Parse promo code fields in forms";
const char kAutofillParseMerchantPromoCodeFieldsDescription[] =
    "When enabled, Autofill will attempt to find merchant promo/coupon/gift "
    "code fields when parsing forms.";

const char kAutofillPruneSuggestionsName[] = "Autofill Prune Suggestions";
const char kAutofillPruneSuggestionsDescription[] =
    "Further limits the number of suggestions in the Autofill dropdown.";

const char kAutofillSaveCardDismissOnNavigationName[] =
    "Save Card Dismiss on Navigation";
const char kAutofillSaveCardDismissOnNavigationDescription[] =
    "Dismisses the Save Card Infobar on a user initiated Navigation, other "
    "than one caused by submitted form.";

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

const char kAutofillUseRendererIDsName[] =
    "Autofill logic uses unqiue renderer IDs";
const char kAutofillUseRendererIDsDescription[] =
    "When enabled, Autofill logic uses unique numeric renderer IDs instead "
    "of string form and field identifiers in form filling logic.";

extern const char kLogBreadcrumbsName[] = "Log Breadcrumb Events";
extern const char kLogBreadcrumbsDescription[] =
    "When enabled, breadcrumb events will be logged.";

const char kSyntheticCrashReportsForUteName[] =
    "Generate synthetic crash reports for UTE";
const char kSyntheticCrashReportsForUteDescription[] =
    "When enabled the app will create synthetic crash report when chrome "
    "starts up after Unexplained Termination Event (UTE).";

const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

const char kCommercePriceTrackingName[] = "Price Tracking";
const char kCommercePriceTrackingDescription[] =
    "Allows users to track product prices through Chrome.";

const char kContextMenuActionsRefreshName[] = "Context menu actions refresh";
const char kContextMenuActionsRefreshDescription[] =
    "When enabled, context menu actions are updated";

const char kCrashpadIOSName[] = "Use Crashpad for crash collection.";
const char kCrashpadIOSDescription[] =
    "When enabled use Crashpad to generate crash reports crash collection. "
    "When disabled use Breakpad. This flag takes two restarts to take effect";

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

const char kUseDefaultUserAgentInWebClientName[] =
    "[Refactoring] Let the client choose default mode";
const char kUseDefaultUserAgentInWebClientDescription[] =
    "When enabled, the web client will choose the default mode. It is a "
    "refactoring, no visible impact.";

const char kDefaultBrowserFullscreenPromoExperimentName[] =
    "Default Browser Fullscreen modal experiment";
const char kDefaultBrowserFullscreenPromoExperimentDescription[] =
    "When enabled, will show a modified default browser fullscreen modal promo "
    "UI.";

const char kDefaultWebViewContextMenuName[] =
    "Use the default WebKit context menus";
const char kDefaultWebViewContextMenuDescription[] =
    "When enabled, the default context menus from WebKit will be used in web "
    "content.";

const char kDelayThresholdMinutesToUpdateGaiaCookieName[] =
    "Delay for polling (in minutes) to verify the existence of GAIA cookies.";
const char kDelayThresholdMinutesToUpdateGaiaCookieDescription[] =
    "Used for testing purposes to reduce the amount of delay between polling "
    "intervals.";

const char kDetectFormSubmissionOnFormClearIOSName[] =
    "Detect form submission when the form is cleared.";
const char kDetectFormSubmissionOnFormClearIOSDescription[] =
    "Detect form submissions for change password forms that are cleared and "
    "not removed from the page.";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDiscoverFeedInNtpName[] = "Enable new content Suggestion Feed";
const char kDiscoverFeedInNtpDescription[] =
    "When enabled, replaces articles feed with new content Suggestion Feed in "
    "the NTP.";

const char kDownloadMobileConfigFileName[] = "Download .mobileconfig file";
const char kDownloadMobileConfigFileDescription[] =
    "Allows user to download and install a .mobileconfig file";

const char kDownloadVcardName[] = "Download Vcard";
const char kDownloadVcardDescription[] = "Allows user to download & open Vcard";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kRestoreSessionFromCacheName[] =
    "Use native WKWebView sesion restoration (iOS15 only).";
const char kRestoreSessionFromCacheDescription[] =
    "Enable instant session restoration for faster web session restoration "
    "(iOS15 only).";

const char kEnableAutofillAccountWalletStorageName[] =
    "Enable the account data storage for autofill";
const char kEnableAutofillAccountWalletStorageDescription[] =
    "Enable the ephemeral storage for account data for autofill.";

const char kEnableAutofillAddressSavePromptAddressVerificationName[] =
    "Autofill Address Save Prompts Address Verification";
const char kEnableAutofillAddressSavePromptAddressVerificationDescription[] =
    "Enable the address verification support in Autofill address save prompts.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnableAutofillSaveCardInfoBarAccountIndicationFooterName[] =
    "Display save card infobar with account indication information";
const char kEnableAutofillSaveCardInfoBarAccountIndicationFooterDescription[] =
    "When enabled, UI indicating user's e-mail address will appear at the"
    "bottom of save card infobar.";

const char kEnableDiscoverFeedDiscoFeedEndpointName[] =
    "Enable discover feed discofeed";
const char kEnableDiscoverFeedDiscoFeedEndpointDescription[] =
    "Enable using the discofeed endpoint for the discover feed.";

const char kEnableDiscoverFeedPreviewName[] = "Enable discover feed preview";
const char kEnableDiscoverFeedPreviewDescription[] =
    "Enable showing a live preview for discover feed long-press menu.";

const char kEnableDiscoverFeedStaticResourceServingName[] =
    "Enable discover feed static resource serving";
const char kEnableDiscoverFeedStaticResourceServingDescription[] =
    "When enabled the discover feed will optimize the request of resources "
    "coming from the server.";

const char kEnableFREDefaultBrowserScreenName[] =
    "Enable FRE default browser screen";
const char kEnableFREDefaultBrowserScreenDescription[] =
    "When enabled, displays the 'default browser' screen in the new FRE which "
    "suggests changing the default browser in the Settings.app with a button "
    "to go directly to it.";

const char kEnableFREDefaultBrowserScreenTestingName[] =
    "Enable FRE default browser screen testing";
const char kEnableFREDefaultBrowserScreenTestingDescription[] =
    "This test display the FRE default browser screen and other default "
    "browser promo depending on experiment.";

const char kEnableFREUIModuleIOSName[] = "Enable FRE UI module";
const char kEnableFREUIModuleIOSDescription[] =
    "Enable the option of using new FRE UI module to show first run screens.";

const char kEnableLongMessageDurationName[] = "Enable long message duration";
const char kEnableLongMessageDurationDescription[] =
    "Enables a long duration when an overlay message is shown.";

const char kEnableManualPasswordGenerationName[] =
    "Enable manual password generation.";
const char kEnableManualPasswordGenerationDescription[] =
    "Enable UI that allows to generate a strong password for any password "
    "field";

const char kEnableNTPMemoryEnhancementName[] = "Enable NTP Memory Enhancement";
const char kEnableNTPMemoryEnhancementDescription[] =
    "NTP will consume less memory.";

const char kEnableOptimizationGuideName[] = "Enable optimization guide";
const char kEnableOptimizationGuideDescription[] =
    "Enables the optimization guide to provide intelligence for page loads.";

const char kEnableOptimizationGuideMetadataValidationName[] =
    "Enable optimization guide metadata validation";
const char kEnableOptimizationGuideMetadataValidationDescription[] =
    "Enables the validation of optimization guide metadata fetch and "
    "allowlist/blocklist bloom filter.";

const char kEnableOptimizationHintsFetchingMSBBName[] =
    "Enable MSBB optimization hints fetching";
const char kEnableOptimizationHintsFetchingMSBBDescription[] =
    "Enable optimization hints fetching for users who have enabled the 'Make "
    "Searches and Browsing Better' setting.";

const char kExpandedTabStripName[] = "Enable expanded tabstrip";
const char kExpandedTabStripDescription[] =
    "Enables the new expanded tabstrip. Activated by swiping down the tabstrip"
    " or the toolbar";

const char kFillingAcrossAffiliatedWebsitesName[] =
    "Fill passwords across affiliated websites.";
const char kFillingAcrossAffiliatedWebsitesDescription[] =
    "Enables filling password on a website when there is saved "
    "password on affiliated website.";

const char kForceDisableExtendedSyncPromosName[] =
    "Disable all extended sync promos";
const char kForceDisableExtendedSyncPromosDescription[] =
    "When enabled, will not display any extended sync promos";

const char kForceMajorVersion100InUserAgentName[] =
    "Force major version to 100 in User-Agent";
const char kForceMajorVersion100InUserAgentDescription[] =
    "Force the Chrome major version in the User-Agent string to 100, which "
    "allows testing the 3-digit major version number before the actual M100 "
    "release. This flag is only available from M96-M99.";

const char kForceStartupSigninPromoName[] = "Display the startup sign-in promo";
const char kForceStartupSigninPromoDescription[] =
    "When enabled, the startup sign-in promo is always displayed when starting "
    "Chrome.";

const char kCredentialProviderExtensionPromoName[] =
    "Enable the credential provider extension promo";
const char kCredentialProviderExtensionPromoDescription[] =
    "When enabled, a new item 'Passwords In Other Apps' item will be available "
    "Chrome passwords settings, containing promotional instructions to enable"
    "password autofill using Chrome.";

const char kFullscreenSmoothScrollingName[] = "Fullscreen Smooth Scrolling";
const char kFullscreenSmoothScrollingDescription[] =
    "When enabled, the web view's insets are updated for scoll events. If "
    "disabled, the the web view's frame are updated.";

const char kIncognitoBrandConsistencyForIOSName[] =
    "Enable Incognito brand consistency in iOS.";
const char kIncognitoBrandConsistencyForIOSDescription[] =
    "When enabled, keeps Incognito UI consistent regardless of any selected "
    "theme.";

const char kIncognitoNtpRevampName[] = "Revamped Incognito New Tab Page";
const char kIncognitoNtpRevampDescription[] =
    "When enabled, Incognito new tab page will have an updated UI.";

const char kInterestFeedNoticeCardAutoDismissName[] =
    "New Content Suggestions notice card auto-dismiss";
const char kInterestFeedNoticeCardAutoDismissDescription[] =
    "Auto-dismiss the notice card when there are enough clicks or views on the "
    "notice card.";

const char kInterestFeedV2ClickAndViewActionsConditionalUploadName[] =
    "New Content Suggestions taps/views conditional upload";
const char kInterestFeedV2ClickAndViewActionsConditionalUploadDescription[] =
    "Only enable the upload of taps/views after satisfying conditions (e.g., "
    "user views X cards)";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kIOSLegacyTLSInterstitialsName[] = "Show legacy TLS interstitials";
const char kIOSLegacyTLSInterstitialsDescription[] =
    "When enabled, an interstitial will be shown on main-frame navigations "
    "that use legacy TLS connections, and subresources using legacy TLS "
    "connections will be blocked.";

const char kIOSPersistCrashRestoreName[] = "Persist Crash Restore Infobar";
const char kIOSPersistCrashRestoreDescription[] =
    "When enabled, the Crash Restore Infobar will persist through navigations "
    "instead of dismissing.";

const char kIOSSharedHighlightingColorChangeName[] =
    "IOS Shared Highlighting color change";
const char kIOSSharedHighlightingColorChangeDescription[] =
    "Changes the Shared Highlighting color of the text fragment"
    "away from the default yellow in iOS. Works with #scroll-to-text-ios flag.";

const char kSharedHighlightingUseBlocklistIOSName[] =
    "Shared Highlighting blocklist";
const char kSharedHighlightingUseBlocklistIOSDescription[] =
    "Uses a blocklist to disable Shared Highlighting link generation on "
    "certain sites where personalized or dynamic content or other technical "
    "restrictions make it unlikely that a URL can be generated and actually "
    "work when shared.";

const char kLocationPermissionsPromptName[] =
    "Location Permisssions Prompt Experiment";
const char kLocationPermissionsPromptDescription[] =
    "When enabled, a different user experience flow will be shown to ask for "
    "location permissions.";

const char kLockBottomToolbarName[] = "Lock bottom toolbar";
const char kLockBottomToolbarDescription[] =
    "When enabled, the bottom toolbar will not get collapsed when scrolling "
    "into fullscreen mode.";

const char kMetrickitCrashReportName[] = "Metrickit crash reports";
const char kMetrickitCrashReportDescription[] =
    "Enables sending Metrickit crash reports";

const char kMICEWebSignInName[] = "MICE Web Sign-in";
const char kMICEWebSignInDescription[] = "Enables MICE web sign-in.";

const char kModernTabStripName[] = "Modern TabStrip";
const char kModernTabStripDescription[] =
    "When enabled, the newly implemented tabstrip can be tested.";

const char kNewOverflowMenuName[] = "New Overflow Menu";
const char kNewOverflowMenuDescription[] = "Enables the new overflow menu";

const char kNTPViewHierarchyRepairName[] = "NTP View Hierarchy Repair";
const char kNTPViewHierarchyRepairDescription[] =
    "Checks if NTP view hierarchy is broken and fixes it if necessary.";

const char kOldSyncStringFREName[] = "Use previous sync screen strings";
const char kOldSyncStringFREDescription[] =
    "When enabled, the strings of the previous sync screen are used in the "
    "new FRE.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxOnDeviceHeadSuggestionsIncognitoName[] =
    "Omnibox on device head suggestions (incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for incognito";

const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoName[] =
    "Omnibox on device head suggestions (non-incognito only)";
const char kOmniboxOnDeviceHeadSuggestionsNonIncognitoDescription[] =
    "Shows Google head non personalized search suggestions provided by a "
    "compact on device model for non-incognito";

const char kOmniboxOnFocusSuggestionsName[] = "Omnibox on-focus suggestions";
const char kOmniboxOnFocusSuggestionsDescription[] =
    "Configures Omnibox on-focus suggestions - suggestions displayed on-focus "
    "before the user has typed any input. This provides overrides for the "
    "default suggestion locations.";

const char kOmniboxLocalHistoryZeroSuggestName[] =
    "Omnibox local zero-prefix suggestions";
const char kOmniboxLocalHistoryZeroSuggestDescription[] =
    "Configures the omnibox zero-prefix suggestion to use local search "
    "history.";

const char kOmniboxNewImplementationName[] =
    "Use experimental omnibox textfield";
const char kOmniboxNewImplementationDescription[] =
    "Uses a textfield implementation that doesn't use UILabels internally";

const char kPasswordReuseDetectionName[] =
    "PhishGuard password reuse detection";
const char kPasswordReuseDetectionDescription[] =
    "Displays warning when user types or pastes a saved password into a "
    "phishing website.";

const char kReadingListMessagesName[] = "Enables Reading List Messages";
const char kReadingListMessagesDescription[] =
    "When enabled, a Messages prompt may be presented to allow the user to "
    "save the current page to Reading List";

const char kRestoreGaiaCookiesOnUserActionName[] =
    "Restore GAIA cookies on user action";
const char kRestoreGaiaCookiesOnUserActionDescription[] =
    "When enabled, will restore GAIA cookies for signed-in Chrome users if "
    "the user explicitly requests a Google service.";

const char kSafeBrowsingAvailableName[] = "Make Safe Browsing available";
const char kSafeBrowsingAvailableDescription[] =
    "When enabled, navigation URLs are compared to Safe Browsing blocklists, "
    "subject to an opt-out preference.";

const char kSafeBrowsingRealTimeLookupName[] = "Enable real-time Safe Browsing";
const char kSafeBrowsingRealTimeLookupDescription[] =
    "When enabled, navigation URLs are checked using real-time queries to Safe "
    "Browsing servers, subject to an opt-in preference.";

const char kScreenTimeIntegrationName[] = "Enables ScreenTime Integration";
const char kScreenTimeIntegrationDescription[] =
    "Enables integration with ScreenTime in iOS 14.0 and above.";

const char kSearchHistoryLinkIOSName[] = "Enables Search History Link";
const char kSearchHistoryLinkIOSDescription[] =
    "Changes the Clear Browsing Data "
    "UI to display a link to clear search history on My Google Activity.";

const char kSendTabToSelfWhenSignedInName[] = "Send-tab-to-self when signed in";
const char kSendTabToSelfWhenSignedInDescription[] =
    "Makes the tab sharing feature also available for users who have \"only\" "
    "signed-in to their Google Account (as opposed to having enabled Sync).";

const char kSendTabToSelfManageDevicesLinkName[] =
    "Send-tab-to-self manage devices link";
const char kSendTabToSelfManageDevicesLinkDescription[] =
    "Shows a link to manage the user's devices below the device list when "
    "sharing";

const char kSendUmaOverAnyNetwork[] =
    "Send UMA data over any network available.";
const char kSendUmaOverAnyNetworkDescription[] =
    "When enabled, will send UMA data over either WiFi or cellular by default.";

const char kSharedHighlightingIOSName[] = "Enable Shared Highlighting features";
const char kSharedHighlightingIOSDescription[] =
    "Adds a Link to Text option in the Edit Menu which generates URLs with a "
    "text fragment.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncTrustedVaultPassphraseiOSRPCName[] =
    "Enable RPC for sync trusted vault passphrase.";
const char kSyncTrustedVaultPassphraseiOSRPCDescription[] =
    "Enables RPC for an experimental sync passphrase type, referred to as "
    "trusted vault.";

const char kSyncTrustedVaultPassphrasePromoName[] =
    "Enable promos for sync trusted vault passphrase.";
const char kSyncTrustedVaultPassphrasePromoDescription[] =
    "Enables promos for an experimental sync passphrase type, referred to as "
    "trusted vault.";

const char kSyncTrustedVaultPassphraseRecoveryName[] =
    "Enable sync trusted vault passphrase with improved recovery.";
const char kSyncTrustedVaultPassphraseRecoveryDescription[] =
    "Enables support for an experimental sync passphrase type, referred to as "
    "trusted vault, including logic and APIs for improved account recovery "
    "flows.";

const char kStartSurfaceName[] = "Start Surface";
const char kStartSurfaceDescription[] =
    "Enable showing the Start Surface when launching Chrome via clicking the "
    "icon or the app switcher.";

const char kTabsBulkActionsName[] = "Enable Tab Grid Bulk Actions";
const char kTabsBulkActionsDescription[] =
    "Enables the selection mode in the Tab grid where users can perform "
    "actions on multiple tabs at once for iOS 13 and above.";

const char kToolbarContainerName[] = "Use Toolbar Containers";
const char kToolbarContainerDescription[] =
    "When enabled, the toolbars and their fullscreen animations will be "
    "managed by the toolbar container coordinator rather than BVC.";

const char kUpdateHistoryEntryPointsInIncognitoName[] =
    "Update history entry points in Incognito.";
const char kUpdateHistoryEntryPointsInIncognitoDescription[] =
    "When enabled, the entry points to history UI from Incognito mode will be "
    "removed.";

const char kURLBlocklistIOSName[] = "URL Blocklist Policy";
const char kURLBlocklistIOSDescription[] =
    "When enabled, URLs can be blocked/allowed by the URLBlocklist/URLAllowlist"
    "enterprise policies.";

const char kUseLensToSearchForImageName[] =
    "Use Google Lens to Search for images";
const char kUseLensToSearchForImageDescription[] =
    "When enabled, use Lens to search for images from the long press context "
    "menu when Google is the selected search engine.";

const char kWaitThresholdMillisecondsForCapabilitiesApiName[] =
    "Maximum wait time (in seconds) for a response from the Account "
    "Capabilities API";
const char kWaitThresholdMillisecondsForCapabilitiesApiDescription[] =
    "Used for testing purposes to test waiting thresholds in dev.";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kEnableNewDownloadAPIName[] = "Enable new download API";
const char kEnableNewDownloadAPIDescription[] =
    "Enable new download API (restricted to iOS 15.0+).";

const char kWebPageDefaultZoomFromDynamicTypeName[] =
    "Use dynamic type size for default text zoom level";
const char kWebPageDefaultZoomFromDynamicTypeDescription[] =
    "When enabled, the default text zoom level for a website comes from the "
    "current dynamic type setting.";

const char kWebPageTextAccessibilityName[] =
    "Enable text accessibility in web pages";
const char kWebPageTextAccessibilityDescription[] =
    "When enabled, text in web pages will respect the user's Dynamic Type "
    "setting.";

const char kWebPageAlternativeTextZoomName[] =
    "Use different method for zooming web pages";
const char kWebPageAlternativeTextZoomDescription[] =
    "When enabled, switches the method used to zoom web pages.";

const char kWebViewNativeContextMenuName[] =
    "Use the native Context Menus in the WebView";
const char kWebViewNativeContextMenuDescription[] =
    "When enabled, the native context menu are displayed when the user long "
    "press on a link or an image.";

const char kWebViewNativeContextMenuPhase2Name[] =
    "Context Menu with non-live preview";
const char kWebViewNativeContextMenuPhase2Description[] =
    "When enabled, the context menu displayed when long pressing on a link or "
    "an image has a non-live preview.";

const char kRecordSnapshotSizeName[] =
    "Record the size of image and PDF snapshots in UMA histograms";
const char kRecordSnapshotSizeDescription[] =
    "When enabled, the app will record UMA histograms for image and PDF "
    "snapshots. PDF snaphot will be taken just for the purpose of the "
    "histogram recording.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions
