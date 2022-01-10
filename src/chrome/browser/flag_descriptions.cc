// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/flag_descriptions.h"

#include "build/chromeos_buildflags.h"

// Keep in identical order as the header file, see the comment at the top
// for formatting rules.

namespace flag_descriptions {

const char kAccelerated2dCanvasName[] = "Accelerated 2D canvas";
const char kAccelerated2dCanvasDescription[] =
    "Enables the use of the GPU to perform 2d canvas rendering instead of "
    "using software rendering.";

const char kCanvasOopRasterizationName[] =
    "Out-of-process 2D canvas rasterization.";
const char kCanvasOopRasterizationDescription[] =
    "The rasterization of 2d canvas contents is performed in the GPU process. "
    "Requires that out-of-process rasterization be enabled.";

const char kAcceleratedVideoDecodeName[] = "Hardware-accelerated video decode";
const char kAcceleratedVideoDecodeDescription[] =
    "Hardware-accelerated video decode where available.";

const char kAcceleratedVideoEncodeName[] = "Hardware-accelerated video encode";
const char kAcceleratedVideoEncodeDescription[] =
    "Hardware-accelerated video encode where available.";

const char kEnableMediaInternalsName[] = "Media-internals page";
const char kEnableMediaInternalsDescription[] =
    "Enables the chrome://media-internals debug page.";

#if BUILDFLAG(ENABLE_PLUGINS)
const char kAccessiblePDFFormName[] = "Accessible PDF Forms";
const char kAccessiblePDFFormDescription[] =
    "Enables accessibility support for PDF forms.";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

const char kAccountIdMigrationName[] = "Account ID migration";
const char kAccountIdMigrationDescription[] =
    "Migrate to use Gaia ID instead of the email as the account identifer for "
    "the Identity Manager.";

const char kAddPasswordsInSettingsName[] = "Add passwords in settings";
const char kAddPasswordsInSettingsDescription[] =
    "Enables manual password adding in settings.";

const char kLauncherAppSortName[] = "Productivity experiment: Reorder Apps";
const char kLauncherAppSortDescription[] =
    "To evaluate an enhanced Launcher experience that enables users to reorder "
    "their apps in order to find them more easily.";

const char kAllowInsecureLocalhostName[] =
    "Allow invalid certificates for resources loaded from localhost.";
const char kAllowInsecureLocalhostDescription[] =
    "Allows requests to localhost over HTTPS even when an invalid certificate "
    "is presented.";

const char kAllowSyncXHRInPageDismissalName[] =
    "Allows synchronous XHR requests in page dismissal";
const char kAllowSyncXHRInPageDismissalDescription[] =
    "Allows synchronous XHR requests during page dismissal when the page is "
    "being navigated away or closed by the user.";

const char kWindowsFollowCursorName[] =
    "Windows open on the display with the cursor";
const char kWindowsFollowCursorDescription[] =
    "When there are multiple displays, windows open on the display where "
    "cursor is located.";

const char kAnimatedImageResumeName[] = "Use animated image resume behavior";
const char kAnimatedImageResumeDescription[] =
    "Resumes animated images from the last frame drawn rather than attempt "
    "to catch up to the frame that should be drawn based on current time.";

const char kAriaElementReflectionName[] = "Enable ARIA element reflection";
const char kAriaElementReflectionDescription[] =
    "Enable setting ARIA relationship attributes that reference other elements "
    "directly without an IDREF";

const char kCOLRV1FontsName[] = "COLR v1 Fonts";
const char kCOLRV1FontsDescription[] =
    "Display COLR v1 color gradient vector fonts.";

const char kCSSCascadeLayersName[] = "Enable CSS Cascade Layers";
const char kCSSCascadeLayersDescription[] =
    "Enables support for CSS @layer rules and layered @import syntax.";

extern const char kCSSContainerQueriesName[] = "Enable CSS Container Queries";
extern const char kCSSContainerQueriesDescription[] =
    "Enables support for @container, inline-size and block-size values for the "
    "contain property, and the LayoutNG Grid implementation.";

const char kConditionalTabStripAndroidName[] = "Conditional Tab Strip";
const char kConditionalTabStripAndroidDescription[] =
    "Allows users to access conditional tab strip.";

const char kContentLanguagesInLanguagePickerName[] =
    "Content languages in language picker";
const char kContentLanguagesInLanguagePickerDescription[] =
    "Enables bringing user's content languages that are translatable to the "
    "top of the list with all languages shown in the translate menu";

const char kConversionMeasurementDebugModeName[] =
    "Conversion Measurement Debug Mode";
const char kConversionMeasurementDebugModeDescription[] =
    "Enables debug mode for the Conversion Measurement API. This removes all "
    "reporting delays and noise. Only works if the Conversion Measurement API "
    "is already enabled.";

const char kForceStartupSigninPromoName[] = "Force Start-up Signin Promo";
const char kForceStartupSigninPromoDescription[] =
    "If enabled, the full screen signin promo will be forced to show up at "
    "Chrome start-up.";

const char kDebugHistoryInterventionNoUserActivationName[] =
    "Debug flag for history intervention on no user activation";
const char kDebugHistoryInterventionNoUserActivationDescription[] =
    "This flag when enabled, will be used to debug an issue where a page that "
    "did not get user activation "
    "is able to work around the history intervention which is not the expected "
    "behavior";

const char kDetectedSourceLanguageOptionName[] =
    "Use Detected Language string on Desktop and Android";
const char kDetectedSourceLanguageOptionDescription[] =
    "Renames the 'Unknown' source language option to 'Detected Language' and "
    "enables translation of unknown source language pages on Android.";

const char kDetectFormSubmissionOnFormClearName[] =
    "Detect form submission when the form is cleared.";
const char kDetectFormSubmissionOnFormClearDescription[] =
    "Detect form submissions for change password forms that are cleared and "
    "not removed from the page.";

const char kEditPasswordsInSettingsName[] = "Edit passwords in settings";
const char kEditPasswordsInSettingsDescription[] =
    "Enables password editing in settings.";

const char kEnableBluetoothSerialPortProfileInSerialApiName[] =
    "Enable Bluetooth Serial Port Profile in Serial API";
const char kEnableBluetoothSerialPortProfileInSerialApiDescription[] =
    "When enabled, Bluetooth Serial Port Profile devices will be enumerated "
    "for use with the Serial API.";

const char kWebBluetoothBondOnDemandName[] =
    "Enable on-demand Bluetooth device bonding";
const char kWebBluetoothBondOnDemandDescription[] =
    "When enabled, Bluetooth will start the bonding process, if necessary, "
    "to access protected characteristics.";

const char kEnableDrDcName[] =
    "Enables Display Compositor to use a new gpu thread.";
const char kEnableDrDcDescription[] =
    "When enabled, chrome uses 2 gpu threads instead of 1. "
    " Display compositor uses new dr-dc gpu thread and all other clients "
    "(raster, webgl, video) "
    " continues using the gpu main thread.";

const char kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedName[] =
    "Url blocklist throttle wait for policies to be loaded";
const char kEnablePolicyBlocklistThrottleRequiresPoliciesLoadedDescription[] =
    "Enables behaviour for Url blocklist throttle to wait for all policies to "
    "load";

const char kEnableSignedExchangePrefetchCacheForNavigationsName[] =
    "Enable Signed Exchange prefetch cache for navigations";
const char kEnableSignedExchangePrefetchCacheForNavigationsDescription[] =
    "When enabled, the prefetched signed exchanges is stored to a prefetch "
    "cache attached to the frame. The body of the inner response is stored as "
    "a blob and the verification process of the signed exchange is skipped for "
    "the succeeding navigation.";

const char kU2FPermissionPromptName[] =
    "Enable a permission prompt for the U2F Security Key API";
const char kU2FPermissionPromptDescription[] =
    "Show a permission prompt when making requests to the legacy U2F Security "
    "Key API (CryptoToken). The U2F Security "
    "Key API has been deprecated and will be removed soon. For more "
    "information, refer to the deprecation announcement at "
    "https://groups.google.com/a/chromium.org/g/blink-dev/c/xHC3AtU_65A";

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
const char kWebFilterInterstitialRefreshName[] =
    "Web filter interstitial refresh.";
const char kWebFilterInterstitialRefreshDescription[] =
    "Enable web filter interstitial refresh for Family Link users on Chrome "
    "OS.";
#endif  // ENABLE_SUPERVISED_USERS

const char kU2FSecurityKeyAPIName[] = "Enable the U2F Security Key API";
const char kU2FSecurityKeyAPIDescription[] =
    "Enable the legacy U2F Security Key API (CryptoToken). The U2F Security "
    "Key API has been deprecated and will be removed soon. For more "
    "information, refer to the deprecation announcement at "
    "https://groups.google.com/a/chromium.org/g/blink-dev/c/xHC3AtU_65A";

const char kUpcomingSharingFeaturesName[] = "Enable upcoming sharing features.";
const char kUpcomingSharingFeaturesDescription[] =
    "This flag enables all upcoming sharing features, in the experiment "
    "arms that are most likely to be shipped. This is a meta-flag so which "
    "features are upcoming at any given time may change.";

const char kUseStorkSmdsServerAddressName[] = "Use Stork SM-DS address";
const char kUseStorkSmdsServerAddressDescription[] =
    "Use the Stork SM-DS address to fetch pending eSIM profiles managed by the "
    "Stork prod server. Note that Stork profiles can be created with an EID at "
    "go/stork-profile, and managed at go/stork-batch > View Profiles. Also "
    "note that an external EUICC card is required to use this feature, and "
    "that the kCellularUseExternal flag must be enabled. Go to "
    "go/cros-connectivity > Dev Tips for more instructions.";

const char kUseWallpaperStagingUrlName[] = "Use Wallpaper staging URL";
const char kUseWallpaperStagingUrlDescription[] =
    "Use the staging server as part of the Wallpaper App to verify "
    "additions/removals of wallpapers.";

const char kSemanticColorsDebugOverrideName[] =
    "Use semantic color debug override";
const char kSemanticColorsDebugOverrideDescription[] =
    "Use debug override colors to find system components that are not using "
    "semantic colors.";

const char kUseMessagesStagingUrlName[] = "Use Messages staging URL";
const char kUseMessagesStagingUrlDescription[] =
    "Use the staging server as part of the \"Messages\" feature under "
    "\"Connected Devices\" settings.";

const char kUseCustomMessagesDomainName[] = "Use custom Messages domain";
const char kUseCustomMessagesDomainDescription[] =
    "Use a custom URL as part of the \"Messages\" feature under "
    "\"Connected Devices\" settings.";

const char kAndroidPictureInPictureAPIName[] =
    "Picture-in-Picture Web API for Android";
const char kAndroidPictureInPictureAPIDescription[] =
    "Enable Picture-in-Picture Web API for Android";

const char kDnsHttpssvcName[] = "Support for HTTPSSVC records in DNS.";
const char kDnsHttpssvcDescription[] =
    "When enabled, Chrome may query a configured DoH server for HTTPSSVC "
    "records. If any HTTPSSVC records are returned, Chrome may upgrade the URL "
    "to HTTPS. If the records indicate support for QUIC, Chrome may attempt "
    "QUIC on the first connection.";

const char kEnableFirstPartySetsName[] = "Enable First-Party Sets";
const char kEnableFirstPartySetsDescription[] =
    "When enabled, Chrome will apply First-Party Sets to features such as the "
    "SameParty cookie attribute.";

const char kDnsOverHttpsName[] = "Secure DNS lookups";
const char kDnsOverHttpsDescription[] =
    "Enables DNS over HTTPS. When this feature is enabled, your browser may "
    "try to use a secure HTTPS connection to look up the addresses of websites "
    "and other web resources.";

extern const char kAssistantConsentSimplifiedTextName[] =
    "AssistantConsentSimplifiedText";
extern const char kAssistantConsentSimplifiedTextDescription[] =
    "Enables simplified consent copy in the Assistant voice search consent "
    "dialog.";

extern const char kAssistantConsentV2Name[] = "AssistanConsentV2";
extern const char kAssistantConsentV2Description[] =
    "Enables different strategies for handling backing off from the consent "
    "screen without explicitly clicking yes/no buttons, i.e. when a user taps "
    "outside of the sheet.";

const char kAutofillAlwaysReturnCloudTokenizedCardName[] =
    "Return cloud token details for server credit cards when possible";
const char kAutofillAlwaysReturnCloudTokenizedCardDescription[] =
    "When enabled and where available, forms filled using Google Payments "
    "server cards are populated with cloud token details, including CPAN "
    "(cloud tokenized version of the Primary Account Number) and dCVV (dynamic "
    "CVV).";

const char kAutofillAutoTriggerManualFallbackForCardsName[] =
    "Auto trigger manual fallback for credit card form-filling failure cases";
const char kAutofillAutoTriggerManualFallbackForCardsDescription[] =
    "When enabled, manual fallback will be auto-triggered on form interaction "
    "in the case where autofill failed to fill a credit card form accurately.";

const char kAutofillCenterAligngedSuggestionsName[] =
    "Center-aligned Autofill suggestions.";
const char kAutofillCenterAligngedSuggestionsDescription[] =
    "When enabled, the Autofill suggestion popup will be aligned to the center "
    "of the initiating field and not to its border.";

const char kAutofillVisualImprovementsForSuggestionUiName[] =
    "Visual improvements for the Autofill and Password Manager suggestion UI.";
const char kAutofillVisualImprovementsForSuggestionUiDescription[] =
    "Non function changes that visually improve the suggestion UI used for "
    "addresses, passswords and credit cards.";

const char kAutofillTypeSpecificPopupWidthName[] =
    "Type-specific width limits for the Autofill popup";
const char kAutofillTypeSpecificPopupWidthDescription[] =
    "Controls if different width limits are used for the popup that provides "
    "Autofill suggestions, depending on the type of data that is filled.";

const char kAutofillEnableGoogleIssuedCardName[] =
    "Enable Autofill Google-issued card";
const char kAutofillEnableGoogleIssuedCardDescription[] =
    "When enabled, Google-issued cards will be available in the autofill "
    "suggestions.";

const char kAutofillEnableMerchantBoundVirtualCardsName[] =
    "Offer merchant bound virtual cards in Autofill";
const char kAutofillEnableMerchantBoundVirtualCardsDescription[] =
    "When enabled, Autofill will offer to use merchant bound virtual cards in "
    "payment forms.";

const char kAutofillEnableOfferNotificationCrossTabTrackingName[] =
    "Enable cross tab status tracking for Autofill offer notification";
const char kAutofillEnableOfferNotificationCrossTabTrackingDescription[] =
    "When enabled, the offer notification showing will be tracked cross-tab, "
    "and on one merchant, the notification will only be shown once.";

const char kAutofillEnableOfferNotificationForPromoCodesName[] =
    "Extend Autofill offers and rewards notification to promo code offers";
const char kAutofillEnableOfferNotificationForPromoCodesDescription[] =
    "When enabled, a notification will be displayed on page navigation if the "
    "domain has an eligible merchant promo code offer or reward.";

const char kAutofillEnableOffersInClankKeyboardAccessoryName[] =
    "Enable Autofill offers in keyboard accessory";
const char kAutofillEnableOffersInClankKeyboardAccessoryDescription[] =
    "When enabled, offers will be displayed in the keyboard accessory when "
    "available.";

const char kAutofillEnableOffersInDownstreamName[] =
    "Enable Autofill offers in downstream";
const char kAutofillEnableOffersInDownstreamDescription[] =
    "When enabled, offer data will be retrieved during downstream and shown in "
    "the dropdown list.";

const char kAutofillEnableStickyManualFallbackForCardsName[] =
    "Make manual fallback sticky for credit cards";
const char kAutofillEnableStickyManualFallbackForCardsDescription[] =
    "When enabled, if the user interacts with the manual fallback bottom "
    "sheet, it'll remain sticky until the user dismisses it.";

const char kAutofillEnableToolbarStatusChipName[] =
    "Move Autofill omnibox icons next to the profile avatar icon";
const char kAutofillEnableToolbarStatusChipDescription[] =
    "When enabled, Autofill data related icon will be shown in the status "
    "chip next to the profile avatar icon in the toolbar.";

const char kAutofillEnableVirtualCardName[] =
    "Offer to use cloud token virtual card in Autofill";
const char kAutofillEnableVirtualCardDescription[] =
    "When enabled, if all requirements are met, Autofill will offer to use "
    "virtual credit cards in form filling.";

const char kAutofillEnableVirtualCardsRiskBasedAuthenticationName[] =
    "Enable risk based authentication for Autofill Virtual Card Numbers";
const char kAutofillEnableVirtualCardsRiskBasedAuthenticationDescription[] =
    "When enabled, risk based authentication is used before autofilling "
    "Virtual Card information into forms.";

const char kAutofillFillMerchantPromoCodeFieldsName[] =
    "Enable Autofill of promo code fields in forms";
const char kAutofillFillMerchantPromoCodeFieldsDescription[] =
    "When enabled, Autofill will attempt to fill merchant promo/coupon/gift "
    "code fields when data is available.";

const char kAutofillFixOfferInIncognitoName[] =
    "Enable the fix for Autofill offer in Incognito mode";
const char kAutofillFixOfferInIncognitoDescription[] =
    "When enabled, the fix will be enabled and offers should work correctly in "
    "Incognito mode.";

const char kAutofillHighlightOnlyChangedValuesInPreviewModeName[] =
    "Highlight only changed values in preview mode.";
const char kAutofillHighlightOnlyChangedValuesInPreviewModeDescription[] =
    "When Autofill is previewing filling a form, already autofilled values "
    "and other values that are not changed by accepting the preview should "
    "not be highlighted.";

const char kAutofillParseMerchantPromoCodeFieldsName[] =
    "Parse promo code fields in forms";
const char kAutofillParseMerchantPromoCodeFieldsDescription[] =
    "When enabled, Autofill will attempt to find merchant promo/coupon/gift "
    "code fields when parsing forms.";

const char kAutofillProfileClientValidationName[] =
    "Autofill Validates Profiles By Client";
const char kAutofillProfileClientValidationDescription[] =
    "Allows autofill to validate profiles on the client side";

const char kAutofillProfileServerValidationName[] =
    "Autofill Uses Server Validation";
const char kAutofillProfileServerValidationDescription[] =
    "Allows autofill to use server side validation";

const char kAutofillUseConsistentPopupSettingsIconsName[] =
    "Consistent Autofill settings icon";
const char kAutofillUseConsistentPopupSettingsIconsDescription[] =
    "If enabled, all Autofill data types including addresses, credit cards and "
    "passwords will use a consistent icon in the popup settings footer.";

const char kAutofillSaveAndFillVPAName[] =
    "Offer save and autofill of UPI/VPA values";
const char kAutofillSaveAndFillVPADescription[] =
    "If enabled, when autofill recognizes a UPI/VPA value in a payment form, "
    "it will offer to save it. If saved, it will be offered for filling in "
    "fields which expect a VPA.";

const char kAutofillSuggestVirtualCardsOnIncompleteFormName[] =
    "Autofill suggests virtual cards on incomplete forms";
const char kAutofillSuggestVirtualCardsOnIncompleteFormDescription[] =
    "When enabled, merchant bound virtual cards will be suggested even if not "
    "all "
    "of the card number, exp date and CVC fields are detected in a payment "
    "form.";

const char kAutofillUseImprovedLabelDisambiguationName[] =
    "Autofill Uses Improved Label Disambiguation";
const char kAutofillUseImprovedLabelDisambiguationDescription[] =
    "When enabled, the Autofill dropdown's suggestions' labels are displayed "
    "using the improved disambiguation format.";

const char kAutoScreenBrightnessName[] = "Auto Screen Brightness model";
const char kAutoScreenBrightnessDescription[] =
    "Uses Auto Screen Brightness ML model (if it exists) to adjust screen "
    "brightness based on ambient light. If disabled, screen brightness "
    "will be controlled by the heuristic model provided by powerd (and only "
    "on devices that have ambient light sensors).";

const char kBackForwardCacheName[] = "Back-forward cache";
const char kBackForwardCacheDescription[] =
    "If enabled, caches eligible pages after cross-site navigations."
    "To enable caching pages on same-site navigations too, choose 'enabled "
    "same-site support'.";

const char kBentoBarName[] = "Persistent desks bar";
const char kBentoBarDescription[] =
    "Showing a persistent desks bar at the top of the screen in clamshell mode "
    "when there are more than one desk.";

const char kDragWindowToNewDeskName[] = "Drag window to new desk";
const char kDragWindowToNewDeskDescription[] =
    "Enable dragging and dropping a window to the new desk button in overview "
    "when there are less than the maximum number of desks.";

const char kBiometricReauthForPasswordFillingName[] =
    "Biometric reauth for password filling";
const char kBiometricReauthForPasswordFillingDescription[] =
    "Enables biometric"
    "re-authentication before password filling";

const char kBorealisBigGlName[] = "Borealis Big GL";
const char kBorealisBigGlDescription[] = "Enable Big GL when running Borealis.";

const char kBorealisDiskManagementName[] = "Borealis Disk management";
const char kBorealisDiskManagementDescription[] =
    "Enable experimental disk management settings.";

const char kBorealisForceBetaClientName[] = "Borealis Force Beta Client";
const char kBorealisForceBetaClientDescription[] =
    "Force the client to run its beta version.";

const char kBorealisLinuxModeName[] = "Borealis Linux Mode";
const char kBorealisLinuxModeDescription[] =
    "Do not run ChromeOS-specific code in the client.";

const char kBypassAppBannerEngagementChecksName[] =
    "Bypass user engagement checks";
const char kBypassAppBannerEngagementChecksDescription[] =
    "Bypasses user engagement checks for displaying app banners, such as "
    "requiring that users have visited the site before and that the banner "
    "hasn't been shown recently. This allows developers to test that other "
    "eligibility requirements for showing app banners, such as having a "
    "manifest, are met.";

const char kCheckOfflineCapabilityName[] = "Check offline capability for PWAs";
const char kCheckOfflineCapabilityDescription[] =
    "Use advanced offline capability check to decide whether the browser "
    "displays install prompts for PWAs.";

const char kChromeLabsName[] = "Chrome Labs";
const char kChromeLabsDescription[] =
    "Access Chrome Labs through the toolbar menu to see featured user-facing "
    "experimental features.";

const char kClosedTabCacheName[] = "Closed Tab Cache";
const char kClosedTabCacheDescription[] =
    "Enables closed tab cache to instantaneously restore recently closed tabs. "
    "NOTE: This feature is higly experimental and will lead to various "
    "breakages, enable at your own risk.";

const char kConsolidatedSiteStorageControlsName[] =
    "Consolidated Site Storage Controls";
const char kConsolidatedSiteStorageControlsDescription[] =
    "Enables the consolidated version of Site Storage controls in settings";

const char kContextMenuGoogleLensChipName[] =
    "Google Lens powered image search for surfaced as a chip below the context "
    "menu.";
const char kContextMenuGoogleLensChipDescription[] =
    "Enable a chip for a Shopping intent into Google Lens when supported. ";

const char kContextMenuSearchWithGoogleLensName[] =
    "Google Lens powered image search in the context menu.";
const char kContextMenuSearchWithGoogleLensDescription[] =
    "Replaces default image search with an intent to Google Lens when "
    "supported.";

const char kContextMenuShopWithGoogleLensName[] =
    "Google Lens powered image search for shoppable images in the context "
    "menu.";
const char kContextMenuShopWithGoogleLensDescription[] =
    "Enable a menu item for a Shopping intent into Google Lens when supported. "
    "By default replaces the Search with Google Lens option.";

const char kContextMenuSearchAndShopWithGoogleLensName[] =
    "Additional menu item for Google Lens image search for shoppable images in "
    "the context menu.";
const char kContextMenuSearchAndShopWithGoogleLensDescription[] =
    "Display an additional menu item for a Shopping intent to Google Lens "
    "below Search with Google Lens when Lens shopping feature is enabled";

const char kContextMenuTranslateWithGoogleLensName[] =
    "Google Lens powered image search for translatable images surfaced as a "
    "chip under the context menu.";
const char kContextMenuTranslateWithGoogleLensDescription[] =
    "Enable a chip for a Translate intent into Google Lens when supported.";

const char kClipboardCustomFormatsName[] = "Clipboard Custom Formats";
const char kClipboardCustomFormatsDescription[] =
    "Allows read/write of custom formats with unsanitized clipboard content. "
    "See crbug.com/106449";

const char kClientStorageAccessContextAuditingName[] =
    "Access contexts for client-side storage";
const char kClientStorageAccessContextAuditingDescription[] =
    "Record the first-party contexts in which client-side storage was accessed";

const char kClearCrossSiteCrossBrowsingContextGroupWindowNameName[] =
    "Clear window name in top-level cross-site cross-browsing-context-group "
    "navigation";
const char kClearCrossSiteCrossBrowsingContextGroupWindowNameDescription[] =
    "Clear the preserved window.name property when it's a top-level cross-site "
    "navigation that swaps BrowsingContextGroup.";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kChromeTipsInMainMenuName[] =
    "Show 'Tips for Chrome' in Help portion of main menu.";
const char kChromeTipsInMainMenuDescription[] =
    "Enables 'Tips for Chrome' in main menu; the menu item will take users to "
    "an official Google site with information about the latest and most "
    "popular Chrome features.";

const char kChromeTipsInMainMenuNewBadgeName[] =
    "Show 'New' promo badge on 'Tips for Chrome' in Help portion of main menu.";
const char kChromeTipsInMainMenuNewBadgeDescription[] =
    "Enables 'New' promo badge on 'Tips for Chrome' in main menu; experiment to"
    " test the value of this user education feature.";
#endif

const char kChromeWhatsNewUIName[] =
    "Show Chrome What's New page at chrome://whats-new";
const char kChromeWhatsNewUIDescription[] =
    "Enables Chrome What's New page at chrome://whats-new.";

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
const char kChromeWhatsNewInMainMenuNewBadgeName[] =
    "Show 'New' badge on 'What's New' menu item.";
const char kChromeWhatsNewInMainMenuNewBadgeDescription[] =
    "Enables 'New' promo badge on 'What's New' in the Help portion of the main "
    "menu.";
#endif

const char kCompositingBasedThrottling[] = "Compositing-based Throttling";
const char kCompositingBasedThrottlingDescription[] =
    "Enables compositing-based throttling to throttle appropriate frame sinks "
    "that do not need to be refreshed at high fps.";

const char kDarkLightTestName[] = "Dark/light mode of system UI";
const char kDarkLightTestDescription[] =
    "Enables the dark/light mode of system UI, which includes shelf, launcher, "
    "system tray etc.";

const char kDevicePostureName[] = "Device Posture API";
const char kDevicePostureDescription[] =
    "Enables Device Posture API (foldable devices)";

extern const char kRestrictedApiOriginsName[] = "Restricted API Origins";
extern const char kRestrictedApiOriginsDescription[] =
    "Enables Restricted APIs (Direct Sockets API) for development purposes for "
    "a set of origins, specified as a comma-separated list.";

const char kDoubleBufferCompositingName[] = "Double buffered compositing";
const char kDoubleBufferCompositingDescription[] =
    "Use double buffer for compositing (instead of triple-buffering). "
    "Latency should be reduced in some cases. On the other hand, more skipped "
    "frames are expected.";

const char kFontAccessAPIName[] = "Font Access APIs";
const char kFontAccessAPIDescription[] =
    "Enables the experimental Font Access APIs, giving websites access "
    "to enumerate local fonts and access their table data.";

const char kFontAccessPersistentName[] =
    "Enable persistent access to the Font Access API";
const char kFontAccessPersistentDescription[] =
    "Enables persistent access to the Font Access API, giving websites access "
    "to enumerate local fonts after being granted a permission.";

const char kForceColorProfileSRGB[] = "sRGB";
const char kForceColorProfileP3[] = "Display P3 D65";
const char kForceColorProfileColorSpin[] = "Color spin with gamma 2.4";
const char kForceColorProfileSCRGBLinear[] =
    "scRGB linear (HDR where available)";
const char kForceColorProfileHDR10[] = "HDR10 (HDR where available)";

const char kForceColorProfileName[] = "Force color profile";
const char kForceColorProfileDescription[] =
    "Forces Chrome to use a specific color profile instead of the color "
    "of the window's current monitor, as specified by the operating system.";

const char kDynamicColorGamutName[] = "Dynamic color gamut";
const char kDynamicColorGamutDescription[] =
    "Displays in wide color when the content is wide. When the content is "
    "not wide, displays sRGB";

const char kCooperativeSchedulingName[] = "Cooperative Scheduling";
const char kCooperativeSchedulingDescription[] =
    "Enables cooperative scheduling in Blink.";

const char kDarkenWebsitesCheckboxInThemesSettingName[] =
    "Darken websites checkbox in themes setting";
const char kDarkenWebsitesCheckboxInThemesSettingDescription[] =
    "Show a darken websites checkbox in themes settings when system default or "
    "dark is selected. The checkbox can toggle the auto-darkening web contents "
    "feature";

const char kDebugPackedAppName[] = "Debugging for packed apps";
const char kDebugPackedAppDescription[] =
    "Enables debugging context menu options such as Inspect Element for packed "
    "applications.";

const char kDebugShortcutsName[] = "Debugging keyboard shortcuts";
const char kDebugShortcutsDescription[] =
    "Enables additional keyboard shortcuts that are useful for debugging Ash.";

const char kDetectTargetEmbeddingLookalikesName[] =
    "Detect target embedding domains as lookalikes.";
const char kDetectTargetEmbeddingLookalikesDescription[] =
    "Shows a lookalike interstitial when navigating to target embedding domains"
    "(e.g. google.com.example.com).";

const char kDisableProcessReuse[] = "Disable subframe process reuse";
const char kDisableProcessReuseDescription[] =
    "Prevents out-of-process iframes from reusing compatible processes from "
    "unrelated tabs. This is an experimental mode that will result in more "
    "processes being created.";

const char kDisallowDocWrittenScriptsUiName[] =
    "Block scripts loaded via document.write";
const char kDisallowDocWrittenScriptsUiDescription[] =
    "Disallows fetches for third-party parser-blocking scripts inserted into "
    "the main frame via document.write.";

const char kDocumentTransitionName[] = "documentTransition API";
const char kDocumentTransitionDescription[] =
    "Controls the availability of the documentTransition JavaScript API.";

const char kDocumentTransitionSlowdownFactorName[] =
    "documentTransition API Duration Control";
const char kDocumentTransitionSlowdownFactorDescription[] =
    "Slows down animations triggered by documentTransition JavaScript API for "
    "debugging.";

const char kEnableAutofillAddressSavePromptName[] =
    "Autofill Address Save Prompts";
const char kEnableAutofillAddressSavePromptDescription[] =
    "Enable the Autofill address save prompts.";

const char kEnableAutofillCreditCardAuthenticationName[] =
    "Allow using platform authenticators to retrieve server cards";
const char kEnableAutofillCreditCardAuthenticationDescription[] =
    "When enabled, users will be given the option to use a platform "
    "authenticator (if available) to verify card ownership when retrieving "
    "credit cards from Google Payments.";

const char
    kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersName[] =
        "Display InfoBar footers with account indication information for "
        "single account users";
const char
    kEnableAutofillInfoBarAccountIndicationFooterForSingleAccountUsersDescription
        [] = "When enabled and user has single account, a footer indicating "
             "user's e-mail address  will appear at the bottom of InfoBars "
             "which has corresponding account indication footer flags on.";

const char kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersName[] =
    "Display InfoBar footers with account indication information for "
    "sync users";
const char
    kEnableAutofillInfoBarAccountIndicationFooterForSyncUsersDescription[] =
        "When enabled and user is signed in, a footer indicating user's e-mail "
        "address  will appear at the bottom of InfoBars which has "
        "corresponding account indication footer flags on.";

const char kEnableAutofillPasswordInfoBarAccountIndicationFooterName[] =
    "Display password InfoBar footers with account indication information";
const char kEnableAutofillPasswordInfoBarAccountIndicationFooterDescription[] =
    "When enabled, a footer indicating user's e-mail address will appear at "
    "the bottom of corresponding password InfoBars.";

const char kEnableAutofillSaveCardInfoBarAccountIndicationFooterName[] =
    "Display SaveCardInfoBar footer with account indication information";
const char kEnableAutofillSaveCardInfoBarAccountIndicationFooterDescription[] =
    "When enabled, a footer indicating user's e-mail address will appear at "
    "the bottom of SaveCardInfoBar.";

const char kEnableAutofillCreditCardUploadFeedbackName[] =
    "Enable feedback for credit card upload flow";
const char kEnableAutofillCreditCardUploadFeedbackDescription[] =
    "When enabled, if credit card upload succeeds, the avatar button will "
    "show a highlight, otherwise the icon will be updated and if it is "
    "clicked, the save card failure bubble will be shown.";

const char kEnableExperimentalCookieFeaturesName[] =
    "Enable experimental cookie features";
const char kEnableExperimentalCookieFeaturesDescription[] =
    "Enable new features that affect setting, sending, and managing cookies. "
    "The enabled features are subject to change at any time.";

const char kEnableSaveDataName[] = "Enables save data feature";
const char kEnableSaveDataDescription[] =
    "Enables save data feature. May cause user's traffic to be proxied via "
    "Google's data reduction proxy.";

const char kEnableNavigationPredictorName[] = "Enables navigation predictor";
const char kEnableNavigationPredictorDescription[] =
    "Enables navigation predictor feature that predicts the next likely "
    "navigation using a set of heuristics.";

const char kEnablePreconnectToSearchName[] =
    "Enables preconnections to default search engine";
const char kEnablePreconnectToSearchDescription[] =
    "Enables the feature that preconnects to the user's default search engine.";

const char kEnableRawDrawName[] = "Enable raw draw";
const char kEnableRawDrawDescription[] =
    "When enabled, web content will be rastered on output surface directly.";

const char kEnableRemovingAllThirdPartyCookiesName[] =
    "Enable removing SameSite=None cookies";
const char kEnableRemovingAllThirdPartyCookiesDescription[] =
    "Enables UI on chrome://settings/siteData to remove all third-party "
    "cookies and site data.";

const char kEnableBrowsingDataLifetimeManagerName[] =
    "Enables the BrowsingDataLifetimeManager service to run.";
const char kEnableBrowsingDataLifetimeManagerDescription[] =
    "Enables the BrowsingDataLifetimeManager service to run and periodically "
    "delete browsing data as specified by the BrowsingDataLifetime policy.";

const char kColorProviderRedirectionForThemeProviderName[] =
    "Color Provider Redirection For Theme Provider";
const char kColorProviderRedirectionForThemeProviderDescription[] =
    "Redirects color requests from the ThemeProvider to the ColorProvider "
    "where possible.";

const char kDesktopPWAsAdditionalWindowingControlsName[] =
    "Desktop PWA Window Minimize/maximize/restore";
const char kDesktopPWAsAdditionalWindowingControlsDescription[] =
    "Enable PWAs to manually recreate the minimize, maximize and restore "
    "window functionalities with respective APIs.";

const char kDesktopPWAsPrefixAppNameInWindowTitleName[] =
    "Desktop PWAs prefix window title with app name.";
const char kDesktopPWAsPrefixAppNameInWindowTitleDescription[] =
    "Prefix the window title of installed PWAs with the name of the PWA. On "
    "ChromeOS this is visible only in the window/activity switcher.";

const char kDesktopPWAsAppIconShortcutsMenuUIName[] =
    "Desktop PWAs app icon shortcuts menu UI";
const char kDesktopPWAsAppIconShortcutsMenuUIDescription[] =
    "Show web app shortcuts in the shelf context menu";

const char kDesktopPWAsRemoveStatusBarName[] = "Desktop PWAs remove status bar";
const char kDesktopPWAsRemoveStatusBarDescription[] =
    "Hides the status bar popup in Desktop PWA app windows.";

const char kDesktopPWAsElidedExtensionsMenuName[] =
    "Desktop PWAs elided extensions menu";
const char kDesktopPWAsElidedExtensionsMenuDescription[] =
    "Moves the Extensions \"puzzle piece\" icon from the title bar into the "
    "app menu for web app windows.";

const char kDesktopPWAsFlashAppNameInsteadOfOriginName[] =
    "Desktop PWAs flash app name instead of origin";
const char kDesktopPWAsFlashAppNameInsteadOfOriginDescription[] =
    "Replaces the origin flash with an app name flash when launching a web app "
    "window.";

const char kDesktopPWAsNotificationIconAndTitleName[] =
    "Desktop PWAs improvements in notification icon and title";
const char kDesktopPWAsNotificationIconAndTitleDescription[] =
    "Replaces the websites origin and the Chrome icon with the web app's name "
    "and app icon in notifications.";

const char kDesktopPWAsLinkCapturingName[] =
    "Desktop PWA declarative link capturing";
const char kDesktopPWAsLinkCapturingDescription[] =
    "Enable web app manifests to declare link capturing behavior. Prototype "
    "implementation of: "
    "https://github.com/WICG/sw-launch/blob/master/"
    "declarative_link_capturing.md";

const char kDesktopPWAsLaunchHandlerName[] = "Desktop PWA launch handler";
const char kDesktopPWAsLaunchHandlerDescription[] =
    "Enable web app manifests to declare app launch behavior. Prototype "
    "implementation of: "
    "https://github.com/WICG/sw-launch/blob/main/launch_handler.md";

const char kDesktopPWAsManifestIdName[] = "Desktop PWA manifest id";
const char kDesktopPWAsManifestIdDescription[] =
    "Enable web app manifests to declare id. Prototype "
    "implementation of: "
    "https://github.com/philloooo/pwa-unique-id/blob/main/explainer.md";

const char kDesktopPWAsTabStripName[] = "Desktop PWA tab strips";
const char kDesktopPWAsTabStripDescription[] =
    "Experimental UI for exploring what PWA windows would look like with a tab "
    "strip.";

const char kDesktopPWAsTabStripLinkCapturingName[] =
    "Desktop PWA tab strip link capturing";
const char kDesktopPWAsTabStripLinkCapturingDescription[] =
    "Experimental behaviour for \"Desktop PWA tab strips\" to capture link "
    "navigations within the app scope and bring them into the app's tabbed "
    "window.";

const char kDesktopPWAsTabStripSettingsName[] =
    "Desktop PWA tab strips settings";
const char kDesktopPWAsTabStripSettingsDescription[] =
    "Experimental UI for selecting whether a PWA should open in tabbed mode.";

const char kDesktopPWAsSubAppsName[] = "Desktop PWA Sub Apps";
const char kDesktopPWAsSubAppsDescription[] =
    "Enable installed PWAs to create shortcuts by installing their sub apps. "
    "Prototype implementation of: "
    "https://github.com/ivansandrk/multi-apps/blob/main/explainer.md";

const char kDesktopPWAsProtocolHandlingName[] = "Desktop PWA Protocol handling";
const char kDesktopPWAsProtocolHandlingDescription[] =
    "Enable web app manifests to declare protocol handling behavior."
    "See: https://crbug.com/1019239.";

const char kDesktopPWAsUrlHandlingName[] = "Desktop PWA URL handling";
const char kDesktopPWAsUrlHandlingDescription[] =
    "Enable web app manifests to declare URL handling behavior. Prototype "
    "implementation of: "
    "https://github.com/WICG/pwa-url-handler/blob/master/explainer.md";

const char kDesktopPWAsWindowControlsOverlayName[] =
    "Desktop PWA Window Controls Overlay";
const char kDesktopPWAsWindowControlsOverlayDescription[] =
    "Enable web app manifests to declare Window Controls Overlay as a display "
    "override. Prototype implementation of: "
    "https://github.com/WICG/window-controls-overlay/blob/main/explainer.md";

const char kDesktopPWAsWebBundlesName[] = "Desktop PWAs Web Bundles";
const char kDesktopPWAsWebBundlesDescription[] =
    "Adds support for web bundles, making web apps able to be launched "
    "offline.";

const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteName[] =
    "Migrate default G Suite Chrome apps to web apps";
const char kEnableMigrateDefaultChromeAppToWebAppsGSuiteDescription[] =
    "Enable the migration of default installed G Suite Chrome apps over to "
    "their corresponding web apps.";

const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteName[] =
    "Migrate default non-G Suite Chrome apps to web apps";
const char kEnableMigrateDefaultChromeAppToWebAppsNonGSuiteDescription[] =
    "Enable the migration of default installed non-G Suite Chrome apps over to "
    "their corresponding web apps.";

const char kEnableSyncRequiresPoliciesLoadedName[] =
    "Sync waits for all policies to load before starting";
const char kEnableSyncRequiresPoliciesLoadedDescription[] =
    "Enables behaviour for Sync to wait for all policies to load before "
    "starting";

const char kEnableTLS13EarlyDataName[] = "TLS 1.3 Early Data";
const char kEnableTLS13EarlyDataDescription[] =
    "This option enables TLS 1.3 Early Data, allowing GET requests to be sent "
    "during the handshake when resuming a connection to a compatible TLS 1.3 "
    "server.";

const char kEnhancedNetworkVoicesName[] = "Enhanced network voices";
const char kEnhancedNetworkVoicesDescription[] =
    "This option enables high-quality, network-based voices in "
    "Select-to-speak.";

const char kPostQuantumCECPQ2Name[] = "TLS Post-Quantum Confidentiality";
const char kPostQuantumCECPQ2Description[] =
    "This option enables a post-quantum (i.e. resistent to quantum computers) "
    "key exchange algorithm in TLS (CECPQ2).";

const char kMacCoreLocationBackendName[] = "Core Location Backend";
const char kMacCoreLocationBackendDescription[] =
    "Enables usage of the Core Location APIs as the backend for Geolocation "
    "API";

const char kNewMacNotificationAPIName[] =
    "Determines which notification API to use on macOS devices";
const char kNewMacNotificationAPIDescription[] =
    "Enables the usage of Apple's new notification API which will run on macOS "
    "10.14+";

const char kWinrtGeolocationImplementationName[] =
    "WinRT Geolocation Implementation";
const char kWinrtGeolocationImplementationDescription[] =
    "Enables usage of the Windows.Devices.Geolocation WinRT APIs on Windows "
    "for geolocation";

const char kEnableFencedFramesName[] = "Enable the <fencedframe> element.";
const char kEnableFencedFramesDescription[] =
    "Fenced frames are an experimental web platform feature that allows "
    "embedding an isolated top-level page. See "
    "https://github.com/shivanigithub/fenced-frame";

const char kEnableFirmwareUpdaterAppName[] = "Enable firmware updater app";
const char kEnableFirmwareUpdaterAppDescription[] =
    "Enable the firmware updater SWA, allowing users to update firmware "
    "on supported peripherals.";

const char kEnableGamepadButtonAxisEventsName[] =
    "Gamepad Button and Axis Events";
const char kEnableGamepadButtonAxisEventsDescription[] =
    "Enables the ability to subscribe to changes in buttons and/or axes "
    "on the gamepad object.";

const char kEnableGenericSensorExtraClassesName[] =
    "Generic Sensor Extra Classes";
const char kEnableGenericSensorExtraClassesDescription[] =
    "Enables an extra set of sensor classes based on Generic Sensor API, which "
    "expose previously unavailable platform features, i.e. AmbientLightSensor "
    "and Magnetometer interfaces.";

const char kEnableGpuServiceLoggingName[] = "Enable gpu service logging";
const char kEnableGpuServiceLoggingDescription[] =
    "Enable printing the actual GL driver calls.";

const char kEnableShortcutCustomizationAppName[] =
    "Enable shortcut customization app";
const char kEnableShortcutCustomizationAppDescription[] =
    "Enable the shortcut customization SWA, allowing users to customize system "
    "shortcuts.";

const char kEnableSRPIsolatedPrerendersName[] =
    "Enable Navigation Predictor Isolated Prerenders";
const char kEnableSRPIsolatedPrerendersDescription[] =
    "Enable Navigation Predictions on the Google SRP to be fully isolated.";

const char kEnableSRPIsolatedPrerenderProbingName[] =
    "Enable Probing on Navigation Predictor Isolated Prerenders";
const char kEnableSRPIsolatedPrerenderProbingDescription[] =
    "Enable probing checks for Isolated Prerenders which will block commit.";

const char kEnableSRPIsolatedPrerendersNSPName[] =
    "Enable NoStatePrefetch on Navigation Predictor Isolated Prerenders";
const char kEnableSRPIsolatedPrerendersNSPDescription[] =
    "Enables NoStatePrefetch on Isolated Prerenders.";

const char kDownloadAutoResumptionNativeName[] =
    "Enable download auto-resumption in native";
const char kDownloadAutoResumptionNativeDescription[] =
    "Enables download auto-resumption in native";

const char kDownloadLaterName[] = "Enable download later";
const char kDownloadLaterDescription[] = "Enables download later feature.";

const char kDownloadLaterDebugOnWifiName[] =
    "Show download later dialog on WIFI.";
const char kDownloadLaterDebugOnWifiNameDescription[] =
    "Show download later dialog on WIFI.";

const char kDownloadProgressMessageName[] = "Show download progress message";
const char kDownloadProgressMessageDescription[] =
    "Shows download progress message UI.";

const char kDownloadRangeName[] = "Enable download range support";
const char kDownloadRangeDescription[] =
    "Enables arbitrary download range request support.";

const char kEnableCanvasContextLostInBackgroundName[] =
    "Enable canvas context to be lost in background";
const char kEnableCanvasContextLostInBackgroundDescription[] =
    "Enable canvas context to be cleared when it is running in background";

const char kEnableLazyFrameLoadingName[] = "Enable lazy frame loading";
const char kEnableLazyFrameLoadingDescription[] =
    "Defers the loading of iframes marked with the attribute 'loading=lazy' "
    "until the page is scrolled down near them.";

const char kEnableLazyImageLoadingName[] = "Enable lazy image loading";
const char kEnableLazyImageLoadingDescription[] =
    "Defers the loading of images marked with the attribute 'loading=lazy' "
    "until the page is scrolled down near them.";

const char kEnableNetworkLoggingToFileName[] = "Enable network logging to file";
const char kEnableNetworkLoggingToFileDescription[] =
    "Enables network logging to a file named netlog.json in the user data "
    "directory. The file can be imported into chrome://net-internals.";

const char kEnableNewDownloadBackendName[] = "Enable new download backend";
const char kEnableNewDownloadBackendDescription[] =
    "Enables the new download backend that uses offline content provider";

const char kEnablePortalsName[] = "Enable Portals.";
const char kEnablePortalsDescription[] =
    "Portals are an experimental web platform feature that allows embedding"
    " and seamless transitions between pages."
    " See https://github.com/WICG/portals and https://wicg.github.io/portals/";

const char kEnablePortalsCrossOriginName[] = "Enable cross-origin Portals.";
const char kEnablePortalsCrossOriginDescription[] =
    "Allows portals to load cross-origin URLs in addition to same-origin ones."
    " Has no effect if Portals are not enabled.";

const char kEnableTranslateSubFramesName[] = "Translate sub frames";
const char kEnableTranslateSubFramesDescription[] =
    "Enable the translation of sub frames (as well as the main frame)";

const char kEnableWindowsGamingInputDataFetcherName[] =
    "Enable Windows.Gaming.Input";
const char kEnableWindowsGamingInputDataFetcherDescription[] =
    "Enable Windows.Gaming.Input by default to provide game controller "
    "support on Windows 10 desktop.";

const char kBlockInsecurePrivateNetworkRequestsName[] =
    "Block insecure private network requests.";
const char kBlockInsecurePrivateNetworkRequestsDescription[] =
    "Prevents non-secure contexts from making sub-resource requests to "
    "more-private IP addresses. An IP address IP1 is more private than IP2 if "
    "1) IP1 is localhost and IP2 is not, or 2) IP1 is private and IP2 is "
    "public. This is a first step towards full enforcement of CORS-RFC1918: "
    "https://wicg.github.io/cors-rfc1918";

const char kCrossOriginEmbedderPolicyCredentiallessName[] =
    "Enable Cross-Origin-Embedder-Policy: credentialless";
const char kCrossOriginEmbedderPolicyCredentiallessDescription[] =
    "Credentialless is a Cross-Origin-Embedder-Policy (COEP) variant. "
    "COEP:credentialless causes no-cors cross-origin requests not to include "
    "credentials (cookies, client certificates, etc...). Similarly to "
    "require-corp, it can be used to enable cross-origin-isolation.";

const char kDeprecateAltClickName[] =
    "Enable Alt+Click deprecation notifications";
const char kDeprecateAltClickDescription[] =
    "Start providing notifications about Alt+Click deprecation and enable "
    "Search+Click as an alternative.";

const char kDeprecateAltBasedSixPackName[] =
    "Deprecate Alt based six-pack (PgUp, PgDn, Home, End, Delete, Insert)";
const char kDeprecateAltBasedSixPackDescription[] =
    "Show deprecation notifications and disable functionality for Alt based "
    "six pack deprecations. The Search based versions continue to work.";

const char kDiagnosticsAppName[] = "Diagnostics app";
const char kDiagnosticsAppDescription[] =
    "Enables the Diagnostics app that allows Chrome OS users to be able to "
    "view their system telemetric information and run diagnostic tests for "
    "their device.";

const char kDiagnosticsAppNavigationName[] = "Diagnostics app navigation";
const char kDiagnosticsAppNavigationDescription[] =
    "Enables the navigation panel in the Diagnostics app.";

const char kDisableKeepaliveFetchName[] = "Disable fetch with keepalive set";
const char kDisableKeepaliveFetchDescription[] =
    "Disable fetch with keepalive set "
    "(https://fetch.spec.whatwg.org/#request-keepalive-flag).";

const char kExperimentalAccessibilityLanguageDetectionName[] =
    "Experimental accessibility language detection";
const char kExperimentalAccessibilityLanguageDetectionDescription[] =
    "Enable language detection for in-page content which is then exposed to "
    "assistive technologies such as screen readers.";

const char kExperimentalAccessibilityLanguageDetectionDynamicName[] =
    "Experimental accessibility language detection for dynamic content";
const char kExperimentalAccessibilityLanguageDetectionDynamicDescription[] =
    "Enable language detection for dynamic content which is then exposed to "
    "assistive technologies such as screen readers.";

const char kCompositorThreadedScrollbarScrollingName[] =
    "Compositor threaded scrollbar scrolling";
const char kCompositorThreadedScrollbarScrollingDescription[] =
    "Enables pointer-based scrollbar scrolling on the compositor thread "
    "instead of the main thread";

const char kMemlogName[] = "Chrome heap profiler start mode.";
const char kMemlogDescription[] =
    "Starts heap profiling service that records sampled memory allocation "
    "profile having each sample attributed with a callstack. "
    "The sampling resolution is controlled with --memlog-sampling-rate flag. "
    "Recorded heap dumps can be obtained at chrome://tracing "
    "[category:memory-infra] and chrome://memory-internals. This setting "
    "controls which processes will be profiled since their start. To profile "
    "any given process at a later time use chrome://memory-internals page.";
const char kMemlogModeMinimal[] = "Browser and GPU";
const char kMemlogModeAll[] = "All processes";
const char kMemlogModeAllRenderers[] = "All renderers";
const char kMemlogModeRendererSampling[] = "Single renderer";
const char kMemlogModeBrowser[] = "Browser only";
const char kMemlogModeGpu[] = "GPU only";

const char kMemlogSamplingRateName[] =
    "Heap profiling sampling interval (in bytes).";
const char kMemlogSamplingRateDescription[] =
    "Heap profiling service uses Poisson process to sample allocations. "
    "Default value for the interval between samples is 1000000 (1MB). "
    "This results in low noise for large and/or frequent allocations "
    "[size * frequency >> 1MB]. This means that aggregate numbers [e.g. "
    "total size of malloc-ed objects] and large and/or frequent allocations "
    "can be trusted with high fidelity. "
    "Lower intervals produce higher samples resolution, but come at a cost of "
    "higher performance overhead.";
const char kMemlogSamplingRate10KB[] = "10KB";
const char kMemlogSamplingRate50KB[] = "50KB";
const char kMemlogSamplingRate100KB[] = "100KB";
const char kMemlogSamplingRate500KB[] = "500KB";
const char kMemlogSamplingRate1MB[] = "1MB";
const char kMemlogSamplingRate5MB[] = "5MB";

const char kMemlogStackModeName[] = "Heap profiling stack traces type.";
const char kMemlogStackModeDescription[] =
    "By default heap profiling service records native stacks. "
    "A post-processing step is required to symbolize the stacks. "
    "'Native with thread names' adds the thread name as the first frame of "
    "each native stack. It's also possible to record a pseudo stack using "
    "trace events as identifiers. It's also possible to do a mix of both.";
const char kMemlogStackModeMixed[] = "Mixed";
const char kMemlogStackModeNative[] = "Native";
const char kMemlogStackModeNativeWithThreadNames[] = "Native with thread names";
const char kMemlogStackModePseudo[] = "Trace events";

const char kEnableAutomaticSnoozeName[] = "Enable Automatic Snooze";
const char kEnableAutomaticSnoozeDescription[] =
    "Enables automatic snoozing on In-Product Help with no snooze button.";

const char kEnableLensRegionSearchFlagId[] = "enable-lens-region-search";
const char kEnableLensRegionSearchName[] =
    "Search your screen with Google Lens";
const char kEnableLensRegionSearchDescription[] =
    "Right click and select \"Search images with Google Lens\" to "
    "search any region of the site to learn more about the visual content you "
    "see while you browse and shop on the web.";

const char kEnableLoginDetectionName[] = "Enable login detection";
const char kEnableLoginDetectionDescription[] =
    "Allow user sign-in to be detected based on heuristics.";

const char kEnableManagedConfigurationWebApiName[] =
    "Enable Managed Configuration Web API";
const char kEnableManagedConfigurationWebApiDescription[] =
    "Allows website to access a managed configuration provided by the device "
    "administrator for the origin.";

const char kEnablePciguardUiName[] =
    "Enable Pciguard (Thunderbolt + USB4 tunneling) UI for settings";
const char kEnablePciguardUiDescription[] =
    "Enable toggling Pciguard settings through the Settings App. By default, "
    "this flag is enabled.";

const char kEnablePenetratingImageSelectionName[] =
    "Penetrating Image Selection";
const char kEnablePenetratingImageSelectionDescription[] =
    "Enables image options to be surfaced in the context menu for nodes "
    "covered by transparent overlays.";

const char kEnablePixelCanvasRecordingName[] = "Enable pixel canvas recording";
const char kEnablePixelCanvasRecordingDescription[] =
    "Pixel canvas recording allows the compositor to raster contents aligned "
    "with the pixel and improves text rendering. This should be enabled when a "
    "device is using fractional scale factor.";

const char kReduceHorizontalFlingVelocityName[] =
    "Reduce horizontal fling velocity";
const char kReduceHorizontalFlingVelocityDescription[] =
    "Reduces the velocity of horizontal flings to 20\% of their original"
    "velocity.";

const char kEnableCssSelectorFragmentAnchorName[] =
    "Enables CSS selector fragment anchors";
const char kEnableCssSelectorFragmentAnchorDescription[] =
    "Similar to text directives, CSS selector directives can be specified "
    "in a url which is to be scrolled into view and highlighted.";

const char kRetailCouponsName[] = "Enable to fetch for retail coupons";
const char kRetailCouponsDescription[] =
    "Allow to fetch retail coupons for consented users";

const char kEnableResamplingInputEventsName[] =
    "Enable resampling input events";
const char kEnableResamplingInputEventsDescription[] =
    "Predicts mouse and touch inputs position at rAF time based on previous "
    "input";
const char kEnableResamplingScrollEventsName[] =
    "Enable resampling scroll events";
const char kEnableResamplingScrollEventsDescription[] =
    "Predicts the scroll amount at vsync time based on previous input";
const char kEnableResamplingScrollEventsExperimentalPredictionName[] =
    "Enable experimental prediction for scroll events";
const char kEnableResamplingScrollEventsExperimentalPredictionDescription[] =
    "Predicts the scroll amount after the vsync time to more closely match "
    "when the frame is visible.";

const char kEnableRestrictedWebApisName[] =
    "Enable the restriced web APIs for high-trusted apps.";
const char kEnableRestrictedWebApisDescription[] =
    "Enable the restricted web APIs for dev trial. This will be replaced with "
    "permission policies to control the capabilities afterwards.";

const char kEnableUseZoomForDsfName[] =
    "Use Blink's zoom for device scale factor.";
const char kEnableUseZoomForDsfDescription[] =
    "If enabled, Blink uses its zooming mechanism to scale content for device "
    "scale factor.";
const char kEnableUseZoomForDsfChoiceDefault[] = "Default";
const char kEnableUseZoomForDsfChoiceEnabled[] = "Enabled";
const char kEnableUseZoomForDsfChoiceDisabled[] = "Disabled";

const char kEnableSubresourceRedirectName[] =
    "Enable Subresource Redirect Compression";
const char kEnableSubresourceRedirectDescription[] =
    "Allow subresource compression for data savings";

const char kEnableWebAuthenticationCableV2SupportName[] =
    "Web Authentication caBLE v2 support";
const char kEnableWebAuthenticationCableV2SupportDescription[] =
    "Enable use of phones that are signed into the same account, with Sync "
    "enabled, to be used as 2nd-factor security keys.";

const char kEnableWebAuthenticationChromeOSAuthenticatorName[] =
    "ChromeOS platform Web Authentication support";
const char kEnableWebAuthenticationChromeOSAuthenticatorDescription[] =
    "Enable the ChromeOS platform authenticator for the Web Authentication "
    "API.";

const char kExperimentalWebAssemblyFeaturesName[] = "Experimental WebAssembly";
const char kExperimentalWebAssemblyFeaturesDescription[] =
    "Enable web pages to use experimental WebAssembly features.";

const char kEnableWasmBaselineName[] = "WebAssembly baseline compiler";
const char kEnableWasmBaselineDescription[] =
    "Enables WebAssembly baseline compilation and tier up.";

const char kEnableWasmLazyCompilationName[] = "WebAssembly lazy compilation";
const char kEnableWasmLazyCompilationDescription[] =
    "Enables lazy (JIT on first call) compilation of WebAssembly modules.";

const char kEnableWasmTieringName[] = "WebAssembly tiering";
const char kEnableWasmTieringDescription[] =
    "Enables tiered compilation of WebAssembly (will tier up to TurboFan if "
    "#enable-webassembly-baseline is enabled).";

const char kEvDetailsInPageInfoName[] = "EV certificate details in Page Info.";
const char kEvDetailsInPageInfoDescription[] =
    "Shows the EV certificate details in the Page Info bubble.";

const char kExperimentalWebPlatformFeaturesName[] =
    "Experimental Web Platform features";
const char kExperimentalWebPlatformFeaturesDescription[] =
    "Enables experimental Web Platform features that are in development.";

const char kExtensionContentVerificationName[] =
    "Extension Content Verification";
const char kExtensionContentVerificationDescription[] =
    "This flag can be used to turn on verification that the contents of the "
    "files on disk for extensions from the webstore match what they're "
    "expected to be. This can be used to turn on this feature if it would not "
    "otherwise have been turned on, but cannot be used to turn it off (because "
    "this setting can be tampered with by malware).";
const char kExtensionContentVerificationBootstrap[] =
    "Bootstrap (get expected hashes, but do not enforce them)";
const char kExtensionContentVerificationEnforce[] =
    "Enforce (try to get hashes, and enforce them if successful)";
const char kExtensionContentVerificationEnforceStrict[] =
    "Enforce strict (hard fail if we can't get hashes)";

const char kExtensionsMenuAccessControlName[] =
    "Extensions Menu Access Control";
const char kExtensionsMenuAccessControlDescription[] =
    "Enables a redesigned extensions menu that allows the user to control "
    "extensions site access.";

const char kExtensionsOnChromeUrlsName[] = "Extensions on chrome:// URLs";
const char kExtensionsOnChromeUrlsDescription[] =
    "Enables running extensions on chrome:// URLs, where extensions explicitly "
    "request this permission.";

const char kFilteringScrollPredictionName[] = "Filtering scroll prediction";
const char kFilteringScrollPredictionDescription[] =
    "Enable filtering of predicted scroll events";

const char kFractionalScrollOffsetsName[] = "Fractional Scroll Offsets";
const char kFractionalScrollOffsetsDescription[] =
    "Enables fractional scroll offsets inside Blink, exposing non-integer "
    "offsets to web APIs.";

const char kForceEffectiveConnectionTypeName[] =
    "Override effective connection type";
const char kForceEffectiveConnectionTypeDescription[] =
    "Overrides the effective connection type of the current connection "
    "returned by the network quality estimator. Slow 2G on Cellular returns "
    "Slow 2G when connected to a cellular network, and the actual estimate "
    "effective connection type when not on a cellular network.";
const char kEffectiveConnectionTypeUnknownDescription[] = "Unknown";
const char kEffectiveConnectionTypeOfflineDescription[] = "Offline";
const char kEffectiveConnectionTypeSlow2GDescription[] = "Slow 2G";
const char kEffectiveConnectionTypeSlow2GOnCellularDescription[] =
    "Slow 2G On Cellular";
const char kEffectiveConnectionType2GDescription[] = "2G";
const char kEffectiveConnectionType3GDescription[] = "3G";
const char kEffectiveConnectionType4GDescription[] = "4G";

const char kFedCmName[] = "FedCM";
const char kFedCmDescription[] =
    "Enables JavaScript API to intermediate federated identity requests.";

const char kFileHandlingAPIName[] = "File Handling API";
const char kFileHandlingAPIDescription[] =
    "Enables the file handling API, allowing websites to register as file "
    "handlers.";

const char kFileHandlingIconsName[] = "File Handling Icons";
const char kFileHandlingIconsDescription[] =
    "Allows websites using the file handling API to also register file type "
    "icons. See https://github.com/WICG/file-handling/blob/main/explainer.md "
    "for more information.";

const char kFillingAcrossAffiliatedWebsitesName[] =
    "Fill passwords across affiliated websites.";
const char kFillingAcrossAffiliatedWebsitesDescription[] =
    "Enables filling password on a website when there is saved "
    "password on affiliated website.";

const char kFillOnAccountSelectName[] = "Fill passwords on account selection";
const char kFillOnAccountSelectDescription[] =
    "Filling of passwords when an account is explicitly selected by the user "
    "rather than autofilling credentials on page load.";

const char kForceTextDirectionName[] = "Force text direction";
const char kForceTextDirectionDescription[] =
    "Explicitly force the per-character directionality of UI text to "
    "left-to-right (LTR) or right-to-left (RTL) mode, overriding the default "
    "direction of the character language.";
const char kForceDirectionLtr[] = "Left-to-right";
const char kForceDirectionRtl[] = "Right-to-left";

const char kForceUiDirectionName[] = "Force UI direction";
const char kForceUiDirectionDescription[] =
    "Explicitly force the UI to left-to-right (LTR) or right-to-left (RTL) "
    "mode, overriding the default direction of the UI language.";

const char kGlobalMediaControlsModernUIName[] =
    "Global Media Controls Modern UI";

const char kGlobalMediaControlsModernUIDescription[] =
    "Use a redesigned version of the Global Media Controls UI. Requires "
    "#global-media-controls to also be enabled.";

const char kGlobalMediaControlsOverlayControlsName[] =
    "Enable overlay controls for Global Media Controls";
const char kGlobalMediaControlsOverlayControlsDescription[] =
    "Allowing controls to be dragged out from Global Media Controls dialog. "
    "Requires #global-media-controls to also be enabled.";

const char kOpenscreenCastStreamingSessionName[] =
    "Enable Open Screen Library (libcast) as the Mirroring Service's Cast "
    "Streaming implementation";
const char kOpenscreenCastStreamingSessionDescription[] =
    "Enables Open Screen Library's (libcast) Cast Streaming implementation to "
    "be used for negotiating and executing mirroring and remoting sessions.";

const char kCastStreamingAv1Name[] =
    "Enable AV1 codec video encoding in Cast mirroring sessions";
const char kCastStreamingAv1Description[] =
    "Enables the inclusion of AV1 codec video encoding in Cast mirroring "
    "session negotiations.";

const char kCastStreamingVp9Name[] =
    "Enable VP9 codec video encoding in Cast mirroring sessions";
const char kCastStreamingVp9Description[] =
    "Enables the inclusion of VP9 codec video encoding in Cast mirroring "
    "session negotiations.";

const char kGoogleLensSdkIntentName[] =
    "Enable the use of the Lens SDK when starting intent into Lens.";
const char kGoogleLensSdkIntentDescription[] =
    "Starts Lens using the Lens SDK if supported.";

const char kGpuRasterizationName[] = "GPU rasterization";
const char kGpuRasterizationDescription[] = "Use GPU to rasterize web content.";

const char kHandwritingGestureEditingName[] = "Handwriting Gestures Editing";
const char kHandwritingGestureEditingDescription[] =
    "Enables editing with handwriting gestures within the virtual keyboard.";

const char kHandwritingLegacyRecognitionName[] =
    "Handwriting Legacy Recognition";
const char kHandwritingLegacyRecognitionDescription[] =
    "Enables new on-device recognition for handwriting legacy paths.";

const char kHandwritingLegacyRecognitionAllLangName[] =
    "Handwriting Legacy Recognition All Languages";
const char kHandwritingLegacyRecognitionAllLangDescription[] =
    "Enables new on-device recognition for handwriting legacy paths in all "
    "supported languages.";

const char kHardwareMediaKeyHandling[] = "Hardware Media Key Handling";
const char kHardwareMediaKeyHandlingDescription[] =
    "Enables using media keys to control the active media session. This "
    "requires MediaSessionService to be enabled too";

const char kHeavyAdPrivacyMitigationsName[] = "Heavy ad privacy mitigations";
const char kHeavyAdPrivacyMitigationsDescription[] =
    "Enables privacy mitigations for the heavy ad intervention. Disabling "
    "this makes the intervention deterministic. Defaults to enabled.";

const char kHeavyAdInterventionName[] = "Heavy Ad Intervention";
const char kHeavyAdInterventionDescription[] =
    "Unloads ads that use too many device resources.";

const char kHideShelfControlsInTabletModeName[] =
    "Hide shelf control buttons in tablet mode.";

const char kHideShelfControlsInTabletModeDescription[] =
    "Hides home, back, and overview button from the shelf while the device is "
    "in tablet mode. Predicated on shelf-hotseat feature being enabled.";

const char kTabSearchMediaTabsId[] = "tab-search-media-tabs";
const char kTabSearchMediaTabsName[] = "Tab Search Media Tabs";
const char kTabSearchMediaTabsDescription[] =
    "Enable indicators on media tabs in Tab Search.";

const char kTabSwitcherOnReturnName[] = "Tab switcher on return";
const char kTabSwitcherOnReturnDescription[] =
    "Enable tab switcher on return after specified time has elapsed";

const char kHttpsOnlyModeName[] = "HTTPS-First Mode Setting";
const char kHttpsOnlyModeDescription[] =
    "Adds a setting under chrome://settings/security to opt-in to HTTPS-First "
    "Mode.";

const char kIgnoreGpuBlocklistName[] = "Override software rendering list";
const char kIgnoreGpuBlocklistDescription[] =
    "Overrides the built-in software rendering list and enables "
    "GPU-acceleration on unsupported system configurations.";

const char kImprovedDesksKeyboardShortcutsName[] =
    "Enable improved desks keyboard shortcuts";
const char kImprovedDesksKeyboardShortcutsDescription[] =
    "Enable keyboard shortcuts for activating desks at specific indices and "
    "toggling whether a window is assigned to all desks. Must be used with "
    "the #improved-keyboard-shortcuts flag.";

const char kImprovedKeyboardShortcutsName[] =
    "Enable improved keyboard shortcuts";
const char kImprovedKeyboardShortcutsDescription[] =
    "Ensure keyboard shortcuts work consistently with international keyboard "
    "layouts and deprecate legacy shortcuts.";

const char kImpulseScrollAnimationsName[] = "Impulse-style scroll animations";
const char kImpulseScrollAnimationsDescription[] =
    "Replaces the default scroll animation with Impulse-style scroll "
    "animations.";

const char kIncognitoBrandConsistencyForAndroidName[] =
    "Enable Incognito brand consistency in Android.";
const char kIncognitoBrandConsistencyForAndroidDescription[] =
    "When enabled, keeps Incognito UI consistent regardless of any selected "
    "theme.";

const char kIncognitoDownloadsWarningName[] =
    "Enable Incognito downloads warning";
const char kIncognitoDownloadsWarningDescription[] =
    "When enabled, users will be warned that downloaded files are saved on the "
    "device and might be seen by other users even if they are in Incognito.";

const char kIncognitoReauthenticationForAndroidName[] =
    "Enable device reauthentication for Incognito.";
const char kIncognitoReauthenticationForAndroidDescription[] =
    "When enabled, a setting appears in Settings > Privacy and Security, to "
    "enable reauthentication for accessing your existing Incognito tabs.";

const char kIncognitoBrandConsistencyForDesktopName[] =
    "Enable Incognito brand consistency in desktop.";
const char kIncognitoBrandConsistencyForDesktopDescription[] =
    "When enabled, removes any theme or background customization done by the "
    "user on the Incognito UI.";

const char kIncognitoClearBrowsingDataDialogForDesktopName[] =
    "Enable clear browsing data dialog in Incognito.";
const char kIncognitoClearBrowsingDataDialogForDesktopDescription[] =
    "When enabled, clear browsing data option would be enabled in Incognito "
    "which upon clicking would show a dialog to close all Incognito windows.";

const char kUpdateHistoryEntryPointsInIncognitoName[] =
    "Update history entry points in Incognito.";
const char kUpdateHistoryEntryPointsInIncognitoDescription[] =
    "When enabled, the entry points to history UI from Incognito mode will be "
    "removed for iOS and Desktop. An educative placeholder will be shown for "
    "Android history page.";

const char kIncognitoNtpRevampName[] = "Revamped Incognito New Tab Page";
const char kIncognitoNtpRevampDescription[] =
    "When enabled, Incognito new tab page will have an updated UI";

const char kIncognitoScreenshotName[] = "Incognito Screenshot";
const char kIncognitoScreenshotDescription[] =
    "Enables Incognito screenshots on Android. It will also make Incognito "
    "thumbnails visible.";

const char kInheritNativeThemeFromParentWidgetName[] =
    "Allow widgets to inherit native theme from its parent widget.";
const char kInheritNativeThemeFromParentWidgetDescription[] =
    "When enabled, secondary UI like menu, dialog etc would be in dark mode "
    "when Incognito mode is open.";

const char kInProductHelpDemoModeChoiceName[] = "In-Product Help Demo Mode";
const char kInProductHelpDemoModeChoiceDescription[] =
    "Selects the In-Product Help demo mode.";

const char kInProductHelpSnoozeName[] = "In-Product Help Snooze";
const char kInProductHelpSnoozeDescription[] =
    "Enables the snooze button on In-Product Help.";

const char kInProductHelpUseClientConfigName[] = "IPH Use Client Config";
const char kInProductHelpUseClientConfigDescription[] =
    "Enable In-Product Help to use client side configuration.";

const char kInstalledAppsInCbdName[] = "Installed Apps in Clear Browsing Data";
const char kInstalledAppsInCbdDescription[] =
    "Adds the installed apps warning dialog to the clear browsing data flow "
    "which allows users to protect installed apps' data from being deleted.";

const char kJavascriptHarmonyName[] = "Experimental JavaScript";
const char kJavascriptHarmonyDescription[] =
    "Enable web pages to use experimental JavaScript features.";

const char kJavascriptHarmonyShippingName[] =
    "Latest stable JavaScript features";
const char kJavascriptHarmonyShippingDescription[] =
    "Some web pages use legacy or non-standard JavaScript extensions that may "
    "conflict with the latest JavaScript features. This flag allows disabling "
    "support of those features for compatibility with such pages.";

const char kJourneysName[] = "History Journeys";
const char kJourneysDescription[] = "Enables the History Journeys UI.";

const char kJourneysOmniboxActionName[] = "History Journeys Omnibox Action";
const char kJourneysOmniboxActionDescription[] =
    "Enables the History Journeys Omnibox Action.";

const char kLargeFaviconFromGoogleName[] = "Large favicons from Google";
const char kLargeFaviconFromGoogleDescription[] =
    "Request large favicons from Google's favicon service";

const char kLensCameraAssistedSearchName[] =
    "Google Lens in Omnibox and New Tab Page";
const char kLensCameraAssistedSearchDescription[] =
    "Enable an entry point to Google Lens to allow users to search what they "
    "see using their mobile camera.";

const char kLogJsConsoleMessagesName[] =
    "Log JS console messages in system logs";
const char kLogJsConsoleMessagesDescription[] =
    "Enable logging JS console messages in system logs, please note that they "
    "may contain PII.";

const char kMediaRouterCastAllowAllIPsName[] =
    "Connect to Cast devices on all IP addresses";
const char kMediaRouterCastAllowAllIPsDescription[] =
    "Have the Media Router connect to Cast devices on all IP addresses, not "
    "just RFC1918/RFC4193 private addresses.";

const char kMediaSessionWebRTCName[] = "Enable WebRTC actions in Media Session";
const char kMediaSessionWebRTCDescription[] =
    "Adds new actions into Media Session for video conferencing.";

const char kMetricsSettingsAndroidName[] = "Metrics Settings on Android";
const char kMetricsSettingsAndroidDescription[] =
    "Enables the new design of metrics settings.";

const char kMobileIdentityConsistencyFREName[] =
    "Mobile identity consistency FRE";
const char kMobileIdentityConsistencyFREDescription[] =
    "Enables stronger identity consistency on mobile with different UIs for "
    "the First Run Experience.";

const char kMojoLinuxChannelSharedMemName[] =
    "Enable Mojo Shared Memory Channel";
const char kMojoLinuxChannelSharedMemDescription[] =
    "If enabled Mojo on Linux based platforms can use shared memory as an "
    "alternate channel for most messages.";

const char kMouseSubframeNoImplicitCaptureName[] =
    "Disable mouse implicit capture for iframe";
const char kMouseSubframeNoImplicitCaptureDescription[] =
    "When enable, mouse down does not implicit capture for iframe.";

const char kCanvas2DLayersName[] =
    "Enables canvas 2D methods BeginLayer and EndLayer";
const char kCanvas2DLayersDescription[] =
    "Enables the canvas 2D methods BeginLayer and EndLayer.";

const char kNewCanvas2DAPIName[] = "Experimental canvas 2D API features";
const char kNewCanvas2DAPIDescription[] =
    "Enables in-progress features for the canvas 2D API. See "
    "https://github.com/fserb/canvas2d.";

const char kSystemProxyForSystemServicesName[] =
    "Enable system-proxy for selected system services";
const char kSystemProxyForSystemServicesDescription[] =
    "Enabling this flag will allow Chrome OS system service which require "
    "network connectivity to use the system-proxy daemon for authentication to "
    "remote HTTP web proxies.";

const char kDestroyProfileOnBrowserCloseName[] =
    "Destroy Profile on browser close";
const char kDestroyProfileOnBrowserCloseDescription[] =
    "Release memory and other resources when a Profile's last browser window "
    "is closed, rather than when Chrome closes completely.";

const char kNewUsbBackendName[] = "Enable new USB backend";
const char kNewUsbBackendDescription[] =
    "Enables the new experimental USB backend for macOS";

const char kNotificationsRevampName[] = "Notifications Revamp";
const char kNotificationsRevampDescription[] =
    "Enable notification UI revamp and grouped web notifications.";

const char kNotificationSchedulerName[] = "Notification scheduler";
const char kNotificationSchedulerDescription[] =
    "Enable notification scheduler feature.";

const char kNotificationSchedulerDebugOptionName[] =
    "Notification scheduler debug options";
const char kNotificationSchedulerDebugOptionDescription[] =
    "Enable debugging mode to override certain behavior of notification "
    "scheduler system for easier manual testing.";
const char kNotificationSchedulerImmediateBackgroundTaskDescription[] =
    "Show scheduled notification right away.";

const char kNotificationsSystemFlagName[] = "Enable system notifications.";
const char kNotificationsSystemFlagDescription[] =
    "Enable support for using the system notification toasts and notification "
    "center on platforms where these are available.";

const char kOmniboxActiveSearchEnginesName[] =
    "Active Search Engines section on settings page";
const char kOmniboxActiveSearchEnginesDescription[] =
    "Enables a 'Your Search Engines' section on "
    "chrome://settings/searchEngines.";

const char kOmniboxAdaptiveSuggestionsCountName[] =
    "Adaptive Omnibox Suggestions count";
const char kOmniboxAdaptiveSuggestionsCountDescription[] =
    "Dynamically adjust number of presented Omnibox suggestions depending on "
    "available space. When enabled, this feature will increase (or decrease) "
    "amount of offered Omnibox suggestions to fill in the space between the "
    "Omnibox and soft keyboard (if any). See also Max Autocomplete Matches "
    "flag to adjust the limit of offered suggestions. The number of shown "
    "suggestions will be no less than the platform default limit.";

const char kOmniboxAssistantVoiceSearchName[] =
    "Omnibox Assistant Voice Search";
const char kOmniboxAssistantVoiceSearchDescription[] =
    "When enabled, use Assistant for omnibox voice query recognition instead of"
    " Android's built-in voice recognition service. Only works on Android.";

const char kOmniboxBookmarkPathsName[] = "Omnibox Bookmark Paths";
const char kOmniboxBookmarkPathsDescription[] =
    "Allows inputs to match with bookmark paths. E.g. 'planets jupiter' can "
    "suggest a bookmark titled 'Jupiter' with URL "
    "'en.wikipedia.org/wiki/Jupiter' located in a path containing 'planet.'";

const char kOmniboxClobberTriggersContextualWebZeroSuggestName[] =
    "Omnibox Clobber Triggers Contextual Web ZeroSuggest";
const char kOmniboxClobberTriggersContextualWebZeroSuggestDescription[] =
    "If enabled, when the user clears the whole omnibox text (i.e. via "
    "Backspace), Chrome will request ZeroSuggest suggestions for the OTHER "
    "page classification (contextual web).";

const char kOmniboxDisableCGIParamMatchingName[] =
    "Disable CGI Param Name Matching";
const char kOmniboxDisableCGIParamMatchingDescription[] =
    "Disables using matches in CGI parameter names while scoring suggestions.";

const char kOmniboxDocumentProviderAsoName[] = "Omnibox Document Provider ASO";
const char kOmniboxDocumentProviderAsoDescription[] =
    "If document suggestions are enabled, swaps the backend from cloudsearch "
    "to ASO (Apps Search Overlay) search.";

const char kOmniboxExperimentalSuggestScoringName[] =
    "Omnibox Experimental Suggest Scoring";
const char kOmniboxExperimentalSuggestScoringDescription[] =
    "Enables an experimental scoring mode for suggestions when Google is the "
    "default search engine.";

const char kOmniboxKeywordSpaceTriggeringSettingName[] =
    "Omnibox Keyword Space Triggering Setting";
const char kOmniboxKeywordSpaceTriggeringSettingDescription[] =
    "Adds a setting to the search engines setting page to control whether "
    "spacebar activates keyword mode.";

const char kOmniboxMostVisitedTilesName[] = "Omnibox Most Visited Tiles";
const char kOmniboxMostVisitedTilesDescription[] =
    "Display a list of frquently visited pages from history as a single row "
    "with a carousel instead of one URL per line.";

const char kOmniboxPreserveLongerShortcutsTextName[] =
    "Omnibox Preserve Longer Shortcuts Text";
const char kOmniboxPreserveLongerShortcutsTextDescription[] =
    "When disabled, updating shortcuts truncates its text to the user input. "
    "When enabled, 3 additional characters are preserved.";

const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPName[] =
    "Omnibox Trending Zero Prefix Suggestions";
const char kOmniboxTrendingZeroPrefixSuggestionsOnNTPDescription[] =
    "Enables trending zero prefix suggestions for signed-in users with no or "
    "insufficient search history.";

const char kOmniboxZeroSuggestPrefetchingName[] =
    "Omnibox Zero Prefix Suggestion Prefetching";
const char kOmniboxZeroSuggestPrefetchingDescription[] =
    "Enables prefetching of the zero prefix suggestions for signed-in users. "
    "The options indicate the duration for which the response will be stored "
    "in the HTTP cache. If no or zero duration is provided, the existing "
    "in-memory cache will used instead of HTTP cache.";

const char kOmniboxRichAutocompletionName[] = "Omnibox Rich Autocompletion";
const char kOmniboxRichAutocompletionDescription[] =
    "Allow autocompletion for titles and non-prefixes. I.e. suggestions whose "
    "titles or URLs contain the user input as a continuous chunk, but not "
    "necessarily a prefix, can be the default suggestion. Typically, only "
    "suggestions whose URLs are prefixed by the user input can be. The "
    "potential variations toggle 4 params: 1) 'Title UI' displays titles, 2) "
    "'2-Line UI' includes titles (and URLs when autocompleting titles) on a "
    "2nd line, 3) 'Title AC' autocompletes titles, and 4) 'Non-Prefix AC' "
    "autocompletes non-prefixes.";
const char kOmniboxRichAutocompletionMinCharName[] =
    "Omnibox Rich Autocompletion Min Characters";
const char kOmniboxRichAutocompletionMinCharDescription[] =
    "Specifies min input character length to trigger rich autocompletion.";
const char kOmniboxRichAutocompletionShowAdditionalTextName[] =
    "Omnibox Rich Autocompletion Show Additional Text";
const char kOmniboxRichAutocompletionShowAdditionalTextDescription[] =
    "Show the suggestion title or URL additional text when the input matches "
    "the URL or title respectively. Defaults to true.";
const char kOmniboxRichAutocompletionSplitName[] =
    "Omnibox Rich Autocompletion Split";
const char kOmniboxRichAutocompletionSplitDescription[] =
    "Allow splitting the user input to intermix with autocompletions; e.g., "
    "the user input 'x z' could be autocompleted as 'x [y ]z'.";
const char kOmniboxRichAutocompletionPreferUrlsOverPrefixesName[] =
    "Omnibox Rich Autocompletion Prefer URLs over prefixes";
const char kOmniboxRichAutocompletionPreferUrlsOverPrefixesDescription[] =
    "When the input matches both a suggestion's title's prefix and its URL's "
    "non-prefix, autocomplete the URL.";
const char kOmniboxRichAutocompletionPromisingName[] =
    "Omnibox Rich Autocompletion Promising Combinations";
const char kOmniboxRichAutocompletionPromisingDescription[] =
    "Allow autocompletion for titles and non-prefixes; see Omnibox Rich "
    "Autocompletion.";

const char kOmniboxOnFocusSuggestionsContextualWebAllowSRPName[] =
    "Allow Omnibox contextual web on-focus suggestions on the SRP";
const char kOmniboxOnFocusSuggestionsContextualWebAllowSRPDescription[] =
    "Enables on-focus suggestions on the Search Results page. "
    "Requires on-focus suggestions for the contextual web to be enabled. "
    "Will only work if user is signed-in and syncing.";

const char kOmniboxOnFocusSuggestionsContextualWebName[] =
    "Omnibox on-focus suggestions for the contextual Web";
const char kOmniboxOnFocusSuggestionsContextualWebDescription[] =
    "Enables on-focus suggestions on the Open Web, that are contextual to the "
    "current URL. Will only work if user is signed-in and syncing, or is "
    "otherwise eligible to send the current page URL to the suggest server.";

const char kOmniboxSpareRendererName[] =
    "Start spare renderer on omnibox focus";
const char kOmniboxSpareRendererDescription[] =
    "When the omnibox is focused, start an empty spare renderer. This can "
    "speed up the load of the navigation from the omnibox.";

const char kOmniboxTabSwitchSuggestionsName[] =
    "Omnibox switch to tab suggestions";
const char kOmniboxTabSwitchSuggestionsDescription[] =
    "Enable URL suggestions to optionally take the user to a tab where a "
    "website is already opened.";

const char kOmniboxMaxZeroSuggestMatchesName[] =
    "Omnibox Max Zero Suggest Matches";
const char kOmniboxMaxZeroSuggestMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed when zero "
    "suggest is active (i.e. displaying suggestions without input).";

const char kOmniboxUIMaxAutocompleteMatchesName[] =
    "Omnibox UI Max Autocomplete Matches";
const char kOmniboxUIMaxAutocompleteMatchesDescription[] =
    "Changes the maximum number of autocomplete matches displayed in the "
    "Omnibox UI.";

const char kOmniboxUpdatedConnectionSecurityIndicatorsName[] =
    "Omnibox Updated connection security indicators";
const char kOmniboxUpdatedConnectionSecurityIndicatorsDescription[] =
    "Use new connection security indicators for https pages in the omnibox.";

const char kOmniboxMaxURLMatchesName[] = "Omnibox Max URL Matches";
const char kOmniboxMaxURLMatchesDescription[] =
    "The maximum number of URL matches to show, unless there are no "
    "replacements.";

const char kOmniboxDynamicMaxAutocompleteName[] =
    "Omnibox Dynamic Max Autocomplete";
const char kOmniboxDynamicMaxAutocompleteDescription[] =
    "Configures the maximum number of autocomplete matches displayed in the "
    "Omnibox UI dynamically based on the number of URL matches.";

const char kOmniboxWebUIOmniboxPopupName[] = "WebUI Omnibox Popup";
const char kOmniboxWebUIOmniboxPopupDescription[] =
    "If enabled, uses WebUI to render the omnibox suggestions popup, similar "
    "to how the NTP \"realbox\" is implemented.";

const char kEnableSearchPrefetchName[] = "Search Prefetch";
const char kEnableSearchPrefetchDescription[] =
    "Allow the default search engine to specify prefetch behavior for "
    "suggestions to search results pages.";

const char kOopRasterizationName[] = "Out-of-process rasterization";
const char kOopRasterizationDescription[] =
    "Perform Ganesh raster in the GPU Process instead of the renderer.  "
    "Must also enable GPU rasterization";

const char kOopRasterizationDDLName[] =
    "Out of process rasterization using DDLs";
const char kOopRasterizationDDLDescription[] =
    "Use Skia Deferred Display Lists when performing rasterization in the GPU "
    "process  "
    "Must also enable OOP rasterization";

const char kOptimizationGuideModelDownloadingName[] =
    "Allow optimization guide model downloads";
const char kOptimizationGuideModelDownloadingDescription[] =
    "Enables the optimization guide to download prediction models.";

const char kOptimizationGuideModelPushNotificationName[] =
    "Enable optimization guide push notifications";
const char kOptimizationGuideModelPushNotificationDescription[] =
    "Enables the optimization guide to receive push notifications.";

const char kEnableDeJellyName[] = "Experimental de-jelly effect";
const char kEnableDeJellyDescription[] =
    "Enables an experimental effect which attempts to mitigate "
    "\"jelly-scrolling\". This is an experimental implementation with known "
    "bugs, visual artifacts, and performance cost. This implementation may be "
    "removed at any time.";

const char kOsSettingsAppNotificationsPageName[] =
    "CrOS Settings App Notifications Page";
const char kOsSettingsAppNotificationsPageDescription[] =
    "If enabled, a new App Notifications subpage will appear in the "
    "CrOS Settings Apps section.";

const char kOverlayScrollbarsName[] = "Overlay Scrollbars";
const char kOverlayScrollbarsDescription[] =
    "Enable the experimental overlay scrollbars implementation. You must also "
    "enable threaded compositing to have the scrollbars animate.";

const char kOverlayStrategiesName[] = "Select HW overlay strategies";
const char kOverlayStrategiesDescription[] =
    "Select strategies used to promote quads to HW overlays.";
const char kOverlayStrategiesDefault[] = "Default";
const char kOverlayStrategiesNone[] = "None";
const char kOverlayStrategiesUnoccludedFullscreen[] =
    "Unoccluded fullscreen buffers (single-fullscreen)";
const char kOverlayStrategiesUnoccluded[] =
    "Unoccluded buffers (single-fullscreen,single-on-top)";
const char kOverlayStrategiesOccludedAndUnoccluded[] =
    "Occluded and unoccluded buffers "
    "(single-fullscreen,single-on-top,underlay)";

const char kOverrideLanguagePrefsForHrefTranslateName[] =
    "Override user-blocklisted languages for hrefTranslate";
const char kOverrideLanguagePrefsForHrefTranslateDescription[] =
    "When using hrefTranslate, ignore the user's blocklist of languages that "
    "shouldn't be translated.";
const char kOverrideSitePrefsForHrefTranslateName[] =
    "Override user-blocklisted sites for hrefTranslate";
const char kOverrideSitePrefsForHrefTranslateDescription[] =
    "When using hrefTranslate, ignore the user's blocklist of websites that "
    "shouldn't be translated.";
const char kOverrideUnsupportedPageLanguageForHrefTranslateName[] =
    "Force translation on pages with unsupported languages for hrefTranslate";
const char kOverrideUnsupportedPageLanguageForHrefTranslateDescription[] =
    "When using hrefTranslate, force translation on pages where the page's "
    "language cannot be determined or is unsupported.";
const char kOverrideSimilarLanguagesForHrefTranslateName[] =
    "Force translation on pages with a similar page language for hrefTranslate";
const char kOverrideSimilarLanguagesForHrefTranslateDescription[] =
    "When using hrefTranslate, force translation on pages where the page's "
    "language is similar to the target language specified via hrefTranslate.";

const char kOverscrollHistoryNavigationName[] = "Overscroll history navigation";
const char kOverscrollHistoryNavigationDescription[] =
    "History navigation in response to horizontal overscroll.";

const char kOverviewButtonName[] = "Overview button at the status area";
const char kOverviewButtonDescription[] =
    "If enabled, always show the overview button at the status area.";

const char kPageContentAnnotationsName[] = "Page content annotations";
const char kPageContentAnnotationsDescription[] =
    "Enables page content to be annotated on-device.";

const char kPageInfoAboutThisSiteName[] =
    "'About this site' section in page info";
const char kPageInfoAboutThisSiteDescription[] =
    "Enable the 'About this site' section in the page info.";

const char kParallelDownloadingName[] = "Parallel downloading";
const char kParallelDownloadingDescription[] =
    "Enable parallel downloading to accelerate download speed.";

const char kPasswordChangeInSettingsName[] =
    "Rework password change flow from settings";
const char kPasswordChangeInSettingsDescription[] =
    "Change password when bulk leak check detected an issue.";

const char kPasswordChangeName[] = "Rework password change flow";
const char kPasswordChangeDescription[] =
    "Change password when password leak is detected.";

const char kPasswordImportName[] = "Password import";
const char kPasswordImportDescription[] =
    "Import functionality in password settings.";

const char kPasswordsAccountStorageRevisedOptInFlowName[] =
    "Revised opt-in flow for account-scoped passwore storage";
const char kPasswordsAccountStorageRevisedOptInFlowDescription[] =
    "Enables the revised opt-in flow for the account-scoped passwords storage "
    "during first-time save.";

const char kPasswordScriptsFetchingName[] = "Fetch password scripts";
const char kPasswordScriptsFetchingDescription[] =
    "Fetches scripts for password change flows.";

const char kPdfUnseasonedName[] = "Pepper-free PDF viewer";
const char kPdfUnseasonedDescription[] = "Enables the Pepper-free PDF viewer.";

const char kPdfXfaFormsName[] = "PDF XFA support";
const char kPdfXfaFormsDescription[] =
    "Enables support for XFA forms in PDFs. "
    "Has no effect if Chrome was not built with XFA support.";

const char kAutoWebContentsDarkModeName[] = "Auto Dark Mode for Web Contents";
const char kAutoWebContentsDarkModeDescription[] =
    "Automatically render all web contents using a dark theme.";

const char kForcedColorsName[] = "Forced Colors";
const char kForcedColorsDescription[] =
    "Enables forced colors mode for web content.";

const char kPercentBasedScrollingName[] = "Percent-based Scrolling";
const char kPercentBasedScrollingDescription[] =
    "If enabled, mousewheel and keyboard scrolls will scroll by a percentage "
    "of the scroller size.";

const char kPermissionChipName[] = "Permissions Chip Experiment";
const char kPermissionChipDescription[] =
    "Enables an experimental permission prompt that uses a chip in the location"
    " bar.";

const char kPermissionChipGestureSensitiveName[] =
    "Gesture-sensitive Permissions Chip";
const char kPermissionChipGestureSensitiveDescription[] =
    "If the Permissions Chip Experiment is enabled, controls whether or not "
    "the chip should be more prominent when the request is associated with a "
    "gesture.";

const char kPermissionChipRequestTypeSensitiveName[] =
    "Request-type-sensitive Permissions Chip";
const char kPermissionChipRequestTypeSensitiveDescription[] =
    "If the Permissions Chip Experiment is enabled, controls whether or not "
    "the chip should be more or less prominent depending on the request type.";

const char kPermissionPredictionsName[] = "Permission Predictions";
const char kPermissionPredictionsDescription[] =
    "Use the Permission Predictions Service to surface permission requests "
    "using a quieter UI when the likelihood of the user granting the "
    "permission is predicted to be low. Requires "
    "chrome://flags/#quiet-notification-prompts and `Safe Browsing` to be "
    "enabled.";

const char kPermissionQuietChipName[] = "Quiet Permission Chip Experiment";
const char kPermissionQuietChipDescription[] =
    "Enables an experimental permission prompt that uses the quiet chip "
    "instead of the right-hand side address bar icon for quiet permission "
    "prompts. Requires chrome://flags/#quiet-notification-prompts to be "
    "enabled.";

const char kPersistentQuotaIsTemporaryQuotaName[] =
    "window.PERSISTENT is temporary quota.";
const char kPersistentQuotaIsTemporaryQuotaDescription[] =
    "Causes the window.PERSISTENT quota type to have the same semantics as "
    "window.TEMPORARY.";

const char kPersonalizationHubName[] = "Personalization Hub UI";
const char kPersonalizationHubDescription[] =
    "Enable the UI to let users customize their wallpapers, screensaver, and "
    "avatars.";

const char kPlaybackSpeedButtonName[] = "Playback Speed Button";
const char kPlaybackSpeedButtonDescription[] =
    "Enable the playback speed button on the media controls.";

const char kPointerLockOptionsName[] = "Enables pointer lock options";
const char kPointerLockOptionsDescription[] =
    "Enables pointer lock unadjustedMovement. When unadjustedMovement is set "
    "to true, pointer movements wil not be affected by the underlying platform "
    "modications such as mouse accelaration.";

const char kBookmarksImprovedSaveFlowName[] = "Improved bookmarks save flow";
const char kBookmarksImprovedSaveFlowDescription[] =
    "Enabled an improved save flow for bookmarks.";

const char kBookmarksRefreshName[] = "Bookmarks refresh";
const char kBookmarksRefreshDescription[] =
    "Enable various changes to bookmarks.";

const char kPrerender2Name[] = "Prerender2";
const char kPrerender2Description[] =
    "Enables the new prerenderer implementation for "
    "<script type=speculationrules> that specifies prerender candidates.";

const char kOmniboxTriggerForPrerender2Name[] =
    "Omnibox trigger for Prerender2";
const char kOmniboxTriggerForPrerender2Description[] =
    "Enables the new omnibox trigger prerenderer implementation.";

const char kPrivacyAdvisorName[] = "Privacy Advisor";
const char kPrivacyAdvisorDescription[] =
    "Provides contextual entry points for adjusting privacy settings";

const char kPrivacyReviewName[] = "Privacy Review";
const char kPrivacyReviewDescription[] =
    "Shows a new subpage in Settings that helps the user to review various "
    "privacy settings.";

const char kSafeBrowsingPerProfileNetworkContextsName[] =
    "Per-profile Safe Browsing network contexts";
const char kSafeBrowsingPerProfileNetworkContextsDescription[] =
    "Keys the Safe Browsing network context by the profile initiating the "
    "request";

const char kProminentDarkModeActiveTabTitleName[] =
    "Prominent Dark Mode Active Tab Titles";
const char kProminentDarkModeActiveTabTitleDescription[] =
    "Makes the active tab title in dark mode bolder so the active tab is "
    "easier "
    "to identify.";

const char kPullToRefreshName[] = "Pull-to-refresh gesture";
const char kPullToRefreshDescription[] =
    "Pull-to-refresh gesture in response to vertical overscroll.";
const char kPullToRefreshEnabledTouchscreen[] = "Enabled for touchscreen only";

const char kPwaUpdateDialogForNameAndIconName[] =
    "Enable PWA install update dialog for name/icon changes";
const char kPwaUpdateDialogForNameAndIconDescription[] =
    "Enable a confirmation dialog that shows up when a PWA changes its "
    "icon/name";

const char kQuicName[] = "Experimental QUIC protocol";
const char kQuicDescription[] = "Enable experimental QUIC protocol support.";

const char kQuickActionSearchWidgetAndroidName[] = "Quick Action Search Widget";
const char kQuickActionSearchWidgetAndroidDescription[] =
    "When enabled, the quick action search widget will be available to add to "
    "the homescreen.";

const char kQuickActionSearchWidgetAndroidDinoVariantName[] =
    "Quick Action Search Widget - Dino Variant";
const char kQuickActionSearchWidgetAndroidDinoVariantDescription[] =
    "When enabled, the Dino widget will be available to add to the homescreen";

const char kSettingsAppNotificationSettingsName[] =
    "Split notification permission settings";
const char kSettingsAppNotificationSettingsDescription[] =
    "Remove per-app notification permissions settings from the quick settings "
    "menu. Notification permission settings will be split between the "
    "lacros-chrome browser's notification permission page "
    "and the ChromeOS settings app.";

const char kReadLaterNewBadgePromoName[] = "Reading list 'New' badge promo";
const char kReadLaterNewBadgePromoDescription[] =
    "Causes a 'New' badge to appear on the entry point for adding to the "
    "reading list in the tab context menu.";

const char kRecordWebAppDebugInfoName[] = "Record web app debug info";
const char kRecordWebAppDebugInfoDescription[] =
    "Enables recording additional web app related debugging data to be "
    "displayed in: chrome://web-app-internals";

const char kReduceUserAgentName[] = "Reduce User-Agent request header";
const char kReduceUserAgentDescription[] =
    "Reduce (formerly, \"freeze\") the amount of information available in "
    "the User-Agent request header. "
    "See https://www.chromium.org/updates/ua-reduction for more info.";

const char kRestrictGamepadAccessName[] = "Restrict gamepad access";
const char kRestrictGamepadAccessDescription[] =
    "Enables Permissions Policy and Secure Context restrictions on the Gamepad "
    "API";

const char kMBIModeName[] = "MBI Scheduling Mode";
const char kMBIModeDescription[] =
    "Enables independent agent cluster scheduling, via the "
    "AgentSchedulingGroup infrastructure.";

const char kIntensiveWakeUpThrottlingName[] =
    "Throttle Javascript timers in background.";
const char kIntensiveWakeUpThrottlingDescription[] =
    "When enabled, wake ups from DOM Timers are limited to 1 per minute in a "
    "page that has been hidden for 5 minutes. For additional details, see "
    "https://www.chromestatus.com/feature/4718288976216064.";

const char kSafetyTipName[] =
    "Show Safety Tip UI when visiting low-reputation websites";
const char kSafetyTipDescription[] =
    "If enabled, a Safety Tip UI may be displayed when visiting or interacting "
    "with a site Chrome believes may be suspicious.";

const char kSamePartyCookiesConsideredFirstPartyName[] =
    "Consider SameParty cookies to be first-party.";
const char kSamePartyCookiesConsideredFirstPartyDescription[] =
    "If enabled, SameParty cookies will not be blocked even if third-party "
    "cookies are blocked.";

const char kPartitionedCookiesName[] = "Partitioned cookies";
const char kPartitionedCookiesDescription[] =
    "Controls if the Partitioned cookie attribute is enabled.";

const char kScrollableTabStripFlagId[] = "scrollable-tabstrip";
const char kScrollableTabStripName[] = "Tab Scrolling";
const char kScrollableTabStripDescription[] =
    "Enables tab strip to scroll left and right when full.";

const char kScrollableTabStripButtonsName[] = "Tab Scrolling Buttons";
const char kScrollableTabStripButtonsDescription[] =
    "When the scrollable-tabstrip flag is enabled, this enables buttons to "
    "permanently appear on the tabstrip.";

const char kScrollUnificationName[] = "Scroll Unification";
const char kScrollUnificationDescription[] =
    "Refactoring project that eliminates scroll handling code from Blink. "
    "Does not affect behavior or performance.";

const char kSearchHistoryLinkName[] = "Search History Link";
const char kSearchHistoryLinkDescription[] =
    "Changes the Clear Browsing Data UI to display a link to clear search "
    "history on My Google Activity.";

const char kSecurePaymentConfirmationDebugName[] =
    "Secure Payment Confirmation Debug Mode";
const char kSecurePaymentConfirmationDebugDescription[] =
    "This flag removes the restriction that PaymentCredential in WebAuthn and "
    "secure payment confirmation in PaymentRequest API must use user verifying "
    "platform authenticators.";

const char kSendTabToSelfWhenSignedInName[] = "Send-tab-to-self when signed in";
const char kSendTabToSelfWhenSignedInDescription[] =
    "Makes the tab sharing feature also available for users who have \"only\" "
    "signed-in to their Google Account (as opposed to having enabled Sync).";

const char kSendTabToSelfManageDevicesLinkName[] =
    "Send-tab-to-self manage devices link";
const char kSendTabToSelfManageDevicesLinkDescription[] =
    "Shows a link to manage the user's devices below the device list when "
    "sharing";

extern const char kSendTabToSelfV2Name[] = "Send tab to self 2.0";
extern const char kSendTabToSelfV2Description[] =
    "Enables new received tab "
    "UI shown next to the profile icon instead of using system notifications.";

const char kShoppingListName[] = "Shopping List";
const char kShoppingListDescription[] = "Enable shopping list in bookmarks.";

const char kSidePanelFlagId[] = "side-panel";
const char kSidePanelName[] = "Side panel";
const char kSidePanelDescription[] =
    "Enables a browser-level side panel for a useful and persistent way to "
    "access your Reading List and Bookmarks.";

const char kSidePanelDragAndDropFlagId[] = "side-panel-drag-and-drop";
const char kSidePanelDragAndDropName[] = "Side panel drag and drop";
const char kSidePanelDragAndDropDescription[] =
    "Enables drag and drop of bookmarks within the side panel.";

const char kSharedClipboardUIName[] =
    "Enable shared clipboard feature signals to be handled";
const char kSharedClipboardUIDescription[] =
    "Enables shared clipboard feature signals to be handled by showing "
    "a list of user's available devices to share the clipboard.";

const char kSharingDesktopScreenshotsName[] = "Desktop Screenshots";
const char kSharingDesktopScreenshotsDescription[] =
    "Enables taking"
    " screenshots from the desktop sharing hub.";

const char kSharingHubDesktopAppMenuName[] = "Desktop Sharing Hub in App Menu";
const char kSharingHubDesktopAppMenuDescription[] =
    "Enables the Chrome Sharing Hub in the 3-dot menu for desktop.";

const char kSharingHubDesktopOmniboxName[] = "Desktop Sharing Hub in Omnibox";
const char kSharingHubDesktopOmniboxDescription[] =
    "Enables the Chrome Sharing Hub in the omnibox for desktop.";

const char kSharingPreferVapidName[] =
    "Prefer sending Sharing message via VAPID";
const char kSharingPreferVapidDescription[] =
    "Prefer sending Sharing message via FCM WebPush authenticated using VAPID.";

const char kSharingSendViaSyncName[] =
    "Enable sending Sharing message via Sync";
const char kSharingSendViaSyncDescription[] =
    "Enables sending Sharing message via commiting to Chrome Sync's "
    "SHARING_MESSAGE data type";

const char kShelfDragToPinName[] = "Pin apps in shelf using drag";

const char kShelfDragToPinDescription[] =
    "Enables pinning unpinned items in shelf by dragging them to the part of "
    "the shelf that contains pinned apps.";

const char kShelfHoverPreviewsName[] =
    "Show previews of running apps when hovering over the shelf.";
const char kShelfHoverPreviewsDescription[] =
    "Shows previews of the open windows for a given running app when hovering "
    "over the shelf.";

const char kShowAutofillSignaturesName[] = "Show autofill signatures.";
const char kShowAutofillSignaturesDescription[] =
    "Annotates web forms with Autofill signatures as HTML attributes. Also "
    "marks password fields suitable for password generation.";

const char kShowAutofillTypePredictionsName[] = "Show Autofill predictions";
const char kShowAutofillTypePredictionsDescription[] =
    "Annotates web forms with Autofill field type predictions as placeholder "
    "text.";

const char kShowPerformanceMetricsHudName[] = "Show performance metrics in HUD";
const char kShowPerformanceMetricsHudDescription[] =
    "Display the performance metrics of current page in a heads up display on "
    "the page.";

const char kShowOverdrawFeedbackName[] = "Show overdraw feedback";
const char kShowOverdrawFeedbackDescription[] =
    "Visualize overdraw by color-coding elements based on if they have other "
    "elements drawn underneath.";

const char kSkiaRendererName[] = "Skia API for compositing";
const char kSkiaRendererDescription[] =
    "If enabled, the display compositor will use Skia as the graphics API "
    "instead of OpenGL ES.";

const char kIsolateOriginsName[] = "Isolate additional origins";
const char kIsolateOriginsDescription[] =
    "Requires dedicated processes for an additional set of origins, "
    "specified as a comma-separated list.";

const char kIsolationByDefaultName[] =
    "Change web-facing behaviors that prevent origin-level isolation";
const char kIsolationByDefaultDescription[] =
    "Change several web APIs that make it difficult to isolate origins into "
    "distinct processes. While these changes will ideally become new default "
    "behaviors for the web, this flag is likely to break your experience on "
    "sites you visit today.";

const char kSiteIsolationOptOutName[] = "Disable site isolation";
const char kSiteIsolationOptOutDescription[] =
    "Disables site isolation "
    "(SitePerProcess, IsolateOrigins, etc). Intended for diagnosing bugs that "
    "may be due to out-of-process iframes. Opt-out has no effect if site "
    "isolation is force-enabled using a command line switch or using an "
    "enterprise policy. "
    "Caution: this disables important mitigations for the Spectre CPU "
    "vulnerability affecting most computers.";
const char kSiteIsolationOptOutChoiceDefault[] = "Default";
const char kSiteIsolationOptOutChoiceOptOut[] = "Disabled (not recommended)";

const char kSmoothScrollingName[] = "Smooth Scrolling";
const char kSmoothScrollingDescription[] =
    "Animate smoothly when scrolling page content.";

const char kWebOTPCrossDeviceName[] = "WebOTP Cross Device";
const char kWebOTPCrossDeviceDescription[] =
    "Enable the WebOTP API to work across devices";

const char kSplitCacheByNetworkIsolationKeyName[] = "HTTP Cache Partitioning";
const char kSplitCacheByNetworkIsolationKeyDescription[] =
    "Partitions the HTTP Cache by (top-level site, current-frame site) to "
    "disallow cross-site tracking.";

const char kStrictExtensionIsolationName[] = "Strict Extension Isolation";
const char kStrictExtensionIsolationDescription[] =
    "Experimental security mode that prevents extensions from sharing a "
    "process with each other.";

const char kStrictOriginIsolationName[] = "Strict-Origin-Isolation";
const char kStrictOriginIsolationDescription[] =
    "Experimental security mode that strengthens the site isolation policy. "
    "Controls whether site isolation should use origins instead of scheme and "
    "eTLD+1.";

const char kStorageAccessAPIName[] = "Storage Access API";
const char kStorageAccessAPIDescription[] =
    "Enables the Storage Access API, allowing websites to request storage "
    "access when it would otherwise be restricted.";

const char kStoragePressureEventName[] = "Enable storage pressure Event";
const char kStoragePressureEventDescription[] =
    "If enabled, Chrome will dispatch a DOM event, informing applications "
    "about storage pressure (low disk space)";

const char kStoreHoursAndroidName[] = "Store Hours";
const char kStoreHoursAndroidDescription[] =
    "When enabled, shows store hours for stores in tab grid view.";

const char kSuggestionsWithSubStringMatchName[] =
    "Substring matching for Autofill suggestions";
const char kSuggestionsWithSubStringMatchDescription[] =
    "Match Autofill suggestions based on substrings (token prefixes) rather "
    "than just prefixes.";

const char kSyncAutofillWalletOfferDataName[] =
    "Enable syncing autofill offer data";
const char kSyncAutofillWalletOfferDataDescription[] =
    "When enabled, allows syncing autofill wallet offer data type.";

const char kSyncSandboxName[] = "Use Chrome Sync sandbox";
const char kSyncSandboxDescription[] =
    "Connects to the testing server for Chrome Sync.";

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

const char kSystemKeyboardLockName[] = "Experimental system keyboard lock";
const char kSystemKeyboardLockDescription[] =
    "Enables websites to use the keyboard.lock() API to intercept system "
    "keyboard shortcuts and have the events routed directly to the website "
    "when in fullscreen mode.";

const char kStylusBatteryStatusName[] =
    "Show stylus battery stylus in the stylus tools menu";
const char kStylusBatteryStatusDescription[] =
    "Enables viewing the current stylus battery level in the stylus tools "
    "menu.";

const char kSubframeShutdownDelayName[] =
    "Add delay to subframe renderer process shutdown";
const char kSubframeShutdownDelayDescription[] =
    "Delays shutdown of subframe renderer processes by a few seconds to allow "
    "them to be potentially reused. This aims to reduce process churn in "
    "navigations where the source and destination share subframes.";

const char kTabEngagementReportingName[] = "Tab Engagement Metrics";
const char kTabEngagementReportingDescription[] =
    "Tracks tab engagement and lifetime metrics.";

const char kTabGridLayoutAndroidName[] = "Tab Grid Layout";
const char kTabGridLayoutAndroidDescription[] =
    "Allows users to see their tabs in a grid layout in the tab switcher on "
    "phones.";

const char kCommerceDeveloperName[] = "Commerce developer mode";
const char kCommerceDeveloperDescription[] =
    "Allows users in the allowlist to enter the developer mode";

const char kCommerceMerchantViewerAndroidName[] = "Merchant Viewer";
const char kCommerceMerchantViewerAndroidDescription[] =
    "Allows users to view merchant trust signals on eligible pages.";

const char kCommercePriceTrackingAndroidName[] = "Price Tracking";
const char kCommercePriceTrackingAndroidDescription[] =
    "Allows users to track product prices through Chrome.";

const char kTabGroupsAndroidName[] = "Tab Groups";
const char kTabGroupsAndroidDescription[] =
    "Allows users to create groups to better organize their tabs on phones.";

const char kTabGroupsContinuationAndroidName[] = "Tab Groups Continuation";
const char kTabGroupsContinuationAndroidDescription[] =
    "Allows users to access continuation features in Tab Group on phones.";

const char kTabGroupsUiImprovementsAndroidName[] = "Tab Groups UI Improvements";
const char kTabGroupsUiImprovementsAndroidDescription[] =
    "Allows users to access new features in Tab Group UI on phones.";

const char kTabToGTSAnimationAndroidName[] = "Enable Tab-to-GTS Animation";
const char kTabToGTSAnimationAndroidDescription[] =
    "Allows users to see an animation when entering or leaving the "
    "Grid Tab Switcher on phones.";

const char kAppsShortcutDefaultOffName[] = "Apps Shortcut Default Off";
const char kAppsShortcutDefaultOffDescription[] =
    "Changes the apps shortcut on the bookmarks bar to default to off.";

const char kTabGroupsAutoCreateName[] = "Tab Groups Auto Create";
const char kTabGroupsAutoCreateDescription[] =
    "Automatically creates groups for users, if tab groups are enabled.";

const char kTabGroupsCollapseFreezingName[] = "Tab Groups Collapse Freezing";
const char kTabGroupsCollapseFreezingDescription[] =
    "Experimental tab freezing upon collapsing a tab group.";

const char kTabGroupsFeedbackName[] = "Tab Groups Feedback";
const char kTabGroupsFeedbackDescription[] =
    "Enables the feedback app to appear in the tab group editor bubble, if tab "
    "groups are enabled.";

const char kTabGroupsNewBadgePromoName[] = "Tab Groups 'New' Badge Promo";
const char kTabGroupsNewBadgePromoDescription[] =
    "Causes a 'New' badge to appear on the entry point for creating a tab "
    "group in the tab context menu.";

const char kTabGroupsSaveName[] = "Tab Groups Save";
const char kTabGroupsSaveDescription[] =
    "Enables users to explicitly save and recall tab groups.";

const char kTabHoverCardImagesName[] = "Tab Hover Card Images";
const char kTabHoverCardImagesDescription[] =
    "Shows a preview image in tab hover cards, if tab hover cards are enabled.";

const char kTabOutlinesInLowContrastThemesName[] =
    "Tab Outlines in Low Contrast Themes";
const char kTabOutlinesInLowContrastThemesDescription[] =
    "Expands the range of situations in which tab outline strokes are "
    "displayed, improving accessiblity in dark and incognito mode.";

const char kTabSearchFuzzySearchName[] = "Fuzzy search for Tab Search";
const char kTabSearchFuzzySearchDescription[] =
    "Enable fuzzy search for Tab Search.";

const char kTFLiteLanguageDetectionName[] = "TFLite-based Language Detection";
const char kTFLiteLanguageDetectionDescription[] =
    "Uses TFLite for language detection in place of CLD3";

const char kTintCompositedContentName[] = "Tint composited content";
const char kTintCompositedContentDescription[] =
    "Tint contents composited using Viz with a shade of red to help debug and "
    "study overlay support.";

const char kTopChromeTouchUiName[] = "Touch UI Layout";
const char kTopChromeTouchUiDescription[] =
    "Enables touch UI layout in the browser's top chrome.";

const char kThreadedScrollingName[] = "Threaded scrolling";
const char kThreadedScrollingDescription[] =
    "Threaded handling of scroll-related input events. Disabling this will "
    "force all such scroll events to be handled on the main thread. Note that "
    "this can dramatically hurt scrolling performance of most websites and is "
    "intended for testing purposes only.";

const char kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesName[] =
    "Throttle non-visible cross-origin iframes";
const char
    kThrottleDisplayNoneAndVisibilityHiddenCrossOriginIframesDescription[] =
        "When enabled, all cross-origin iframes with zero visibility (either "
        "display:none or zero area) will be throttled, regardless of whether "
        "they are same-process or cross-process. When disabled, only cross-"
        "process iframes will be throttled.";

const char kTouchDragDropName[] = "Touch initiated drag and drop";
const char kTouchDragDropDescription[] =
    "Touch drag and drop can be initiated through long press on a draggable "
    "element.";

const char kTouchSelectionStrategyName[] = "Touch text selection strategy";
const char kTouchSelectionStrategyDescription[] =
    "Controls how text selection granularity changes when touch text selection "
    "handles are dragged. Non-default behavior is experimental.";
const char kTouchSelectionStrategyCharacter[] = "Character";
const char kTouchSelectionStrategyDirection[] = "Direction";

const char kTranslateAssistContentName[] = "Translate AssistContent";
const char kTranslateAssistContentDescription[] =
    "Enables populating translate details for the current page in "
    "AssistContent.";

const char kTranslateForceTriggerOnEnglishName[] =
    "Select which language model to use to trigger translate on English "
    "content";
const char kTranslateForceTriggerOnEnglishDescription[] =
    "Force the Translate Triggering on English pages experiment to be enabled "
    "with the selected language model active.";

const char kTranslateIntentName[] = "Translate intent";
const char kTranslateIntentDescription[] =
    "Enables an intent that allows Assistant to initiate a translation of the "
    "foreground tab.";

const char kTreatInsecureOriginAsSecureName[] =
    "Insecure origins treated as secure";
const char kTreatInsecureOriginAsSecureDescription[] =
    "Treat given (insecure) origins as secure origins. Multiple origins can be "
    "supplied as a comma-separated list. Origins must have their protocol "
    "specified e.g. \"http://example.com\". For the definition of secure "
    "contexts, see https://w3c.github.io/webappsec-secure-contexts/";

const char kTrustTokensName[] = "Enable Trust Tokens";
const char kTrustTokensDescription[] =
    "Enables the prototype Trust Token API "
    "(https://github.com/wicg/trust-token-api).";

const char kTurnOffStreamingMediaCachingOnBatteryName[] =
    "Turn off caching of streaming media to disk while on battery power.";
const char kTurnOffStreamingMediaCachingOnBatteryDescription[] =
    "Reduces disk activity during media playback, which can result in "
    "power savings.";

const char kTurnOffStreamingMediaCachingAlwaysName[] =
    "Turn off caching of streaming media to disk.";
const char kTurnOffStreamingMediaCachingAlwaysDescription[] =
    "Reduces disk activity during media playback, which can result in "
    "power savings.";

const char kUnifiedSidePanelFlagId[] = "unified-side-panel";
const char kUnifiedSidePanelName[] = "Unified side panel";
const char kUnifiedSidePanelDescription[] = "Revamp the side panel experience.";

const char kUnifiedPasswordManagerAndroidName[] =
    "Google Mobile Services for passwords";
const char kUnifiedPasswordManagerAndroidDescription[] =
    "Uses Google Mobile Services to store and retrieve passwords."
    "Warning: Highly experimental. May lead to loss of passwords and "
    "impact performance.";

const char kUnsafeWebGPUName[] = "Unsafe WebGPU";
const char kUnsafeWebGPUDescription[] =
    "Enables access to the experimental WebGPU API. Warning: As GPU sandboxing "
    "isn't implemented yet for the WebGPU API, it is possible to read GPU data "
    "for other processes.";

const char kUnsafeFastJSCallsName[] = "Unsafe fast JS calls";
const char kUnsafeFastJSCallsDescription[] =
    "Enables experimental fast API between Blink and V8."
    "Warning: type checking, few POD types and array types "
    "are not supported yet, so crashes are possible.";

const char kUiPartialSwapName[] = "Partial swap";
const char kUiPartialSwapDescription[] = "Sets partial swap behavior.";

const char kUseFirstPartySetName[] = "First-Party Set";
const char kUseFirstPartySetDescription[] =
    "Use the provided list of origins as a First-Party Set, with the first "
    "valid origin as the owner of the set.";

const char kUsernameFirstFlowName[] = "Username first flow voting";
const char kUsernameFirstFlowDescription[] =
    "Support of sending votes on username first flow i.e. login "
    "flows where a user has to type username first on one page and then "
    "password on another page. Votes are send on single username forms and are "
    "based on user interaction with the save prompt.";

const char kUsernameFirstFlowFallbackCrowdsourcingName[] =
    "Username first flow fallback crowdsourcing";
const char kUsernameFirstFlowFallbackCrowdsourcingDescription[] =
    "Support of sending additional votes on username first flow i.e. login "
    "flows where a user has to type username first on one page and then "
    "password on another page. These votes are sent on single password forms "
    "and contain information whether a 1-password form follows a 1-text form "
    "and the value's type(or pattern) in the latter (e.g. email-like, "
    "phone-like, arbitrary string).";

const char kUsernameFirstFlowFillingName[] = "Username first flow filling";
const char kUsernameFirstFlowFillingDescription[] =
    "Support of username saving and filling on username first flow i.e. login "
    "flows where a user has to type username first on one page and then "
    "password on another page";

const char kUseSearchClickForRightClickName[] =
    "Use Search+Click for right click";
const char kUseSearchClickForRightClickDescription[] =
    "When enabled search+click will be remapped to right click, allowing "
    "webpages and apps to consume alt+click. When disabled the legacy "
    "behavior of remapping alt+click to right click will remain unchanged.";

const char kV8VmFutureName[] = "Future V8 VM features";
const char kV8VmFutureDescription[] =
    "This enables upcoming and experimental V8 VM features. "
    "This flag does not enable experimental JavaScript features.";

const char kVerticalSnapName[] = "Vertical Snap features";
const char kVerticalSnapDescription[] =
    "This enables Vertical Snap feature in portrait display."
    "This feature allows users to snap windows to top and bottom in portrait "
    "display orientation and maintains left/right snap for landscape display.";

const char kVp9kSVCHWDecodingName[] =
    "Hardware decode acceleration for k-SVC VP9";
const char kVp9kSVCHWDecodingDescription[] =
    "Enable or disable k-SVC VP9 hardware decode acceleration";

const char kWalletServiceUseSandboxName[] =
    "Use Google Payments sandbox servers";
const char kWalletServiceUseSandboxDescription[] =
    "For developers: use the sandbox service for Google Payments API calls.";

const char kWallpaperFullScreenPreviewName[] =
    "Enable wallpaper full screen preview UI";
const char kWallpaperFullScreenPreviewDescription[] =
    "Allows users to minimize all active windows to preview their current "
    "wallpaper";

const char kWallpaperPerDeskName[] =
    "Enable setting different wallpapers per desk";
const char kWallpaperPerDeskDescription[] =
    "Allow users to set different wallpapers on each of their active desks";

const char kWebBluetoothNewPermissionsBackendName[] =
    "Use the new permissions backend for Web Bluetooth";
const char kWebBluetoothNewPermissionsBackendDescription[] =
    "Enables the new permissions backend for Web Bluetooth. This will enable "
    "persistent storage of device permissions and Web Bluetooth features such "
    "as BluetoothDevice.watchAdvertisements() and Bluetooth.getDevices()";

const char kWebBundlesName[] = "Web Bundles";
const char kWebBundlesDescription[] =
    "Enables experimental supports for Web Bundles (Bundled HTTP Exchanges) "
    "navigation.";

const char kWebMidiName[] = "Web MIDI";
const char kWebMidiDescription[] =
    "Enables the implementation of the Web MIDI API. When disabled the "
    "interface will still be exposed by Blink.";

const char kWebOtpBackendName[] = "Web OTP";
const char kWebOtpBackendDescription[] =
    "Enables Web OTP API that uses the specified backend.";
const char kWebOtpBackendSmsVerification[] = "Code Browser API";
const char kWebOtpBackendUserConsent[] = "User Consent API";
const char kWebOtpBackendAuto[] = "Automatically select the backend";

const char kWebglDeveloperExtensionsName[] = "WebGL Developer Extensions";
const char kWebglDeveloperExtensionsDescription[] =
    "Enabling this option allows web applications to access WebGL extensions "
    "intended only for use during development time.";

const char kWebglDraftExtensionsName[] = "WebGL Draft Extensions";
const char kWebglDraftExtensionsDescription[] =
    "Enabling this option allows web applications to access the WebGL "
    "extensions that are still in draft status.";

const char kWebPaymentsExperimentalFeaturesName[] =
    "Experimental Web Payments API features";
const char kWebPaymentsExperimentalFeaturesDescription[] =
    "Enable experimental Web Payments API features";

const char kPaymentRequestBasicCardName[] =
    "PaymentRequest API 'basic-card' method";
const char kPaymentRequestBasicCardDescription[] =
    "The 'basic-card' payment method of the PaymentRequest API.";

const char kAppStoreBillingDebugName[] =
    "Web Payments App Store Billing Debug Mode";
const char kAppStoreBillingDebugDescription[] =
    "App-store purchases (e.g., Google Play Store) within a TWA can be "
    "requested using the Payment Request API. This flag removes the "
    "restriction that the TWA has to be installed from the app-store.";

const char kWebrtcCaptureMultiChannelApmName[] =
    "WebRTC multi-channel capture audio processing.";
const char kWebrtcCaptureMultiChannelApmDescription[] =
    "Support in WebRTC for processing capture audio in multi channel without "
    "downmixing when running APM in the render process.";

const char kWebrtcHideLocalIpsWithMdnsName[] =
    "Anonymize local IPs exposed by WebRTC.";
const char kWebrtcHideLocalIpsWithMdnsDecription[] =
    "Conceal local IP addresses with mDNS hostnames.";

const char kWebrtcHybridAgcName[] = "WebRTC hybrid Agc2/Agc1.";
const char kWebrtcHybridAgcDescription[] =
    "WebRTC Agc2 digital adaptation with Agc1 analog adaptation.";

const char kWebrtcAnalogAgcClippingControlName[] =
    "WebRTC Agc1 analog clipping control.";
const char kWebrtcAnalogAgcClippingControlDescription[] =
    "WebRTC Agc1 analog clipping controller to reduce saturation.";

const char kWebrtcHwDecodingName[] = "WebRTC hardware video decoding";
const char kWebrtcHwDecodingDescription[] =
    "Support in WebRTC for decoding video streams using platform hardware.";

const char kWebrtcHwEncodingName[] = "WebRTC hardware video encoding";
const char kWebrtcHwEncodingDescription[] =
    "Support in WebRTC for encoding video streams using platform hardware.";

const char kWebRtcRemoteEventLogName[] = "WebRTC remote-bound event logging";
const char kWebRtcRemoteEventLogDescription[] =
    "Allow collecting WebRTC event logs and uploading them to Crash. "
    "Please note that, even if enabled, this will still require "
    "a policy to be set, for it to have an effect.";

const char kWebrtcSrtpAesGcmName[] =
    "Negotiation with GCM cipher suites for SRTP in WebRTC";
const char kWebrtcSrtpAesGcmDescription[] =
    "When enabled, WebRTC will try to negotiate GCM cipher suites for SRTP.";

const char kWebrtcUseMinMaxVEADimensionsName[] =
    "WebRTC Min/Max Video Encode Accelerator dimensions";
const char kWebrtcUseMinMaxVEADimensionsDescription[] =
    "When enabled, WebRTC will only use the Video Encode Accelerator for "
    "video resolutions inside those published as supported.";

const char kWebUsbDeviceDetectionName[] =
    "Automatic detection of WebUSB-compatible devices";
const char kWebUsbDeviceDetectionDescription[] =
    "When enabled, the user will be notified when a device which advertises "
    "support for WebUSB is connected. Disable if problems with USB devices are "
    "observed when the browser is running.";

const char kWebXrForceRuntimeName[] = "Force WebXr Runtime";
const char kWebXrForceRuntimeDescription[] =
    "Force the browser to use a particular runtime, even if it would not "
    "usually be enabled or would otherwise not be selected based on the "
    "attached hardware.";

const char kWebXrRuntimeChoiceNone[] = "No Runtime";
const char kWebXrRuntimeChoiceOpenXR[] = "OpenXR";

const char kWebXrIncubationsName[] = "WebXR Incubations";
const char kWebXrIncubationsDescription[] =
    "Enables experimental features for WebXR.";

const char kZeroCopyName[] = "Zero-copy rasterizer";
const char kZeroCopyDescription[] =
    "Raster threads write directly to GPU memory associated with tiles.";

const char kEnableVulkanName[] = "Vulkan";
const char kEnableVulkanDescription[] = "Use vulkan as the graphics backend.";

const char kSharedHighlightingV2Name[] = "Shared Highlighting 2.0";
const char kSharedHighlightingV2Description[] =
    "Improvements to Shared Highlighting. Including ability to reshare or "
    "remove a highlight.";

const char kSharedHighlightingAmpName[] = "Shared Highlighting for AMP Viewers";
const char kSharedHighlightingAmpDescription[] =
    "Enables Shared Highlighting for AMP Viwers.";

const char kPreemptiveLinkToTextGenerationName[] =
    "Preemptive generation of link to text";
const char kPreemptiveLinkToTextGenerationDescription[] =
    "Enables link to text to be generated in advance.";

const char kDraw1PredictedPoint12Ms[] = "1 point 12ms ahead.";
const char kDraw2PredictedPoints6Ms[] = "2 points, each 6ms ahead.";
const char kDraw1PredictedPoint6Ms[] = "1 point 6ms ahead.";
const char kDraw2PredictedPoints3Ms[] = "2 points, each 3ms ahead.";
const char kDrawPredictedPointsDefault[] = "Disabled";
const char kDrawPredictedPointsDescription[] =
    "Draw predicted points when using the delegated ink trails API. Requires "
    "experimental web platform features to be enabled.";
const char kDrawPredictedPointsName[] = "Draw predicted delegated ink points";

const char kSanitizerApiName[] = "Sanitizer API";
const char kSanitizerApiDescription[] =
    "Enable the Sanitizer API. See: https://github.com/WICG/sanitizer-api";

const char kUsePassthroughCommandDecoderName[] =
    "Use passthrough command decoder";
const char kUsePassthroughCommandDecoderDescription[] =
    "Use chrome passthrough command decoder instead of validating command "
    "decoder.";

const char kExtensionWorkflowJustificationName[] =
    "Extension request justification";
const char kExtensionWorkflowJustificationDescription[] =
    "Enables users to justify their extension requests by causing a text field "
    "to appear on the extension request dialog.";

const char kEnterpriseReportingExtensionManifestVersionName[] =
    "Enterprise reporting of extension manifest versions";
const char kEnterpriseReportingExtensionManifestVersionDescription[] =
    "Causes extension manifest versions to be included in the extension info "
    "section of Chrome Browser Cloud Management reports.";

#if !defined(OS_ANDROID)
const char kShareContextMenuName[] = "Share context menu";
const char kShareContextMenuDescription[] =
    "Whether the sharing options in various context menus are grouped into "
    "a common submenu.";
#endif

const char kForceMajorVersion100InUserAgentName[] =
    "Force major version to 100 in User-Agent";
const char kForceMajorVersion100InUserAgentDescription[] =
    "Force the Chrome major version in the User-Agent string to 100, which "
    "allows testing the 3-digit major version number before the actual M100 "
    "release. This flag is only available from M96-M99.";

const char kForceMinorVersion100InUserAgentName[] =
    "Force the minor version to 100 in the User-Agent string";
const char kForceMinorVersion100InUserAgentDescription[] =
    "Force the Chrome minor version in the User-Agent string to 100, which "
    "allows testing a 3-digit minor version number. Currently, the minor "
    "version is always reported as 0, but due to potential breakage with the "
    "upcoming major version 100, this flag allows us to test whether setting "
    "the major version in the minor version part of the User-Agent string "
    "would be an acceptable alternative. If force-major-version-to-100 is set, "
    "then this flag has no effect. See crbug.com/1278459 for details.";

// Android ---------------------------------------------------------------------

#if defined(OS_ANDROID)

const char kAccessibilityPageZoomName[] = "Accessibility Page Zoom";
const char kAccessibilityPageZoomDescription[] =
    "Whether the UI and underlying code for page zoom should be enabled to"
    " allow a user to increase/decrease the web contents zoom factor.";

const char kAddToHomescreenIPHName[] = "Add to homescreen IPH";
const char kAddToHomescreenIPHDescription[] =
    " Shows in-product-help messages educating users about add to homescreen "
    "option in chrome.";

const char kAImageReaderName[] = "Android ImageReader";
const char kAImageReaderDescription[] =
    " Enables MediaPlayer and MediaCodec to use AImageReader on Android. "
    " This feature is only available for android P+ devices. Disabling it also "
    " disables SurfaceControl.";

const char kAndroidDetailedLanguageSettingsName[] =
    "Detailed Language Settings";
const char kAndroidDetailedLanguageSettingsDescription[] =
    "Enable the new detailed language settings page";

const char kAndroidForceAppLanguagePromptName[] =
    "Force second run app language prompt";
const char kAndroidForceAppLanguagePromptDescription[] =
    "When enabled the app language prompt to change the UI language will"
    "always be shown.";

const char kAndroidLayoutChangeTabReparentingName[] =
    "Android Chrome UI phone/tablet layout change tab reparenting";
const char kAndroidLayoutChangeTabReparentingDescription[] =
    "If enabled, when the screen size switches between phone and tablet size, "
    "the UI layout updates to the proper one and the current tabs are "
    "reparented instead of reloaded.";

const char kAndroidSurfaceControlName[] = "Android SurfaceControl";
const char kAndroidSurfaceControlDescription[] =
    " Enables SurfaceControl to manage the buffer queue for the "
    " DisplayCompositor on Android. This feature is only available on "
    " android Q+ devices";

const char kAssistantIntentPageUrlName[] =
    "Include page URL in Assistant intent";
const char kAssistantIntentPageUrlDescription[] =
    "Include the current page's URL in the Assistant voice transcription "
    "intent.";

const char kAssistantIntentTranslateInfoName[] =
    "Translate info in Assistant intent";
const char kAssistantIntentTranslateInfoDescription[] =
    "Include page translation details in the Assistant voice transcription "
    "intent. This includes the page's URL and its original, current, and "
    "default target language.";

const char kAsyncDnsName[] = "Async DNS resolver";
const char kAsyncDnsDescription[] = "Enables the built-in DNS resolver.";

const char kAutofillAccessoryViewName[] =
    "Autofill suggestions as keyboard accessory view";
const char kAutofillAccessoryViewDescription[] =
    "Shows Autofill suggestions on top of the keyboard rather than in a "
    "dropdown.";

const char kAutofillUseMobileLabelDisambiguationName[] =
    "Autofill Uses Mobile Label Disambiguation";
const char kAutofillUseMobileLabelDisambiguationDescription[] =
    "When enabled, Autofill suggestions' labels are displayed using a "
    "mobile-friendly format.";

const char kAppMenuMobileSiteOptionName[] =
    "Show Mobile Site option in app menu";
const char kAppMenuMobileSiteOptionDescription[] =
    "When enabled, app menu should show 'Mobile site' when showing desktop "
    "site, instead of showing 'Desktop Site' with checkbox";

const char kBookmarkBottomSheetName[] = "Enables bookmark bottom sheet";
const char kBookmarkBottomSheetDescription[] =
    "Enables showing a bookmark bottom sheet when adding a bookmark.";

const char kCCTIncognitoName[] = "Chrome Custom Tabs Incognito mode";
const char kCCTIncognitoDescription[] =
    "Enables incognito mode for Chrome Custom Tabs, on Android.";

const char kCCTIncognitoAvailableToThirdPartyName[] =
    "Allow third party to open Custom Tabs Incognito mode";
const char kCCTIncognitoAvailableToThirdPartyDescription[] =
    "Enabling it would allow third party apps to open incognito mode for "
    "Chrome Custom Tabs, on Android.";

const char kCCTResizable90MaximumHeightName[] =
    "Bottom sheet Custom Tabs maximum height";
const char kCCTResizable90MaximumHeightDescription[] =
    "When enabled, the bottom sheet Custom Tabs will have maximum height 90% "
    "of the screen height, otherwise the maximum height is 100% of the screen "
    "height. In both cases, Custom Tabs will yield to the top status bar when "
    "at full stop";
const char kCCTResizableAllowResizeByUserGestureName[] =
    "Bottom sheet Custom Tabs allow resize by user gesture";
const char kCCTResizableAllowResizeByUserGestureDescription[] =
    "Enable user gesture to resize bottom sheet Custom Tabs";
const char kCCTResizableForFirstPartiesName[] =
    "Bottom sheet Custom Tabs (first party)";
const char kCCTResizableForFirstPartiesDescription[] =
    "Enable bottom sheet Custom Tabs for first party apps.";
const char kCCTResizableForThirdPartiesName[] =
    "Bottom sheet Custom Tabs (third party)";
const char kCCTResizableForThirdPartiesDescription[] =
    "Enable bottom sheet Custom Tabs for third party apps.";

const char kChimeAlwaysShowNotificationDescription[] =
    "A debug flag to always show Chime notification after receiving a payload.";
const char kChimeAlwaysShowNotificationName[] =
    "Always show Chime notification";

const char kChimeAndroidSdkDescription[] =
    "Enable Chime SDK to receive push notification.";
const char kChimeAndroidSdkName[] = "Use Chime SDK";

const char kContinuousSearchName[] = "Continuous Search Navigation";
const char kContinuousSearchDescription[] =
    "Enables caching of search results to permit a more seamless search "
    "experience.";

const char kChromeShareLongScreenshotName[] = "Chrome Share Long Screenshots";
const char kChromeShareLongScreenshotDescription[] =
    "Enables UI to edit and share long screenshots on Android";

const char kChromeSharingHubLaunchAdjacentName[] =
    "Launch new share hub actions in adjacent window";
const char kChromeSharingHubLaunchAdjacentDescription[] =
    "In multi-window mode, launches share hub actions in an adjacent window. "
    "For internal debugging.";

const char kCloseTabSuggestionsName[] = "Suggest to close Tabs";
const char kCloseTabSuggestionsDescription[] =
    "Suggests to the user to close Tabs that haven't been used beyond a "
    "configurable threshold or where duplicates of Tabs exist. "
    "The threshold is configurable.";

const char kCriticalPersistedTabDataName[] = "Enable CriticalPersistedTabData";
const char kCriticalPersistedTabDataDescription[] =
    "A new method of persisting Tab data across restarts has been devised "
    "and implemented. This actives the new approach.";

const char kContextMenuPopupStyleName[] = "Context menu popup style";
const char kContextMenuPopupStyleDescription[] =
    "Enable the popup style context menu, where the context menu will be"
    "anchored around the touch point.";

const char kContextualSearchDebugName[] = "Contextual Search debug";
const char kContextualSearchDebugDescription[] =
    "Enables internal debugging of Contextual Search behavior on the client "
    "and server.";

const char kContextualSearchDelayedIntelligenceName[] =
    "Contextual Search Delayed Intelligence";
const char kContextualSearchDelayedIntelligenceDescription[] =
    "Enables an intelligent search for default-enabled users when they open "
    "the bottom sheet.";

const char kContextualSearchForceCaptionName[] =
    "Contextual Search force a caption";
const char kContextualSearchForceCaptionDescription[] =
    "Forces a caption to always be shown in the Touch to Search Bar.";

const char kContextualSearchLiteralSearchTapName[] =
    "Contextual Search literal search with tap";
const char kContextualSearchLiteralSearchTapDescription[] =
    "Enables Contextual Search to be activated with a single tap and produce "
    "a literal search. This is intended to be used in conjunction with the "
    "long-press resolve feature to allow both gestures to trigger a form of "
    "Touch to Search.";

const char kContextualSearchLongpressResolveName[] =
    "Contextual Search long-press Resolves";
const char kContextualSearchLongpressResolveDescription[] =
    "Enables communicating with Google servers when a long-press gesture is "
    "recognized under some privacy-limited conditions, including having Touch "
    "to Search enabled in preferences. The page context data sent to Google is "
    "potentially privacy sensitive!  This disables the tap gesture from "
    "triggering Touch to Search unless that experiment arm is enabled.";

const char kContextualSearchMlTapSuppressionName[] =
    "Contextual Search ML tap suppression";
const char kContextualSearchMlTapSuppressionDescription[] =
    "Enables tap gestures to be suppressed to improve CTR by applying machine "
    "learning.  The \"Contextual Search Ranker prediction\" flag must also be "
    "enabled!";

const char KContextualSearchNewSettingsName[] =
    "Contextual Search new settings";
const char KContextualSearchNewSettingsDescription[] =
    "Adds a toggle to Settings page to specifically control Contextual Search "
    "opt-in state, and update Opt-in messages.";

const char kContextualSearchRankerQueryName[] =
    "Contextual Search Ranker prediction";
const char kContextualSearchRankerQueryDescription[] =
    "Enables prediction of tap gestures using Assist-Ranker machine learning.";

const char kContextualSearchSecondTapName[] =
    "Contextual Search second tap triggering";
const char kContextualSearchSecondTapDescription[] =
    "Enables triggering on a second tap gesture even when Ranker would "
    "normally suppress that tap.";

const char kContextualSearchThinWebViewImplementationName[] =
    "Use Contextual Search ThinWebView implementation";
const char kContextualSearchThinWebViewImplementationDescription[] =
    "Use ThinWebView and BottomSheet based implementation for Contextual"
    "Search.";

const char kContextualSearchTranslationsName[] =
    "Contextual Search translations";
const char kContextualSearchTranslationsDescription[] =
    "Enables automatic translations of words on a page to be presented in the "
    "caption of the bottom bar.";

const char kContextualTriggersSelectionHandlesName[] =
    "Contextual Triggers selection handles";
const char kContextualTriggersSelectionHandlesDescription[] =
    "Shows the selection handles when selecting text in response to a tap "
    "gesture on plain text.";

const char kContextualTriggersSelectionMenuName[] =
    "Contextual Triggers selection menu";
const char kContextualTriggersSelectionMenuDescription[] =
    "Shows the context menu when selecting text in response to a tap gesture "
    "on plain text.";

const char kContextualTriggersSelectionSizeName[] =
    "Contextual Triggers selection size";
const char kContextualTriggersSelectionSizeDescription[] =
    "Selects a sentence instead of a single word when text is selected in "
    "response to "
    "a tap gesture on plain text.";

const char kCpuAffinityRestrictToLittleCoresName[] = "Restrict to LITTLE cores";
const char kCpuAffinityRestrictToLittleCoresDescription[] =
    "Restricts Chrome threads to LITTLE cores on devices with big.LITTLE or "
    "similar CPU architectures.";

const char kDynamicColorAndroidName[] = "Dynamic colors on Android";
const char kDynamicColorAndroidDescription[] =
    "Enabled dynamic colors on supported devices, such as Pixel devices "
    "running Android 12.";

const char kAutofillManualFallbackAndroidName[] =
    "Enable Autofill manual fallback for Addresses and Payments (Android)";
const char kAutofillManualFallbackAndroidDescription[] =
    "If enabled, adds toggle for addresses and payments bottom sheet to the "
    "keyboard accessory.";

const char kEnableAutofillRefreshStyleName[] =
    "Enable Autofill refresh style (Android)";
const char kEnableAutofillRefreshStyleDescription[] =
    "Enable modernized style for Autofill on Android";

const char kChromeManagementPageAndroidName[] =
    "Enable chrome://management page on Android";
const char kChromeManagementPageAndroidDescription[] =
    "Enable chrome://management page on Android, which aims to inform the user "
    "if their browser is managed by their employer along with other useful "
    "information.";

const char kEnableCommandLineOnNonRootedName[] =
    "Enable command line on non-rooted devices";
const char kEnableCommandLineOnNoRootedDescription[] =
    "Enable reading command line file on non-rooted devices (DANGEROUS).";

const char kEnableDangerousDownloadDialogName[] =
    "Enable dangerous download dialog";
const char kEnableDangerousDownloadDialogDescription[] =
    "Use dialog instead of infobar for user to confirm dangerous download";

const char kEnableDuplicateDownloadDialogName[] =
    "Enable duplicate download dialog";
const char kEnableDuplicateDownloadDialogDescription[] =
    "Use dialog instead of infobar for user to confirm duplicate download";

const char kEnableMixedContentDownloadDialogName[] =
    "Enable mixed content download dialog";
const char kEnableMixedContentDownloadDialogDescription[] =
    "Use dialog instead of infobar for user to confirm mixed content download";

const char kExploreSitesName[] = "Explore websites";
const char kExploreSitesDescription[] =
    "Enables portal from new tab page to explore websites.";

const char kFeedBackToTopName[] = "Back to top of the feeds";
const char kFeedBackToTopDescription[] =
    "Enables showing a callout to help users return to the top of the feeds "
    "quickly.";

const char kFeedInteractiveRefreshName[] = "Refresh feeds";
const char kFeedInteractiveRefreshDescription[] =
    "Enables refreshing feeds triggered by the users.";

const char kFeedLoadingPlaceholderName[] = "Feed loading placeholder";
const char kFeedLoadingPlaceholderDescription[] =
    "Enables a placeholder UI in "
    "the feed instead of the loading spinner at first load.";

const char kFeedStampName[] = "StAMP cards in the feed";
const char kFeedStampDescription[] = "Enables StAMP cards in the feed.";

const char kGridTabSwitcherForTabletsName[] = "Grid tab switcher for tablets";
const char kGridTabSwitcherForTabletsDescription[] =
    "Enable grid tab switcher for tablets, replacing the tab strip.";

const char kHomepagePromoCardName[] =
    "Enable homepage promo card on the New Tab Page";
const char kHomepagePromoCardDescription[] =
    "Enable homepage promo card that will be shown to users with partner "
    "configured homepage.";

const char kInstanceSwitcherName[] = "Enable instance switcher";
const char kInstanceSwitcherDescription[] =
    "Enable instance switcher dialog UI that helps users manage multiple "
    "instances of Chrome.";

const char kInstantStartName[] = "Instant start";
const char kInstantStartDescription[] =
    "Show start surface before native library is loaded.";

const char kIntentBlockExternalFormRedirectsNoGestureName[] =
    "Block intents from form submissions without user gesture";
const char kIntentBlockExternalFormRedirectsNoGestureDescription[] =
    "Require a user gesture that triggered a form submission in order to "
    "allow for redirecting to an external intent.";

const char kInterestFeedV2Name[] = "Interest Feed v2";
const char kInterestFeedV2Description[] =
    "Show content suggestions on the New Tab Page and Start Surface using the "
    "new Feed Component.";

const char kInterestFeedV2HeartsName[] = "Interest Feed v2 Hearts";
const char kInterestFeedV2HeartsDescription[] = "Enable hearts on Feedv2.";

const char kInterestFeedV2AutoplayName[] = "Interest Feed v2 Autoplay";
const char kInterestFeedV2AutoplayDescription[] = "Enable autoplay on Feedv2.";

const char kInterestFeedV1ClickAndViewActionsConditionalUploadName[] =
    "Interest Feed V1 clicks/views conditional upload";
const char kInterestFeedV1ClickAndViewActionsConditionalUploadDescription[] =
    "Only enable the upload of clicks/views in Feed V1 after reaching "
    "conditions.";

const char kInterestFeedV2ClickAndViewActionsConditionalUploadName[] =
    "Interest Feed V2 clicks/views conditional upload";
const char kInterestFeedV2ClickAndViewActionsConditionalUploadDescription[] =
    "Only enable the upload of clicks/views in Feed V2 after reaching "
    "conditions.";

const char kLightweightReactionsAndroidName[] =
    "Lightweight Reactions (Android)";
const char kLightweightReactionsAndroidDescription[] =
    "Enables the Lightweight Reactions entry point in the tab share sheet.";

const char kMessagesForAndroidAdsBlockedName[] = "Ads Blocked Messages UI";
const char kMessagesForAndroidAdsBlockedDescription[] =
    "When enabled, ads blocked message will use the new Messages UI.";

const char kMessagesForAndroidChromeSurveyName[] = "Chrome Survey Messages UI";
const char kMessagesForAndroidChromeSurveyDescription[] =
    "When enabled, survey prompt will use the new Messages UI.";

const char kMessagesForAndroidInfrastructureName[] = "Messages infrastructure";
const char kMessagesForAndroidInfrastructureDescription[] =
    "When enabled, will initialize Messages UI infrastructure";

const char kMessagesForAndroidNearOomReductionName[] =
    "Near OOM Reduction Messages UI";
const char kMessagesForAndroidNearOomReductionDescription[] =
    "When enabled, near OOM reduction message will use the new Messages UI.";

const char kMessagesForAndroidNotificationBlockedName[] =
    "Notification Blocked Messages UI";
const char kMessagesForAndroidNotificationBlockedDescription[] =
    "When enabled, notification blocked prompt will use the new Messages UI.";

const char kMessagesForAndroidPasswordsName[] = "Passwords Messages UI";
const char kMessagesForAndroidPasswordsDescription[] =
    "When enabled, password prompt will use the new Messages UI.";

const char kMessagesForAndroidPermissionUpdateName[] =
    "Permission Update Messages UI";
const char kMessagesForAndroidPermissionUpdateDescription[] =
    "When enabled, permission update prompt will use the new Messages UI.";

const char kMessagesForAndroidPopupBlockedName[] = "Popup Blocked Messages UI";
const char kMessagesForAndroidPopupBlockedDescription[] =
    "When enabled, popup blocked prompt will use the new Messages UI.";

const char kMessagesForAndroidPWAInstallName[] = "PWA Installation Messages UI";
const char kMessagesForAndroidPWAInstallDescription[] =
    "When enabled, PWA Installation prompt will use the new Messages UI.";

const char kMessagesForAndroidReaderModeName[] = "Reader Mode Messages UI";
const char kMessagesForAndroidReaderModeDescription[] =
    "When enabled, reader mode prompt will use the new Messages UI.";

const char kMessagesForAndroidSafetyTipName[] = "Safety Tip Messages UI";
const char kMessagesForAndroidSafetyTipDescription[] =
    "When enabled, safety tip prompt will use the new Messages UI.";

extern const char kMessagesForAndroidSaveCardName[] = "Save Card Messages UI";
extern const char kMessagesForAndroidSaveCardDescription[] =
    "When enabled, save card prompt will use the new Messages UI.";

const char kMessagesForAndroidSyncErrorName[] = "Sync Error Messages UI";
const char kMessagesForAndroidSyncErrorDescription[] =
    "When enabled, sync error prompt will use the new Messages UI.";

const char kMessagesForAndroidUpdatePasswordName[] =
    "Update password Messages UI";
const char kMessagesForAndroidUpdatePasswordDescription[] =
    "When enabled, update password prompt will use the new Messages UI.";

const char kNewWindowAppMenuName[] = "Show a menu item 'New Window'";
const char kNewWindowAppMenuDescription[] =
    "Show a new menu item 'New Window' on tablet-sized screen when Chrome "
    "can open a new window and create a new instance in it.";

const char kOfflineIndicatorV2Name[] = "Offline indicator V2";
const char kOfflineIndicatorV2Description[] =
    "Show a persistent offline indicator when offline.";

const char kOfflinePagesLivePageSharingName[] =
    "Enables live page sharing of offline pages";
const char kOfflinePagesLivePageSharingDescription[] =
    "Enables to share current loaded page as offline page by saving as MHTML "
    "first.";

const char kPageInfoHistoryName[] = "Page info history";
const char kPageInfoHistoryDescription[] =
    "Enable a history sub page to the page info menu, and a button to forget "
    "a site, removing all preferences and history.";

const char kPageInfoStoreInfoName[] = "Page info store info";
const char kPageInfoStoreInfoDescription[] =
    "Enable a store info row to the page info menu on eligible pages.";

extern const char kPageInfoV2DesktopName[];
extern const char kPageInfoV2DesktopDescription[];

extern const char kPasswordProtectionForSignedInUsersName[] =
    "Password Protection for Signed-In Users";
extern const char kPasswordProtectionForSignedInUsersDescription[] =
    "Enable signed-in (Google account) password protection for signed-in "
    "users and allows users to change their signed-in password through "
    "password reuse warnings on phishing or low reputation sites.";

const char kPersistShareHubOnAppSwitchName[] = "Persist sharing hub";
const char kPersistShareHubOnAppSwitchDescription[] =
    "Persist the sharing hub across app pauses/resumes.";

const char kPhotoPickerVideoSupportName[] = "Photo Picker Video Support";
const char kPhotoPickerVideoSupportDescription[] =
    "Enables video files to be shown in the Photo Picker dialog";

const char kQueryTilesName[] = "Show query tiles";
const char kQueryTilesDescription[] = "Shows query tiles in Chrome";
const char kQueryTilesNTPName[] = "Show query tiles in NTP";
const char kQueryTilesNTPDescription[] = "Shows query tiles in NTP";
const char kQueryTilesOmniboxName[] = "Show query tiles in omnibox";
const char kQueryTilesOmniboxDescription[] = "Shows query tiles in omnibox";
const char kQueryTilesSingleTierName[] = "Show only one level of query tiles";
const char kQueryTilesSingleTierDescription[] =
    "Show only one level of query tiles";
const char kQueryTilesEnableQueryEditingName[] =
    "Query Tiles - Enable query edit mode";
const char kQueryTilesEnableQueryEditingDescription[] =
    "When a query tile is tapped, the query text will be shown in the omnibox "
    "and user will have a chance to edit the text before submitting";
const char kQueryTilesEnableTrendingName[] =
    "Query Tiles - Enable trending queries";
const char kQueryTilesEnableTrendingDescription[] =
    "Allow tiles of trending queries to show up in front of curated tiles";
const char kQueryTilesCountryCode[] = "Country code for getting tiles";
const char kQueryTilesCountryCodeDescription[] =
    "When query tiles are enabled, this value determines tiles for which "
    "country should be displayed.";
const char kQueryTilesCountryCodeUS[] = "US";
const char kQueryTilesCountryCodeIndia[] = "IN";
const char kQueryTilesCountryCodeBrazil[] = "BR";
const char kQueryTilesCountryCodeNigeria[] = "NG";
const char kQueryTilesCountryCodeIndonesia[] = "ID";
const char kQueryTilesLocalOrderingName[] =
    "Query Tiles - Enable local ordering";
const char kQueryTilesLocalOrderingDescription[] =
    "Enables ordering query tiles locally based on user interactions.";
const char kQueryTilesInstantFetchName[] = "Query tile instant fetch";
const char kQueryTilesInstantFetchDescription[] =
    "Immediately schedule background task to fetch query tiles";
const char kQueryTilesMoreTrendingName[] =
    "Query Tiles - more trending queries";
const char kQueryTilesMoreTrendingDescription[] =
    "Request more trending queries from the server";
const char kQueryTilesRankTilesName[] = "Query Tiles - rank tiles on server";
const char kQueryTilesRankTilesDescription[] =
    "Rank tiles on server based on client context";
const char kQueryTilesSegmentationName[] =
    "Query Tiles - use segmentation rules";
const char kQueryTilesSegmentationDescription[] =
    "enable segmentation rules to decide whether to show query tiles";
const char kQueryTilesSwapTrendingName[] =
    "Query Tiles - Swap trending queries";
const char kQueryTilesSwapTrendingDescription[] =
    "Swap trending queries if user didn't click on them after several "
    "impressions";

const char kReadLaterFlagId[] = "read-later";
const char kReadLaterName[] = "Reading List";
const char kReadLaterDescription[] =
    "Allow users to save tabs for later. Enables a new button and menu for "
    "accessing tabs saved for later.";

const char kReaderModeHeuristicsName[] = "Reader Mode triggering";
const char kReaderModeHeuristicsDescription[] =
    "Determines what pages the Reader Mode infobar is shown on.";
const char kReaderModeHeuristicsMarkup[] = "With article structured markup";
const char kReaderModeHeuristicsAdaboost[] = "Non-mobile-friendly articles";
const char kReaderModeHeuristicsAllArticles[] = "All articles";
const char kReaderModeHeuristicsAlwaysOff[] = "Never";
const char kReaderModeHeuristicsAlwaysOn[] = "Always";

const char kReaderModeInCCTName[] = "Reader Mode in CCT";
const char kReaderModeInCCTDescription[] =
    "Open Reader Mode in Chrome Custom Tabs.";

const char kReadLaterReminderNotificationName[] =
    "Read later reminder notification";
const char kReadLaterReminderNotificationDescription[] =
    "Enables read later weekly reminder notification.";

const char kRecoverFromNeverSaveAndroidName[] =
    "UI to recover from never save passwords on Android";
const char kRecoverFromNeverSaveAndroidDescription[] =
    "Enables showing UI which allows for easy reverting of the decision to "
    "never save passwords on a certain webiste";

const char kReengagementNotificationName[] =
    "Enable re-engagement notifications";
const char kReengagementNotificationDescription[] =
    "Enables Chrome to use the in-product help system to decide when "
    "to show re-engagement notifications.";

const char kRelatedSearchesName[] =
    "Enables an experiment for Related Searches on Android";
const char kRelatedSearchesDescription[] =
    "Enables requesting related searches suggestions. These will be requested "
    "but not shown unless the UI flag is also enabled.";

const char kRelatedSearchesAlternateUxName[] =
    "Enables showing Related Searches in an alternate user experience.";
const char kRelatedSearchesAlternateUxDescription[] =
    "Enables showing related searches with an alternative from the normal "
    "user experience treatment.";

const char kRelatedSearchesInBarName[] =
    "Enables showing Related Searches in the peeking bar.";
const char kRelatedSearchesInBarDescription[] =
    "Enables showing related searches suggestions in a carousel in the "
    "peeking bar of the bottom sheet on Android.";

const char kRelatedSearchesSimplifiedUxName[] =
    "Enables showing Related Searches in a simplified user experience.";
const char kRelatedSearchesSimplifiedUxDescription[] =
    "Enables showing related searches with a simplified form of the normal "
    "user experience treatment.";

const char kRelatedSearchesUiName[] =
    "Forces showing of the Related Searches UI on Android";
const char kRelatedSearchesUiDescription[] =
    "Forces the Related Searches UI and underlying requests to be enabled "
    "regardless of whether they are safe or useful. This requires the Related "
    "Searches feature flag to also be enabled.";

const char kRequestDesktopSiteExceptionsName[] =
    "Per-site setting to request desktop site on Android.";
const char kRequestDesktopSiteExceptionsDescription[] =
    "An option in `Site settings` to request the desktop version of websites "
    "based on site level settings. Flag request-desktop-site-global "
    "needs to be enabled when enabling this flag.";

const char kRequestDesktopSiteGlobalName[] =
    "Global setting to request desktop site on Android.";
const char kRequestDesktopSiteGlobalDescription[] =
    "An option in `Site settings` to persistently request the "
    "desktop version of websites.";

const char kShareUsageRankingName[] =
    "Incorporate usage history into share target ranking.";
const char kShareUsageRankingDescription[] =
    "Incorporate the history of which apps were shared to when producing the "
    "ordered list of 3P share targets in the share hub.";
const char kShareUsageRankingFixedMoreName[] =
    "Fix the position of the 'More' item in the Android share hub.";
const char kShareUsageRankingFixedMoreDescription[] =
    "When enabled with #share-usage-ranking, forces the 'More' option to "
    "occupy the right-most slot on the screen instead of moving depending on "
    "the length of the target list.";
const char kSwapAndroidShareHubRowsName[] = "Swap Android share hub rows.";
const char kSwapAndroidShareHubRowsDescription[] =
    "Swap the order of the first-party and third-party rows in the Android "
    "share hub.";

const char kRequestDesktopSiteForTabletsName[] =
    "Request desktop site for tablets on Android";
const char kRequestDesktopSiteForTabletsDescription[] =
    "Requests a desktop site, if the screen size is large enough on Android."
    " On tablets with small screens a mobile site will be requested by "
    "default.";

const char kSafeBrowsingClientSideDetectionAndroidName[] =
    "Safe Browsing Client Side Detection on Android";
const char kSafeBrowsingClientSideDetectionAndroidDescription[] =
    "Enable DOM feature collection on Safe Browsing pings on Android";

const char kEnhancedProtectionPromoAndroidName[] =
    "Enable enhanced protection promo card on Android on the New Tab Page";
const char kEnhancedProtectionPromoAndroidDescription[] =
    "Enable enhanced protection promo card for users that have not signed up "
    "for enhanced protection.";

const char kScrollCaptureName[] = "Scroll Capture";
const char kScrollCaptureDescription[] =
    "Enables scrolling screenshot capture for web contents.";

const char kSecurePaymentConfirmationAndroidName[] =
    "Secure Payment Confirmation on Android";
const char kSecurePaymentConfirmationAndroidDescription[] =
    "Enables Secure Payment Confirmation on Android.";

const char kSetMarketUrlForTestingName[] = "Set market URL for testing";
const char kSetMarketUrlForTestingDescription[] =
    "When enabled, sets the market URL for use in testing the update menu "
    "item.";

const char kSiteIsolationForPasswordSitesName[] =
    "Site Isolation For Password Sites";
const char kSiteIsolationForPasswordSitesDescription[] =
    "Security mode that enables site isolation for sites based on "
    "password-oriented heuristics, such as a user typing in a password.";

const char kSmartSuggestionForLargeDownloadsName[] =
    "Smart suggestion for large downloads";
const char kSmartSuggestionForLargeDownloadsDescription[] =
    "Smart suggestion that offers download locations for large files.";

const char kStartSurfaceAndroidName[] = "Start Surface";
const char kStartSurfaceAndroidDescription[] =
    "Enable showing the start surface when launching Chrome via the "
    "launcher.";

const char kSharingHubLinkToggleName[] = "Sharing Hub Link Toggle";
const char kSharingHubLinkToggleDescription[] =
    "Enable the link toggle in the Sharing Hub.";

const char kStrictSiteIsolationName[] = "Strict site isolation";
const char kStrictSiteIsolationDescription[] =
    "Security mode that enables site isolation for all sites (SitePerProcess). "
    "In this mode, each renderer process will contain pages from at most one "
    "site, using out-of-process iframes when needed. "
    "Check chrome://process-internals to see the current isolation mode. "
    "Setting this flag to 'Enabled' turns on site isolation regardless of the "
    "default. Here, 'Disabled' is a legacy value that actually means "
    "'Default,' in which case site isolation may be already enabled based on "
    "platform, enterprise policy, or field trial. See also "
    "#site-isolation-trial-opt-out for how to disable site isolation for "
    "testing.";

const char kTabGroupsForTabletsName[] = "Tab groups on tablets";
const char kTabGroupsForTabletsDescription[] = "Enable tab groups on tablets.";

const char kThemeRefactorAndroidName[] = "Theme refactor on Android";
const char kThemeRefactorAndroidDescription[] =
    "Enables the theme refactoring on Android.";

const char kToolbarIphAndroidName[] = "Enable Toolbar IPH on Android";
const char kToolbarIphAndroidDescription[] =
    "Enables in product help bubbles on the toolbar. In particular, the home "
    "button and the tab switcher button.";

const char kToolbarMicIphAndroidName[] =
    "Enable IPH on Android on the mic in the toolbar";
const char kToolbarMicIphAndroidDescription[] =
    "Enables in product help bubble on the mic button in the toolbar.";

const char kUpdateMenuBadgeName[] = "Force show update menu badge";
const char kUpdateMenuBadgeDescription[] =
    "When enabled, a badge will be shown on the app menu button if the update "
    "type is Update Available or Unsupported OS Version.";

const char kUpdateMenuItemCustomSummaryDescription[] =
    "When this flag and the force show update menu item flag are enabled, a "
    "custom summary string will be displayed below the update menu item.";
const char kUpdateMenuItemCustomSummaryName[] =
    "Update menu item custom summary";

const char kUpdateMenuTypeName[] =
    "Forces the update menu type to a specific type";
const char kUpdateMenuTypeDescription[] =
    "When set, forces the update type to be a specific one, which impacts "
    "the app menu badge and menu item for updates.";
const char kUpdateMenuTypeNone[] = "None";
const char kUpdateMenuTypeUpdateAvailable[] = "Update Available";
const char kUpdateMenuTypeUnsupportedOSVersion[] = "Unsupported OS Version";

extern const char kUseRealColorSpaceForAndroidVideoName[] =
    "Use color space from MediaCodec";
extern const char kUseRealColorSpaceForAndroidVideoDescription[] =
    "When enabled video will use real color space instead of srgb.";

const char kUserMediaScreenCapturingName[] = "Screen Capture API";
const char kUserMediaScreenCapturingDescription[] =
    "Allows sites to request a video stream of your screen.";

const char kUseULPLanguagesInChromeName[] = "Use ULP languages in Chrome";
const char kUseULPLanguagesInChromeDescription[] =
    "Enables use of ULP language data in Chrome";

const char kVideoTutorialsName[] = "Enable video tutorials";
const char kVideoTutorialsDescription[] = "Show video tutorials in Chrome";
const char kVideoTutorialsInstantFetchName[] =
    "Video tutorials fetch on startup";
const char kVideoTutorialsInstantFetchDescription[] =
    "Fetch video tutorials on startup";

const char kAdaptiveButtonInTopToolbarName[] = "Adaptive button in top toolbar";
const char kAdaptiveButtonInTopToolbarDescription[] =
    "Enables showing an adaptive action button in the top toolbar";
const char kAdaptiveButtonInTopToolbarCustomizationName[] =
    "Adaptive button in top toolbar customization";
const char kAdaptiveButtonInTopToolbarCustomizationDescription[] =
    "Enables UI for customizing the adaptive action button in the top toolbar";
const char kShareButtonInTopToolbarName[] = "Share button in top toolbar";
const char kShareButtonInTopToolbarDescription[] =
    "Enables UI to initiate sharing from the top toolbar. Enabling Adaptive "
    "Button overrides this.";
const char kVoiceButtonInTopToolbarName[] = "Voice button in top toolbar";
const char kVoiceButtonInTopToolbarDescription[] =
    "Enables showing the voice search button in the top toolbar. Enabling "
    "Adaptive Button overrides this.";

const char kWalletRequiresFirstSyncSetupCompleteName[] =
    "Controls whether Wallet (GPay) integration on Android requires "
    "first-sync-setup to be complete";
const char kWalletRequiresFirstSyncSetupCompleteDescription[] =
    "Controls whether the Wallet (GPay) integration on Android requires "
    "first-sync-setup to be complete. Only has an effect if "
    "enable-autofill-account-wallet-storage is also enabled.";

const char kWebBluetoothRequestLargerMtuName[] =
    "Request larger MTU for Web Bluetooth";
const char kWebBluetoothRequestLargerMtuDescription[] =
    "Controls whether Web Bluetooth should request for a larger ATT MTU so "
    "that more information can be exchanged per transmission.";

const char kWebFeedName[] = "Web Feed";
const char kWebFeedDescription[] =
    "Allows users to keep up with and consume web content.";

const char kWebFeedSortName[] = "Web Feed Sort";
const char kWebFeedSortDescription[] =
    "Allows users to sort their web content in the web feed. "
    "Only works if Web Feed is also enabled.";

const char kWebNotesStylizeName[] = "WebNotes Stylize";
const char kWebNotesStylizeDescription[] =
    "Allows users to create and share stylized webnotes.";

const char kXsurfaceMetricsReportingName[] = "Xsurface Metrics Reporting";
const char kXsurfaceMetricsReportingDescription[] =
    "Allows metrics reporting state to be passed to Xsurface";

const char kWebNotesPublishName[] = "WebNotes Publish";
const char kWebNotesPublishDescription[] =
    "Allows users to save their created notes.";

const char kOmniboxPedalsAndroidBatch1Name[] = "Omnibox Pedals Android batch 1";
const char kOmniboxPedalsAndroidBatch1Description[] =
    "Enable the first batch of Omnibox Pedals on Android.";

// Non-Android -----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

const char kAllowAllSitesToInitiateMirroringName[] =
    "Allow all sites to initiate mirroring";
const char kAllowAllSitesToInitiateMirroringDescription[] =
    "When enabled, allows all websites to request to initiate tab mirroring "
    "via Presentation API. Requires #cast-media-route-provider to also be "
    "enabled";

const char kAppManagementIntentSettingsName[] =
    "App Management intent settings";
const char kAppManagementIntentSettingsDescription[] =
    "Enables and displays the intent settings link handling setting for App "
    "Management.";

const char kEnableAccessibilityLiveCaptionName[] = "Live Caption";
const char kEnableAccessibilityLiveCaptionDescription[] =
    "Enables the live caption feature which generates captions for "
    "media playing in Chrome. Turn the feature on in "
    "chrome://settings/accessibility.";

const char kEnableAutoDisableAccessibilityName[] = "Auto-disable Accessibility";
const char kEnableAutoDisableAccessibilityDescription[] =
    "When accessibility APIs are no longer being requested, automatically "
    "disables accessibility. This might happen if an assistive technology is "
    "turned off or if an extension which uses accessibility APIs no longer "
    "needs them.";

const char kCopyLinkToTextName[] = "Copy Link To Text";
const char kCopyLinkToTextDescription[] =
    "Adds an item to the context menu to allow a user to copy a link to the "
    "page with the selected text highlighted.";

const char kGlobalMediaControlsCastStartStopName[] =
    "Global media controls control Cast start/stop";
const char kGlobalMediaControlsCastStartStopDescription[] =
    "Allows global media controls to control when a Cast session is started "
    "or stopped instead of relying on the Cast dialog.";

const char kMuteNotificationSnoozeActionName[] =
    "Snooze action for mute notifications";
const char kMuteNotificationSnoozeActionDescription[] =
    "Adds a Snooze action to mute notifications shown while sharing a screen.";

const char kNtpCacheOneGoogleBarName[] = "Cache OneGoogleBar";
const char kNtpCacheOneGoogleBarDescription[] =
    "Enables using the OneGoogleBar cached response in chrome://new-tab-page, "
    "when available.";

const char kNtpModulesName[] = "NTP Modules";
const char kNtpModulesDescription[] = "Shows modules on the New Tab Page.";

const char kNtpDriveModuleName[] = "NTP Drive Module";
const char kNtpDriveModuleDescription[] =
    "Shows the Google Drive module on the New Tab Page";

const char kNtpPhotosModuleName[] = "NTP Photos Module";
const char kNtpPhotosModuleDescription[] =
    "Shows the Google Photos module on the New Tab Page";

const char kNtpRecipeTasksModuleName[] = "NTP Recipe Tasks Module";
const char kNtpRecipeTasksModuleDescription[] =
    "Shows the recipe tasks module on the New Tab Page.";

const char kNtpShoppingTasksModuleName[] = "NTP Shopping Tasks Module";
const char kNtpShoppingTasksModuleDescription[] =
    "Shows the shopping tasks module on the New Tab Page.";

const char kNtpChromeCartModuleName[] = "NTP Chrome Cart Module";
const char kNtpChromeCartModuleDescription[] =
    "Shows the chrome cart module on the New Tab Page.";

const char kNtpSafeBrowsingModuleName[] = "NTP Safe Browsing Module";
const char kNtpSafeBrowsingModuleDescription[] =
    "Shows the safe browsing module on the New Tab Page.";

const char kNtpModulesDragAndDropName[] = "NTP Modules Drag and Drop";
const char kNtpModulesDragAndDropDescription[] =
    "Enables modules to be reordered via dragging and dropping on the "
    "New Tab Page.";

const char kNtpModulesRedesignedName[] = "NTP Modules Redesigned";
const char kNtpModulesRedesignedDescription[] =
    "Shows the redesigned modules on the New Tab Page.";

const char kNtpModulesRedesignedLayoutName[] = "Ntp Modules Redesigned Layout";
const char kNtpModulesRedesignedLayoutDescription[] =
    "Changes the layout of modules on New Tab Page";

const char kNtpRealboxPedalsName[] = "NTP Realbox Pedals";
const char kNtpRealboxPedalsDescription[] =
    "Shows pedals in the NTP Realbox when enabled.";

const char kNtpRealboxSuggestionAnswersName[] =
    "NTP Realbox Suggestion Answers";
const char kNtpRealboxSuggestionAnswersDescription[] =
    "Shows suggestion answers in the NTP Realbox when enabled.";

const char kNtpRealboxTailSuggestName[] = "NTP Realbox Tail Suggest";
const char kNtpRealboxTailSuggestDescription[] =
    "Properly formats the tail suggestions to match the Omnibox";

const char kEnableReaderModeName[] = "Enable Reader Mode";
const char kEnableReaderModeDescription[] =
    "Allows viewing of simplified web pages by selecting 'Customize and "
    "control Chrome'>'Distill page'";

const char kHappinessTrackingSurveysForDesktopDemoName[] =
    "Happiness Tracking Surveys Demo";
const char kHappinessTrackingSurveysForDesktopDemoDescription[] =
    "Enable showing Happiness Tracking Surveys Demo to users on Desktop";

const char kKernelnextVMsName[] = "Enable VMs on experimental kernels.";
const char kKernelnextVMsDescription[] =
    "Enables VM support on devices running experimental kernel versions.";

const char kOmniboxDriveSuggestionsName[] =
    "Omnibox Google Drive Document suggestions";
const char kOmniboxDriveSuggestionsDescriptions[] =
    "Display suggestions for Google Drive documents in the omnibox when Google "
    "is the default search engine.";

const char kOmniboxExperimentalKeywordModeName[] =
    "Omnibox Experimental Keyword Mode";
const char kOmniboxExperimentalKeywordModeDescription[] =
    "Enables various experimental features related to keyword mode, its "
    "suggestions and layout.";

const char kOmniboxPedalsBatch2NonEnglishName[] =
    "Omnibox Pedals batch 2 for non-English locales";
const char kOmniboxPedalsBatch2NonEnglishDescription[] =
    "Enable the second batch of Omnibox Pedals (Safety Check, etc.) for "
    "locales other than 'en' and 'en-GB'. This flag has no effect unless "
    "\"Omnibox Pedals batch 2\" is also enabled.";

const char kOmniboxPedalsBatch3Name[] = "Omnibox Pedals batch 3";
const char kOmniboxPedalsBatch3Description[] =
    "Enable the third batch of Omnibox Pedals.";

const char kOmniboxPedalsBatch3NonEnglishName[] =
    "Omnibox Pedals batch 3 for non-English locales";
const char kOmniboxPedalsBatch3NonEnglishDescription[] =
    "Enable the second batch of Omnibox Pedals (Find your phone, etc.) for "
    "locales other than 'en' and 'en-GB'. This flag has no effect unless "
    "\"Omnibox Pedals batch 3\" is also enabled.";

const char kOmniboxPedalsTranslationConsoleName[] =
    "Omnibox Pedals Translation Console";
const char kOmniboxPedalsTranslationConsoleDescription[] =
    "Use translation strings sourced from Translation Console "
    "for triggering some omnibox Pedals (aka Chrome Actions).";

const char kOmniboxKeywordSearchButtonName[] = "Omnibox keyword search button";
const char kOmniboxKeywordSearchButtonDescription[] =
    "Enable the omnibox keyword search button which offers a way to search "
    "on specific sites from the omnibox.";

const char kOmniboxShortBookmarkSuggestionsName[] =
    "Omnibox short bookmark suggestions";
const char kOmniboxShortBookmarkSuggestionsDescription[] =
    "Match very short input words to beginning of words in bookmark "
    "suggestions.";

const char kReadLaterFlagId[] = "read-later";
const char kReadLaterName[] = "Reading List";
const char kReadLaterDescription[] =
    "Click on the Bookmark icon or right click on a tab to add tabs to a "
    "reading list.";

const char kSCTAuditingName[] = "SCT auditing";
const char kSCTAuditingDescription[] =
    "Enables SCT auditing for users who have opted in to Safe Browsing "
    "Extended Reporting.";

const char kSharingDesktopScreenshotsEditName[] =
    "Desktop Screenshots Edit Mode";
const char kSharingDesktopScreenshotsEditDescription[] =
    "Enables an edit flow for users who create screenshots on desktop";

#endif  // !defined(OS_ANDROID)

// Windows ---------------------------------------------------------------------

#if defined(OS_WIN)

const char kCalculateNativeWinOcclusionName[] =
    "Calculate window occlusion on Windows";
const char kCalculateNativeWinOcclusionDescription[] =
    "Calculate window occlusion on Windows will be used in the future "
    "to throttle and potentially unload foreground tabs in occluded windows";

const char kEnableIncognitoShortcutOnDesktopName[] =
    "Enable Incognito Desktop Shortcut";
const char kEnableIncognitoShortcutOnDesktopDescription[] =
    "Enables users to create a desktop shortcut for incognito mode.";

const char kEnableMediaFoundationVideoCaptureName[] =
    "MediaFoundation Video Capture";
const char kEnableMediaFoundationVideoCaptureDescription[] =
    "Enable/Disable the usage of MediaFoundation for video capture. Fall back "
    "to DirectShow if disabled.";

const char kHardwareSecureDecryptionName[] = "Hardware Secure Decryption";
const char kHardwareSecureDecryptionDescription[] =
    "Enable/Disable the use of hardware secure Content Decryption Module (CDM) "
    "for protected content playback.";

const char kHardwareSecureDecryptionExperimentName[] =
    "Hardware Secure Decryption Experiment";
const char kHardwareSecureDecryptionExperimentDescription[] =
    "Enable/Disable the use of hardware secure Content Decryption Module (CDM) "
    "for experimental protected content playback.";

const char kMediaFoundationClearName[] = "MediaFoundation for Clear";
const char kMediaFoundationClearDescription[] =
    "Enable/Disable the use of MediaFoundation for non-protected content "
    "playback on supported systems.";

const char kPervasiveSystemAccentColorName[] = "Pervasive system accent color";
const char kPervasiveSystemAccentColorDescription[] =
    "Use the Windows system accent color as the Chrome accent color, if \"Show "
    "accent color on title bars and windows borders\" is toggled on in the "
    "Windows system settings.";

const char kPwaUninstallInWindowsOsName[] =
    "Enable PWAs to register as an uninstallable app in Windows on "
    "installation.";
const char kPwaUninstallInWindowsOsDescription[] =
    "This allows the PWA to show up in Windows Control Panel (and other OS "
    "surfaces), and be uninstallable from those surfaces. For example, "
    "uninstalling by right-clicking on the app in the Start Menu.";

const char kRawAudioCaptureName[] = "Raw audio capture";
const char kRawAudioCaptureDescription[] =
    "Enable/Disable the usage of WASAPI raw audio capture. When enabled, the "
    "audio stream is a 'raw' stream that bypasses all signal processing except "
    "for endpoint specific, always-on processing in the Audio Processing Object"
    " (APO), driver, and hardware.";

const char kRunVideoCaptureServiceInBrowserProcessName[] =
    "Run video capture service in browser";
const char kRunVideoCaptureServiceInBrowserProcessDescription[] =
    "Run the video capture service in the browser process.";

const char kUseAngleDescriptionWindows[] =
    "Choose the graphics backend for ANGLE. D3D11 is used on most Windows "
    "computers by default. Using the OpenGL driver as the graphics backend may "
    "result in higher performance in some graphics-heavy applications, "
    "particularly on NVIDIA GPUs. It can increase battery and memory usage of "
    "video playback.";

const char kUseAngleD3D11[] = "D3D11";
const char kUseAngleD3D9[] = "D3D9";
const char kUseAngleD3D11on12[] = "D3D11on12";

const char kUseWinrtMidiApiName[] = "Use Windows Runtime MIDI API";
const char kUseWinrtMidiApiDescription[] =
    "Use Windows Runtime MIDI API for WebMIDI (effective only on Windows 10 or "
    "later).";

#if BUILDFLAG(ENABLE_PRINTING)
const char kPrintWithPostScriptType42FontsName[] =
    "Print with PostScript Type 42 fonts";
const char kPrintWithPostScriptType42FontsDescription[] =
    "When using PostScript level 3 printing, render text with Type 42 fonts if "
    "possible.";

const char kPrintWithReducedRasterizationName[] =
    "Print with reduced rasterization";
const char kPrintWithReducedRasterizationDescription[] =
    "When using GDI printing, avoid rasterization if possible.";

const char kUseXpsForPrintingName[] = "Use XPS for printing";
const char kUseXpsForPrintingDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API.";

const char kUseXpsForPrintingFromPdfName[] = "Use XPS for printing from PDF";
const char kUseXpsForPrintingFromPdfDescription[] =
    "When enabled, use XPS printing API instead of the GDI print API when "
    "printing PDF documents.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

const char kWin10TabSearchCaptionButtonName[] =
    "Windows 10 Tab Search Caption Button";
const char kWin10TabSearchCaptionButtonDescription[] =
    "Move the Tab Search entrypoint besides the window caption buttons on "
    "Windows 10 platforms.";

const char kWin11StyleMenusName[] = "Windows 11 Style Menus";
const char kWin11StyleMenusDescription[] =
    "Use Windows 11 style menus where possible.";

#endif  // defined(OS_WIN)

// Mac -------------------------------------------------------------------------

#if defined(OS_MAC)

#if BUILDFLAG(ENABLE_PRINTING)
const char kCupsIppPrintingBackendName[] = "CUPS IPP Printing Backend";
const char kCupsIppPrintingBackendDescription[] =
    "Use the CUPS IPP printing backend instead of the original CUPS backend "
    "that calls the PPD API.";
#endif  // BUILDFLAG(ENABLE_PRINTING)

const char kEnableUniversalLinksName[] = "Universal Links";
const char kEnableUniversalLinksDescription[] =
    "Include Universal Links in the intent picker.";

const char kImmersiveFullscreenName[] = "Immersive Fullscreen Toolbar";
const char kImmersiveFullscreenDescription[] =
    "Automatically hide and show the toolbar in fullscreen.";

const char kMacSyscallSandboxName[] = "Mac Syscall Filtering Sandbox";
const char kMacSyscallSandboxDescription[] =
    "Controls whether the macOS sandbox filters syscalls.";

const char kMetalName[] = "Metal";
const char kMetalDescription[] =
    "Use Metal instead of OpenGL for rasterization (if out-of-process "
    "rasterization is enabled) and display (if the Skia renderer is enabled)";

const char kScreenTimeName[] = "Screen Time";
const char kScreenTimeDescription[] =
    "Integrate with the macOS Screen Time system. Only enabled on macOS 12.1 "
    "and later.";

const char kUseAngleDescriptionMac[] =
    "Choose the graphics backend for ANGLE. The OpenGL backend is soon to be "
    "deprecated on Mac, and may contain driver bugs that are not planned to be "
    "fixed. The Metal backend is still experimental, and may contain bugs that "
    "are still being worked on. The Metal backend should be more performant, "
    "but may still be behind the OpenGL backend until fully released.";

const char kUseAngleMetal[] = "Metal";

#endif

// Windows and Mac -------------------------------------------------------------

#if defined(OS_WIN) || defined(OS_MAC)

const char kUseAngleName[] = "Choose ANGLE graphics backend";
const char kUseAngleDefault[] = "Default";
const char kUseAngleGL[] = "OpenGL";

#endif  // defined(OS_WIN) || defined(OS_MAC)

// Chrome OS -------------------------------------------------------------------

#if BUILDFLAG(IS_CHROMEOS_ASH)

const char kAcceleratedMjpegDecodeName[] =
    "Hardware-accelerated mjpeg decode for captured frame";
const char kAcceleratedMjpegDecodeDescription[] =
    "Enable hardware-accelerated mjpeg decode for captured frame where "
    "available.";

const char kAllowDisableMouseAccelerationName[] =
    "Allow disabling mouse acceleration";
const char kAllowDisableMouseAccelerationDescription[] =
    "Shows a setting to disable mouse acceleration.";

const char kAllowDisableTouchpadHapticFeedbackName[] =
    "Allow disabling touchpad haptic feedback";
const char kAllowDisableTouchpadHapticFeedbackDescription[] =
    "Shows settings to adjust and disable touchpad haptic feedback.";

const char kAllowRepeatedUpdatesName[] =
    "Continue checking for updates before reboot and after initial update.";
const char kAllowRepeatedUpdatesDescription[] =
    "Continues checking to see if there is a more recent update, even if user"
    "has not rebooted to apply the previous update.";

const char kAllowScrollSettingsName[] =
    "Allow changes to scroll acceleration/sensitivity for mice/touchpads.";
const char kAllowScrollSettingsDescription[] =
    "Shows settings to enable/disable scroll acceleration and to adjust the "
    "sensitivity for scrolling.";

const char kAllowTouchpadHapticClickSettingsName[] =
    "Allow changes to the click sensitivity for haptic touchpads.";
const char kAllowTouchpadHapticClickSettingsDescription[] =
    "Shows settings to adjust click sensitivity for haptic touchpads.";

extern const char kAmbientModeNewUrlName[] =
    "Use new backend URL format for ChromeOS Screensaver";
extern const char kAmbientModeNewUrlDescription[] =
    "Use new backend URL format for ChromeOS Screensaver.  This helps with "
    "testing new backend migration.";

extern const char kAppDiscoveryForOobeName[] =
    "OOBE app recommendations with App Discovery Service.";
extern const char kAppDiscoveryForOobeDescription[] =
    "Use the App Discovery Service to request recommended apps for OOBE.";

extern const char kAppDiscoveryRemoteUrlSearchName[] =
    "Remote URL app discovery results";
extern const char kAppDiscoveryRemoteUrlSearchDescription[] =
    "Surface results from a URL in the app discovery service.";

const char kAppServiceExternalProtocolName[] = "App Service External Protocol";
const char kAppServiceExternalProtocolDescription[] =
    "Use the App Service to provide data for external protocol dialog.";

const char kArcAccountRestrictionsName[] = "Enable ARC account restrictions";
const char kArcAccountRestrictionsDescription[] =
    "ARC account restrictions feature for multi-profile account consistency";

const char kArcCustomTabsExperimentName[] =
    "Enable Custom Tabs experiment for ARC";
const char kArcCustomTabsExperimentDescription[] =
    "Allow Android apps to use Custom Tabs."
    "This feature only works on the Canary and Dev channels.";

const char kArcDocumentsProviderUnknownSizeName[] =
    "Enable ARC DocumentsProvider unknown file size handling";
const char kArcDocumentsProviderUnknownSizeDescription[] =
    "Allow opening DocumentsProvider files where size is not reported.";

const char kArcFilePickerExperimentName[] =
    "Enable file picker experiment for ARC";
const char kArcFilePickerExperimentDescription[] =
    "Enables using Chrome OS file picker in ARC.";

const char kArcKeyboardShortcutHelperIntegrationName[] =
    "Enable keyboard shortcut helper integration for ARC";
const char kArcKeyboardShortcutHelperIntegrationDescription[] =
    "Shows keyboard shortcuts from Android apps in Chrome OS Shortcut Viewer";

const char kArcMouseWheelSmoothScrollName[] =
    "Enable ARC mouse wheel smooth scroll compatibility feature.";
const char kArcMouseWheelSmoothScrollDescription[] =
    "Mouse wheel will be converted to simulated smooth scroll in phone-"
    "optimized Android apps.";

const char kArcNativeBridgeToggleName[] =
    "Toggle between native bridge implementations for ARC";
const char kArcNativeBridgeToggleDescription[] =
    "Toggle between native bridge implementations for ARC.";

const char kArcNativeBridge64BitSupportExperimentName[] =
    "Enable experimental 64-bit native bridge support for ARC";
const char kArcNativeBridge64BitSupportExperimentDescription[] =
    "Enable experimental 64-bit native bridge support for ARC where available.";

const char kArcRightClickLongPressName[] =
    "Enable ARC right click long press compatibility feature.";
const char kArcRightClickLongPressDescription[] =
    "Right click will be converted to simulated long press in phone-optimized "
    "Android apps.";

const char kArcRtVcpuDualCoreName[] =
    "Enable ARC real time vcpu on a device with 2 logical cores online.";
const char kArcRtVcpuDualCoreDesc[] =
    "Enable ARC real time vcpu on a device with 2 logical cores online to "
    "reduce media playback glitch.";

const char kArcRtVcpuQuadCoreName[] =
    "Enable ARC real time vcpu on a device with 3+ logical cores online.";
const char kArcRtVcpuQuadCoreDesc[] =
    "Enable ARC real time vcpu on a device with 3+ logical cores online to "
    "reduce media playback glitch.";

const char kArcUsbDeviceDefaultAttachToVmName[] =
    "Attach unclaimed USB devices to ARCVM";
const char kArcUsbDeviceDefaultAttachToVmDescription[] =
    "When ARCVM is enabled, always attach unclaimed USB devices to ARCVM";

const char kArcVmBalloonPolicyName[] =
    "Enable ARCVM limit cache balloon policy";
const char kArcVmBalloonPolicyDesc[] =
    "Trigger reclaim in ARCVM to reduce memory use when ChromeOS is running "
    "low on memory.";

const char kArcEnableUsapName[] =
    "Enable ARC Unspecialized Application Processes";
const char kArcEnableUsapDesc[] =
    "Enable ARC Unspecialized Application Processes when applicable for "
    "high-memory devices.";

const char kAshEnablePipRoundedCornersName[] =
    "Enable Picture-in-Picture rounded corners.";
const char kAshEnablePipRoundedCornersDescription[] =
    "Enable rounded corners on the Picture-in-Picture window.";

const char kAshEnableUnifiedDesktopName[] = "Unified desktop mode";
const char kAshEnableUnifiedDesktopDescription[] =
    "Enable unified desktop mode which allows a window to span multiple "
    "displays.";

const char kAudioUrlName[] = "Enable chrome://audio";
const char kAudioUrlDescription[] =
    "Enable chrome://audio that is designed for debugging ChromeOS audio "
    "issues";

const char kAutoFramingOverrideName[] = "Auto-framing control override";
const char kAutoFramingOverrideDescription[] =
    "Overrides the default to forcibly enable or disable the auto-framing "
    "feature";

const char kBluetoothFixA2dpPacketSizeName[] = "Bluetooth fix A2DP packet size";
const char kBluetoothFixA2dpPacketSizeDescription[] =
    "Fixes Bluetooth A2DP packet size to a smaller default value to improve "
    "audio quality and may fix audio stutter.";

const char kBluetoothRevampName[] = "Bluetooth Revamp";
const char kBluetoothRevampDescription[] =
    "Enables the Chrome OS Bluetooth Revamp, which updates Bluetooth system UI "
    "and related infrastructure.";

const char kBluetoothWbsDogfoodName[] = "Bluetooth WBS dogfood";
const char kBluetoothWbsDogfoodDescription[] =
    "Enables Bluetooth wideband speech mic as default audio option. "
    "Note that flipping this flag makes no difference on most of the "
    "Chrome OS models, because Bluetooth WBS is either unsupported "
    "or fully launched. Only on the few models that Bluetooth WBS is "
    "still stablizing this flag will take effect.";

const char kButtonARCNetworkDiagnosticsName[] = "ARC Network Tests Button";
const char kButtonARCNetworkDiagnosticsDescription[] =
    "Enables the display of a button on the ARC Provisioning failure dialog "
    "that opens the connectivity section of the diagnostics app.";

const char kCalendarViewName[] =
    "Productivity experiment: Monthly Calendar View";
const char kCalendarViewDescription[] =
    "Show Monthly Calendar View with Google Calendar events to increase "
    "productivity by helping users view their schedules more quickly.";

const char kDefaultLinkCapturingInBrowserName[] =
    "Default link capturing in the browser";
const char kDefaultLinkCapturingInBrowserDescription[] =
    "When enabled, newly installed apps will not capture links clicked in the "
    "browser.";

const char kDesksTemplatesName[] = "Desks Templates";
const char kDesksTemplatesDescription[] =
    "Streamline workflows by saving a group of applications and windows as a "
    "launchable template.";

const char kDesksTrackpadSwipeImprovementsName[] =
    "Experiment: Trackpad swiping to switch desks.";
const char kDesksTrackpadSwipeImprovementsDescription[] =
    "Adds some modifications to the four finger trackpad gesture which "
    "switches desks.";

const char kPreferConstantFrameRateName[] = "Prefer Constant Frame Rate";
const char kPreferConstantFrameRateDescription[] =
    "Enables this flag to prefer using constant frame rate for camera when "
    "streaming";

const char kForceControlFaceAeName[] = "Force control face AE";
const char kForceControlFaceAeDescription[] =
    "Control this flag to force enable or disable face AE for camera";

const char kHdrNetOverrideName[] = "HDRnet control override";
const char kHdrNetOverrideDescription[] =
    "Overrides the default to forcibly enable or disable the HDRnet feature";

const char kCameraAppDocumentManualCropName[] =
    "Enables document manual crop in camera app.";
const char kCameraAppDocumentManualCropDescription[] =
    "Enables document manual crop in camera app for allowing to tweak the "
    "crop area and orientation in the document photo preview page.";

const char kCategoricalSearchName[] = "Launcher Categorical Search";
const char kCategoricalSearchDescription[] =
    "Launcher search results grouped by categories";

const char kCellularBypassESimInstallationConnectivityCheckName[] =
    "Bypass eSIM installation connectivity check";
const char kCellularBypassESimInstallationConnectivityCheckDescription[] =
    "Bypass the non-cellular internet connectivity check during eSIM "
    "installation.";

const char kCellularForbidAttachApnName[] = "Forbid Use Attach APN";
const char kCellularForbidAttachApnDescription[] =
    "If enabled, the value of |kCellularUseAttachApn| should have no effect "
    "and the LTE attach APN configuration will not be sent to the modem. This "
    "flag exists because the |kCellularUseAttachApn| flag can be enabled "
    "by command-line arguments via board overlays which takes precedence over "
    "finch configs, which may be needed to turn off the Attach APN feature.";

const char kCellularUseAttachApnName[] = "Cellular use Attach APN";
const char kCellularUseAttachApnDescription[] =
    "Use the mobile operator database to set explicitly an Attach APN "
    "for the LTE connections rather than letting the modem decide which "
    "attach APN to use or retrieve it from the network";

const char kCellularUseExternalEuiccName[] = "Use external Euicc";
const char kCellularUseExternalEuiccDescription[] =
    "When enabled Cellular Setup and Settings UI will use the first available "
    "external Euicc.";

const char kComponentUpdaterTestRequestName[] =
    "Enable the component updater check 'test-request' parameter";
const char kComponentUpdaterTestRequestDescription[] =
    "Enables the 'test-request' parameter for component updater check requests."
    " Overrides any other component updater check request parameters that may "
    "have been specified.";

const char kContextualNudgesName[] =
    "Contextual nudges for user gesture education";
const char kContextualNudgesDescription[] =
    "Enables contextual nudges, periodically showing the user a label "
    "explaining how to interact with a particular UI element using gestures.";

const char kCroshSWAName[] = "Crosh System Web App";
const char kCroshSWADescription[] =
    "When enabled, crosh (Chrome OS Shell) will run as a tabbed System Web App "
    "rather than a normal browser tab.";

const char kCrosLanguageSettingsUpdate2Name[] = "Language Settings Update 2";
const char kCrosLanguageSettingsUpdate2Description[] =
    "Enables the second language settings update. Requires "
    "#enable-cros-language-settings-update to be enabled.";

const char kCrosOnDeviceGrammarCheckName[] = "On-device Grammar Check";
const char kCrosOnDeviceGrammarCheckDescription[] =
    "Enable new on-device grammar check component.";

const char kSystemExtensionsName[] = "Chrome OS System Extensions";
const char kSystemExtensionsDescription[] =
    "Enable the Chrome OS System Extension platform.";

const char kCrostiniBullseyeUpgradeName[] = "Upgrade Crostini to Bullseye";
const char kCrostiniBullseyeUpgradeDescription[] =
    "Offer to upgrade Crostini containers on older versions to bullseye.";

const char kCrostiniDiskResizingName[] = "Allow resizing Crostini disks";
const char kCrostiniDiskResizingDescription[] =
    "Use preallocated user-resizeable disks for Crostini instead of sparse "
    "automatically sized disks.";

const char kCrostiniContainerInstallName[] =
    "Debian version for new Crostini containers";
const char kCrostiniContainerInstallDescription[] =
    "New Crostini containers will use this debian version";

const char kCrostiniGpuSupportName[] = "Crostini GPU Support";
const char kCrostiniGpuSupportDescription[] = "Enable Crostini GPU support.";

const char kCrostiniUseDlcName[] = "Crostini Use DLC";
const char kCrostiniUseDlcDescription[] =
    "Download the termina VM using the new DLC service instead of the old "
    "component updater.";

const char kCrostiniResetLxdDbName[] = "Crostini Reset LXD DB on launch";
const char kCrostiniResetLxdDbDescription[] =
    "Recreates the LXD database every time we launch it";

const char kCrostiniUseLxd4Name[] =
    "Use LXD 4 instead of the default - Irreversible";
const char kCrostiniUseLxd4Description[] =
    "Uses LXD version 4 instead of the default version. WARNING: Once this is "
    "set you can't unset it without deleting your entire container";

const char kCrostiniMultiContainerName[] = "Allow multiple Crostini containers";
const char kCrostiniMultiContainerDescription[] =
    "Experimental UI for creating and managing multiple Crostini containers";

const char kCrostiniImeSupportName[] = "Crostini IME support";
const char kCrostiniImeSupportDescription[] =
    "Experimental support for IMEs (excluding VK) on Crostini.";

const char kCrostiniVirtualKeyboardSupportName[] =
    "Crostini Virtual Keyboard Support";
const char kCrostiniVirtualKeyboardSupportDescription[] =
    "Experimental support for the Virtual Keyboard on Crostini.";

extern const char kCryptAuthV2DedupDeviceLastActivityTimeName[] =
    "Dedup devices by last activity time";
extern const char kCryptAuthV2DedupDeviceLastActivityTimeDescription[] =
    "Deduplicates phones in multi-device setup drop-down list by last "
    "activity time";

const char kDisableBufferBWCompressionName[] =
    "Disable buffer bandwidth compression";
const char kDisableBufferBWCompressionDescription[] =
    "Disable bandwidth compression when allocating buffers";

const char kDisableCameraFrameRotationAtSourceName[] =
    "Disable camera frame rotation at source";
const char kDisableCameraFrameRotationAtSourceDescription[] =
    "Disable camera frame rotation to the upright display orientation in the "
    "video capture device";

const char kDisableCancelAllTouchesName[] = "Disable CancelAllTouches()";
const char kDisableCancelAllTouchesDescription[] =
    "If enabled, a canceled touch will not force all other touches to be "
    "canceled.";

const char kDisableIdleSocketsCloseOnMemoryPressureName[] =
    "Disable closing idle sockets on memory pressure";
const char kDisableIdleSocketsCloseOnMemoryPressureDescription[] =
    "If enabled, idle sockets will not be closed when chrome detects memory "
    "pressure. This applies to web pages only and not to internal requests.";

const char kDisableExplicitDmaFencesName[] = "Disable explicit dma-fences";
const char kDisableExplicitDmaFencesDescription[] =
    "Always rely on implicit syncrhonization between GPU and display "
    "controller instead of using dma-fences explcitily when available.";

const char kDisplayAlignmentAssistanceName[] =
    "Enable Display Alignment Assistance";
const char kDisplayAlignmentAssistanceDescription[] =
    "Show indicators on shared edges of the displays when user is "
    "attempting to move their mouse over to another display. Show preview "
    "indicators when the user is moving a display in display layouts.";

const char kEnableLibinputToHandleTouchpadName[] =
    "Enable libinput to handle touchpad.";
const char kEnableLibinputToHandleTouchpadDescription[] =
    "Use libinput instead of the gestures library to handle touchpad."
    "Libgesures works very well on modern devices but fails on legacy"
    "devices. Use libinput if an input device doesn't work or is not working"
    "well.";

const char kFastPairName[] = "Enable Fast Pair";
const char kFastPairDescription[] =
    "Enables Google Fast Pair service which uses BLE to discover supported "
    "nearby Bluetooth devices and surfaces a notification for quick pairing. "
    "Use along with #bluetooth-advertisement-monitoring to allow background "
    "scanning.";

const char kUseHDRTransferFunctionName[] =
    "Monitor/Display HDR transfer function";
const char kUseHDRTransferFunctionDescription[] =
    "Allows using the HDR transfer functions of any connected monitor that "
    "supports it";

const char kDisableOfficeEditingComponentAppName[] =
    "Disable Office Editing for Docs, Sheets & Slides";
const char kDisableOfficeEditingComponentAppDescription[] =
    "Disables Office Editing for Docs, Sheets & Slides component app so "
    "handlers won't be registered, making it possible to install another "
    "version for testing.";

const char kDoubleTapToZoomInTabletModeName[] =
    "Double-tap to zoom in tablet mode";
const char kDoubleTapToZoomInTabletModeDescription[] =
    "If Enabled, double tapping in webpages while in tablet mode will zoom the "
    "page.";

const char kQuickSettingsPWANotificationsName[] =
    "Enable setting of PWA notification permissions in quick settings ";
const char kQuickSettingsPWANotificationsDescription[] =
    "Replace website notification permissions with PWA notification "
    "permissions in the quick settings menu. Website notification permissions "
    "settings will be migrated to the lacros - chrome browser.";

const char kDriveFsBidirectionalNativeMessagingName[] =
    "Enable bidirectional native messaging for DriveFS";
const char kDriveFsBidirectionalNativeMessagingDescription[] =
    "Enable enhanced native messaging host to communicate with DriveFS.";

const char kCrOSEnforceSystemAecName[] = "Enforce using the system AEC in CrAS";
const char kCrOSEnforceSystemAecDescription[] =
    "Enforces using the system variant in CrAS of the AEC";

const char kCrOSEnforceSystemAecAgcName[] =
    "Enforce using the system AEC and AGC in CrAS";
const char kCrOSEnforceSystemAecAgcDescription[] =
    "Enforces using the system variants in CrAS of the AEC and AGC.";

const char kCrOSEnforceSystemAecNsName[] =
    "Enforce using the system AEC and NS in CrAS";
const char kCrOSEnforceSystemAecNsDescription[] =
    "Enforces using the system variants in CrAS of the AEC and NS.";

const char kCrOSEnforceSystemAecNsAgcName[] =
    "Enforce using the system AEC, NS and AGC in CrAS";
const char kCrOSEnforceSystemAecNsAgcDescription[] =
    "Enforces using the system variants in CrAS of the AEC, NS and AGC.";

const char kEnableAppReinstallZeroStateName[] =
    "Enable Zero State App Reinstall Suggestions.";
const char kEnableAppReinstallZeroStateDescription[] =
    "Enable Zero State App Reinstall Suggestions feature in launcher, which "
    "will show app reinstall recommendations at end of zero state list.";

const char kEnableAssistantAppSupportName[] = "Enable Assistant App Support";
const char kEnableAssistantAppSupportDescription[] =
    "Enable the Assistant App Support feature";

const char kEnableAssistantLauncherIntegrationName[] =
    "Assistant & Launcher integration";
const char kEnableAssistantLauncherIntegrationDescription[] =
    "Combine Launcher search with the power of Assistant to provide the most "
    "useful answer for each query. Requires Assistant to be enabled.";

const char kEnableAssistantRoutinesName[] = "Assistant Routines";
const char kEnableAssistantRoutinesDescription[] = "Enable Assistant Routines.";

const char kEnableBackgroundBlurName[] = "Enable background blur.";
const char kEnableBackgroundBlurDescription[] =
    "Enables background blur for the Launcher, Shelf, Unified System Tray etc.";

const char kEnhancedClipboardName[] =
    "Productivity Experiment: Enable Enhanced Clipboard";
const char kEnhancedClipboardDescription[] =
    "Enables an experimental clipboard history which aims to reduce context "
    "switching. After copying to the clipboard, press search + v to show the "
    "history. Selecting something from the menu will result in a paste to the "
    "active window.";

const char kEnhancedClipboardNudgeSessionResetName[] =
    "Enable resetting enhanced clipboard nudge data";
const char kEnhancedClipboardNudgeSessionResetDescription[] =
    "When enabled, this will reset the clipboard nudge shown data on every new "
    "user session, allowing the nudge to be shown again.";

const char kEnhancedClipboardScreenshotNudgeName[] =
    "Enable clipboard history screenshot nudge";
const char kEnhancedClipboardScreenshotNudgeDescription[] =
    "When enabled, the keyboard shortcut for clipboard history will show in "
    "the screenshot notification banner in clamshell mode.";

const char kEnableCrOSActionRecorderName[] = "Enable CrOS action recorder";
const char kEnableCrOSActionRecorderDescription[] =
    "When enabled, each app launching, file opening, setting change, and url "
    "visiting will be logged locally into an encrypted file. Should not be "
    "enabled. Be aware that hash option only provides a thin layer of privacy.";

const char kEnableDnsProxyName[] = "Enable DNS proxy service";
const char kEnableDnsProxyDescription[] =
    "When enabled, standard DNS queries will be proxied through the system "
    "service";

const char kDnsProxyEnableDOHName[] =
    "Enable DNS-over-HTTPS in the DNS proxy service";
const char kDnsProxyEnableDOHDescription[] =
    "When enabled, the DNS proxy will perform DNS-over-HTTPS in accordance "
    "with the Chrome OS SecureDNS settings.";

const char kEnableHostnameSettingName[] = "Enable setting the device hostname";
const char kEnableHostnameSettingDescription[] =
    "Enables the ability to set the Chrome OS hostname, the name of the device "
    "that is exposed to the local network";

const char kEnableGeneratedWebApksName[] = "Generated WebAPKs";
const char kEnableGeneratedWebApksDescription[] =
    "When enabled, generates and installs a WebAPK inside ARC for each "
    "installed and eligible PWA.";

const char kEnableGesturePropertiesDBusServiceName[] =
    "Enable gesture properties D-Bus service";
const char kEnableGesturePropertiesDBusServiceDescription[] =
    "Enable a D-Bus service for accessing gesture properties, which are used "
    "to configure input devices.";

const char kEnableGoogleAssistantDspName[] =
    "Enable Google Assistant with hardware-based hotword";
const char kEnableGoogleAssistantDspDescription[] =
    "Enable an experimental feature that uses hardware-based hotword detection "
    "for Assistant. Only a limited number of devices have this type of "
    "hardware support.";

const char kEnableGoogleAssistantStereoInputName[] =
    "Enable Google Assistant with stereo audio input";
const char kEnableGoogleAssistantStereoInputDescription[] =
    "Enable an experimental feature that uses stereo audio input for hotword "
    "and voice to text detection in Google Assistant.";

const char kEnableGoogleAssistantAecName[] = "Enable Google Assistant AEC";
const char kEnableGoogleAssistantAecDescription[] =
    "Enable an experimental feature that removes local feedback from audio "
    "input to help hotword and ASR when background audio is playing.";

const char kEnableHeuristicStylusPalmRejectionName[] =
    "Enable Heuristic for Stylus/Palm Rejection.";
const char kEnableHeuristicStylusPalmRejectionDescription[] =
    "Enable additional heuristic palm rejection logic when interacting with "
    "stylus usage. Not intended for all devices.";

const char kEnableInputEventLoggingName[] = "Enable input event logging";
const char kEnableInputEventLoggingDescription[] =
    "Enable detailed logging of input events from touchscreens, touchpads, and "
    "mice. These events include the locations of all touches as well as "
    "relative pointer movements, and so may disclose sensitive data. They "
    "will be included in feedback reports and system logs, so DO NOT ENTER "
    "SENSITIVE INFORMATION with this flag enabled.";

const char kEnableInputInDiagnosticsAppName[] =
    "Enable input device cards in the Diagnostics App";
const char kEnableInputInDiagnosticsAppDescription[] =
    "Enable input device cards in the Diagnostics App";

const char kEnableInputNoiseCancellationUiName[] =
    "Enable Input Noise Cancellation UI.";
const char kEnableInputNoiseCancellationUiDescription[] =
    "Enable toggling input noise cancellation through the Quick Settings. By "
    "default, this flag is disabled.";

const char kEnableLauncherSearchNormalizationName[] =
    "Enable normalization of launcher search results";
const char kEnableLauncherSearchNormalizationDescription[] =
    "Enable normalization of scores from different providers to the "
    "launcher.";

const char kEnableNeuralStylusPalmRejectionName[] =
    "Enable Neural Palm Detection";
const char kEnableNeuralStylusPalmRejectionDescription[] =
    "Experimental: Enable Neural Palm detection. Not compatible with all "
    "devices.";

const char kEnableOsFeedbackName[] = "Enable updated Feedback Tool App";
const char kEnableOsFeedbackDescription[] =
    "Enable the feedback tool with new UX design that helps users mitigate "
    "the issues while writing feedback and makes the UI easier to use.";

const char kEnableNewShortcutMappingName[] = "Enable New Shortcut Mapping";
const char kEnableNewShortcutMappingDescription[] =
    "Enables experimental new shortcut mapping";

const char kEnablePalmOnMaxTouchMajorName[] =
    "Enable Palm when Touch is Maximum";
const char kEnablePalmOnMaxTouchMajorDescription[] =
    "Experimental: Enable Palm detection when the touchscreen reports max "
    "size. Not compatible with all devices.";

const char kEnablePalmOnToolTypePalmName[] =
    "Enable Palm when Tool Type is Palm";
const char kEnablePalmOnToolTypePalmDescription[] =
    "Experimental: Enable palm detection when touchscreen reports "
    "TOOL_TYPE_PALM. Not compatible with all devices.";

const char kEnablePalmSuppressionName[] =
    "Enable Palm Suppression with Stylus.";
const char kEnablePalmSuppressionDescription[] =
    "If enabled, suppresses touch when a stylus is on a touchscreen.";

const char kDisableQuickAnswersV2TranslationName[] =
    "Disable Quick Answers Translation";
const char kDisableQuickAnswersV2TranslationDescription[] =
    "Disable translation services of the Quick Answers.";

const char kTrimOnMemoryPressureName[] = "Trim Working Set on memory pressure";
const char kTrimOnMemoryPressureDescription[] =
    "Trim Working Set periodically on memory pressure";

const char kEchePhoneHubPermissionsOnboardingName[] =
    "Enable Eche Phone Hub Permissions Onboarding";
const char kEchePhoneHubPermissionsOnboardingDescription[] =
    "Enable the new permissions onboarding flow for Phone Hub notifications "
    "and Eche.";

const char kEcheSWAName[] = "Enable Eche App SWA.";
const char kEcheSWADescription[] = "Enable the SWA version of the Eche.";

const char kEcheSWAResizingName[] = "Allow resizing Eche App.";
const char kEcheSWAResizingDescription[] =
    "Enable a naive resize for the Eche window";

const char kEcheSWADebugModeName[] = "Enable Eche Debug Mode";
const char kEcheSWADebugModeDescription[] = "Enable the Debug Mode of the Eche";

const char kEnableIdleInhibitName[] = "Enable Idle Inhibit Protocol";
const char kEnableIdleInhibitDescription[] =
    "Enables the Wayland idle-inhibit-unstable-v1 protocol";

const char kEnableNetworkingInDiagnosticsAppName[] =
    "Enable networking cards in the Diagnostics App";
const char kEnableNetworkingInDiagnosticsAppDescription[] =
    "Enable networking cards in the Diagnostics App";

const char kEnableOAuthIppName[] =
    "Enable OAuth when printing via the IPP protocol";
const char kEnableOAuthIppDescription[] =
    "Enable OAuth when printing via the IPP protocol";

const char kEnableRevenLogSourceName[] =
    "Enable Reven Log Source on chrome://system and feedback logs";
const char kEnableRevenLogSourceDescription[] =
    "Enable Reven Log Source on chrome://system and feedback logs";

const char kEnableSuggestedFilesName[] = "Enable Suggested Files";
const char kEnableSuggestedFilesDescription[] =
    "Enable the Suggested Files feature in Launcher, which will show Drive "
    "file suggestions in the suggestion chips when the launcher is opened.";

const char kEnableSuggestedLocalFilesName[] = "Enable Suggested Local Files";
const char kEnableSuggestedLocalFilesDescription[] =
    "Enable the Suggested local Files feature in Launcher, which will show "
    "local file suggestions in the suggestion chips when the launcher is "
    "opened.";

const char kEnableWireGuardName[] = "Enable WireGuard VPN";
const char kEnableWireGuardDescription[] =
    "Enable the support of WireGuard VPN as a native VPN option. Requires a "
    "kernel version that support it.";

const char kESimPolicyName[] = "Enable ESim Policy";
const char kESimPolicyDescription[] =
    "Enable the support for policy controlled provisioning and configuration "
    "of eSIM cellular networks";

const char kExoGamepadVibrationName[] = "Gamepad Vibration for Exo Clients";
const char kExoGamepadVibrationDescription[] =
    "Allow Exo clients like Android to request vibration events for gamepads "
    "that support it.";

const char kExoOrdinalMotionName[] =
    "Raw (unaccelerated) motion for Linux applications";
const char kExoOrdinalMotionDescription[] =
    "Send unaccelerated values as raw motion events to linux applications.";

const char kExoPointerLockName[] = "Pointer lock for Linux applications";
const char kExoPointerLockDescription[] =
    "Allow Linux applications to request a pointer lock, i.e. exclusive use of "
    "the mouse pointer.";

const char kExoLockNotificationName[] = "Notification bubble for UI lock";
const char kExoLockNotificationDescription[] =
    "Show a notification bubble once an application has switched to "
    "non-immersive fullscreen mode or obtained pointer lock.";

const char kExperimentalAccessibilityDictationExtensionName[] =
    "Experimental accessibility dictation extension.";
const char kExperimentalAccessibilityDictationExtensionDescription[] =
    "Enables the JavaScript dictation extension.";

const char kExperimentalAccessibilityDictationOfflineName[] =
    "Experimental accessibility dictation offline.";
const char kExperimentalAccessibilityDictationOfflineDescription[] =
    "Enables offline speech recognition for the accessibility dictation "
    "feature.";

const char kExperimentalAccessibilityDictationCommandsName[] =
    "Experimental accessibility dictation commands";
const char kExperimentalAccessibilityDictationCommandsDescription[] =
    "Enables text editing commands for the accessibility dictation feature.";

const char kExperimentalAccessibilitySwitchAccessTextName[] =
    "Enable enhanced Switch Access text input.";
const char kExperimentalAccessibilitySwitchAccessTextDescription[] =
    "Enable experimental or in-progress Switch Access features for improved "
    "text input";

const char kExperimentalAccessibilitySwitchAccessMultistepAutomationName[] =
    "Enable multistep automation for Switch Access.";
const char
    kExperimentalAccessibilitySwitchAccessMultistepAutomationDescription[] =
        "Enable multistep automation for Switch Access, which is a project for "
        "the 2021 accessibility sprint.";

const char kMagnifierContinuousMouseFollowingModeSettingName[] =
    "Enable ability to choose continuous mouse following mode in Magnifier "
    "settings";
const char kMagnifierContinuousMouseFollowingModeSettingDescription[] =
    "Enable feature which adds ability to choose new continuous mouse "
    "following mode in Magnifier settings.";

const char kDockedMagnifierResizingName[] =
    "Enable ability to resize Docked Magnifier";
const char kDockedMagnifierResizingDescription[] =
    "Enable feature which adds ability for user to grab and resize divider of "
    "Docked Magnifier.";

const char kFilesArchivemountName[] = "Archivemount in Files App (1st Tier)";
const char kFilesArchivemountDescription[] =
    "Enable mounting various archive formats in File Manager.";

const char kFilesArchivemount2Name[] = "Archivemount in Files App (2nd Tier)";
const char kFilesArchivemount2Description[] =
    "Enable mounting additional archive formats in File Manager. This has no "
    "effect unless #files-archivemount is also enabled.";

const char kFilesBannerFrameworkName[] =
    "Updated Banner framework in Files app";
const char kFilesBannerFrameworkDescription[] =
    "Enable the updated banner framework in Files app";

const char kFilesExtractArchiveName[] = "Extract archive in Files app";
const char kFilesExtractArchiveDescription[] =
    "Enable the simplified archive extraction feature in Files app";

const char kFilesSinglePartitionFormatName[] =
    "Enable Partitioning of Removable Disks.";
const char kFilesSinglePartitionFormatDescription[] =
    "Enable partitioning of removable disks into single partition.";

const char kFilesSWAName[] = "Enable Files App SWA.";
const char kFilesSWADescription[] =
    "Enable the SWA version of the file manager.";

const char kFilesTrashName[] = "Enable Files Trash.";
const char kFilesTrashDescription[] =
    "Enable trash for My files volume in Files App.";

const char kForceSpectreVariant2MitigationName[] =
    "Force Spectre variant 2 mitigagtion";
const char kForceSpectreVariant2MitigationDescription[] =
    "Forces Spectre variant 2 mitigation. Setting this to enabled will "
    "override #spectre-variant2-mitigation and any system-level setting that "
    "disables Spectre variant 2 mitigation.";

const char kFiltersInRecentsName[] = "Enable filters in Recents";
const char kFiltersInRecentsDescription[] =
    "Enable file-type filters (Audio, Images, Videos) in Files App Recents "
    "view.";

const char kFocusFollowsCursorName[] = "Focus follows cursor";
const char kFocusFollowsCursorDescription[] =
    "Enable window focusing by moving the cursor.";

const char kFrameThrottleFpsName[] = "Set frame throttling fps.";
const char kFrameThrottleFpsDescription[] =
    "Set the throttle fps for compositor frame submission.";
const char kFrameThrottleFpsDefault[] = "Default";
const char kFrameThrottleFps5[] = "5 fps";
const char kFrameThrottleFps10[] = "10 fps";
const char kFrameThrottleFps15[] = "15 fps";
const char kFrameThrottleFps20[] = "20 fps";
const char kFrameThrottleFps25[] = "25 fps";
const char kFrameThrottleFps30[] = "30 fps";

const char kFullRestoreName[] = "Full restore";
const char kFullRestoreDescription[] = "Chrome OS full restore";

const char kFuseBoxName[] = "Enable ChromeOS FuseBox service";
const char kFuseBoxDescription[] = "ChromeOS FuseBox service.";

const char kHelpAppBackgroundPageName[] = "Help App Background Page";
const char kHelpAppBackgroundPageDescription[] =
    "Enables the Background page in the help app. The background page is used "
    "to initialize the Help App Launcher search index and show the Discover "
    "tab notification.";

const char kHelpAppDiscoverTabName[] = "Help App Discover Tab";
const char kHelpAppDiscoverTabDescription[] =
    "Enables the Discover tab in the help app. Even if the feature is enabled, "
    "internal app logic might decide not to show the tab.";

const char kHelpAppLauncherSearchName[] = "Help App launcher search";
const char kHelpAppLauncherSearchDescription[] =
    "Enables showing search results from the help app in the launcher.";

const char kHelpAppSearchServiceIntegrationName[] =
    "Help App search service integration";
const char kHelpAppSearchServiceIntegrationDescription[] =
    "Enables the integration between the help app and the local search"
    " service. Includes using the search service for in app search.";

const char kHoldingSpaceInProgressDownloadsIntegrationName[] =
    "Enable showing in-progress downloads in Tote.";
const char kHoldingSpaceInProgressDownloadsIntegrationDescription[] =
    "Show in-progress download functionality in Tote to increase productivity "
    "by giving users one place to go to monitor and access their downloads.";

const char kImeAssistAutocorrectName[] = "Enable assistive autocorrect";
const char kImeAssistAutocorrectDescription[] =
    "Enable assistive auto-correct features for native IME";

const char kImeAssistEmojiEnhancedName[] = "Enable enhanced assistive emojis";
const char kImeAssistEmojiEnhancedDescription[] =
    "Enable enhanced assistive emoji suggestion features for native IME";

const char kImeAssistMultiWordName[] =
    "Enable assistive multi word suggestions";
const char kImeAssistMultiWordDescription[] =
    "Enable assistive multi word suggestions for native IME";

const char kImeAssistMultiWordExpandedName[] =
    "Enable expanded assistive multi word suggestions";
const char kImeAssistMultiWordExpandedDescription[] =
    "Enable expanded assistive multi word suggestions for native IME";

const char kImeAssistMultiWordLacrosSupportName[] =
    "Multi word suggestions lacros support";
const char kImeAssistMultiWordLacrosSupportDescription[] =
    "Enable lacros support for assistive multi word suggestions in native IME";

const char kImeAssistPersonalInfoName[] = "Enable assistive personal info";
const char kImeAssistPersonalInfoDescription[] =
    "Enable auto-complete suggestions on personal infomation for native IME.";

const char kVirtualKeyboardDarkModeName[] =
    "Enable Dark Mode support for virtual keyboard";
const char kVirtualKeyboardDarkModeDescription[] =
    "Enable dark mode colors for the virtual keyboard when dark mode is "
    "active.";

const char kImeMozcProtoName[] = "Enable protobuf on Japanese IME";
const char kImeMozcProtoDescription[] =
    "Enable Japanese IME to use protobuf as interactive message format to "
    "replace JSON";

const char kImeSystemEmojiPickerName[] = "System emoji picker";
const char kImeSystemEmojiPickerDescription[] =
    "Controls whether a System emoji picker, or the virtual keyboard is used "
    "for inserting emoji.";

const char kImeSystemEmojiPickerClipboardName[] =
    "System emoji picker clipboard";
const char kImeSystemEmojiPickerClipboardDescription[] =
    "Emoji picker will insert emoji into clipboard if they can't be inserted "
    "into a text field";

const char kImeSystemEmojiPickerExtensionName[] =
    "System emoji picker extension";
const char kImeSystemEmojiPickerExtensionDescription[] =
    "Emoji picker extension allows users to select emoticons and symbols to "
    "input.";

const char kImeStylusHandwritingName[] = "Stylus Handwriting";
const char kImeStylusHandwritingDescription[] =
    "Enable VK UI for stylus in text fields";

const char kCrosLanguageSettingsImeOptionsInSettingsName[] =
    "Ime settings in settings";
const char kCrosLanguageSettingsImeOptionsInSettingsDescription[] =
    "Adds IME settings to the settings menu";

const char kIntentPickerPWAPersistenceName[] = "Intent picker PWA Persistence";
const char kIntentPickerPWAPersistenceDescription[] =
    "Allow user to always open with PWA in intent picker.";

const char kKeyboardBasedDisplayArrangementInSettingsName[] =
    "Keyboard-based Display Arrangement in Settings";
const char kKeyboardBasedDisplayArrangementInSettingsDescription[] =
    "Enables using arrow keys to rearrange displays on Settings > Device > "
    "Displays page.";

const char kLacrosAvailabilityIgnoreName[] =
    "Ignore lacros-availability policy";
const char kLacrosAvailabilityIgnoreDescription[] =
    "Makes the lacros-availability policy have no effect. Instead Lacros "
    "availability will be controlled by experiment and/or user flags.";

const char kLacrosPrimaryName[] = "Lacros as the primary browser";
const char kLacrosPrimaryDescription[] =
    "Use Lacros-chrome as the primary web browser on Chrome OS. "
    "This flag is ignored if Lacros support is disabled.";

const char kLacrosStabilityName[] = "Lacros stability";
const char kLacrosStabilityDescription[] = "Lacros update channel.";

const char kLacrosSelectionName[] = "Lacros selection";
const char kLacrosSelectionDescription[] =
    "Choosing between rootfs or stateful Lacros.";

const char kLacrosSelectionRootfsDescription[] = "Rootfs";
const char kLacrosSelectionStatefulDescription[] = "Stateful";

const char kLacrosSupportName[] = "Lacros support";
const char kLacrosSupportDescription[] =
    "Support for the experimental lacros-chrome browser. Please note that the "
    "first restart can take some time to setup lacros-chrome. Please DO NOT "
    "attempt to turn off the device during the restart.";

const char kLimitShelfItemsToActiveDeskName[] =
    "Limit Shelf items to active desk";
const char kLimitShelfItemsToActiveDeskDescription[] =
    "Limits items on the shelf to the ones associated with windows on the "
    "active desk";

const char kImprovedScreenCaptureSettingsName[] =
    "Advanced screen capture settings";
const char kImprovedScreenCaptureSettingsDescription[] =
    "Enable the advanced settings view for screen capture mode";

const char kListAllDisplayModesName[] = "List all display modes";
const char kListAllDisplayModesDescription[] =
    "Enables listing all external displays' modes in the display settings.";

const char kLocalWebApprovalsName[] = "Local web approvals";
const char kLocalWebApprovalsDescription[] =
    "Enable local web approvals for Family Link users on Chrome OS. Web filter "
    "interstitial refresh needs to also be enabled.";

const char kEnableHardwareMirrorModeName[] = "Enable Hardware Mirror Mode";
const char kEnableHardwareMirrorModeDescription[] =
    "Enables hardware support when multiple displays are set to mirror mode.";

const char kLockScreenNotificationName[] = "Lock screen notification";
const char kLockScreenNotificationDescription[] =
    "Enable notifications on the lock screen.";

const char kMediaAppHandlesAudioName[] = "Media App Handles Audio";
const char kMediaAppHandlesAudioDescription[] =
    "Enables opening audio files by default in chrome://media-app";

const char kMediaAppHandlesPdfName[] = "Media App Handles PDF";
const char kMediaAppHandlesPdfDescription[] =
    "Enables opening PDF files by default in chrome://media-app";

const char kMeteredShowToggleName[] = "Show Metered Toggle";
const char kMeteredShowToggleDescription[] =
    "Shows a Metered toggle in the Network settings UI for WiFI and Cellular. "
    "The toggle allows users to set whether a network should be considered "
    "metered for purposes of bandwith usage (e.g. for automatic updates).";

const char kMicrophoneMuteNotificationsName[] = "Microphone Mute Notifications";
const char kMicrophoneMuteNotificationsDescription[] =
    "Enables notifications that are shown when an app tries to use microphone "
    "while audio input is muted.";

const char kMicrophoneMuteSwitchDeviceName[] = "Microphone Mute Switch Device";
const char kMicrophoneMuteSwitchDeviceDescription[] =
    "Support for detecting the state of hardware microphone mute toggle. Only "
    "effective on devices that have a microphone mute toggle. Enabling the "
    "flag does not affect the toggle functionality, it only affects how the "
    "System UI handles the mute toggle state.";

const char kMultilingualTypingName[] = "Multilingual typing on CrOS";
const char kMultilingualTypingDescription[] =
    "Enables support for multilingual assistive typing on Chrome OS.";

const char kNearbyKeepAliveFixName[] = "Nearby Keep Alive Fix";
const char kNearbyKeepAliveFixDescription[] =
    "Enables custom KeepAlive interval and timeout for Nearby Connections and "
    "makes Nearby Connections WebRTC KeepAlive less chatty to help with "
    "battery life.";

const char kNearbySharingArcName[] = "ARC Nearby Sharing";
const char kNearbySharingArcDescription[] =
    "Enables Nearby Sharing from ARC apps.";

const char kNearbySharingBackgroundScanningName[] =
    "Nearby Sharing Background Scanning";
const char kNearbySharingBackgroundScanningDescription[] =
    "Enables background scanning for Nearby Share, allowing devices to "
    "persistently scan and present a notification when a nearby device is "
    "attempting to share.";

const char kPcieBillboardNotificationName[] = "Pcie billboard notification";
const char kPcieBillboardNotificationDescription[] =
    "Enable Pcie peripheral billboard notification.";

const char kPerformantSplitViewResizing[] = "Performant Split View Resizing";
const char kPerformantSplitViewResizingDescription[] =
    "If enabled, windows may be moved instead of scaled when resizing split "
    "view in tablet mode.";

const char kPhoneHubCallNotificationName[] =
    "Incoming call notification in Phone Hub";
const char kPhoneHubCallNotificationDescription[] =
    "Enables the incoming/ongoing call feature in Phone Hub.";

const char kPhoneHubCameraRollName[] = "Camera Roll in Phone Hub";
const char kPhoneHubCameraRollDescription[] =
    "Enables the Camera Roll feature in Phone Hub, which allows users to "
    "access recent photos and videos taken on a connected Android device.";

const char kPhoneHubRecentAppsName[] = "Recent Apps in Phone Hub";
const char kPhoneHubRecentAppsDescription[] =
    "Enables the Recent Apps feature in Phone Hub, which allows users to "
    "relaunch a recently streamed app.";

const char kProductivityLauncherName[] =
    "Productivity experiment: App Launcher";
const char kProductivityLauncherDescription[] =
    "To evaluate an enhanced Launcher experience that aims to improve app "
    "workflows by optimizing access to apps, app content, and app actions.";

const char kProductivityLauncherAnimationName[] = "App Launcher: Animation";
const char kProductivityLauncherAnimationDescription[] =
    "Enables new animation in the enhanced app launcher.";

const char kForceShowContinueSectionName[] =
    "App Launcher: Force Continue Section Suggestions";
const char kForceShowContinueSectionDescription[] =
    "Forces the continue section of the app launcher to show. If there are no "
    "file suggestions available, the suggestions will be faked.";

const char kReduceDisplayNotificationsName[] = "Reduce display notifications";
const char kReduceDisplayNotificationsDescription[] =
    "If enabled, notifications for display rotation, display removed, display "
    "mirroring, and display extending will be suppressed.";

const char kReleaseNotesNotificationAllChannelsName[] =
    "Release Notes Notification All Channels";
const char kReleaseNotesNotificationAllChannelsDescription[] =
    "Enables the release notes notification for all Chrome OS channels";

const char kArcGhostWindowName[] = "Enable ARC ghost window";
const char kArcGhostWindowDescription[] =
    "Enables the pre-load app window for "
    "ARC++ app during ARCVM booting stage on full restore process";

const char kArcInputOverlayName[] = "Enable ARC Input Overlay";
const char kArcInputOverlayDescription[] =
    "Enables the input overlay feature for some Android game apps, "
    "so it can play with a keyboard and a mouse instead of touch screen";

const char kScanAppMultiPageScanName[] =
    "Enable multi-page scanning in Scan app";
const char kScanAppMultiPageScanDescription[] =
    "Enables creating a single PDF file from multiple flatbed scans";

extern const char kScanAppSearchablePdfName[] =
    "Enable saving scans as a searchable PDF.";
extern const char kScanAppSearchablePdfDescription[] =
    "Allow selecting Searchable PDF file type in Scan app"
    " with incorporation of OCR service.";

extern const char kSharesheetCopyToClipboardName[] =
    "Enable copy to clipboard in the Chrome OS Sharesheet.";
extern const char kSharesheetCopyToClipboardDescription[] =
    "Enables a share action in the sharesheet that copies the selected data to "
    "the clipboard.";

const char kShimlessRMAFlowName[] = "Enable shimless RMA flow";
const char kShimlessRMAFlowDescription[] = "Enable shimless RMA flow";

const char kSchedulerConfigurationName[] = "Scheduler Configuration";
const char kSchedulerConfigurationDescription[] =
    "Instructs the OS to use a specific scheduler configuration setting.";
const char kSchedulerConfigurationConservative[] =
    "Disables Hyper-Threading on relevant CPUs.";
const char kSchedulerConfigurationPerformance[] =
    "Enables Hyper-Threading on relevant CPUs.";

const char kChromeOSSharingHubName[] = "Chrome OS Sharing Hub";
const char kChromeOSSharingHubDescription[] =
    "Enables the Sharing Hub (share sheet) on ChromeOS via the Omnibox.";

const char kShowBluetoothDebugLogToggleName[] =
    "Show Bluetooth debug log toggle";
const char kShowBluetoothDebugLogToggleDescription[] =
    "Enables a toggle which can enable debug (i.e., verbose) logs for "
    "Bluetooth";

const char kShowFeedbackReportQuestionnaireName[] =
    "Show feedback report questionnaire";
const char kShowFeedbackReportQuestionnaireDescription[] =
    "Show domain-related questionnaire in feedback report UI";

const char kBluetoothSessionizedMetricsName[] =
    "Enable Bluetooth sessionized metrics";
const char kBluetoothSessionizedMetricsDescription[] =
    "Enables collecting and processing Bluetooth sessionized metrics.";

const char kShowTapsName[] = "Show taps";
const char kShowTapsDescription[] =
    "Draws a circle at each touch point, which makes touch points more obvious "
    "when projecting or mirroring the display. Similar to the Android OS "
    "developer option.";

const char kShowTouchHudName[] = "Show HUD for touch points";
const char kShowTouchHudDescription[] =
    "Shows a trail of colored dots for the last few touch points. Pressing "
    "Ctrl-Alt-I shows a heads-up display view in the top-left corner. Helps "
    "debug hardware issues that generate spurious touch events.";

const char kSmartLockUIRevampName[] = "Enable Smart Lock UI Revamp";
const char kSmartLockUIRevampDescription[] =
    "Replaces the existing Smart Lock UI on the lock screen with a new design "
    "and adds Smart Lock to the 'Lock screen and sign-in' section of settings.";

const char kSnoopingProtectionName[] = "Enable snooping detection";
const char kSnoopingProtectionDescription[] =
    "Enables snooping protection to notify you whenever there is a 'snooper' "
    "looking over your shoulder. Can be enabled and disabled from the Smart "
    "privacy section of your device settings.";

const char kSpectreVariant2MitigationName[] = "Spectre variant 2 mitigation";
const char kSpectreVariant2MitigationDescription[] =
    "Controls whether Spectre variant 2 mitigation is enabled when "
    "bootstrapping the Seccomp BPF sandbox. Can be overridden by "
    "#force-spectre-variant2-mitigation.";

const char kSyncSettingsCategorizationName[] = "Split OS and browser sync";
const char kSyncSettingsCategorizationDescription[] =
    "Allows OS sync to be configured separately from browser sync. Changes the "
    "OS settings UI to provide controls for OS data types.";

const char kSystemChinesePhysicalTypingName[] =
    "Use system IME for Chinese typing";
const char kSystemChinesePhysicalTypingDescription[] =
    "Use the system input engine instead of the Chrome extension for physical "
    "typing in Chinese.";

const char kSystemJapanesePhysicalTypingName[] =
    "Use system IME for Japanese typing";
const char kSystemJapanesePhysicalTypingDescription[] =
    "Use the system input engine instead of the Chrome extension for physical "
    "typing in Japanese.";

const char kSystemKoreanPhysicalTypingName[] =
    "Use system IME for Korean typing";
const char kSystemKoreanPhysicalTypingDescription[] =
    "Use the system input engine instead of the Chrome extension for physical "
    "typing in Korean.";

const char kQuickSettingsNetworkRevampName[] =
    "Enables the Quick Settings Network revamp.";
const char kQuickSettingsNetworkRevampDescription[] =
    "Enables the Quick Settings Network revamp, which updates Network Quick "
    "Settings UI and related infrastructure. See https://crbug.com/1169479.";

const char kTerminalSSHName[] = "Terminal SSH tabs";
const char kTerminalSSHDescription[] =
    "Enables SSH tabs in the Terminal System App.";

const char kTetherName[] = "Instant Tethering";
const char kTetherDescription[] =
    "Enables Instant Tethering. Instant Tethering allows your nearby Google "
    "phone to share its Internet connection with this device.";

const char kTouchscreenCalibrationName[] =
    "Enable/disable touchscreen calibration option in material design settings";
const char kTouchscreenCalibrationDescription[] =
    "If enabled, the user can calibrate the touch screen displays in "
    "chrome://settings/display.";

const char kTrafficCountersSettingsUiName[] = "Traffic Counters Settings UI";
const char kTrafficCountersSettingsUiDescription[] =
    "If enabled, the SettingsUI will show data usage for cellular networks";

const char kUseFakeDeviceForMediaStreamName[] = "Use fake video capture device";
const char kUseFakeDeviceForMediaStreamDescription[] =
    "Forces Chrome to use a fake video capture device (a rolling pacman with a "
    "timestamp) instead of the system audio/video devices, for debugging "
    "purposes.";

const char kUiDevToolsName[] = "Enable native UI inspection";
const char kUiDevToolsDescription[] =
    "Enables inspection of native UI elements. For local inspection use "
    "chrome://inspect#other";

const char kUiSlowAnimationsName[] = "Slow UI animations";
const char kUiSlowAnimationsDescription[] = "Makes all UI animations slow.";

const char kVaapiJpegImageDecodeAccelerationName[] =
    "VA-API JPEG decode acceleration for images";
const char kVaapiJpegImageDecodeAccelerationDescription[] =
    "Enable or disable decode acceleration of JPEG images (as opposed to camera"
    " captures) using the VA-API.";

const char kVaapiWebPImageDecodeAccelerationName[] =
    "VA-API WebP decode acceleration for images";
const char kVaapiWebPImageDecodeAccelerationDescription[] =
    "Enable or disable decode acceleration of WebP images using the VA-API.";

const char kVirtualKeyboardName[] = "Virtual Keyboard";
const char kVirtualKeyboardDescription[] =
    "Always show virtual keyboard regardless of having a physical keyboard "
    "present";

const char kVirtualKeyboardBorderedKeyName[] = "Virtual Keyboard Bordered Key";
const char kVirtualKeyboardBorderedKeyDescription[] =
    "Show virtual keyboard with bordered key";

const char kVirtualKeyboardDisabledName[] = "Disable Virtual Keyboard";
const char kVirtualKeyboardDisabledDescription[] =
    "Always disable virtual keyboard regardless of device mode. Workaround for "
    "virtual keyboard showing with some external keyboards.";

const char kVirtualKeyboardMultipasteName[] = "Virtual Keyboard MultiPaste";
const char kVirtualKeyboardMultipasteDescription[] =
    "Show virtual keyboard with multipaste UI";

const char kVirtualKeyboardMultipasteSuggestionName[] =
    "Virtual Keyboard MultiPaste Suggestion";
const char kVirtualKeyboardMultipasteSuggestionDescription[] =
    "Show multipaste items in virtual keyboard suggestion bar if they are "
    "copied recently";

const char kWakeOnWifiAllowedName[] = "Allow enabling wake on WiFi features";
const char kWakeOnWifiAllowedDescription[] =
    "Allows wake on WiFi features in shill to be enabled.";

const char kWebAppsCrosapiName[] = "Web Apps Crosapi";
const char kWebAppsCrosapiDescription[] =
    "Support web apps publishing from Lacros browser.";

const char kWebuiDarkModeName[] = "WebUI dark mode";
const char kWebuiDarkModeDescription[] =
    "Allows dark mode usage in WebUI. Note that this does not necessary enable "
    "dark mode, which is enabled via the #enable-force-dark flag.";

const char kWifiConnectMacAddressRandomizationName[] =
    "MAC address randomization";
const char kWifiConnectMacAddressRandomizationDescription[] =
    "Randomize MAC address when connecting to unmanaged (non-enterprise) "
    "WiFi networks.";

const char kWifiSyncAllowDeletesName[] =
    "Sync removal of Wi-Fi network configurations";
const char kWifiSyncAllowDeletesDescription[] =
    "Enables the option to sync deletions of Wi-Fi networks to other Chrome OS "
    "devices when Wi-Fi Sync is enabled.";

const char kWifiSyncAndroidName[] =
    "Sync Wi-Fi network configurations with Android";
const char kWifiSyncAndroidDescription[] =
    "Enables the option to sync Wi-Fi network configurations between Chrome OS "
    "devices and a connected Android phone";

const char kWindowControlMenu[] = "Float current active window";
const char kWindowControlMenuDescription[] =
    "Enables the accelerator (Control + Alt + F) to float current active "
    "window.";

const char kLauncherNudgeName[] = "Enable launcher nudge";
const char kLauncherNudgeDescription[] =
    "Enables nudges that bring new users' attention to the launcher button.";

const char kLauncherNudgeShortIntervalName[] =
    "Enable short intervals for launcher nudge";
const char kLauncherNudgeShortIntervalDescription[] =
    "Enables short intervals for launcher nudge for testing";

// Prefer keeping this section sorted to adding new definitions down here.

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
const char kAllowDefaultWebAppMigrationForChromeOsManagedUsersName[] =
    "Allow default web app migration for Chrome OS managed users";
const char kAllowDefaultWebAppMigrationForChromeOsManagedUsersDescription[] =
    "The web app migration flags "
    "(chrome://flags/#enable-migrate-default-chrome-app-to-web-apps-gsuite and "
    "chrome://flags/#enable-migrate-default-chrome-app-to-web-apps-non-gsuite) "
    "are ignored for managed Chrome OS users unless this feature is enabled.";

const char kBluetoothAdvertisementMonitoringName[] =
    "Bluetooth Advertisement Monitoring";
const char kBluetoothAdvertisementMonitoringDescription[] =
    "Advertisement monitoring allows applications to register low energy "
    "scanners that filter low energy advertisements in a power-efficient "
    "manner.";

const char kDefaultCalculatorWebAppName[] = "Default install Calculator PWA";
const char kDefaultCalculatorWebAppDescription[] =
    "Enable default installing of the calculator PWA instead of the deprecated "
    "chrome app.";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)

#if defined(OS_CHROMEOS)
const char kDeprecateLowUsageCodecsName[] = "Deprecates low usage media codecs";
const char kDeprecateLowUsageCodecsDescription[] =
    "Deprecates low usage codecs. Disable this feature to allow playback of "
    "AMR and GSM.";

const char kVaapiAV1DecoderName[] = "VA-API decode acceleration for AV1";
const char kVaapiAV1DecoderDescription[] =
    "Enable or disable decode acceleration of AV1 videos using the VA-API.";

const char kEnableTtsLacrosSupportName[] = "Enable tts lacros support";
const char kEnableTtsLacrosSupportDescription[] =
    "Enable or disable lacros support for text to speech.";
#endif  // defined(OS_CHROMEOS)

#if defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)
const char kVaapiVP9kSVCEncoderName[] =
    "VA-API encode acceleration for k-SVC VP9";
const char kVaapiVP9kSVCEncoderDescription[] =
    "Enable or disable k-SVC VP9 encode acceleration using VA-API.";
#endif  // defined(ARCH_CPU_X86_FAMILY) && BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
const char kChromeOSDirectVideoDecoderName[] = "ChromeOS Direct Video Decoder";
const char kChromeOSDirectVideoDecoderDescription[] =
    "Enables the hardware-accelerated ChromeOS direct media::VideoDecoder "
    "implementation. Note that this might be entirely disallowed by the "
    "--platform-disallows-chromeos-direct-video-decoder command line switch "
    "which is added for platforms where said direct VideoDecoder does not work "
    "or is not well tested (see the disable_cros_video_decoder USE flag in "
    "Chrome OS)";
#endif  // defined(OS_CHROMEOS) && BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || defined(OS_WIN)
const char kZeroCopyVideoCaptureName[] = "Enable Zero-Copy Video Capture";
const char kZeroCopyVideoCaptureDescription[] =
    "Camera produces a gpu friendly buffer on capture and, if there is, "
    "hardware accelerated video encoder consumes the buffer";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_MAC) || defined(OS_WIN)

// All views-based platforms --------------------------------------------------

#if defined(TOOLKIT_VIEWS)

const char kDownloadShelfWebUI[] = "Download Shelf WebUI";
const char kDownloadShelfWebUIDescription[] =
    "Replaces the Views download shelf with a WebUI download shelf.";

#endif  // defined(TOOLKIT_VIEWS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS) || defined(OS_FUCHSIA)

const char kWebUIBrandingUpdateName[] = "WebUI Branding Update";
const char kWebUIBrandingUpdateDescription[] =
    "Changes various UI components in WebUI pages to have a more modern look.";

const char kWebuiFeedbackName[] = "WebUI Feedback";
const char kWebuiFeedbackDescription[] =
    "If enabled, Chrome will show the Feedback WebUI, as opposed to Chrome "
    "App Feedback UI, when clicking on \"Report an issue...\"";

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS) || defined(OS_FUCHSIA)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_FUCHSIA)

const char kCommanderName[] = "Commander";
const char kCommanderDescription[] =
    "Enable a text interface to browser features. Invoke with Ctrl-Space.";

const char kDesktopRestructuredLanguageSettingsName[] =
    "Restructured Language Settings (Desktop)";
const char kDesktopRestructuredLanguageSettingsDescription[] =
    "Enable the new restructured language settings page";

const char kDesktopDetailedLanguageSettingsName[] =
    "Detailed Language Settings (Desktop)";
const char kDesktopDetailedLanguageSettingsDescription[] =
    "Enable the new detailed language settings page";

#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_FUCHSIA)

#if defined(OS_CHROMEOS) || defined(OS_LINUX)
#if BUILDFLAG(USE_TCMALLOC)
const char kDynamicTcmallocName[] = "Dynamic Tcmalloc Tuning";
const char kDynamicTcmallocDescription[] =
    "Allows tcmalloc to dynamically adjust tunables based on system resource "
    "utilization.";
#endif  // BUILDFLAG(USE_TCMALLOC)
#endif  // #if defined(OS_CHROMEOS) || defined(OS_LINUX)

#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MAC)
const char kWebShareName[] = "Web Share";
const char kWebShareDescription[] =
    "Enables the Web Share (navigator.share) APIs on experimentally supported "
    "platforms.";
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MAC)

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
const char kOzonePlatformHintChoiceDefault[] = "Default";
const char kOzonePlatformHintChoiceAuto[] = "Auto";
const char kOzonePlatformHintChoiceX11[] = "X11";
const char kOzonePlatformHintChoiceWayland[] = "Wayland";

const char kOzonePlatformHintName[] = "Preferred Ozone platform";
const char kOzonePlatformHintDescription[] =
    "Selects the preferred platform backend used on Linux. The default one is "
    "\"X11\". \"Auto\" selects Wayland if possible, X11 otherwise. ";
#endif  // defined(OS_LINUX) && !defined(OS_CHROMEOS)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
const char kDcheckIsFatalName[] = "DCHECKs are fatal";
const char kDcheckIsFatalDescription[] =
    "By default Chrome will evaluate in this build, but only log failures, "
    "rather than crashing. If enabled, DCHECKs will crash the calling process.";
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_JXL_DECODER)
const char kEnableJXLName[] = "Enable JXL image format";
const char kEnableJXLDescription[] =
    "Adds image decoding support for the JPEG XL image format.";
#endif  // BUILDFLAG(ENABLE_JXL_DECODER)

#if BUILDFLAG(ENABLE_NACL)
const char kNaclName[] = "Native Client";
const char kNaclDescription[] =
    "Support Native Client for all web applications, even those that were not "
    "installed from the Chrome Web Store.";
#endif  // ENABLE_NACL

#if BUILDFLAG(ENABLE_OOP_PRINTING)
const char kEnableOopPrintDriversName[] =
    "Enables Out-of-Process Printer Drivers";
const char kEnableOopPrintDriversDescription[] =
    "Enables printing interactions with the operating system to be performed "
    "out-of-process.";
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PAINT_PREVIEW) && defined(OS_ANDROID)
const char kPaintPreviewDemoName[] = "Paint Preview Demo";
const char kPaintPreviewDemoDescription[] =
    "If enabled a menu item is added to the Android main menu to demo paint "
    "previews.";
const char kPaintPreviewStartupName[] = "Paint Preview Startup";
const char kPaintPreviewStartupDescription[] =
    "If enabled, paint previews for each tab are captured when a tab is hidden "
    "and are deleted when a tab is closed. If a paint preview was captured for "
    "the tab to be restored on startup, the paint preview will be shown "
    "instead.";
#endif  // ENABLE_PAINT_PREVIEW && defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_SIDE_SEARCH)
const char kSideSearchName[] = "Side search";
const char kSideSearchDescription[] =
    "Enables an easily accessible way to access your most recent Google search "
    "results page embedded in a browser side panel";

const char kSideSearchClearCacheWhenClosedName[] =
    "Side search clear cache when closed";
const char kSideSearchClearCacheWhenClosedDescription[] =
    "Clears the side search cache when the side panel is closed.";

const char kSideSearchStatePerTabName[] = "Side search state per tab";
const char kSideSearchStatePerTabDescription[] =
    "Enables a per-tab toggled state for the side search side panel";
#endif  // BUILDFLAG(ENABLE_SIDE_SEARCH)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
const char kWebUITabStripFlagId[] = "webui-tab-strip";
const char kWebUITabStripName[] = "WebUI tab strip";
const char kWebUITabStripDescription[] =
    "When enabled makes use of a WebUI-based tab strip.";

const char kWebUITabStripContextMenuAfterTapName[] =
    "WebUI tab strip context menu after tap";
const char kWebUITabStripContextMenuAfterTapDescription[] =
    "Enables the context menu to appear after a tap gesture rather than "
    "following a press gesture.";
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)
const char kWebUITabStripTabDragIntegrationName[] =
    "ChromeOS drag-drop extensions for WebUI tab strip";
const char kWebUITabStripTabDragIntegrationDescription[] =
    "Enables special handling in ash for WebUI tab strip tab drags. Allows "
    "dragging tabs out to new windows.";
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP) && BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

const char kAutofillCreditCardUploadName[] =
    "Enable offering upload of Autofilled credit cards";
const char kAutofillCreditCardUploadDescription[] =
    "Enables a new option to upload credit cards to Google Payments for sync "
    "to all Chrome devices.";

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

#if !defined(OS_WIN) && !defined(OS_FUCHSIA)
const char kSendWebUIJavaScriptErrorReportsName[] =
    "Send WebUI JavaScript Error Reports";
const char kSendWebUIJavaScriptErrorReportsDescription[] =
    "If enabled, and if the user has consented to sending metrics to Google, "
    "then when the JavaScript has an error on a WebUI page, an error report "
    "will be sent to Google.";
#endif

#if defined(OS_WIN) || defined(OS_ANDROID)
const char kElasticOverscrollName[] = "Elastic Overscroll";
const char kElasticOverscrollDescription[] =
    "Enables Elastic Overscrolling on touchscreens and precision touchpads.";
#endif  // defined(OS_WIN) || defined(OS_ANDROID)

#if defined(OS_WIN) || (defined(OS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) || \
    defined(OS_MAC) || defined(OS_FUCHSIA)
const char kUIDebugToolsName[] = "Debugging tools for UI";
const char kUIDebugToolsDescription[] =
    "Enables additional keyboard shortcuts to help debugging.";
#endif

#if defined(WEBRTC_USE_PIPEWIRE)
const char kWebrtcPipeWireCapturerName[] = "WebRTC PipeWire support";
const char kWebrtcPipeWireCapturerDescription[] =
    "When enabled the WebRTC will use the PipeWire multimedia server for "
    "capturing the desktop content on the Wayland display server.";
#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kWebKioskEnableLacrosName[] =
    "Enables Lacros in the web (PWA) Kiosk";
const char kWebKioskEnableLacrosDescription[] =
    "Uses Lacros-chrome as the web browser in the web (PWA) Kiosk session on "
    "Chrome OS. When disabled, the Ash-chrome will be used";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order just like the header file.
// ============================================================================

}  // namespace flag_descriptions
