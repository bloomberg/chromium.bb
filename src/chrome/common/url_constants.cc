// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/url_constants.h"

#include "build/branding_buildflags.h"
#include "chrome/common/webui_url_constants.h"

#include "build/chromeos_buildflags.h"

namespace chrome {

const char kAccessibilityLabelsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=image_descriptions";

const char kAutomaticSettingsResetLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_automatic_settings_reset";

const char kAdvancedProtectionDownloadLearnMoreURL[] =
    "https://support.google.com/accounts/accounts?p=safe-browsing";

const char kBluetoothAdapterOffHelpURL[] =
    "https://support.google.com/chrome?p=bluetooth";

const char kCastCloudServicesHelpURL[] =
    "https://support.google.com/chromecast/?p=casting_cloud_services";

const char kCastNoDestinationFoundURL[] =
    "https://support.google.com/chromecast/?p=no_cast_destination";

const char kChooserBluetoothOverviewURL[] =
    "https://support.google.com/chrome?p=bluetooth";

const char kChooserHidOverviewUrl[] =
    "https://support.google.com/chrome?p=webhid";

const char kChooserSerialOverviewUrl[] =
    "https://support.google.com/chrome?p=webserial";

const char kChooserUsbOverviewURL[] =
    "https://support.google.com/chrome?p=webusb";

const char kChromeBetaForumURL[] =
    "https://support.google.com/chrome/?p=beta_forum";

const char kChromeFixUpdateProblems[] =
    "https://support.google.com/chrome?p=fix_chrome_updates";

const char kChromeHelpViaKeyboardURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=keyboard";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.google.com/chrome/?p=help&ctx=keyboard";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kChromeHelpViaMenuURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=menu";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#else
    "https://support.google.com/chrome/?p=help&ctx=menu";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kChromeHelpViaWebUIURL[] =
    "https://support.google.com/chrome/?p=help&ctx=settings";
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kChromeOsHelpViaWebUIURL[] =
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome-extension://honijodknafkokifofgiaalefdiedpko/main.html";
#else
    "https://support.google.com/chromebook/?p=help&ctx=settings";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

const char kChromeNativeScheme[] = "chrome-native";

const char kChromeSearchLocalNtpHost[] = "local-ntp";
const char kChromeSearchLocalNtpUrl[] =
    "chrome-search://local-ntp/local-ntp.html";

const char kChromeSearchMostVisitedHost[] = "most-visited";
const char kChromeSearchMostVisitedUrl[] = "chrome-search://most-visited/";

const char kChromeSearchLocalNtpBackgroundUrl[] =
    "chrome-search://local-ntp/background.jpg";
const char kChromeSearchLocalNtpBackgroundFilename[] = "background.jpg";

const char kChromeSearchRemoteNtpHost[] = "remote-ntp";

const char kChromeSearchScheme[] = "chrome-search";

const char kChromeUIUntrustedNewTabPageUrl[] =
    "chrome-untrusted://new-tab-page/";

const char kChromiumProjectURL[] = "https://www.chromium.org/";

const char kCloudPrintCertificateErrorLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook?p=cloudprint_error_troubleshoot";
#elif defined(OS_MAC)
    "https://support.google.com/cloudprint?p=cloudprint_error_offline_mac";
#elif defined(OS_WIN)
        "https://support.google.com/"
        "cloudprint?p=cloudprint_error_offline_windows";
#else
        "https://support.google.com/"
        "cloudprint?p=cloudprint_error_offline_linux";
#endif

const char kContentSettingsExceptionsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_manage_exceptions";

const char kCookiesSettingsHelpCenterURL[] =
    "https://support.google.com/chrome?p=cpn_cookies";

const char kCpuX86Sse2ObsoleteURL[] =
    "https://support.google.com/chrome/?p=chrome_update_sse3";

const char kCrashReasonURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=e_awsnap";
#else
    "https://support.google.com/chrome/?p=e_awsnap";
#endif

const char kCrashReasonFeedbackDisplayedURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=e_awsnap_rl";
#else
    "https://support.google.com/chrome/?p=e_awsnap_rl";
#endif

const char kDoNotTrackLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_do_not_track";
#else
    "https://support.google.com/chrome/?p=settings_do_not_track";
#endif

const char kDownloadInterruptedLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_download_errors";

const char kDownloadScanningLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_download_blocked";

const char kExtensionControlledSettingLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_settings_api_extension";

const char kExtensionInvalidRequestURL[] = "chrome-extension://invalid/";

const char kFlashDeprecationLearnMoreURL[] =
    "https://blog.chromium.org/2017/07/so-long-and-thanks-for-all-flash.html";

const char kGoogleAccountActivityControlsURL[] =
    "https://myaccount.google.com/activitycontrols/search";

const char kGoogleAccountURL[] = "https://myaccount.google.com";

const char kGoogleAccountChooserURL[] =
    "https://accounts.google.com/AccountChooser";

const char kGooglePasswordManagerURL[] = "https://passwords.google.com";

const char kGooglePhotosURL[] = "https://photos.google.com";

const char kLearnMoreReportingURL[] =
    "https://support.google.com/chrome/?p=ui_usagestat";

const char kManagedUiLearnMoreUrl[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=is_chrome_managed";
#else
    "https://support.google.com/chrome/?p=is_chrome_managed";
#endif

const char kMixedContentDownloadBlockingLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=mixed_content_downloads";

const char kMyActivityUrlInClearBrowsingData[] =
    "https://myactivity.google.com/myactivity/?utm_source=chrome_cbd";

const char kOmniboxLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_omnibox";
#else
    "https://support.google.com/chrome/?p=settings_omnibox";
#endif

const char kPageInfoHelpCenterURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=ui_security_indicator";
#else
    "https://support.google.com/chrome/?p=ui_security_indicator";
#endif

const char kPasswordCheckLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/"
    "?p=settings_password#leak_detection_privacy";
#else
    "https://support.google.com/chrome/"
    "?p=settings_password#leak_detection_privacy";
#endif

const char kPasswordGenerationLearnMoreURL[] =
    "https://support.google.com/chrome/answer/7570435";

const char kPasswordManagerLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_password";
#else
    "https://support.google.com/chrome/?p=settings_password";
#endif

const char kPaymentMethodsURL[] =
    "https://pay.google.com/payments/"
    "home?utm_source=chrome&utm_medium=settings&utm_campaign=chrome-payment#"
    "paymentMethods";

const char kPaymentMethodsLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/answer/"
    "142893?visit_id=636857416902558798-696405304&p=settings_autofill&rd=1";
#else
    "https://support.google.com/chrome/answer/"
    "142893?visit_id=636857416902558798-696405304&p=settings_autofill&rd=1";
#endif

const char kPrivacyLearnMoreURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_privacy";
#else
    "https://support.google.com/chrome/?p=settings_privacy";
#endif

const char kRemoveNonCWSExtensionURL[] =
    "https://support.google.com/chrome/?p=ui_remove_non_cws_extensions";

const char kResetProfileSettingsLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ui_reset_settings";

const char kSafeBrowsingHelpCenterURL[] =
    "https://support.google.com/chrome?p=cpn_safe_browsing";

const char kSafetyTipHelpCenterURL[] =
    "https://support.google.com/chrome/?p=safety_tip";

const char kSeeMoreSecurityTipsURL[] =
    "https://support.google.com/accounts/answer/32040";

const char kSettingsSearchHelpURL[] =
    "https://support.google.com/chrome/?p=settings_search_help";

const char kSyncAndGoogleServicesLearnMoreURL[] =
    "https://support.google.com/chrome?p=syncgoogleservices";

const char kSyncEncryptionHelpURL[] =
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=settings_encryption";
#else
    "https://support.google.com/chrome/?p=settings_encryption";
#endif

const char kSyncErrorsHelpURL[] =
    "https://support.google.com/chrome/?p=settings_sync_error";

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync/";

const char kSyncLearnMoreURL[] =
    "https://support.google.com/chrome/?p=settings_sign_in";

const char kUpgradeHelpCenterBaseURL[] =
    "https://support.google.com/installer/?product="
    "{8A69D345-D564-463c-AFF1-A69D9E530F96}&error=";

const char kWhoIsMyAdministratorHelpURL[] =
    "https://support.google.com/chrome?p=your_administrator";

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_ANDROID)
const char kEnhancedPlaybackNotificationLearnMoreURL[] =
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    "https://support.google.com/chromebook/?p=enhanced_playback";
#elif defined(OS_ANDROID)
// Keep in sync with chrome/browser/ui/android/strings/android_chrome_strings.grd
    "https://support.google.com/chrome/?p=mobile_protected_content";
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
const char kAccountManagerLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=google_accounts";

const char kAccountRecoveryURL[] =
    "https://accounts.google.com/signin/recovery";

const char kAndroidAppsLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=playapps";

const char kArcAdbSideloadingLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=develop_android_apps";

const char kArcExternalStorageLearnMoreURL[] =
    "https://support.google.com/chromebook?p=open_files";

const char kArcPrivacyPolicyURLPath[] = "arc/privacy_policy";

const char kArcTermsURLPath[] = "arc/terms";

const char kChromeAccessibilityHelpURL[] =
    "https://support.google.com/chromebook/topic/6323347";

const char kChromeOSAssetHost[] = "chromeos-asset";
const char kChromeOSAssetPath[] = "/usr/share/chromeos-assets/";

const char kChromeOSCreditsPath[] =
    "/opt/google/chrome/resources/about_os_credits.html";

// TODO(carpenterr): Have a solution for plink mapping in Help App.
// The magic numbers in this url are the topic and article ids currently
// required to navigate directly to a help article in the Help App.
const char kChromeOSGestureEducationHelpURL[] =
    "chrome://help-app/help/sub/3399710/id/9739838";

const char kChromePaletteHelpURL[] =
    "https://support.google.com/chromebook?p=stylus_help";

const char kCrosScheme[] = "cros";

const char kCupsPrintLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_printing";

const char kCupsPrintPPDLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=printing_advancedconfigurations";

const char kEasyUnlockLearnMoreUrl[] =
    "https://support.google.com/chromebook/?p=smart_lock";

const char kEchoLearnMoreURL[] =
    "chrome://help-app/help/sub/3399709/id/2703646";

const char kArcTermsPathFormat[] = "arc_tos/%s/terms.html";

const char kArcPrivacyPolicyPathFormat[] = "arc_tos/%s/privacy_policy.pdf";

const char kEolNotificationURL[] = "https://www.google.com/chromebook/older/";

const char kAutoUpdatePolicyURL[] =
    "http://support.google.com/chrome/a?p=auto-update-policy";

const char kGoogleNameserversLearnMoreURL[] =
    "https://developers.google.com/speed/public-dns";

const char kGsuiteTermsEducationPrivacyURL[] =
    "https://gsuite.google.com/terms/education_privacy.html";

const char kInstantTetheringLearnMoreURL[] =
    "https://support.google.com/chromebook?p=instant_tethering";

const char kKerberosAccountsLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=kerberos_accounts";

const char kMultiDeviceLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multi_device";

const char kAndroidMessagesLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=multi_device_messages";

const char kLanguageSettingsLearnMoreUrl[] =
    "https://support.google.com/chromebook/answer/1059490";

const char kLearnMoreEnterpriseURL[] =
    "https://support.google.com/chromebook/?p=managed";

const char kLinuxAppsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_linuxapps";

const char kLinuxExportImportHelpURL[] =
    "https://support.google.com/chromebook?p=linux_backup_restore";

const char kNaturalScrollHelpURL[] =
    "https://support.google.com/chromebook/?p=simple_scrolling";

const char kOemEulaURLPath[] = "oem";

const char kOnlineEulaURLPath[] =
    "https://policies.google.com/terms/embedded?hl=%s";

const char kAdditionalToSOnlineURLPath[] =
    "https://www.google.com/intl/%s/chrome/terms/";

const char kOsSettingsSearchHelpURL[] =
    "https://support.google.com/chromebook/?p=settings_search_help";

const char kPeripheralDataAccessHelpURL[] =
    "https://support.google.com/chromebook?p=connect_thblt_usb4_accy";

const char kTPMFirmwareUpdateLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=tpm_update";

const char kTimeZoneSettingsLearnMoreURL[] =
    "https://support.google.com/chromebook?p=chromebook_timezone&hl=%s";

const char kSmbSharesLearnMoreURL[] =
    "https://support.google.com/chromebook?p=network_file_shares";

const char kSuggestedContentLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=explorecontent";

const char kTabletModeGesturesLearnMoreURL[] =
    "https://support.google.com/chromebook?p=tablet_mode_gestures";

const char kWifiSyncLearnMoreURL[] =
    "https://support.google.com/chromebook/?p=wifisync";

const char kNearbyShareLearnMoreURL[] =
    "https://support.google.com/chromebook?p=nearby_share";

extern const char kNearbyShareManageContactsURL[] =
    "https://contacts.google.com";

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if defined(OS_MAC)
const char kChromeEnterpriseSignInLearnMoreURL[] =
    "https://support.google.com/chromebook/answer/1331549";

const char kMac10_10_ObsoleteURL[] =
    "https://support.google.com/chrome/?p=unsupported_mac";
#endif

#if defined(OS_WIN)
const char kChromeCleanerLearnMoreURL[] =
    "https://support.google.com/chrome/?p=chrome_cleanup_tool";

const char kWindowsXPVistaDeprecationURL[] =
    "https://chrome.blogspot.com/2015/11/updates-to-chrome-platform-support.html";
#endif

#if BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)
const char kChromeSyncLearnMoreURL[] =
    "https://support.google.com/chrome/answer/165139";
#endif  // BUILDFLAG(ENABLE_ONE_CLICK_SIGNIN)

#if BUILDFLAG(ENABLE_PLUGINS)
const char kOutdatedPluginLearnMoreURL[] =
    "https://support.google.com/chrome/?p=ib_outdated_plugin";
#endif

}  // namespace chrome
