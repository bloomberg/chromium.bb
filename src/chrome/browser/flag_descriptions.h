// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
#define CHROME_BROWSER_FLAG_DESCRIPTIONS_H_

// Includes needed for macros allowing conditional compilation of some strings.
#include "base/logging.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/common/buildflags.h"
#include "components/feature_engagement/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "media/media_buildflags.h"
#include "ppapi/buildflags/buildflags.h"

#if defined(OS_ANDROID)
#include "ui/android/buildflags.h"
#endif  // defined(OS_ANDROID)

// This file declares strings used in chrome://flags. These messages are not
// translated, because instead of end-users they target Chromium developers and
// testers. See https://crbug.com/587272 and https://crbug.com/703134 for more
// details.
//
// Comments are not necessary. The contents of the strings (which appear in the
// UI) should be good enough documentation for what flags do and when they
// apply. If they aren't, fix them.
//
// Sort flags in each section alphabetically by the k...Name constant. Follow
// that by the k...Description constant and any special values associated with
// that.
//
// Put #ifdefed flags in the appropriate section toward the bottom, don't
// intersperse the file with ifdefs.

namespace flag_descriptions {

// Cross-platform -------------------------------------------------------------

extern const char kAccelerated2dCanvasName[];
extern const char kAccelerated2dCanvasDescription[];

extern const char kAcceleratedVideoDecodeName[];
extern const char kAcceleratedVideoDecodeDescription[];

extern const char kAffiliationBasedMatchingName[];
extern const char kAffiliationBasedMatchingDescription[];

extern const char kAllowInsecureLocalhostName[];
extern const char kAllowInsecureLocalhostDescription[];

extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionName[];
extern const char kAllowSignedHTTPExchangeCertsWithoutExtensionDescription[];

extern const char kAllowStartingServiceManagerOnlyName[];
extern const char kAllowStartingServiceManagerOnlyDescription[];

extern const char kUseMessagesGoogleComDomainName[];
extern const char kUseMessagesGoogleComDomainDescription[];

extern const char kUseMessagesStagingUrlName[];
extern const char kUseMessagesStagingUrlDescription[];

extern const char kEnableMessagesWebPushName[];
extern const char kEnableMessagesWebPushDescription[];

extern const char kAndroidSiteSettingsUIRefreshName[];
extern const char kAndroidSiteSettingsUIRefreshDescription[];

extern const char kAppBannersName[];
extern const char kAppBannersDescription[];

extern const char kAutomaticPasswordGenerationName[];
extern const char kAutomaticPasswordGenerationDescription[];

extern const char kEnableBlinkHeapUnifiedGarbageCollectionName[];
extern const char kEnableBlinkHeapUnifiedGarbageCollectionDescription[];

extern const char kEnableBloatedRendererDetectionName[];
extern const char kEnableBloatedRendererDetectionDescription[];

extern const char kAsyncImageDecodingName[];
extern const char kAsyncImageDecodingDescription[];

extern const char kAutofillAlwaysShowServerCardsInSyncTransportName[];
extern const char kAutofillAlwaysShowServerCardsInSyncTransportDescription[];

extern const char kAutofillAssistantChromeEntryName[];
extern const char kAutofillAssistantChromeEntryDescription[];

extern const char kAutofillCacheQueryResponsesName[];
extern const char kAutofillCacheQueryResponsesDescription[];

extern const char kAutofillEnableCompanyNameName[];
extern const char kAutofillEnableCompanyNameDescription[];

extern const char kAutofillDynamicFormsName[];
extern const char kAutofillDynamicFormsDescription[];

// Enforcing restrictions to enable/disable autofill small form support.
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsName[];
extern const char kAutofillEnforceMinRequiredFieldsForHeuristicsDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryName[];
extern const char kAutofillEnforceMinRequiredFieldsForQueryDescription[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadName[];
extern const char kAutofillEnforceMinRequiredFieldsForUploadDescription[];

extern const char kAutofillNoLocalSaveOnUploadSuccessName[];
extern const char kAutofillNoLocalSaveOnUploadSuccessDescription[];

extern const char kAutofillPrefilledFieldsName[];
extern const char kAutofillPrefilledFieldsDescription[];

extern const char kAutofillProfileClientValidationName[];
extern const char kAutofillProfileClientValidationDescription[];

extern const char kAutofillProfileServerValidationName[];
extern const char kAutofillProfileServerValidationDescription[];

extern const char kAutofillShowFullDisclosureLabelName[];
extern const char kAutofillShowFullDisclosureLabelDescription[];

extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutName[];
extern const char kAutofillRestrictUnownedFieldsToFormlessCheckoutDescription[];

extern const char kAutofillRichMetadataQueriesName[];
extern const char kAutofillRichMetadataQueriesDescription[];

extern const char kAutofillSettingsSplitByCardTypeName[];
extern const char kAutofillSettingsSplitByCardTypeDescription[];

extern const char kAutoplayPolicyName[];
extern const char kAutoplayPolicyDescription[];

extern const char kAutoplayPolicyUserGestureRequiredForCrossOrigin[];
extern const char kAutoplayPolicyNoUserGestureRequired[];
extern const char kAutoplayPolicyUserGestureRequired[];
extern const char kAutoplayPolicyDocumentUserActivation[];

extern const char kAwaitOptimizationName[];
extern const char kAwaitOptimizationDescription[];

extern const char kBleAdvertisingInExtensionsName[];
extern const char kBleAdvertisingInExtensionsDescription[];

extern const char kBlockTabUndersName[];
extern const char kBlockTabUndersDescription[];

extern const char kBrowserTaskSchedulerName[];
extern const char kBrowserTaskSchedulerDescription[];

extern const char kBundledConnectionHelpName[];
extern const char kBundledConnectionHelpDescription[];

extern const char kBypassAppBannerEngagementChecksName[];
extern const char kBypassAppBannerEngagementChecksDescription[];

extern const char kClickToOpenPDFName[];
extern const char kClickToOpenPDFDescription[];

extern const char kClipboardContentSettingName[];
extern const char kClipboardContentSettingDescription[];

extern const char kCloudImportName[];
extern const char kCloudImportDescription[];

extern const char kCloudPrinterHandlerName[];
extern const char kCloudPrinterHandlerDescription[];

extern const char kExperimentalAccessibilityFeaturesName[];
extern const char kExperimentalAccessibilityFeaturesDescription[];

extern const char kExperimentalAccessibilityAutoclickName[];
extern const char kExperimentalAccessibilityAutoclickDescription[];

extern const char kExperimentalAccessibilityLanguageDetectionName[];
extern const char kExperimentalAccessibilityLanguageDetectionDescription[];

extern const char kExperimentalAccessibilitySwitchAccessName[];
extern const char kExperimentalAccessibilitySwitchAccessDescription[];

extern const char kFCMInvalidationsName[];
extern const char kFCMInvalidationsDescription[];

extern const char kForceColorProfileSRGB[];
extern const char kForceColorProfileP3[];
extern const char kForceColorProfileColorSpin[];
extern const char kForceColorProfileHdr[];

extern const char kForceColorProfileName[];
extern const char kForceColorProfileDescription[];

extern const char kCompositedLayerBordersName[];
extern const char kCompositedLayerBordersDescription[];

extern const char kContextualSuggestionsButtonName[];
extern const char kContextualSuggestionsButtonDescription[];

extern const char kContextualSuggestionsIPHReverseScrollName[];
extern const char kContextualSuggestionsIPHReverseScrollDescription[];

extern const char kContextualSuggestionsOptOutName[];
extern const char kContextualSuggestionsOptOutDescription[];

extern const char kCreditCardAssistName[];
extern const char kCreditCardAssistDescription[];

extern const char kDataSaverServerPreviewsName[];
extern const char kDataSaverServerPreviewsDescription[];

extern const char kDebugPackedAppName[];
extern const char kDebugPackedAppDescription[];

extern const char kDebugShortcutsName[];
extern const char kDebugShortcutsDescription[];

extern const char kDeviceDiscoveryNotificationsName[];
extern const char kDeviceDiscoveryNotificationsDescription[];

extern const char kDevtoolsExperimentsName[];
extern const char kDevtoolsExperimentsDescription[];

extern const char kDisableAudioForDesktopShareName[];
extern const char kDisableAudioForDesktopShareDescription[];

extern const char kDisableBestEffortTasksName[];
extern const char kDisableBestEffortTasksDescription[];

extern const char kDisableIpcFloodingProtectionName[];
extern const char kDisableIpcFloodingProtectionDescription[];

extern const char kDisablePushStateThrottleName[];
extern const char kDisablePushStateThrottleDescription[];

extern const char kDisableTabForDesktopShareName[];
extern const char kDisableTabForDesktopShareDescription[];

extern const char kDisallowDocWrittenScriptsUiName[];
extern const char kDisallowDocWrittenScriptsUiDescription[];

extern const char kDisallowUnsafeHttpDownloadsName[];
extern const char kDisallowUnsafeHttpDownloadsNameDescription[];

extern const char kDisplayList2dCanvasName[];
extern const char kDisplayList2dCanvasDescription[];

extern const char kEmbeddedExtensionOptionsName[];
extern const char kEmbeddedExtensionOptionsDescription[];

extern const char kEnableAccessibilityObjectModelName[];
extern const char kEnableAccessibilityObjectModelDescription[];

extern const char kEnableAudioFocusEnforcementName[];
extern const char kEnableAudioFocusEnforcementDescription[];

extern const char kEnableAutocompleteDataRetentionPolicyName[];
extern const char kEnableAutocompleteDataRetentionPolicyDescription[];

extern const char kEnableAutofillAccountWalletStorageName[];
extern const char kEnableAutofillAccountWalletStorageDescription[];

extern const char kEnableAutofillCreditCardAblationExperimentDisplayName[];
extern const char kEnableAutofillCreditCardAblationExperimentDescription[];

extern const char kEnableAutofillCreditCardLastUsedDateDisplayName[];
extern const char kEnableAutofillCreditCardLastUsedDateDisplayDescription[];

extern const char kEnableAutofillCreditCardLocalCardMigrationName[];
extern const char kEnableAutofillCreditCardLocalCardMigrationDescription[];

extern const char kEnableAutofillCreditCardUploadEditableCardholderNameName[];
extern const char
    kEnableAutofillCreditCardUploadEditableCardholderNameDescription[];

extern const char kEnableAutofillCreditCardUploadEditableExpirationDateName[];
extern const char
    kEnableAutofillCreditCardUploadEditableExpirationDateDescription[];

extern const char kEnableAutofillDoNotUploadSaveUnsupportedCardsName[];
extern const char kEnableAutofillDoNotUploadSaveUnsupportedCardsDescription[];

extern const char kEnableAutofillImportNonFocusableCreditCardFormsName[];
extern const char kEnableAutofillImportNonFocusableCreditCardFormsDescription[];

extern const char kEnableAutofillLocalCardMigrationShowFeedbackName[];
extern const char kEnableAutofillLocalCardMigrationShowFeedbackDescription[];

extern const char kEnableAutofillLocalCardMigrationUsesStrikeSystemV2Name[];
extern const char
    kEnableAutofillLocalCardMigrationUsesStrikeSystemV2Description[];

extern const char kEnableAutofillSaveCardImprovedUserConsentName[];
extern const char kEnableAutofillSaveCardImprovedUserConsentDescription[];

extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemName[];
extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemDescription[];

extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemV2Name[];
extern const char kEnableAutofillSaveCreditCardUsesStrikeSystemV2Description[];

extern const char kEnableAutofillToolkitViewsCreditCardDialogsMac[];
extern const char kEnableAutofillToolkitViewsCreditCardDialogsMacDescription[];

extern const char kEnableAutofillNativeDropdownViewsName[];
extern const char kEnableAutofillNativeDropdownViewsDescription[];

extern const char kEnableAutofillSaveCardDialogUnlabeledExpirationDateName[];
extern const char
    kEnableAutofillSaveCardDialogUnlabeledExpirationDateDescription[];

extern const char kEnableAutofillSaveCardSignInAfterLocalSaveName[];
extern const char kEnableAutofillSaveCardSignInAfterLocalSaveDescription[];

extern const char kEnableAutofillSendExperimentIdsInPaymentsRPCsName[];
extern const char kEnableAutofillSendExperimentIdsInPaymentsRPCsDescription[];

extern const char kEnableAutoplayIgnoreWebAudioName[];
extern const char kEnableAutoplayIgnoreWebAudioDescription[];

extern const char kEnableAutoplayUnifiedSoundSettingsName[];
extern const char kEnableAutoplayUnifiedSoundSettingsDescription[];

extern const char kEnableBrotliName[];
extern const char kEnableBrotliDescription[];

extern const char kEnableDataSaverLiteModeRebrandName[];
extern const char kEnableDataSaverLiteModeRebrandDescription[];

extern const char kEnableClientLoFiName[];
extern const char kEnableClientLoFiDescription[];

extern const char kEnableCSSFragmentIdentifiersName[];
extern const char kEnableCSSFragmentIdentifiersDescription[];

extern const char kEnableFilesystemInIncognitoName[];
extern const char kEnableFilesystemInIncognitoDescription[];

extern const char kEnableNoScriptPreviewsName[];
extern const char kEnableNoScriptPreviewsDescription[];

extern const char kDataReductionProxyServerAlternative1[];
extern const char kDataReductionProxyServerAlternative2[];
extern const char kDataReductionProxyServerAlternative3[];
extern const char kDataReductionProxyServerAlternative4[];
extern const char kDataReductionProxyServerAlternative5[];
extern const char kDataReductionProxyServerAlternative6[];
extern const char kDataReductionProxyServerAlternative7[];
extern const char kDataReductionProxyServerAlternative8[];
extern const char kDataReductionProxyServerAlternative9[];
extern const char kDataReductionProxyServerAlternative10[];
extern const char kEnableDataReductionProxyNetworkServiceName[];
extern const char kEnableDataReductionProxyNetworkServiceDescription[];
extern const char kEnableDataReductionProxyServerExperimentName[];
extern const char kEnableDataReductionProxyServerExperimentDescription[];

extern const char kEnableDataReductionProxySavingsPromoName[];
extern const char kEnableDataReductionProxySavingsPromoDescription[];

extern const char kEnableDesktopPWAsName[];
extern const char kEnableDesktopPWAsDescription[];

extern const char kEnableDesktopPWAsLinkCapturingName[];
extern const char kEnableDesktopPWAsLinkCapturingDescription[];

extern const char kDesktopPWAsCustomTabUIName[];
extern const char kDesktopPWAsCustomTabUIDescription[];

extern const char kDesktopPWAsStayInWindowName[];
extern const char kDesktopPWAsStayInWindowDescription[];

extern const char kDesktopPWAsOmniboxInstallName[];
extern const char kDesktopPWAsOmniboxInstallDescription[];

extern const char kEnableSystemWebAppsName[];
extern const char kEnableSystemWebAppsDescription[];

extern const char kEnforceTLS13DowngradeName[];
extern const char kEnforceTLS13DowngradeDescription[];

extern const char kEnableGenericSensorName[];
extern const char kEnableGenericSensorDescription[];

extern const char kEnableGenericSensorExtraClassesName[];
extern const char kEnableGenericSensorExtraClassesDescription[];

extern const char kEnableGpuServiceLoggingName[];
extern const char kEnableGpuServiceLoggingDescription[];

extern const char kEnableHDRName[];
extern const char kEnableHDRDescription[];

extern const char kEnableImplicitRootScrollerName[];
extern const char kEnableImplicitRootScrollerDescription[];

extern const char kEnablePreviewsAndroidOmniboxUIName[];
extern const char kEnablePreviewsAndroidOmniboxUIDescription[];

extern const char kEnableLitePageServerPreviewsName[];
extern const char kEnableLitePageServerPreviewsDescription[];

extern const char kEnableURLLoaderLitePageServerPreviewsName[];
extern const char kEnableURLLoaderLitePageServerPreviewsDescription[];

extern const char kBuiltInModuleAllName[];
extern const char kBuiltInModuleAllDescription[];

extern const char kBuiltInModuleInfraName[];
extern const char kBuiltInModuleInfraDescription[];

extern const char kBuiltInModuleKvStorageName[];
extern const char kBuiltInModuleKvStorageDescription[];

extern const char kEnableBlinkGenPropertyTreesName[];
extern const char kEnableBlinkGenPropertyTreesDescription[];

extern const char kEnableLayoutNGName[];
extern const char kEnableLayoutNGDescription[];

extern const char kEnableLazyFrameLoadingName[];
extern const char kEnableLazyFrameLoadingDescription[];

extern const char kEnableLazyImageLoadingName[];
extern const char kEnableLazyImageLoadingDescription[];

extern const char kEnableMacMaterialDesignDownloadShelfName[];
extern const char kEnableMacMaterialDesignDownloadShelfDescription[];

extern const char kEnableMaterialDesignBookmarksName[];
extern const char kEnableMaterialDesignBookmarksDescription[];

extern const char kEnableMediaSessionServiceName[];
extern const char kEnableMediaSessionServiceDescription[];

extern const char kEnableNavigationTracingName[];
extern const char kEnableNavigationTracingDescription[];

extern const char kEnableNetworkLoggingToFileName[];
extern const char kEnableNetworkLoggingToFileDescription[];

extern const char kEnableNetworkServiceName[];
extern const char kEnableNetworkServiceDescription[];

extern const char kEnableNetworkServiceInProcessName[];
extern const char kEnableNetworkServiceInProcessDescription[];

extern const char kEnableNotificationScrollBarName[];
extern const char kEnableNotificationScrollBarDescription[];

extern const char kEnableNotificationExpansionAnimationName[];
extern const char kEnableNotificationExpansionAnimationDescription[];

extern const char kEnableOptimizationHintsName[];
extern const char kEnableOptimizationHintsDescription[];

extern const char kEnableOutOfBlinkCorsName[];
extern const char kEnableOutOfBlinkCorsDescription[];

extern const char kVizDisplayCompositorName[];
extern const char kVizDisplayCompositorDescription[];

extern const char kVizHitTestDrawQuadName[];
extern const char kVizHitTestDrawQuadDescription[];

extern const char kEnableOutOfProcessHeapProfilingName[];
extern const char kEnableOutOfProcessHeapProfilingDescription[];
extern const char kEnableOutOfProcessHeapProfilingModeMinimal[];
extern const char kEnableOutOfProcessHeapProfilingModeAll[];
extern const char kEnableOutOfProcessHeapProfilingModeAllRenderers[];
extern const char kEnableOutOfProcessHeapProfilingModeBrowser[];
extern const char kEnableOutOfProcessHeapProfilingModeGpu[];
extern const char kEnableOutOfProcessHeapProfilingModeManual[];
extern const char kEnableOutOfProcessHeapProfilingModeRendererSampling[];
extern const char kOutOfProcessHeapProfilingKeepSmallAllocations[];
extern const char kOutOfProcessHeapProfilingKeepSmallAllocationsDescription[];
extern const char kOutOfProcessHeapProfilingInProcess[];
extern const char kOutOfProcessHeapProfilingInProcessDescription[];
extern const char kOutOfProcessHeapProfilingSamplingRate[];
extern const char kOutOfProcessHeapProfilingSamplingRateDescription[];

extern const char kOOPHPStackModeName[];
extern const char kOOPHPStackModeDescription[];
extern const char kOOPHPStackModeMixed[];
extern const char kOOPHPStackModeNative[];
extern const char kOOPHPStackModeNativeWithThreadNames[];
extern const char kOOPHPStackModePseudo[];

extern const char kDownloadAutoResumptionNativeName[];
extern const char kDownloadAutoResumptionNativeDescription[];

extern const char kEnableNewDownloadBackendName[];
extern const char kEnableNewDownloadBackendDescription[];

extern const char kEnablePictureInPictureName[];
extern const char kEnablePictureInPictureDescription[];

extern const char kEnablePixelCanvasRecordingName[];
extern const char kEnablePixelCanvasRecordingDescription[];

extern const char kEnableResamplingInputEventsName[];
extern const char kEnableResamplingInputEventsDescription[];
extern const char kEnableResamplingScrollEventsName[];
extern const char kEnableResamplingScrollEventsDescription[];

extern const char kEnableResourceLoadingHintsName[];
extern const char kEnableResourceLoadingHintsDescription[];

extern const char kEnableSyncPseudoUSSAppListName[];
extern const char kEnableSyncPseudoUSSAppListDescription[];

extern const char kEnableSyncPseudoUSSAppsName[];
extern const char kEnableSyncPseudoUSSAppsDescription[];

extern const char kEnableSyncPseudoUSSDictionaryName[];
extern const char kEnableSyncPseudoUSSDictionaryDescription[];

extern const char kEnableSyncPseudoUSSExtensionSettingsName[];
extern const char kEnableSyncPseudoUSSExtensionSettingsDescription[];

extern const char kEnableSyncPseudoUSSExtensionsName[];
extern const char kEnableSyncPseudoUSSExtensionsDescription[];

extern const char kEnableSyncPseudoUSSFaviconsName[];
extern const char kEnableSyncPseudoUSSFaviconsDescription[];

extern const char kEnableSyncPseudoUSSHistoryDeleteDirectivesName[];
extern const char kEnableSyncPseudoUSSHistoryDeleteDirectivesDescription[];

extern const char kEnableSyncPseudoUSSPasswordsName[];
extern const char kEnableSyncPseudoUSSPasswordsDescription[];

extern const char kEnableSyncPseudoUSSPreferencesName[];
extern const char kEnableSyncPseudoUSSPreferencesDescription[];

extern const char kEnableSyncPseudoUSSPriorityPreferencesName[];
extern const char kEnableSyncPseudoUSSPriorityPreferencesDescription[];

extern const char kEnableSyncPseudoUSSSearchEnginesName[];
extern const char kEnableSyncPseudoUSSSearchEnginesDescription[];

extern const char kEnableSyncPseudoUSSSupervisedUsersName[];
extern const char kEnableSyncPseudoUSSSupervisedUsersDescription[];

extern const char kEnableSyncPseudoUSSThemesName[];
extern const char kEnableSyncPseudoUSSThemesDescription[];

extern const char kEnableSyncUSSBookmarksName[];
extern const char kEnableSyncUSSBookmarksDescription[];

extern const char kEnableSyncUSSPasswordsName[];
extern const char kEnableSyncUSSPasswordsDescription[];

extern const char kEnableSyncUSSSessionsName[];
extern const char kEnableSyncUSSSessionsDescription[];

extern const char kEnableTextFragmentAnchorName[];
extern const char kEnableTextFragmentAnchorDescription[];

extern const char kEnableUseZoomForDsfName[];
extern const char kEnableUseZoomForDsfDescription[];
extern const char kEnableUseZoomForDsfChoiceDefault[];
extern const char kEnableUseZoomForDsfChoiceEnabled[];
extern const char kEnableUseZoomForDsfChoiceDisabled[];

extern const char kEnableScrollAnchorSerializationName[];
extern const char kEnableScrollAnchorSerializationDescription[];

extern const char kEnableSharedArrayBufferName[];
extern const char kEnableSharedArrayBufferDescription[];

extern const char kEnableWasmName[];
extern const char kEnableWasmDescription[];

extern const char kEnableWebAuthenticationCableSupportName[];
extern const char kEnableWebAuthenticationCableSupportDescription[];

extern const char kEnableWebUsbName[];
extern const char kEnableWebUsbDescription[];

extern const char kEnableIncognitoWindowCounterName[];
extern const char kEnableIncognitoWindowCounterDescription[];

extern const char kEnableWasmBaselineName[];
extern const char kEnableWasmBaselineDescription[];

extern const char kEnableWasmCodeCacheName[];
extern const char kEnableWasmCodeCacheDescription[];

extern const char kEnableWasmThreadsName[];
extern const char kEnableWasmThreadsDescription[];

extern const char kExpensiveBackgroundTimerThrottlingName[];
extern const char kExpensiveBackgroundTimerThrottlingDescription[];

extern const char kExperimentalAppBannersName[];
extern const char kExperimentalAppBannersDescription[];

extern const char kExperimentalCanvasFeaturesName[];
extern const char kExperimentalCanvasFeaturesDescription[];

extern const char kExperimentalExtensionApisName[];
extern const char kExperimentalExtensionApisDescription[];

extern const char kExperimentalProductivityFeaturesName[];
extern const char kExperimentalProductivityFeaturesDescription[];

extern const char kExperimentalSecurityFeaturesName[];
extern const char kExperimentalSecurityFeaturesDescription[];

extern const char kExperimentalWebPlatformFeaturesName[];
extern const char kExperimentalWebPlatformFeaturesDescription[];

extern const char kExtensionContentVerificationName[];
extern const char kExtensionContentVerificationDescription[];
extern const char kExtensionContentVerificationBootstrap[];
extern const char kExtensionContentVerificationEnforce[];
extern const char kExtensionContentVerificationEnforceStrict[];

extern const char kExtensionsOnChromeUrlsName[];
extern const char kExtensionsOnChromeUrlsDescription[];

extern const char kFeaturePolicyName[];
extern const char kFeaturePolicyDescription[];

extern const char kFontCacheScalingName[];
extern const char kFontCacheScalingDescription[];

extern const char kForceEffectiveConnectionTypeName[];
extern const char kForceEffectiveConnectionTypeDescription[];
extern const char kEffectiveConnectionTypeUnknownDescription[];
extern const char kEffectiveConnectionTypeOfflineDescription[];
extern const char kEffectiveConnectionTypeSlow2GDescription[];
extern const char kEffectiveConnectionTypeSlow2GOnCellularDescription[];
extern const char kEffectiveConnectionType2GDescription[];
extern const char kEffectiveConnectionType3GDescription[];
extern const char kEffectiveConnectionType4GDescription[];

extern const char kFillOnAccountSelectName[];
extern const char kFillOnAccountSelectDescription[];

extern const char kFillOnAccountSelectHttpName[];
extern const char kFillOnAccountSelectHttpDescription[];

extern const char kFocusMode[];
extern const char kFocusModeDescription[];

extern const char kForceTextDirectionName[];
extern const char kForceTextDirectionDescription[];
extern const char kForceDirectionLtr[];
extern const char kForceDirectionRtl[];

extern const char kForceUiDirectionName[];
extern const char kForceUiDirectionDescription[];

extern const char kFramebustingName[];
extern const char kFramebustingDescription[];

extern const char kGpuRasterizationName[];
extern const char kGpuRasterizationDescription[];
extern const char kForceGpuRasterization[];

extern const char kGooglePasswordManagerName[];
extern const char kGooglePasswordManagerDescription[];

extern const char kGoogleProfileInfoName[];
extern const char kGoogleProfileInfoDescription[];

extern const char kHandwritingGestureName[];
extern const char kHandwritingGestureDescription[];

extern const char kHardwareMediaKeyHandling[];
extern const char kHardwareMediaKeyHandlingDescription[];

extern const char kHarfbuzzRendertextName[];
extern const char kHarfbuzzRendertextDescription[];

extern const char kHorizontalTabSwitcherAndroidName[];
extern const char kHorizontalTabSwitcherAndroidDescription[];

extern const char kTabSwitcherOnReturnName[];
extern const char kTabSwitcherOnReturnDescription[];

extern const char kViewsCastDialogName[];
extern const char kViewsCastDialogDescription[];

extern const char kHideActiveAppsFromShelfName[];
extern const char kHideActiveAppsFromShelfDescription[];

extern const char kHistoryRequiresUserGestureName[];
extern const char kHistoryRequiresUserGestureDescription[];

extern const char kHostedAppQuitNotificationName[];
extern const char kHostedAppQuitNotificationDescription[];

extern const char kHostedAppShimCreationName[];
extern const char kHostedAppShimCreationDescription[];

extern const char kHtmlBasedUsernameDetectorName[];
extern const char kHtmlBasedUsernameDetectorDescription[];

extern const char kIconNtpName[];
extern const char kIconNtpDescription[];

extern const char kIgnoreGpuBlacklistName[];
extern const char kIgnoreGpuBlacklistDescription[];

extern const char kIgnorePreviewsBlacklistName[];
extern const char kIgnorePreviewsBlacklistDescription[];

extern const char kImprovedGeoLanguageDataName[];
extern const char kImprovedGeoLanguageDataDescription[];

extern const char kInProductHelpDemoModeChoiceName[];
extern const char kInProductHelpDemoModeChoiceDescription[];

extern const char kJavascriptHarmonyName[];
extern const char kJavascriptHarmonyDescription[];

extern const char kJavascriptHarmonyShippingName[];
extern const char kJavascriptHarmonyShippingDescription[];

extern const char kKeepAliveRendererForKeepaliveRequestsName[];
extern const char kKeepAliveRendererForKeepaliveRequestsDescription[];

extern const char kLcdTextName[];
extern const char kLcdTextDescription[];

extern const char kLoadMediaRouterComponentExtensionName[];
extern const char kLoadMediaRouterComponentExtensionDescription[];

extern const char kLookalikeUrlNavigationSuggestionsName[];
extern const char kLookalikeUrlNavigationSuggestionsDescription[];

extern const char kMarkHttpAsName[];
extern const char kMarkHttpAsDescription[];
extern const char kMarkHttpAsDangerous[];
extern const char kMarkHttpAsWarning[];
extern const char kMarkHttpAsWarningAndDangerousOnFormEdits[];
extern const char kMarkHttpAsWarningAndDangerousOnPasswordsAndCreditCards[];

extern const char kMediaRouterCastAllowAllIPsName[];
extern const char kMediaRouterCastAllowAllIPsDescription[];

extern const char kMessageCenterNewStyleNotificationName[];
extern const char kMessageCenterNewStyleNotificationDescription[];

extern const char kMobileIdentityConsistencyName[];
extern const char kMobileIdentityConsistencyDescription[];

extern const char kNativeFilesystemAPIName[];
extern const char kNativeFilesystemAPIDescription[];

extern const char kNewAudioRenderingMixingStrategyName[];
extern const char kNewAudioRenderingMixingStrategyDescription[];

extern const char kNewBookmarkAppsName[];
extern const char kNewBookmarkAppsDescription[];

extern const char kNewPasswordFormParsingName[];
extern const char kNewPasswordFormParsingDescription[];

extern const char kNewPasswordFormParsingForSavingName[];
extern const char kNewPasswordFormParsingForSavingDescription[];

extern const char kOnlyNewPasswordFormParsingName[];
extern const char kOnlyNewPasswordFormParsingDescription[];

extern const char kUseSurfaceLayerForVideoName[];
extern const char kUseSurfaceLayerForVideoDescription[];

extern const char kNewUsbBackendName[];
extern const char kNewUsbBackendDescription[];

extern const char kNewblueName[];
extern const char kNewblueDescription[];

extern const char kNewTabLoadingAnimation[];
extern const char kNewTabLoadingAnimationDescription[];

extern const char kNewTabButtonPosition[];
extern const char kNewTabButtonPositionDescription[];
extern const char kNewTabButtonPositionOppositeCaption[];
extern const char kNewTabButtonPositionLeading[];
extern const char kNewTabButtonPositionAfterTabs[];
extern const char kNewTabButtonPositionTrailing[];

extern const char kNostatePrefetchName[];
extern const char kNostatePrefetchDescription[];

extern const char kNotificationIndicatorName[];
extern const char kNotificationIndicatorDescription[];

extern const char kNotificationsNativeFlagName[];
extern const char kNotificationsNativeFlagDescription[];

extern const char kUseMultiloginEndpointName[];
extern const char kUseMultiloginEndpointDescription[];

#if defined(OS_POSIX)
extern const char kNtlmV2EnabledName[];
extern const char kNtlmV2EnabledDescription[];
#endif

extern const char kOfferStoreUnmaskedWalletCardsName[];
extern const char kOfferStoreUnmaskedWalletCardsDescription[];

extern const char kOfflineAutoReloadName[];
extern const char kOfflineAutoReloadDescription[];

extern const char kOfflineAutoReloadVisibleOnlyName[];
extern const char kOfflineAutoReloadVisibleOnlyDescription[];

extern const char kOmniboxDisplayTitleForCurrentUrlName[];
extern const char kOmniboxDisplayTitleForCurrentUrlDescription[];

extern const char kOmniboxMaterialDesignWeatherIconsName[];
extern const char kOmniboxMaterialDesignWeatherIconsDescription[];

extern const char kOmniboxNewAnswerLayoutName[];
extern const char kOmniboxNewAnswerLayoutDescription[];

extern const char kOmniboxRichEntitySuggestionsName[];
extern const char kOmniboxRichEntitySuggestionsDescription[];

extern const char kOmniboxSpareRendererName[];
extern const char kOmniboxSpareRendererDescription[];

extern const char kOmniboxUIBlueSearchLoopAndSearchQueryName[];
extern const char kOmniboxUIBlueSearchLoopAndSearchQueryDescription[];

extern const char kOmniboxUIBlueTitlesAndGrayUrlsOnPageSuggestionsName[];
extern const char kOmniboxUIBlueTitlesAndGrayUrlsOnPageSuggestionsDescription[];

extern const char kOmniboxUIBlueTitlesOnPageSuggestionsName[];
extern const char kOmniboxUIBlueTitlesOnPageSuggestionsDescription[];

extern const char kOmniboxUIBoldUserTextOnSearchSuggestionsName[];
extern const char kOmniboxUIBoldUserTextOnSearchSuggestionsDescription[];

extern const char kOmniboxUIHideSteadyStateUrlSchemeName[];
extern const char kOmniboxUIHideSteadyStateUrlSchemeDescription[];

extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsName[];
extern const char kOmniboxUIHideSteadyStateUrlTrivialSubdomainsDescription[];

extern const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefName[];
extern const char kOmniboxUIHideSteadyStateUrlPathQueryAndRefDescription[];

extern const char kOmniboxUIOneClickUnelideName[];
extern const char kOmniboxUIOneClickUnelideDescription[];

extern const char kOmniboxUIMaxAutocompleteMatchesName[];
extern const char kOmniboxUIMaxAutocompleteMatchesDescription[];

extern const char kOmniboxUIShowSuffixOnAllSearchSuggestionsName[];
extern const char kOmniboxUIShowSuffixOnAllSearchSuggestionsDescription[];

extern const char kOmniboxUIShowSuggestionFaviconsName[];
extern const char kOmniboxUIShowSuggestionFaviconsDescription[];

extern const char kOmniboxUISwapTitleAndUrlName[];
extern const char kOmniboxUISwapTitleAndUrlDescription[];

extern const char kOmniboxUIUnboldSuggestionTextName[];
extern const char kOmniboxUIUnboldSuggestionTextDescription[];

extern const char kOmniboxUIUseGenericSearchEngineIconName[];
extern const char kOmniboxUIUseGenericSearchEngineIconDescription[];

extern const char kOmniboxUIVerticalMarginName[];
extern const char kOmniboxUIVerticalMarginDescription[];

extern const char kOmniboxUIWhiteBackgroundOnBlurName[];
extern const char kOmniboxUIWhiteBackgroundOnBlurDescription[];

extern const char kOmniboxVoiceSearchAlwaysVisibleName[];
extern const char kOmniboxVoiceSearchAlwaysVisibleDescription[];

extern const char kOnTheFlyMhtmlHashComputationName[];
extern const char kOnTheFlyMhtmlHashComputationDescription[];

extern const char kOopRasterizationName[];
extern const char kOopRasterizationDescription[];

extern const char kOriginTrialsName[];
extern const char kOriginTrialsDescription[];

extern const char kOverlayScrollbarsName[];
extern const char kOverlayScrollbarsDescription[];

extern const char kOverlayScrollbarsFlashAfterAnyScrollUpdateName[];
extern const char kOverlayScrollbarsFlashAfterAnyScrollUpdateDescription[];

extern const char kOverlayScrollbarsFlashWhenMouseEnterName[];
extern const char kOverlayScrollbarsFlashWhenMouseEnterDescription[];

extern const char kOverlayStrategiesName[];
extern const char kOverlayStrategiesDescription[];
extern const char kOverlayStrategiesDefault[];
extern const char kOverlayStrategiesNone[];
extern const char kOverlayStrategiesUnoccludedFullscreen[];
extern const char kOverlayStrategiesUnoccluded[];
extern const char kOverlayStrategiesOccludedAndUnoccluded[];

extern const char kUseNewAcceptLanguageHeaderName[];
extern const char kUseNewAcceptLanguageHeaderDescription[];

extern const char kOverscrollHistoryNavigationName[];
extern const char kOverscrollHistoryNavigationDescription[];

extern const char kOverscrollStartThresholdName[];
extern const char kOverscrollStartThresholdDescription[];
extern const char kOverscrollStartThreshold133Percent[];
extern const char kOverscrollStartThreshold166Percent[];
extern const char kOverscrollStartThreshold200Percent[];

extern const char kParallelDownloadingName[];
extern const char kParallelDownloadingDescription[];

extern const char kPassiveEventListenerDefaultName[];
extern const char kPassiveEventListenerDefaultDescription[];
extern const char kPassiveEventListenerTrue[];
extern const char kPassiveEventListenerForceAllTrue[];

extern const char kPassiveEventListenersDueToFlingName[];
extern const char kPassiveEventListenersDueToFlingDescription[];

extern const char kPassiveDocumentEventListenersName[];
extern const char kPassiveDocumentEventListenersDescription[];

extern const char kPassiveDocumentWheelEventListenersName[];
extern const char kPassiveDocumentWheelEventListenersDescription[];

extern const char kPasswordImportName[];
extern const char kPasswordImportDescription[];

extern const char kPasswordsKeyboardAccessoryName[];
extern const char kPasswordsKeyboardAccessoryDescription[];

extern const char kPasswordsMigrateLinuxToLoginDBName[];
extern const char kPasswordsMigrateLinuxToLoginDBDescription[];

extern const char kPerMethodCanMakePaymentQuotaName[];
extern const char kPerMethodCanMakePaymentQuotaDescription[];

extern const char kPinchScaleName[];
extern const char kPinchScaleDescription[];

extern const char kPreviewsAllowedName[];
extern const char kPreviewsAllowedDescription[];

extern const char kPrintPdfAsImageName[];
extern const char kPrintPdfAsImageDescription[];

extern const char kPrintPreviewRegisterPromosName[];
extern const char kPrintPreviewRegisterPromosDescription[];

extern const char kPullToRefreshName[];
extern const char kPullToRefreshDescription[];
extern const char kPullToRefreshEnabledTouchscreen[];

extern const char kQueryInOmniboxName[];
extern const char kQueryInOmniboxDescription[];

extern const char kQuicName[];
extern const char kQuicDescription[];

extern const char kRecurrentInterstitialName[];
extern const char kRecurrentInterstitialDescription[];

extern const char kReducedReferrerGranularityName[];
extern const char kReducedReferrerGranularityDescription[];

extern const char kRegionalLocalesAsDisplayUIName[];
extern const char kRegionalLocalesAsDisplayUIDescription[];

extern const char kRewriteLevelDBOnDeletionName[];
extern const char kRewriteLevelDBOnDeletionDescription[];

extern const char kRendererSideResourceSchedulerName[];
extern const char kRendererSideResourceSchedulerDescription[];

extern const char kRequestTabletSiteName[];
extern const char kRequestTabletSiteDescription[];

extern const char kResetAppListInstallStateName[];
extern const char kResetAppListInstallStateDescription[];

extern const char kResourceLoadSchedulerName[];
extern const char kResourceLoadSchedulerDescription[];

extern const char kSafeBrowsingUseAPDownloadVerdictsName[];
extern const char kSafeBrowsingUseAPDownloadVerdictsDescription[];

extern const char kSafeSearchUrlReportingName[];
extern const char kSafeSearchUrlReportingDescription[];

extern const char kSamplingHeapProfilerName[];
extern const char kSamplingHeapProfilerDescription[];

extern const char kSaveasMenuLabelExperimentName[];
extern const char kSaveasMenuLabelExperimentDescription[];

extern const char kSavePageAsMhtmlName[];
extern const char kSavePageAsMhtmlDescription[];

extern const char kSendTabToSelfName[];
extern const char kSendTabToSelfDescription[];

extern const char kServiceWorkerImportedScriptUpdateCheckName[];
extern const char kServiceWorkerImportedScriptUpdateCheckDescription[];

extern const char kServiceWorkerLongRunningMessageName[];
extern const char kServiceWorkerLongRunningMessageDescription[];

extern const char kSessionRestorePrioritizesBackgroundUseCasesName[];
extern const char kSessionRestorePrioritizesBackgroundUseCasesDescription[];

extern const char kSettingsWindowName[];
extern const char kSettingsWindowDescription[];

extern const char kShelfHoverPreviewsName[];
extern const char kShelfHoverPreviewsDescription[];

extern const char kShowAndroidFilesInFilesAppName[];
extern const char kShowAndroidFilesInFilesAppDescription[];

extern const char kShowAutofillSignaturesName[];
extern const char kShowAutofillSignaturesDescription[];

extern const char kShowAutofillTypePredictionsName[];
extern const char kShowAutofillTypePredictionsDescription[];

extern const char kShowOverdrawFeedbackName[];
extern const char kShowOverdrawFeedbackDescription[];

extern const char kHistoryManipulationIntervention[];
extern const char kHistoryManipulationInterventionDescription[];

extern const char kSupervisedUserCommittedInterstitialsName[];
extern const char kSupervisedUserCommittedInterstitialsDescription[];

extern const char kEnableDrawOcclusionName[];
extern const char kEnableDrawOcclusionDescription[];

extern const char kSilentDebuggerExtensionApiName[];
extern const char kSilentDebuggerExtensionApiDescription[];

extern const char kSignedHTTPExchangeName[];
extern const char kSignedHTTPExchangeDescription[];

extern const char kSimplifyHttpsIndicatorName[];
extern const char kSimplifyHttpsIndicatorDescription[];

extern const char kSiteIsolationOptOutName[];
extern const char kSiteIsolationOptOutDescription[];
extern const char kSiteIsolationOptOutChoiceDefault[];
extern const char kSiteIsolationOptOutChoiceOptOut[];

extern const char kSiteSettings[];
extern const char kSiteSettingsDescription[];

extern const char kSmoothScrollingName[];
extern const char kSmoothScrollingDescription[];

extern const char kSoleIntegrationName[];
extern const char kSoleIntegrationDescription[];

extern const char kSpeculativeServiceWorkerStartOnQueryInputName[];
extern const char kSpeculativeServiceWorkerStartOnQueryInputDescription[];

extern const char kSpellingFeedbackFieldTrialName[];
extern const char kSpellingFeedbackFieldTrialDescription[];

extern const char kSSLCommittedInterstitialsName[];
extern const char kSSLCommittedInterstitialsDescription[];

extern const char kStopInBackgroundName[];
extern const char kStopInBackgroundDescription[];

extern const char kStopNonTimersInBackgroundName[];
extern const char kStopNonTimersInBackgroundDescription[];

extern const char kSystemKeyboardLockName[];
extern const char kSystemKeyboardLockDescription[];

extern const char kSuggestionsWithSubStringMatchName[];
extern const char kSuggestionsWithSubStringMatchDescription[];

extern const char kSyncSandboxName[];
extern const char kSyncSandboxDescription[];

extern const char kSyncSupportSecondaryAccountName[];
extern const char kSyncSupportSecondaryAccountDescription[];

extern const char kSyncUSSAutofillProfileName[];
extern const char kSyncUSSAutofillProfileDescription[];

extern const char kSyncUSSAutofillWalletDataName[];
extern const char kSyncUSSAutofillWalletDataDescription[];

extern const char kSyncUSSAutofillWalletMetadataName[];
extern const char kSyncUSSAutofillWalletMetadataDescription[];

extern const char kTabGridLayoutAndroidName[];
extern const char kTabGridLayoutAndroidDescription[];

extern const char kTabGroupsAndroidName[];
extern const char kTabGroupsAndroidDescription[];

extern const char kTabGroupsName[];
extern const char kTabGroupsDescription[];

extern const char kTabHoverCardsName[];
extern const char kTabHoverCardsDescription[];

extern const char kTabHoverCardImagesName[];
extern const char kTabHoverCardImagesDescription[];

extern const char kTabsInCbdName[];
extern const char kTabsInCbdDescription[];

extern const char kTintGlCompositedContentName[];
extern const char kTintGlCompositedContentDescription[];

extern const char kTopChromeTouchUiName[];
extern const char kTopChromeTouchUiDescription[];

extern const char kThreadedScrollingName[];
extern const char kThreadedScrollingDescription[];

extern const char kTopSitesFromSiteEngagementName[];
extern const char kTopSitesFromSiteEngagementDescription[];

extern const char kTouchAdjustmentName[];
extern const char kTouchAdjustmentDescription[];

extern const char kTouchDragDropName[];
extern const char kTouchDragDropDescription[];

extern const char kTouchEventsName[];
extern const char kTouchEventsDescription[];

extern const char kTouchpadOverscrollHistoryNavigationName[];
extern const char kTouchpadOverscrollHistoryNavigationDescription[];

extern const char kTouchSelectionStrategyName[];
extern const char kTouchSelectionStrategyDescription[];
extern const char kTouchSelectionStrategyCharacter[];
extern const char kTouchSelectionStrategyDirection[];

extern const char kTraceUploadUrlName[];
extern const char kTraceUploadUrlDescription[];
extern const char kTraceUploadUrlChoiceOther[];
extern const char kTraceUploadUrlChoiceEmloading[];
extern const char kTraceUploadUrlChoiceQa[];
extern const char kTraceUploadUrlChoiceTesting[];

extern const char kTranslateForceTriggerOnEnglishName[];
extern const char kTranslateForceTriggerOnEnglishDescription[];

extern const char kTreatInsecureOriginAsSecureName[];
extern const char kTreatInsecureOriginAsSecureDescription[];

extern const char kTrySupportedChannelLayoutsName[];
extern const char kTrySupportedChannelLayoutsDescription[];

extern const char kUnifiedConsentName[];
extern const char kUnifiedConsentDescription[];

extern const char kUiPartialSwapName[];
extern const char kUiPartialSwapDescription[];

extern const char kUsePdfCompositorServiceName[];
extern const char kUsePdfCompositorServiceDescription[];

extern const char kUserActivationV2Name[];
extern const char kUserActivationV2Description[];

extern const char kUserConsentForExtensionScriptsName[];
extern const char kUserConsentForExtensionScriptsDescription[];

extern const char kUseSuggestionsEvenIfFewFeatureName[];
extern const char kUseSuggestionsEvenIfFewFeatureDescription[];

extern const char kV8CacheOptionsName[];
extern const char kV8CacheOptionsDescription[];
extern const char kV8CacheOptionsParse[];
extern const char kV8CacheOptionsCode[];

extern const char kV8VmFutureName[];
extern const char kV8VmFutureDescription[];

extern const char kV8OrinocoName[];
extern const char kV8OrinocoDescription[];

extern const char kVideoFullscreenOrientationLockName[];
extern const char kVideoFullscreenOrientationLockDescription[];

extern const char kVideoRotateToFullscreenName[];
extern const char kVideoRotateToFullscreenDescription[];

extern const char kWalletServiceUseSandboxName[];
extern const char kWalletServiceUseSandboxDescription[];

extern const char kWebglDraftExtensionsName[];
extern const char kWebglDraftExtensionsDescription[];

extern const char kWebMidiName[];
extern const char kWebMidiDescription[];

extern const char kWebrtcH264WithOpenh264FfmpegName[];
extern const char kWebrtcH264WithOpenh264FfmpegDescription[];

extern const char kWebrtcHideLocalIpsWithMdnsName[];
extern const char kWebrtcHideLocalIpsWithMdnsDecription[];

extern const char kWebrtcHybridAgcName[];
extern const char kWebrtcHybridAgcDescription[];

extern const char kWebrtcHwDecodingName[];
extern const char kWebrtcHwDecodingDescription[];

extern const char kWebrtcHwEncodingName[];
extern const char kWebrtcHwEncodingDescription[];

extern const char kWebrtcHwH264EncodingName[];
extern const char kWebrtcHwH264EncodingDescription[];

extern const char kWebrtcHwVP8EncodingName[];
extern const char kWebrtcHwVP8EncodingDescription[];

extern const char kWebrtcHwVP9EncodingName[];
extern const char kWebrtcHwVP9EncodingDescription[];

extern const char kWebrtcNewEncodeCpuLoadEstimatorName[];
extern const char kWebrtcNewEncodeCpuLoadEstimatorDescription[];

extern const char kWebRtcRemoteEventLogName[];
extern const char kWebRtcRemoteEventLogDescription[];

extern const char kWebrtcSrtpAesGcmName[];
extern const char kWebrtcSrtpAesGcmDescription[];

extern const char kWebrtcSrtpEncryptedHeadersName[];
extern const char kWebrtcSrtpEncryptedHeadersDescription[];

extern const char kWebrtcStunOriginName[];
extern const char kWebrtcStunOriginDescription[];

extern const char kWebrtcUnifiedPlanByDefaultName[];
extern const char kWebrtcUnifiedPlanByDefaultDescription[];

extern const char kWebvrName[];
extern const char kWebvrDescription[];

extern const char kWebXrName[];
extern const char kWebXrDescription[];
extern const char kWebXrGamepadSupportName[];
extern const char kWebXrGamepadSupportDescription[];

extern const char kWebXrHitTestName[];
extern const char kWebXrHitTestDescription[];

extern const char kZeroCopyName[];
extern const char kZeroCopyDescription[];

// Android --------------------------------------------------------------------

#if defined(OS_ANDROID)

extern const char kAiaFetchingName[];
extern const char kAiaFetchingDescription[];

extern const char kAccessibilityTabSwitcherName[];
extern const char kAccessibilityTabSwitcherDescription[];

extern const char kAllowRemoteContextForNotificationsName[];
extern const char kAllowRemoteContextForNotificationsDescription[];

extern const char kAndroidAutofillAccessibilityName[];
extern const char kAndroidAutofillAccessibilityDescription[];

extern const char kAndroidSurfaceControl[];
extern const char kAndroidSurfaceControlDescription[];

extern const char kAndroidWebContentsDarkMode[];
extern const char kAndroidWebContentsDarkModeDescription[];

extern const char kAppNotificationStatusMessagingName[];
extern const char kAppNotificationStatusMessagingDescription[];

extern const char kAsyncDnsName[];
extern const char kAsyncDnsDescription[];

extern const char kAutoFetchOnNetErrorPageName[];
extern const char kAutoFetchOnNetErrorPageDescription[];

extern const char kAutofillAccessoryViewName[];
extern const char kAutofillAccessoryViewDescription[];

extern const char kBackgroundLoaderForDownloadsName[];
extern const char kBackgroundLoaderForDownloadsDescription[];

extern const char kBackgroundTaskComponentUpdateName[];
extern const char kBackgroundTaskComponentUpdateDescription[];

extern const char kCCTModuleName[];
extern const char kCCTModuleDescription[];

extern const char kCCTModuleCacheName[];
extern const char kCCTModuleCacheDescription[];

extern const char kCCTModuleCustomHeaderName[];
extern const char kCCTModuleCustomHeaderDescription[];

extern const char kCCTModuleCustomRequestHeaderName[];
extern const char kCCTModuleCustomRequestHeaderDescription[];

extern const char kCCTModuleDexLoadingName[];
extern const char kCCTModuleDexLoadingDescription[];

extern const char kCCTModulePostMessageName[];
extern const char kCCTModulePostMessageDescription[];

extern const char kCCTModuleUseIntentExtrasName[];
extern const char kCCTModuleUseIntentExtrasDescription[];

extern const char kCCTTargetTranslateLanguageName[];
extern const char kCCTTargetTranslateLanguageDescription[];

extern const char kChromeDuetName[];
extern const char kChromeDuetDescription[];

extern const char kClearOldBrowsingDataName[];
extern const char kClearOldBrowsingDataDescription[];

extern const char kContentSuggestionsCategoryOrderName[];
extern const char kContentSuggestionsCategoryOrderDescription[];

extern const char kContentSuggestionsCategoryRankerName[];
extern const char kContentSuggestionsCategoryRankerDescription[];

extern const char kContentSuggestionsDebugLogName[];
extern const char kContentSuggestionsDebugLogDescription[];

extern const char kContextualSearchDefinitionsName[];
extern const char kContextualSearchDefinitionsDescription[];

extern const char kContextualSearchMlTapSuppressionName[];
extern const char kContextualSearchMlTapSuppressionDescription[];

extern const char kContextualSearchRankerQueryName[];
extern const char kContextualSearchRankerQueryDescription[];

extern const char kContextualSearchSecondTapName[];
extern const char kContextualSearchSecondTapDescription[];

extern const char kContextualSearchUnityIntegrationName[];
extern const char kContextualSearchUnityIntegrationDescription[];

extern const char kDontPrefetchLibrariesName[];
extern const char kDontPrefetchLibrariesDescription[];

extern const char kDownloadsLocationChangeName[];
extern const char kDownloadsLocationChangeDescription[];

extern const char kDownloadProgressInfoBarName[];
extern const char kDownloadProgressInfoBarDescription[];

extern const char kDownloadHomeV2Name[];
extern const char kDownloadHomeV2Description[];

extern const char kAutofillManualFallbackAndroidName[];
extern const char kAutofillManualFallbackAndroidDescription[];

extern const char kEnableAutofillRefreshStyleName[];
extern const char kEnableAutofillRefreshStyleDescription[];

extern const char kEnableAndroidSpellcheckerName[];
extern const char kEnableAndroidSpellcheckerDescription[];

extern const char kEnableCommandLineOnNonRootedName[];
extern const char kEnableCommandLineOnNoRootedDescription[];

extern const char kEnableContentSuggestionsThumbnailDominantColorName[];
extern const char kEnableContentSuggestionsThumbnailDominantColorDescription[];

extern const char kEnableCustomContextMenuName[];
extern const char kEnableCustomContextMenuDescription[];

extern const char kEnableNtpAssetDownloadSuggestionsName[];
extern const char kEnableNtpAssetDownloadSuggestionsDescription[];

extern const char kEnableNtpBookmarkSuggestionsName[];
extern const char kEnableNtpBookmarkSuggestionsDescription[];

extern const char kEnableNtpOfflinePageDownloadSuggestionsName[];
extern const char kEnableNtpOfflinePageDownloadSuggestionsDescription[];

extern const char kEnableNtpRemoteSuggestionsName[];
extern const char kEnableNtpRemoteSuggestionsDescription[];

extern const char kEnableNtpSnippetsVisibilityName[];
extern const char kEnableNtpSnippetsVisibilityDescription[];

extern const char kEnableNtpSuggestionsNotificationsName[];
extern const char kEnableNtpSuggestionsNotificationsDescription[];

extern const char kEnableOfflinePreviewsName[];
extern const char kEnableOfflinePreviewsDescription[];

extern const char kEnableOskOverscrollName[];
extern const char kEnableOskOverscrollDescription[];

extern const char kEnableWebNfcName[];
extern const char kEnableWebNfcDescription[];

extern const char kEphemeralTabName[];
extern const char kEphemeralTabDescription[];

extern const char kExploreSitesName[];
extern const char kExploreSitesDescription[];

extern const char kForegroundNotificationManagerName[];
extern const char kForegroundNotificationManagerDescription[];

extern const char kGrantNotificationsToDSEName[];
extern const char kGrantNotificationsToDSENameDescription[];

extern const char kHomePageButtonName[];
extern const char kHomePageButtonDescription[];

extern const char kHomepageTileName[];
extern const char kHomepageTileDescription[];

extern const char kIncognitoStringsName[];
extern const char kIncognitoStringsDescription[];

extern const char kInterestFeedContentSuggestionsName[];
extern const char kInterestFeedContentSuggestionsDescription[];

extern const char kKeepPrefetchedContentSuggestionsName[];
extern const char kKeepPrefetchedContentSuggestionsDescription[];

extern const char kLanguagesPreferenceName[];
extern const char kLanguagesPreferenceDescription[];

extern const char kLsdPermissionPromptName[];
extern const char kLsdPermissionPromptDescription[];

extern const char kManualPasswordGenerationAndroidName[];
extern const char kManualPasswordGenerationAndroidDescription[];

extern const char kMediaScreenCaptureName[];
extern const char kMediaScreenCaptureDescription[];

extern const char kModalPermissionDialogViewName[];
extern const char kModalPermissionDialogViewDescription[];

extern const char kNewContactsPickerName[];
extern const char kNewContactsPickerDescription[];

extern const char kNewNetErrorPageUIName[];
extern const char kNewNetErrorPageUIDescription[];

extern const char kNewPhotoPickerName[];
extern const char kNewPhotoPickerDescription[];

extern const char kNoCreditCardAbort[];
extern const char kNoCreditCardAbortDescription[];

extern const char kNtpButtonName[];
extern const char kNtpButtonDescription[];

extern const char kOfflineIndicatorAlwaysHttpProbeName[];
extern const char kOfflineIndicatorAlwaysHttpProbeDescription[];

extern const char kOfflineIndicatorChoiceName[];
extern const char kOfflineIndicatorChoiceDescription[];

extern const char kOfflinePagesCtName[];
extern const char kOfflinePagesCtDescription[];

extern const char kOfflinePagesCtV2Name[];
extern const char kOfflinePagesCtV2Description[];

extern const char kOfflinePagesCTSuppressNotificationsName[];
extern const char kOfflinePagesCTSuppressNotificationsDescription[];

extern const char kOfflinePagesDescriptiveFailStatusName[];
extern const char kOfflinePagesDescriptiveFailStatusDescription[];

extern const char kOfflinePagesDescriptivePendingStatusName[];
extern const char kOfflinePagesDescriptivePendingStatusDescription[];

extern const char kOfflinePagesInDownloadHomeOpenInCctName[];
extern const char kOfflinePagesInDownloadHomeOpenInCctDescription[];

extern const char kOfflinePagesLimitlessPrefetchingName[];
extern const char kOfflinePagesLimitlessPrefetchingDescription[];

extern const char kOfflinePagesLoadSignalCollectingName[];
extern const char kOfflinePagesLoadSignalCollectingDescription[];

extern const char kOfflinePagesPrefetchingName[];
extern const char kOfflinePagesPrefetchingDescription[];

extern const char kOfflinePagesResourceBasedSnapshotName[];
extern const char kOfflinePagesResourceBasedSnapshotDescription[];

extern const char kOfflinePagesRenovationsName[];
extern const char kOfflinePagesRenovationsDescription[];

extern const char kOfflinePagesLivePageSharingName[];
extern const char kOfflinePagesLivePageSharingDescription[];

extern const char kOfflinePagesShowAlternateDinoPageName[];
extern const char kOfflinePagesShowAlternateDinoPageDescription[];

extern const char kOfflinePagesSvelteConcurrentLoadingName[];
extern const char kOfflinePagesSvelteConcurrentLoadingDescription[];

extern const char kOffliningRecentPagesName[];
extern const char kOffliningRecentPagesDescription[];

extern const char kProgressBarThrottleName[];
extern const char kProgressBarThrottleDescription[];

extern const char kPullToRefreshEffectName[];
extern const char kPullToRefreshEffectDescription[];

extern const char kPwaImprovedSplashScreenName[];
extern const char kPwaImprovedSplashScreenDescription[];
extern const char kPwaPersistentNotificationName[];
extern const char kPwaPersistentNotificationDescription[];

extern const char kReaderModeHeuristicsName[];
extern const char kReaderModeHeuristicsDescription[];
extern const char kReaderModeHeuristicsMarkup[];
extern const char kReaderModeHeuristicsAdaboost[];
extern const char kReaderModeHeuristicsAllArticles[];
extern const char kReaderModeHeuristicsAlwaysOff[];
extern const char kReaderModeHeuristicsAlwaysOn[];

extern const char kReaderModeInCCTName[];
extern const char kReaderModeInCCTDescription[];

extern const char kSafeBrowsingTelemetryForApkDownloadsName[];
extern const char kSafeBrowsingTelemetryForApkDownloadsDescription[];

extern const char kSafeBrowsingUseLocalBlacklistsV2Name[];
extern const char kSafeBrowsingUseLocalBlacklistsV2Description[];

extern const char kSearchReadyOmniboxName[];
extern const char kSearchReadyOmniboxDescription[];

extern const char kSetMarketUrlForTestingName[];
extern const char kSetMarketUrlForTestingDescription[];

extern const char kSiteExplorationUiName[];
extern const char kSiteExplorationUiDescription[];

extern const char kSiteIsolationForPasswordSitesName[];
extern const char kSiteIsolationForPasswordSitesDescription[];

extern const char kSpannableInlineAutocompleteName[];
extern const char kSpannableInlineAutocompleteDescription[];

extern const char kStrictSiteIsolationName[];
extern const char kStrictSiteIsolationDescription[];

extern const char kTranslateAndroidManualTriggerName[];
extern const char kTranslateAndroidManualTriggerDescription[];

extern const char kUpdateMenuBadgeName[];
extern const char kUpdateMenuBadgeDescription[];

extern const char kUpdateMenuItemCustomSummaryDescription[];
extern const char kUpdateMenuItemCustomSummaryName[];

extern const char kUpdateMenuTypeName[];
extern const char kUpdateMenuTypeDescription[];
extern const char kUpdateMenuTypeNone[];
extern const char kUpdateMenuTypeUpdateAvailable[];
extern const char kUpdateMenuTypeUnsupportedOSVersion[];
extern const char kUpdateMenuTypeInlineUpdateSuccess[];
extern const char kUpdateMenuTypeInlineUpdateDialogCanceled[];
extern const char kUpdateMenuTypeInlineUpdateDialogFailed[];
extern const char kUpdateMenuTypeInlineUpdateDownloadFailed[];
extern const char kUpdateMenuTypeInlineUpdateDownloadCanceled[];
extern const char kUpdateMenuTypeInlineUpdateInstallFailed[];

extern const char kInlineUpdateFlowName[];
extern const char kInlineUpdateFlowDescription[];

#if BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

extern const char kAndroidNightModeName[];
extern const char kAndroidNightModeDescription[];

#endif  // BUILDFLAG(ENABLE_ANDROID_NIGHT_MODE)

// Non-Android ----------------------------------------------------------------

#else  // !defined(OS_ANDROID)

extern const char kAccountConsistencyName[];
extern const char kAccountConsistencyDescription[];
extern const char kAccountConsistencyChoiceMirror[];
extern const char kAccountConsistencyChoiceDice[];

extern const char kAppManagementName[];
extern const char kAppManagementDescription[];

extern const char kAutofillDropdownLayoutName[];
extern const char kAutofillDropdownLayoutDescription[];

extern const char kDoodlesOnLocalNtpName[];
extern const char kDoodlesOnLocalNtpDescription[];

extern const char kSearchSuggestionsOnLocalNtpName[];
extern const char kSearchSuggestionsOnLocalNtpDescription[];

extern const char kPromosOnLocalNtpName[];
extern const char kPromosOnLocalNtpDescription[];

extern const char kRemoveNtpFakeboxName[];
extern const char kRemoveNtpFakeboxDescription[];

extern const char kEnableWebAuthenticationBleSupportName[];
extern const char kEnableWebAuthenticationBleSupportDescription[];

extern const char kEnableWebAuthenticationTestingAPIName[];
extern const char kEnableWebAuthenticationTestingAPIDescription[];

extern const char kHappinessTrackingSurveysForDesktopName[];
extern const char kHappinessTrackingSurveysForDesktopDescription[];

extern const char kLinkManagedNoticeToChromeUIManagementURLName[];
extern const char kLinkManagedNoticeToChromeUIManagementURLDescription[];

extern const char kOmniboxDriveSuggestionsName[];
extern const char kOmniboxDriveSuggestionsDescriptions[];

extern const char kOmniboxDeduplicateDriveUrlsName[];
extern const char kOmniboxDeduplicateDriveUrlsDescription[];

extern const char kOmniboxExperimentalKeywordModeName[];
extern const char kOmniboxExperimentalKeywordModeDescription[];

extern const char kOmniboxPedalSuggestionsName[];
extern const char kOmniboxPedalSuggestionsDescription[];

extern const char kOmniboxReverseAnswersName[];
extern const char kOmniboxReverseAnswersDescription[];

extern const char kOmniboxReverseTabSwitchLogicName[];
extern const char kOmniboxReverseTabSwitchLogicDescription[];

extern const char kOmniboxTabSwitchSuggestionsName[];
extern const char kOmniboxTabSwitchSuggestionsDescription[];

extern const char kOmniboxTailSuggestionsName[];
extern const char kOmniboxTailSuggestionsDescription[];

extern const char kPageAlmostIdleName[];
extern const char kPageAlmostIdleDescription[];

extern const char kProactiveTabFreezeAndDiscardName[];
extern const char kProactiveTabFreezeAndDiscardDescription[];

extern const char kShowManagedUiName[];
extern const char kShowManagedUiDescription[];

extern const char kSiteCharacteristicsDatabaseName[];
extern const char kSiteCharacteristicsDatabaseDescription[];

extern const char kUseGoogleLocalNtpName[];
extern const char kUseGoogleLocalNtpDescription[];

#if defined(GOOGLE_CHROME_BUILD)

extern const char kGoogleBrandedContextMenuName[];
extern const char kGoogleBrandedContextMenuDescription[];

#endif  // defined(GOOGLE_CHROME_BUILD)

#endif  // defined(OS_ANDROID)

// Windows --------------------------------------------------------------------

#if defined(OS_WIN)

extern const char kCalculateNativeWinOcclusionName[];
extern const char kCalculateNativeWinOcclusionDescription[];

extern const char kCloudPrintXpsName[];
extern const char kCloudPrintXpsDescription[];

extern const char kD3D11VideoDecoderName[];
extern const char kD3D11VideoDecoderDescription[];

extern const char kDisablePostscriptPrinting[];
extern const char kDisablePostscriptPrintingDescription[];

extern const char kEnableAppcontainerName[];
extern const char kEnableAppcontainerDescription[];

extern const char kEnableGpuAppcontainerName[];
extern const char kEnableGpuAppcontainerDescription[];

extern const char kGdiTextPrinting[];
extern const char kGdiTextPrintingDescription[];

extern const char kUseAngleName[];
extern const char kUseAngleDescription[];

extern const char kUseAngleDefault[];
extern const char kUseAngleGL[];
extern const char kUseAngleD3D11[];
extern const char kUseAngleD3D9[];

extern const char kUseWinrtMidiApiName[];
extern const char kUseWinrtMidiApiDescription[];

#endif  // defined(OS_WIN)

// Mac ------------------------------------------------------------------------

#if defined(OS_MACOSX)

extern const char kContentFullscreenName[];
extern const char kContentFullscreenDescription[];

extern const char kHostedAppsInWindowsName[];
extern const char kHostedAppsInWindowsDescription[];

extern const char kCreateAppWindowsInAppShimProcessName[];
extern const char kCreateAppWindowsInAppShimProcessDescription[];

extern const char kEnableCustomMacPaperSizesName[];
extern const char kEnableCustomMacPaperSizesDescription[];

extern const char kMacTouchBarName[];
extern const char kMacTouchBarDescription[];

extern const char kMacV2GPUSandboxName[];
extern const char kMacV2GPUSandboxDescription[];

extern const char kMacViewsNativeAppWindowsName[];
extern const char kMacViewsNativeAppWindowsDescription[];

extern const char kMacViewsTaskManagerName[];
extern const char kMacViewsTaskManagerDescription[];

// Non-Mac --------------------------------------------------------------------

#else  // !defined(OS_MACOSX)

extern const char kPermissionPromptPersistenceToggleName[];
extern const char kPermissionPromptPersistenceToggleDescription[];

#endif  // defined(OS_MACOSX)

// Chrome OS ------------------------------------------------------------------

#if defined(OS_CHROMEOS)

extern const char kAcceleratedMjpegDecodeName[];
extern const char kAcceleratedMjpegDecodeDescription[];

extern const char kAllowTouchpadThreeFingerClickName[];
extern const char kAllowTouchpadThreeFingerClickDescription[];

extern const char kAppServiceAshName[];
extern const char kAppServiceAshDescription[];

extern const char kArcAvailableForChildName[];
extern const char kArcAvailableForChildDescription[];

extern const char kArcBootCompleted[];
extern const char kArcBootCompletedDescription[];

extern const char kArcCupsApiName[];
extern const char kArcCupsApiDescription[];

extern const char kArcDocumentsProviderName[];
extern const char kArcDocumentsProviderDescription[];

extern const char kArcFilePickerExperimentName[];
extern const char kArcFilePickerExperimentDescription[];

extern const char kArcGraphicBuffersVisualizationToolName[];
extern const char kArcGraphicBuffersVisualizationToolDescription[];

extern const char kArcNativeBridgeExperimentName[];
extern const char kArcNativeBridgeExperimentDescription[];

extern const char kArcUsbHostName[];
extern const char kArcUsbHostDescription[];

extern const char kArcVpnName[];
extern const char kArcVpnDescription[];

extern const char kAshEnableDisplayMoveWindowAccelsName[];
extern const char kAshEnableDisplayMoveWindowAccelsDescription[];

extern const char kAshEnableOverviewRoundedCornersName[];
extern const char kAshEnableOverviewRoundedCornersDescription[];

extern const char kAshEnablePersistentWindowBoundsName[];
extern const char kAshEnablePersistentWindowBoundsDescription[];

extern const char kAshEnablePipRoundedCornersName[];
extern const char kAshEnablePipRoundedCornersDescription[];

extern const char kAshEnableUnifiedDesktopName[];
extern const char kAshEnableUnifiedDesktopDescription[];

extern const char kAshShelfColorName[];
extern const char kAshShelfColorDescription[];

extern const char kAshShelfColorScheme[];
extern const char kAshShelfColorSchemeDescription[];
extern const char kAshShelfColorSchemeLightVibrant[];
extern const char kAshShelfColorSchemeNormalVibrant[];
extern const char kAshShelfColorSchemeDarkVibrant[];
extern const char kAshShelfColorSchemeLightMuted[];
extern const char kAshShelfColorSchemeNormalMuted[];
extern const char kAshShelfColorSchemeDarkMuted[];

extern const char kBulkPrintersName[];
extern const char kBulkPrintersDescription[];

extern const char kCaptivePortalBypassProxyName[];
extern const char kCaptivePortalBypassProxyDescription[];

extern const char kCrOSContainerName[];
extern const char kCrOSContainerDescription[];

extern const char kCrosRegionsModeName[];
extern const char kCrosRegionsModeDescription[];
extern const char kCrosRegionsModeDefault[];
extern const char kCrosRegionsModeOverride[];
extern const char kCrosRegionsModeHide[];

extern const char kCrostiniAppSearchName[];
extern const char kCrostiniAppSearchDescription[];

extern const char kCrostiniBackupName[];
extern const char kCrostiniBackupDescription[];

extern const char kCrostiniUsbSupportName[];
extern const char kCrostiniUsbSupportDescription[];

extern const char kCryptAuthV2EnrollmentName[];
extern const char kCryptAuthV2EnrollmentDescription[];

extern const char kDisableExplicitDmaFencesName[];
extern const char kDisableExplicitDmaFencesDescription[];

extern const char kDisableSystemTimezoneAutomaticDetectionName[];
extern const char kDisableSystemTimezoneAutomaticDetectionDescription[];

extern const char kDisableTabletAutohideTitlebarsName[];
extern const char kDisableTabletAutohideTitlebarsDescription[];

extern const char kDisableTabletSplitViewName[];
extern const char kDisableTabletSplitViewDescription[];

extern const char kDoubleTapToZoomInTabletModeName[];
extern const char kDoubleTapToZoomInTabletModeDescription[];

extern const char kEnableAppGridGhostName[];
extern const char kEnableAppGridGhostDescription[];

extern const char kEnableAppListSearchAutocompleteName[];
extern const char kEnableAppListSearchAutocompleteDescription[];

extern const char kEnableAppShortcutSearchName[];
extern const char kEnableAppShortcutSearchDescription[];

extern const char kEnableAppDataSearchName[];
extern const char kEnableAppDataSearchDescription[];

extern const char kEnableArcUnifiedAudioFocusName[];
extern const char kEnableArcUnifiedAudioFocusDescription[];

extern const char kEnableAssistantAppSupportName[];
extern const char kEnableAssistantAppSupportDescription[];

extern const char kEnableAssistantLauncherIntegrationName[];
extern const char kEnableAssistantLauncherIntegrationDescription[];

extern const char kEnableAssistantVoiceMatchName[];
extern const char kEnableAssistantVoiceMatchDescription[];

extern const char kEnableChromeOsAccountManagerName[];
extern const char kEnableChromeOsAccountManagerDescription[];

extern const char kEnableDiscoverAppName[];
extern const char kEnableDiscoverAppDescription[];

extern const char kEnableDragTabsInTabletModeName[];
extern const char kEnableDragTabsInTabletModeDescription[];

extern const char kEnableDriveFsName[];
extern const char kEnableDriveFsDescription[];

extern const char kEnableEncryptionMigrationName[];
extern const char kEnableEncryptionMigrationDescription[];

extern const char kEnableFullscreenHandwritingVirtualKeyboardName[];
extern const char kEnableFullscreenHandwritingVirtualKeyboardDescription[];

extern const char kEnableGoogleAssistantName[];
extern const char kEnableGoogleAssistantDescription[];

extern const char kEnableGoogleAssistantDspName[];
extern const char kEnableGoogleAssistantDspDescription[];

extern const char kEnableGoogleAssistantStereoInputName[];
extern const char kEnableGoogleAssistantStereoInputDescription[];

extern const char kEnableHomeLauncherName[];
extern const char kEnableHomeLauncherDescription[];

extern const char kEnableMyFilesVolumeName[];
extern const char kEnableMyFilesVolumeDescription[];

extern const char kEnableOobeRecommendAppsScreenName[];
extern const char kEnableOobeRecommendAppsScreenDescription[];

extern const char kEnablePerUserTimezoneName[];
extern const char kEnablePerUserTimezoneDescription[];

extern const char kEnablePlayStoreSearchName[];
extern const char kEnablePlayStoreSearchDescription[];

extern const char kEnableStylusVirtualKeyboardName[];
extern const char kEnableStylusVirtualKeyboardDescription[];

extern const char kEnableVideoPlayerNativeControlsName[];
extern const char kEnableVideoPlayerNativeControlsDescription[];

extern const char kEnableVirtualKeyboardUkmName[];
extern const char kEnableVirtualKeyboardUkmDescription[];

extern const char kEnableZeroStateSuggestionsName[];
extern const char kEnableZeroStateSuggestionsDescription[];

extern const char kEolNotificationName[];
extern const char kEolNotificationDescription[];

extern const char kExperimentalAccessibilityChromeVoxLanguageSwitchingName[];
extern const char
    kExperimentalAccessibilityChromeVoxLanguageSwitchingDescription[];

extern const char kFirstRunUiTransitionsName[];
extern const char kFirstRunUiTransitionsDescription[];

extern const char kForceEnableStylusToolsName[];
extern const char kForceEnableStylusToolsDescription[];

extern const char kFsNosymfollowName[];
extern const char kFsNosymfollowDescription[];

extern const char kGestureTypingName[];
extern const char kGestureTypingDescription[];

extern const char kImeServiceName[];
extern const char kImeServiceDescription[];

extern const char kListAllDisplayModesName[];
extern const char kListAllDisplayModesDescription[];

extern const char kLockScreenNotificationName[];
extern const char kLockScreenNotificationDescription[];

extern const char kMashOopVizName[];
extern const char kMashOopVizDescription[];

extern const char kMaterialDesignInkDropAnimationSpeedName[];
extern const char kMaterialDesignInkDropAnimationSpeedDescription[];
extern const char kMaterialDesignInkDropAnimationFast[];
extern const char kMaterialDesignInkDropAnimationSlow[];

extern const char kMemoryPressureThresholdName[];
extern const char kMemoryPressureThresholdDescription[];
extern const char kConservativeThresholds[];
extern const char kAggressiveCacheDiscardThresholds[];
extern const char kAggressiveTabDiscardThresholds[];
extern const char kAggressiveThresholds[];

extern const char kMtpWriteSupportName[];
extern const char kMtpWriteSupportDescription[];

extern const char kNetworkPortalNotificationName[];
extern const char kNetworkPortalNotificationDescription[];

extern const char kNewZipUnpackerName[];
extern const char kNewZipUnpackerDescription[];

extern const char kOfficeEditingComponentAppName[];
extern const char kOfficeEditingComponentAppDescription[];

extern const char kPhysicalKeyboardAutocorrectName[];
extern const char kPhysicalKeyboardAutocorrectDescription[];

extern const char kPrinterProviderSearchAppName[];
extern const char kPrinterProviderSearchAppDescription[];

extern const char kSchedulerConfigurationName[];
extern const char kSchedulerConfigurationDescription[];
extern const char kSchedulerConfigurationConservative[];
extern const char kSchedulerConfigurationPerformance[];

extern const char kShillSandboxingName[];
extern const char kShillSandboxingDescription[];

extern const char kShowTapsName[];
extern const char kShowTapsDescription[];

extern const char kShowTouchHudName[];
extern const char kShowTouchHudDescription[];

extern const char kSingleProcessMashName[];
extern const char kSingleProcessMashDescription[];

extern const char kSmartTextSelectionName[];
extern const char kSmartTextSelectionDescription[];

extern const char kTetherName[];
extern const char kTetherDescription[];

extern const char kTouchscreenCalibrationName[];
extern const char kTouchscreenCalibrationDescription[];

extern const char kUiDevToolsName[];
extern const char kUiDevToolsDescription[];

extern const char kUiModeName[];
extern const char kUiModeDescription[];
extern const char kUiModeTablet[];
extern const char kUiModeClamshell[];
extern const char kUiModeAuto[];

extern const char kUiShowCompositedLayerBordersName[];
extern const char kUiShowCompositedLayerBordersDescription[];
extern const char kUiShowCompositedLayerBordersRenderPass[];
extern const char kUiShowCompositedLayerBordersSurface[];
extern const char kUiShowCompositedLayerBordersLayer[];
extern const char kUiShowCompositedLayerBordersAll[];

extern const char kUiSlowAnimationsName[];
extern const char kUiSlowAnimationsDescription[];

extern const char kUnfilteredBluetoothDevicesName[];
extern const char kUnfilteredBluetoothDevicesDescription[];

extern const char kUsbguardName[];
extern const char kUsbguardDescription[];

extern const char kUseMashName[];
extern const char kUseMashDescription[];

extern const char kUseMonitorColorSpaceName[];
extern const char kUseMonitorColorSpaceDescription[];

extern const char kUserActivityPredictionMlServiceName[];
extern const char kUserActivityPredictionMlServiceDescription[];

extern const char kVaapiJpegImageDecodeAccelerationName[];
extern const char kVaapiJpegImageDecodeAccelerationDescription[];

extern const char kVideoPlayerChromecastSupportName[];
extern const char kVideoPlayerChromecastSupportDescription[];

extern const char kVirtualKeyboardName[];
extern const char kVirtualKeyboardDescription[];

extern const char kVirtualKeyboardOverscrollName[];
extern const char kVirtualKeyboardOverscrollDescription[];

extern const char kVoiceInputName[];
extern const char kVoiceInputDescription[];

extern const char kWakeOnPacketsName[];
extern const char kWakeOnPacketsDescription[];

extern const char kEnableAppReinstallZeroStateName[];
extern const char kEnableAppReinstallZeroStateDescription[];

extern const char kAshNotificationStackingBarRedesignName[];
extern const char kAshNotificationStackingBarRedesignDescription[];

#endif  // #if defined(OS_CHROMEOS)

// Random platform combinations -----------------------------------------------

#if defined(OS_WIN) || defined(OS_LINUX)

extern const char kEnableInputImeApiName[];
extern const char kEnableInputImeApiDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

extern const char kWebGL2ComputeContextName[];
extern const char kWebGL2ComputeContextDescription[];

#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_CHROMEOS)

extern const char kExperimentalUiName[];
extern const char kExperimentalUiDescription[];

#if defined(OS_WIN) || defined(OS_MACOSX)

extern const char kAutomaticTabDiscardingName[];
extern const char kAutomaticTabDiscardingDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

extern const char kDirectManipulationStylusName[];
extern const char kDirectManipulationStylusDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)

#if defined(OS_MACOSX) || defined(OS_CHROMEOS)

extern const char kForceEnableSystemAecName[];
extern const char kForceEnableSystemAecDescription[];

#endif  // defined(OS_MACOSX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

extern const char kWebContentsOcclusionName[];
extern const char kWebContentsOcclusionDescription[];

#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)

// Feature flags --------------------------------------------------------------

#if defined(DCHECK_IS_CONFIGURABLE)
extern const char kDcheckIsFatalName[];
extern const char kDcheckIsFatalDescription[];
#endif  // defined(DCHECK_IS_CONFIGURABLE)

#if BUILDFLAG(ENABLE_VR)

extern const char kWebXrOrientationSensorDeviceName[];
extern const char kWebXrOrientationSensorDeviceDescription[];

#if BUILDFLAG(ENABLE_OCULUS_VR)
extern const char kOculusVRName[];
extern const char kOculusVRDescription[];
#endif  // ENABLE_OCULUS_VR

#if BUILDFLAG(ENABLE_OPENVR)
extern const char kOpenVRName[];
extern const char kOpenVRDescription[];
#endif  // ENABLE_OPENVR

#if BUILDFLAG(ENABLE_WINDOWS_MR)
extern const char kWindowsMixedRealityName[];
extern const char kWindowsMixedRealityDescription[];
#endif  // ENABLE_WINDOWS_MR

#if BUILDFLAG(ENABLE_ISOLATED_XR_SERVICE)
extern const char kXRSandboxName[];
extern const char kXRSandboxDescription[];
#endif  // ENABLE_ISOLATED_XR_SERVICE

#endif  // ENABLE_VR

#if BUILDFLAG(ENABLE_PLUGINS)

#if defined(OS_CHROMEOS)

extern const char kPdfAnnotations[];
extern const char kPdfAnnotationsDescription[];

#endif  // defined(OS_CHROMEOS)

extern const char kPdfFormSaveName[];
extern const char kPdfFormSaveDescription[];

extern const char kPdfIsolationName[];
extern const char kPdfIsolationDescription[];

#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

extern const char kAutofillCreditCardUploadName[];
extern const char kAutofillCreditCardUploadDescription[];

#endif  // defined(TOOLKIT_VIEWS) || defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)

extern const char kReopenTabInProductHelpName[];
extern const char kReopenTabInProductHelpDescription[];

#endif  // BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
extern const char kAvoidFlashBetweenNavigationName[];
extern const char kAvoidFlahsBetweenNavigationDescription[];

#if defined(WEBRTC_USE_PIPEWIRE)

extern const char kWebrtcPipeWireCapturerName[];
extern const char kWebrtcPipeWireCapturerDescription[];

#endif  // #if defined(WEBRTC_USE_PIPEWIRE)

// ============================================================================
// Don't just add flags to the end, put them in the right section in
// alphabetical order. See top instructions for more.
// ============================================================================

}  // namespace flag_descriptions

#endif  // CHROME_BROWSER_FLAG_DESCRIPTIONS_H_
