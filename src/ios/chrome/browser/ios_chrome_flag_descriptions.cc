// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_flag_descriptions.h"

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.

namespace flag_descriptions {

const char kEnableAutocompleteDataRetentionPolicyName[] =
    "Enable automatic cleanup of expired Autocomplete entries.";
const char kEnableAutocompleteDataRetentionPolicyDescription[] =
    "If enabled, will clean-up Autocomplete entries whose last use date is "
    "older than the current retention policy. These entries will be "
    "permanently deleted from the client on startup, and will be unlinked "
    "from sync.";

const char kAutofillCacheQueryResponsesName[] =
    "Cache Autofill Query Responses";
const char kAutofillCacheQueryResponsesDescription[] =
    "When enabled, autofill will cache the responses it receives from the "
    "crowd-sourced field type prediction server.";

const char kAutofillEnableCompanyNameName[] =
    "Enable Autofill Company Name field";
const char kAutofillEnableCompanyNameDescription[] =
    "When enabled, Company Name fields will be auto filled";

const char kAutofillCreditCardUploadName[] =
    "Offers uploading Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Offers uploading Autofilled credit cards to Google Payments after form "
    "submission.";

const char kAutofillDownstreamUseGooglePayBrandingOniOSName[] =
    "Enable Google Pay branding when offering credit card downstream";
const char kAutofillDownstreamUseGooglePayBrandingOniOSDescription[] =
    "When enabled, shows the Google Pay logo animation when showing payments"
    "credit card suggestions in downstream keyboard accessory";

extern const char kBreakpadNoDelayInitialUploadName[] =
    "Remove delay on initial crash upload";
extern const char kBreakpadNoDelayInitialUploadDescription[] =
    "When enabled, the initial crash uploading will not be delayed. When "
    "disabled, initial upload is delayed until deferred initialization. This "
    "does not affect recovery mode.";

const char kCompactTranslateInfobarName[] =
    "Enable the compact translate infobar";
const char kCompactTranslateInfobarDescription[] =
    "When enabled, replaces the exisitng translate infobars with a new compact "
    "one.";

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

const char kEnableAutofillCreditCardUploadUpdatePromptExplanationName[] =
    "Enable updated prompt explanation when offering credit card upload";
const char kEnableAutofillCreditCardUploadUpdatePromptExplanationDescription[] =
    "If enabled, changes the server save card prompt's explanation to mention "
    "the saving of the billing address.";

const char kEnableAutofillSaveCreditCardUsesStrikeSystemName[] =
    "Enable limit on offering to save the same credit card repeatedly";
const char kEnableAutofillSaveCreditCardUsesStrikeSystemDescription[] =
    "If enabled, prevents popping up the credit card offer-to-save prompt if "
    "it has repeatedly been ignored, declined, or failed.";

const char kEnableAutofillSaveCreditCardUsesStrikeSystemV2Name[] =
    "Enable limit on offering to save the same credit card repeatedly using the"
    "updated strike system implementation";
const char kEnableAutofillSaveCreditCardUsesStrikeSystemV2Description[] =
    "If enabled, uses the updated strike system implementation to prevent"
    "popping up the credit card offer-to-save prompt if it has repeatedly been"
    "ignored, declined, or failed.";

const char kEnableSyncUSSBookmarksName[] = "Enable USS for bookmarks sync";
const char kEnableSyncUSSBookmarksDescription[] =
    "Enables the new, experimental implementation of bookmark sync";

const char kEnableSyncUSSPasswordsName[] = "Enable USS for passwords sync";
const char kEnableSyncUSSPasswordsDescription[] =
    "Enables the new, experimental implementation of password sync";

const char kSyncPseudoUSSFaviconsName[] = "Enable pseudo-USS for favicon sync.";
const char kSyncPseudoUSSFaviconsDescription[] =
    "Enable new USS-based codepath for sync datatypes FAVICON_IMAGES and "
    "FAVICON_TRACKING.";

const char kSyncPseudoUSSHistoryDeleteDirectivesName[] =
    "Enable pseudo-USS for HISTORY_DELETE_DIRECTIVES sync.";
const char kSyncPseudoUSSHistoryDeleteDirectivesDescription[] =
    "Enable new USS-based codepath for sync datatype "
    "HISTORY_DELETE_DIRECTIVES.";

const char kSyncPseudoUSSPreferencesName[] =
    "Enable pseudo-USS for PREFERENCES sync.";
const char kSyncPseudoUSSPreferencesDescription[] =
    "Enable new USS-based codepath for sync datatype PREFERENCES.";

const char kSyncPseudoUSSPriorityPreferencesName[] =
    "Enable pseudo-USS for PRIORITY_PREFERENCES sync.";
const char kSyncPseudoUSSPriorityPreferencesDescription[] =
    "Enable new USS-based codepath for sync datatype PRIORITY_PREFERENCES.";

const char kSyncPseudoUSSSupervisedUsersName[] =
    "Enable pseudo-USS for supervised users sync.";
const char kSyncPseudoUSSSupervisedUsersDescription[] =
    "Enable new USS-based codepath for sync datatypes SUPERVISED_USER_SETTINGS "
    "and SUPERVISED_USER_WHITELISTS.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

const char kSyncSupportSecondaryAccountName[] =
    "Support secondary accounts for Sync standalone transport";
const char kSyncSupportSecondaryAccountDescription[] =
    "If enabled, allows Chrome Sync to start in standalone transport mode for "
    "a signed-in account that has not been chosen as Chrome's primary account. "
    "This only has an effect if sync-standalone-transport is also enabled.";

const char kWalletServiceUseSandboxName[] = "Use Google Payments sandbox";
const char kWalletServiceUseSandboxDescription[] =
    "Uses the sandbox service for Google Payments API calls.";

const char kAppLauncherRefreshName[] = "Enable the new AppLauncher logic";
const char kAppLauncherRefreshDescription[] =
    "AppLauncher will always prompt if there is no direct link navigation, "
    "also Apps will launch asynchronously and there will be no logic that"
    "depends on the success or the failure of launching an app.";

const char kAutofillDynamicFormsName[] = "Autofill dynamic forms";
const char kAutofillDynamicFormsDescription[] =
    "Refills forms that dynamically change after an initial fill";

const char kAutofillNoLocalSaveOnUploadSuccessName[] =
    "Disable saving local copy of uploaded card when credit card upload "
    "succeeds";
const char kAutofillNoLocalSaveOnUploadSuccessDescription[] =
    "When enabled, no local copy of server card will be saved when credit card "
    "upload succeeds.";

const char kAutofillPrefilledFieldsName[] = "Autofill prefilled forms";
const char kAutofillPrefilledFieldsDescription[] =
    "Fills forms that contain a programmatically filled value.";

const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[] =
    "Autofill Enforce Min Required Fields For Heuristics";
const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before allowing heuristic field-type prediction to occur.";

const char kAutofillEnforceMinRequiredFieldsForQueryName[] =
    "Autofill Enforce Min Required Fields For Query";
const char kAutofillEnforceMinRequiredFieldsForQueryDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fields before querying the autofill server for field-type predictions.";

const char kAutofillEnforceMinRequiredFieldsForUploadName[] =
    "Autofill Enforce Min Required Fields For Upload";
const char kAutofillEnforceMinRequiredFieldsForUploadDescription[] =
    "When enabled, autofill will generally require a form to have at least 3 "
    "fillable fields before uploading field-type votes for that form.";

const char kAutofillIOSDelayBetweenFieldsName[] = "Autofill delay";
const char kAutofillIOSDelayBetweenFieldsDescription[] =
    "Delay between the different fields of a form being autofilled. In "
    "milliseconds.";

const char kAutofillManualFallbackName[] = "Enable Autofill Manual Fallback";
const char kAutofillManualFallbackDescription[] =
    "When enabled, it shows the autofill UI with manual fallback when filling "
    "forms.";

const char kAutofillManualFallbackPhaseTwoName[] = "Enable Addresses and Cards";
const char kAutofillManualFallbackPhaseTwoDescription[] =
    "When enabled, it shows the credit cards and addresses buttons in manual "
    "fallback.";

const char kAutofillShowAllSuggestionsOnPrefilledFormsName[] =
    "Enable showing all suggestions when focusing prefilled field";
const char kAutofillShowAllSuggestionsOnPrefilledFormsDescription[] =
    "When enabled: show all suggestions when the focused field value has not "
    "been entered by the user. When disabled: use the field value as a filter.";

const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[] =
    "Restrict formless form extraction";
const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[] =
    "Restrict extraction of formless forms to checkout flows";

const char kAutofillRichMetadataQueriesName[] =
    "Autofill - Rich metadata queries (Canary/Dev only)";
const char kAutofillRichMetadataQueriesDescription[] =
    "Transmit rich form/field metadata when querying the autofill server. "
    "This feature only works on the Canary and Dev channels.";

const char kBrowserContainerFullscreenName[] = "Browser Container Fullscreen";
const char kBrowserContainerFullscreenDescription[] =
    "When enabled, the BrowserContainer is fullscreen. No UI change should be "
    "visible.";

const char kBrowserContainerContainsNTPName[] = "Browser Container NTP";
const char kBrowserContainerContainsNTPDescription[] =
    "When enabled, the BrowserContainer contains the NTP directly, rather than"
    "via native content.";

const char kBrowserContainerKeepsContentViewName[] =
    "Browser Container retains the content view";
const char kBrowserContainerKeepsContentViewDescription[] =
    "When enable, the browser container keeps the content view in the view "
    "hierarchy, to avoid WKWebView from being unloaded from the process.";

const char kExternalFilesLoadedInWebStateName[] =
    "External files loaded in WebState";
const char kExternalFilesLoadedInWebStateDescription[] =
    "When enabled, external files are loaded in WebState instead of using "
    "ExternalFileController.";

const char kBrowserTaskScheduler[] = "Task Scheduler";
const char kBrowserTaskSchedulerDescription[] =
    "Enables redirection of some task posting APIs to the task scheduler.";

// TODO(crbug.com/893314) : Remove this flag.
const char kClosingLastIncognitoTabName[] = "Closing Last Incognito Tab";
const char kClosingLastIncognitoTabDescription[] =
    "Automatically switches to the regular tabs panel in the tab grid after "
    "closing the last incognito tab";

const char kContextualSearch[] = "Contextual Search";
const char kContextualSearchDescription[] =
    "Whether or not Contextual Search is enabled.";

const char kCopiedContentBehaviorName[] =
    "Enable differentiating between copied urls, text, and images";
const char kCopiedContentBehaviorDescription[] =
    "When enabled, places that handled copied urls (omnibox long-press, toolbar"
    "menus) will differentiate between copied urls, text, and images.";

const char kDetectMainThreadFreezeName[] = "Detect freeze in the main thread.";
const char kDetectMainThreadFreezeDescription[] =
    "A crash report will be uploaded if the main thread is frozen more than "
    "the time specified by this flag.";

const char kDragAndDropName[] = "Drag and Drop";
const char kDragAndDropDescription[] = "Enable support for drag and drop.";

const char kNewClearBrowsingDataUIName[] = "Clear Browsing Data UI";
const char kNewClearBrowsingDataUIDescription[] =
    "Enable new Clear Browsing Data UI.";

const char kEnableClipboardProviderTextSuggestionsName[] =
    "Enable copied text provider";
const char kEnableClipboardProviderTextSuggestionsDescription[] =
    "Enable suggesting a search for text copied to the clipboard";

const char kEnableClipboardProviderImageSuggestionsName[] =
    "Enable copied image provider";
const char kEnableClipboardProviderImageSuggestionsDescription[] =
    "Enable suggesting a search for the image copied to the clipboard";

const char kFCMInvalidationsName[] =
    "Enable invalidations delivery via new FCM based protocol";
const char kFCMInvalidationsDescription[] =
    "Use the new FCM-based protocol for deliveling invalidations";

const char kFullscreenViewportAdjustmentExperimentName[] =
    "Fullscreen Viewport Adjustment Mode";
const char kFullscreenViewportAdjustmentExperimentDescription[] =
    "The different ways in which the web view's viewport is updated for scroll "
    "events.  The default option updates the web view's frame.";

const char kHistoryBatchUpdatesFilterName[] = "History Single Batch Filtering";
const char kHistoryBatchUpdatesFilterDescription[] =
    "When enabled History inserts and deletes history items in the same "
    "BatchUpdates block.";

const char kInfobarUIRebootName[] = "Infobar UI Reboot";
const char kInfobarUIRebootDescription[] =
    "When enabled, Infobar will use the new UI.";

const char kInProductHelpDemoModeName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeDescription[] =
    "When enabled, in-product help promotions occur exactly once per cold "
    "start. Enabled causes all in-product help promotions to occur. Enabling "
    "an individual promotion causes that promotion but no other promotions to "
    "occur.";

const char kMarkHttpAsName[] = "Mark non-secure origins as non-secure";
const char kMarkHttpAsDescription[] = "Change the UI treatment for HTTP pages";

const char kMemexTabSwitcherName[] = "Enable the Memex prototype Tab Switcher.";
const char kMemexTabSwitcherDescription[] =
    "When enabled, the TabSwitcher button will navigate to the chrome memex "
    "prototype site instead of triggering the native Tab Switcher. The native "
    "TabSwitcher is accessible by long pressing the button";

const char kNewPasswordFormParsingName[] = "New password form parsing";
const char kNewPasswordFormParsingDescription[] =
    "Replaces existing form parsing in password manager with a new version, "
    "currently under development. WARNING: when enabled Password Manager might "
    "stop working";

const char kNewPasswordFormParsingForSavingName[] =
    "New password form parsing for saving passwords";
const char kNewPasswordFormParsingForSavingDescription[] =
    "Replaces existing form parsing for saving in password manager with a new "
    "version, currently under development. WARNING: when enabled, Password "
    "Manager might stop working";

const char kNonModalDialogsName[] = "Use non-modal JavaScript dialogs";
const char kNonModalDialogsDescription[] =
    "Presents JavaScript dialogs non-modally so that the user can change tabs "
    "while a dialog is displayed.";

const char kOfflineVersionWithoutNativeContentName[] =
    "Use offline pages without native content";
const char kOfflineVersionWithoutNativeContentDescription[] =
    "Shows offline pages directly in the web view.";

const char kOmniboxPopupShortcutIconsInZeroStateName[] =
    "Show zero-state omnibox shortcuts";
const char kOmniboxPopupShortcutIconsInZeroStateDescription[] =
    "Instead of ZeroSuggest, show most visited sites and collection shortcuts "
    "in the omnibox popup.";

const char kOmniboxTabSwitchSuggestionsName[] =
    "Enable 'switch to this tab' option";
const char kOmniboxTabSwitchSuggestionsDescription[] =
    "Enable the 'switch to this tab' options in the omnibox suggestions.";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxUIElideSuggestionUrlAfterHostName[] =
    "Hide the path, query, and ref of omnibox suggestions";
const char kOmniboxUIElideSuggestionUrlAfterHostDescription[] =
    "Elides the path, query, and ref of suggested URLs in the omnibox "
    "dropdown.";

const char kOnlyNewPasswordFormParsingName[] =
    "Use only new password form parsing";
const char kOnlyNewPasswordFormParsingDescription[] =
    "The old password form parsing is disabled";

const char kOutOfWebFullscreenName[] = "Fullscreen implementation out of web";
const char kOutOfWebFullscreenDescription[] =
    "Use the fullscreen implementation living outside of web. Disable the one "
    "in web.";

const char kPhysicalWeb[] = "Physical Web";
const char kPhysicalWebDescription[] =
    "When enabled, the omnibox will include suggestions for web pages "
    "broadcast by devices near you.";

const char kIgnoresViewportScaleLimitsName[] = "Ignore Viewport Scale Limits";
const char kIgnoresViewportScaleLimitsDescription[] =
    "When enabled the page can always be scaled, regardless of author intent.";

const char kSearchIconToggleName[] = "Change the icon for the search button";
const char kSearchIconToggleDescription[] =
    "Different icons for the search button.";

const char kSendUmaOverCellularName[] = "Send UMA data over cellular.";
const char kSendUmaOverCellularDescription[] =
    "When enabled, will send UMA data over WiFi if available or cellular if "
    "not.";

const char kSettingsRefreshName[] = "Enable the UI Refresh for Settings";
const char kSettingsRefreshDescription[] =
    "Change the UI appearance of the settings to have something in phase with "
    "UI Refresh.";

const char kSlimNavigationManagerName[] = "Use Slim Navigation Manager";
const char kSlimNavigationManagerDescription[] =
    "When enabled, uses the experimental slim navigation manager that provides "
    "better compatibility with HTML navigation spec.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kToolbarContainerName[] = "Use Toolbar Containers";
const char kToolbarContainerDescription[] =
    "When enabled, the toolbars and their fullscreen animations will be "
    "managed by the toolbar container coordinator rather than BVC.";

const char kSnapshotDrawViewName[] = "Use DrawViewHierarchy for Snapshots";
const char kSnapshotDrawViewDescription[] =
    "When enabled, snapshots will be taken using |-drawViewHierarchy:|.";

const char kUnifiedConsentName[] = "Unified Consent";
const char kUnifiedConsentDescription[] =
    "Enables a unified management of user consent for privacy-related "
    "features. This includes new confirmation screens and improved settings "
    "pages.";

const char kUseMultiloginEndpointName[] = "Use Multilogin endpoint.";
const char kUseMultiloginEndpointDescription[] =
    "Use Gaia OAuth multilogin for identity consistency.";

const char kUseNSURLSessionForGaiaSigninRequestsName[] =
    "Use NSURLSession for sign-in requests";
const char kUseNSURLSessionForGaiaSigninRequestsDescription[] =
    "Use NSURLSession to make sign-in requests to Gaia";

const char kUsdzPreviewName[] = "Preview USDZ format 3D models";
const char kUsdzPreviewDescription[] =
    "Enables previewing USDZ format 3D models in AR Quick Look.";

const char kUseDdljsonApiName[] = "Use new ddljson API for Doodles";
const char kUseDdljsonApiDescription[] =
    "Enables the new ddljson API to fetch Doodles for the NTP.";

const char kWebClearBrowsingDataName[] = "Web-API for browsing data";
const char kWebClearBrowsingDataDescription[] =
    "When enabled the Clear Browsing Data feature is using the web API.";

const char kWebFrameMessagingName[] = "Web Frame Messaging";
const char kWebFrameMessagingDescription[] =
    "When enabled, API will be injected into webpages to allow sending messages"
    " directly to any frame of a webpage.";

const char kStorePendingItemInContextName[] =
    "Store pending item in NavigationContext";
const char kStorePendingItemInContextDescription[] =
    "When enabled pending item will be stored in NavigationContext after "
    "context is created. The item is still stored in NavigationManager if the "
    "navigated was requested, but context does not yet exist or when "
    "navigation was aborted.";

const char kWebPageTextAccessibilityName[] =
    "Enable text accessibility in web pages";
const char kWebPageTextAccessibilityDescription[] =
    "When enabled, text in web pages will respect the user's Dynamic Type "
    "setting.";

const char kWebUISchemeHandlingName[] = "WKWebView is handling the WebUI pages";
const char kWebUISchemeHandlingDescription[] =
    "When enabled, the page with a WebUI scheme are directly loaded by the "
    "WKWebView by using setURLSchemeHandler:forURLScheme: API.";

const char kWKHTTPSystemCookieStoreName[] = "Use WKHTTPSystemCookieStore.";
const char kWKHTTPSystemCookieStoreDescription[] =
    "Use WKHTTPCookieStore backed store for main context URL requests.";

const char kCustomSearchEnginesName[] = "Custom Search Engines";
const char kCustomSearchEnginesDescription[] =
    "When enabled, user can add custom search engines in settings.";

const char kFindInPageiFrameName[] = "Find in Page in iFrames.";
const char kFindInPageiFrameDescription[] =
    "When enabled, Find In Page will search in iFrames.";

// Please insert your name/description above in alphabetical order.

}  // namespace flag_descriptions
