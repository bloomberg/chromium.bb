// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the chrome
// module.

#ifndef CHROME_COMMON_CHROME_FEATURES_H_
#define CHROME_COMMON_CHROME_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "ppapi/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#include "ui/base/buildflags.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kActivityReportingSessionType;
#endif  // defined(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAdaptiveScreenBrightnessLogging;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAllowDisableMouseAcceleration;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAllowDisableTouchpadHapticFeedback;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAllowTouchpadHapticClickSettings;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAlwaysReinstallSystemWebApps;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAnonymousUpdateChecks;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kApkWebAppInstalls;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppDiscoveryForOobe;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppManagementIntentSettings;
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceLoadIconWithoutMojom;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppServiceExtension;
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppShimRemoteCocoa;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAppShimNewCloseBehavior;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kArcPiGhostWindow;
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kAsyncDns;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAutofillAddressSurvey;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAutofillCardSurvey;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAutofillPasswordSurvey;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBackgroundModeAllowRestart;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kBorealis;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kBrowserAppInstanceTracking;
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kChangePictureVideoMode;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kChromeAppsDeprecation;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kClientStorageAccessContextAuditing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kConsolidatedSiteStorageControls;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kContinuousSearch;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrOSEnableUSMUserService;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrosCompUpdates;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kCrostini;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAdditionalEnterpriseReporting;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAdvancedAccessControls;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAnsibleInfrastructure;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniAnsibleSoftwareManagement;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniArcSideload;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCrostiniForceClose;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCryptohomeDistributedModel;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCryptohomeUserDataAuth;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kCryptohomeUserDataAuthKillswitch;
#endif

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDataLeakPreventionPolicy;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDataLeakPreventionFilesRestriction;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDefaultLinkCapturingInBrowser;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDMServerOAuthForChildUser;
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPreinstalledWebAppInstallation;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsAppIconShortcutsMenuUI;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsAdditionalWindowingControls;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsCacheDuringDefaultInstall;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsDefaultOfflinePage;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsElidedExtensionsMenu;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsFlashAppNameInsteadOfOrigin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsNotificationIconAndTitle;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsRunOnOsLogin;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsTabStripLinkCapturing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsTabStripSettings;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDesktopPWAsWebBundles;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDisableHttpDiskCache;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kDisableHttpDiskCacheMemoryCacheSizeParam;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDnsOverHttps;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kDnsOverHttpsFallbackParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kDnsOverHttpsShowUiParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kDnsOverHttpsTemplatesParam;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kDnsOverHttpsDisabledProvidersParam;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kDnsProxyEnableDOH;
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEarlyLibraryLoad;
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kElidePrioritizationOfPreNativeBootstrapTasks;
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kElideTabPreloadAtStartup;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAllSystemWebApps;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAmbientAuthenticationInGuestSession;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableAmbientAuthenticationInIncognito;

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableIncognitoShortcutOnDesktop;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableRestrictedWebApis;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnableWebAppUninstallFromOsSettings;

#if !defined(OS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kExtensionDeferredIndividualSettings;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kExtensionWorkflowJustification;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnterpriseReportingExtensionManifestVersion;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kEnterpriseReportingInChromeOS;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kExternalExtensionDefaultButtonControl;

#if BUILDFLAG(ENABLE_PLUGINS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFlashDeprecationWarning;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kFocusMode;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kGeoLanguage;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature
    kGiveJavaUiThreadDefaultTaskTraitsUserBlockingPriority;
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopDemo;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopPrivacySandbox;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacySandboxTime;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopSettings;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopSettingsPrivacy;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoSandbox;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyNoReview;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopSettingsPrivacyTime;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopPrivacyReview;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopPrivacyReviewTime;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopNtpModules;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSurveysForDesktopWhatsNew;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kHappinessTrackingSurveysForDesktopWhatsNewTime;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHaTSDesktopDevToolsIssuesCOEP;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHaTSDesktopDevToolsIssuesMixedContent;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature
    kHappinessTrackingSurveysForDesktopDevToolsIssuesCookiesSameSite;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHaTSDesktopDevToolsIssuesHeavyAd;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHaTSDesktopDevToolsIssuesCSP;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystem;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemEnt;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemStability;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemPerformance;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemOnboarding;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemUnlock;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemSmartLock;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemArcGames;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHappinessTrackingSystemAudio;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHideWebAppOriginText;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kHttpsOnlyMode;

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kImmersiveFullscreen;
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kImproveAccessibilityTreeUsingLocalML;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kInSessionPasswordChange;
#endif

#if BUILDFLAG(IS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncompatibleApplicationsWarning;
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncognitoBrandConsistencyForAndroid;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncognitoNtpRevamp;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncognitoBrandConsistencyForDesktop;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIncognitoClearBrowsingDataDialogForDesktop;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUpdateHistoryEntryPointsInIncognito;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kInvalidatorUniqueOwnerName;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kIPHInWebUIDemo;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kKernelnextVMs;
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLinkCapturingUiUpdate;
#endif

#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kLinuxLowMemoryMonitor;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kLinuxLowMemoryMonitorModerateLevel;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int> kLinuxLowMemoryMonitorCriticalLevel;
#endif  // BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kListWebAppsSwitch;
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacFullSizeContentView;
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacMaterialDesignDownloadShelf;
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMacSystemScreenCapturePermissionCheck;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMeteredShowToggle;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShowHiddenNetworkToggle;
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMetricsSettingsAndroid;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kMoveWebApp;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kMoveWebAppUninstallStartUrlPrefix;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kMoveWebAppUninstallStartUrlPattern;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string> kMoveWebAppInstallStartUrl;

#if BUILDFLAG(ENABLE_SYSTEM_NOTIFICATIONS)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNativeNotifications;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSystemNotifications;
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNewMacNotificationAPI;
#endif

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNoReferrers;

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kNotificationDurationLongForRequireInteraction;
#endif

#if BUILDFLAG(IS_POSIX)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kNtlmV2Enabled;
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOnConnectNative;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOobeMarketingAdditionalCountriesSupported;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOobeMarketingDoubleOptInCountriesSupported;

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kOomIntervention;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kParentAccessCodeForOnlineLogin;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPermissionAuditing;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPermissionPredictions;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double> kPermissionPredictionsHoldbackChance;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPermissionGeolocationPredictions;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kPermissionGeolocationPredictionsHoldbackChance;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPluginVm;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPredictivePrefetchingAllowedOnAllConnectionTypes;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrefixWebAppWindowsWithAppName;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrerenderFallbackToPreconnect;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrivacyAdvisor;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrivacyReview;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPrivacySandboxSettings3;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ForceShowConsent;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kPrivacySandboxSettings3ForceShowNotice;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kPushMessagingBackgroundMode;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kQuietNotificationPrompts;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kRecordWebAppDebugInfo;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kAbusiveNotificationPermissionRevocation;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kRemoveStatusBarInWebApps;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kRemoveSupervisedUsersOnStartup;
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kRequestDesktopSiteForTablets;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSchedulerConfiguration;
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kScrollCapture;
#endif  // BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSCTAuditing;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double> kSCTAuditingSamplingRate;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSearchHistoryLink;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSecurityKeyAttestationPrompt;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kChromeOSSharingHub;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSharesheetCopyToClipboard;
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShareUsageRanking;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShareUsageRankingFixedMore;
#endif

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kShow10_10ObsoleteInfobar;
#endif  // BUILDFLAG(IS_MAC)

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSitePerProcess;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kSmartDim;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSoundContentSetting;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSupportTool;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kSysInternals;

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTPMFirmwareUpdate;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kTabMetricsLogging;
#endif

#if BUILDFLAG(IS_WIN)
// Only has an effect in branded builds.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kThirdPartyModulesBlocking;
#endif

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kTreatUnsafeDownloadsAsActive;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kTrustSafetySentimentSurvey;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMinTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyMaxTimeToPrompt;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyNtpVisitsMinRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<int>
    kTrustSafetySentimentSurveyNtpVisitsMaxRange;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyPrivacySettingsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTrustedSurfaceProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<double>
    kTrustSafetySentimentSurveyTransactionsProbability;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyPrivacySettingsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTrustedSurfaceTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<std::string>
    kTrustSafetySentimentSurveyTransactionsTriggerId;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyPrivacySettingsTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTrustedSurfaceTime;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<base::TimeDelta>
    kTrustSafetySentimentSurveyTransactionsPasswordManagerTime;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUploadZippedSystemLogs;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUserActivityEventLogging;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUserTypeByDeviceTypeMetricsProvider;
#endif

// Android expects this string from Java code, so it is always needed.
// TODO(crbug.com/731802): Use #if BUILDFLAG(ENABLE_VR_BROWSING) instead.
#if BUILDFLAG(ENABLE_VR) || BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kVrBrowsing;
#endif
#if BUILDFLAG(ENABLE_VR)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalFeatures;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kVrBrowsingExperimentalRendering;
#endif  // ENABLE_VR

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebAppManifestIconUpdating;
#endif  // !BUILDFLAG(IS_ANDROID)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebAppManifestPolicyAppIdentityUpdate;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebAppsCrosapi;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebKioskEnableLacros;
#endif

#if !BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLog;
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebRtcRemoteEventLogGzipped;
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWebShare;
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWebTimeLimits;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWebUIDarkMode;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kUmaStorageDimensions;
COMPONENT_EXPORT(CHROME_FEATURES) extern const base::Feature kWilcoDtc;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWin10AcceleratedDefaultBrowserFlow;
#endif  // BUILDFLAG(IS_WIN)

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kWriteBasicSystemProfileToPersistentHistogramsFile;

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(CHROME_FEATURES)
bool IsParentAccessCodeForOnlineLoginEnabled();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// This flag is used for enabling Omnibox triggered prerendering and
// blink::WebRuntimeFeatures::Prerender2RelatedFeatures that enables Prerender2
// related web exposed features. This flag takes effect only when
// blink::features::Prerender2 is enabled. See crbug.com/1166085 for more
// details of Omnibox triggered prerendering.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOmniboxTriggerForPrerender2;

// This param controls whether to trigger prerendering when the default search
// engine suggests to prerender a search result. This parameter works iff
// kOmniboxTriggerForPrerender2 and blink::features::Prerender2 is enabled.
COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::FeatureParam<bool> kSupportSearchSuggestionForPrerender2;

COMPONENT_EXPORT(CHROME_FEATURES)
extern const base::Feature kOmniboxTriggerForNoStatePrefetch;

bool PrefServiceEnabled();

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CHROME_COMMON_CHROME_FEATURES_H_
