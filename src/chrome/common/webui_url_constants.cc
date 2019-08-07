// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"

#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "components/nacl/common/buildflags.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "extensions/buildflags/buildflags.h"

namespace chrome {

// Please keep this file in the same order as the header.

// Note: Add hosts to |kChromeHostURLs| at the bottom of this file to be listed by
// chrome://chrome-urls (about:about) and the built-in AutocompleteProvider.

const char kChromeUIAboutHost[] = "about";
const char kChromeUIAboutURL[] = "chrome://about/";
const char kChromeUIAccessibilityHost[] = "accessibility";
const char kChromeUIAppIconHost[] = "app-icon";
const char kChromeUIAppIconURL[] = "chrome://app-icon/";
const char kChromeUIAppLauncherPageHost[] = "apps";
const char kChromeUIAppManagementHost[] = "app-management";
const char kChromeUIAppsURL[] = "chrome://apps/";
const char kChromeUIAutofillInternalsHost[] = "autofill-internals";
const char kChromeUIBluetoothInternalsHost[] = "bluetooth-internals";
const char kChromeUIBookmarksHost[] = "bookmarks";
const char kChromeUIBookmarksURL[] = "chrome://bookmarks/";
const char kChromeUICertificateViewerHost[] = "view-cert";
const char kChromeUICertificateViewerURL[] = "chrome://view-cert/";
const char kChromeUIChromeSigninHost[] = "chrome-signin";
const char kChromeUIChromeSigninURL[] = "chrome://chrome-signin/";
const char kChromeUIChromeURLsHost[] = "chrome-urls";
const char kChromeUIChromeURLsURL[] = "chrome://chrome-urls/";
const char kChromeUIComponentsHost[] = "components";
const char kChromeUIConflictsHost[] = "conflicts";
const char kChromeUIConflictsURL[] = "chrome://conflicts/";
const char kChromeUIConstrainedHTMLTestURL[] = "chrome://constrained-test/";
const char kChromeUIContentSettingsURL[] = "chrome://settings/content";
const char kChromeUICrashHost[] = "crash";
const char kChromeUICrashesHost[] = "crashes";
const char kChromeUICreditsHost[] = "credits";
const char kChromeUICreditsURL[] = "chrome://credits/";
const char kChromeUIDefaultHost[] = "version";
const char kChromeUIDelayedHangUIHost[] = "delayeduithreadhang";
const char kChromeUIDevToolsBlankPath[] = "blank";
const char kChromeUIDevToolsBundledPath[] = "bundled";
const char kChromeUIDevToolsCustomPath[] = "custom";
const char kChromeUIDevToolsHost[] = "devtools";
const char kChromeUIDevToolsRemotePath[] = "remote";
const char kChromeUIDevToolsURL[] =
    "devtools://devtools/bundled/inspector.html";
const char kChromeUIDeviceLogHost[] = "device-log";
const char kChromeUIDevicesHost[] = "devices";
const char kChromeUIDevicesURL[] = "chrome://devices/";
const char kChromeUIDomainReliabilityInternalsHost[] =
    "domain-reliability-internals";
const char kChromeUIDownloadInternalsHost[] = "download-internals";
const char kChromeUIDownloadsHost[] = "downloads";
const char kChromeUIDownloadsURL[] = "chrome://downloads/";
const char kChromeUIDriveInternalsHost[] = "drive-internals";
const char kChromeUIExtensionIconHost[] = "extension-icon";
const char kChromeUIExtensionIconURL[] = "chrome://extension-icon/";
const char kChromeUIExtensionsHost[] = "extensions";
const char kChromeUIExtensionsInternalsHost[] = "extensions-internals";
const char kChromeUIExtensionsURL[] = "chrome://extensions/";
const char kChromeUIFaviconHost[] = "favicon";
const char kChromeUIFaviconURL[] = "chrome://favicon/";
const char kChromeUIFileiconURL[] = "chrome://fileicon/";
const char kChromeUIFlagsHost[] = "flags";
const char kChromeUIFlagsURL[] = "chrome://flags/";
const char kChromeUIGCMInternalsHost[] = "gcm-internals";
const char kChromeUIHangUIHost[] = "uithreadhang";
const char kChromeUIHelpHost[] = "help";
const char kChromeUIHelpURL[] = "chrome://help/";
const char kChromeUIHistoryHost[] = "history";
const char kChromeUIHistorySyncedTabs[] = "/syncedTabs";
const char kChromeUIHistoryURL[] = "chrome://history/";
const char kChromeUIIdentityInternalsHost[] = "identity-internals";
const char kChromeUIInspectHost[] = "inspect";
const char kChromeUIInspectURL[] = "chrome://inspect/";
const char kChromeUIInterstitialHost[] = "interstitials";
const char kChromeUIInterstitialURL[] = "chrome://interstitials/";
const char kChromeUIInterventionsInternalsHost[] = "interventions-internals";
const char kChromeUIInvalidationsHost[] = "invalidations";
const char kChromeUIKillHost[] = "kill";
const char kChromeUILocalStateHost[] = "local-state";
const char kChromeUIManagementHost[] = "management";
const char kChromeUIManagementURL[] = "chrome://management";
const char kChromeUIMdUserManagerHost[] = "md-user-manager";
const char kChromeUIMdUserManagerUrl[] = "chrome://md-user-manager/";
const char kChromeUIMediaEngagementHost[] = "media-engagement";
const char kChromeUIMediaRouterHost[] = "media-router";
const char kChromeUIMediaRouterURL[] = "chrome://media-router/";
const char kChromeUIMediaRouterInternalsHost[] = "media-router-internals";
const char kChromeUIMediaRouterInternalsURL[] =
    "chrome://media-router-internals/";
const char kChromeUIMemoryInternalsHost[] = "memory-internals";
const char kChromeUINTPTilesInternalsHost[] = "ntp-tiles-internals";
const char kChromeUINaClHost[] = "nacl";
const char kChromeUINetExportHost[] = "net-export";
const char kChromeUINetInternalsHost[] = "net-internals";
const char kChromeUINetInternalsURL[] = "chrome://net-internals/";
const char kChromeUINewTabHost[] = "newtab";
const char kChromeUINewTabIconHost[] = "ntpicon";
const char kChromeUINewTabURL[] = "chrome://newtab/";
const char kChromeUIOmniboxHost[] = "omnibox";
const char kChromeUIOmniboxURL[] = "chrome://omnibox/";
const char kChromeUIPasswordManagerInternalsHost[] =
    "password-manager-internals";
const char kChromeUIPhysicalWebHost[] = "physical-web";
const char kChromeUIPolicyHost[] = "policy";
const char kChromeUIPolicyURL[] = "chrome://policy/";
const char kChromeUIPredictorsHost[] = "predictors";
const char kChromeUIPrefsInternalsHost[] = "prefs-internals";
const char kChromeUIPrintURL[] = "chrome://print/";
const char kChromeUIQuitHost[] = "quit";
const char kChromeUIQuitURL[] = "chrome://quit/";
const char kChromeUIQuotaInternalsHost[] = "quota-internals";
const char kChromeUIResetPasswordHost[] = "reset-password";
const char kChromeUIResetPasswordURL[] = "chrome://reset-password/";
const char kChromeUIRestartHost[] = "restart";
const char kChromeUIRestartURL[] = "chrome://restart/";
const char kChromeUISafetyPixelbookURL[] = "https://g.co/Pixelbook/legal";
const char kChromeUISafetyPixelSlateURL[] = "https://g.co/PixelSlate/legal";
const char kChromeUISettingsHost[] = "settings";
const char kChromeUISettingsURL[] = "chrome://settings/";
const char kChromeUISignInInternalsHost[] = "signin-internals";
const char kChromeUISigninEmailConfirmationHost[] = "signin-email-confirmation";
const char kChromeUISigninEmailConfirmationURL[] =
    "chrome://signin-email-confirmation";
const char kChromeUISigninErrorHost[] = "signin-error";
const char kChromeUISigninErrorURL[] = "chrome://signin-error/";
const char kChromeUISiteDetailsPrefixURL[] =
    "chrome://settings/content/siteDetails?site=";
const char kChromeUISiteEngagementHost[] = "site-engagement";
const char kChromeUISuggestionsHost[] = "suggestions";
const char kChromeUISuggestionsURL[] = "chrome://suggestions/";
const char kChromeUISupervisedUserInternalsHost[] = "supervised-user-internals";
const char kChromeUISupervisedUserPassphrasePageHost[] =
    "managed-user-passphrase";
const char kChromeUISyncConfirmationHost[] = "sync-confirmation";
const char kChromeUISyncConfirmationURL[] = "chrome://sync-confirmation/";
const char kChromeUISyncFileSystemInternalsHost[] = "syncfs-internals";
const char kChromeUISyncHost[] = "sync";
const char kChromeUISyncInternalsHost[] = "sync-internals";
const char kChromeUISyncResourcesHost[] = "syncresources";
const char kChromeUISystemInfoHost[] = "system";
const char kChromeUITermsHost[] = "terms";
const char kChromeUITermsURL[] = "chrome://terms/";
const char kChromeUIThemeHost[] = "theme";
const char kChromeUIThemeURL[] = "chrome://theme/";
const char kChromeUIThumbnailHost2[] = "thumb2";
const char kChromeUIThumbnailHost[] = "thumb";
const char kChromeUIThumbnailListHost[] = "thumbnails";
const char kChromeUIThumbnailURL[] = "chrome://thumb/";
const char kChromeUITranslateInternalsHost[] = "translate-internals";
const char kChromeUIUkmHost[] = "ukm";
const char kChromeUIUberHost[] = "chrome";
const char kChromeUIUsbInternalsHost[] = "usb-internals";
const char kChromeUIUserActionsHost[] = "user-actions";
const char kChromeUIVersionHost[] = "version";
const char kChromeUIVersionURL[] = "chrome://version/";
const char kChromeUIWelcomeHost[] = "welcome";
const char kChromeUIWelcomeURL[] = "chrome://welcome/";
const char kChromeUIWelcomeWin10Host[] = "welcome-win10";
const char kChromeUIWelcomeWin10URL[] = "chrome://welcome-win10/";

#if defined(OS_ANDROID)
const char kChromeUIExploreSitesInternalsHost[] = "explore-sites-internals";
const char kChromeUIJavaCrashURL[] = "chrome://java-crash/";
const char kChromeUINativeBookmarksURL[] = "chrome-native://bookmarks/";
const char kChromeUINativeExploreURL[] = "chrome-native://explore";
const char kChromeUINativeHistoryURL[] = "chrome-native://history/";
const char kChromeUINativeNewTabURL[] = "chrome-native://newtab/";
const char kChromeUINativePhysicalWebDiagnosticsURL[] =
    "chrome-native://physical-web-diagnostics/";
const char kChromeUINativeScheme[] = "chrome-native";
const char kChromeUIOfflineInternalsHost[] = "offline-internals";
const char kChromeUIPhysicalWebDiagnosticsHost[] = "physical-web-diagnostics";
const char kChromeUISnippetsInternalsHost[] = "snippets-internals";
const char kChromeUIWebApksHost[] = "webapks";
#endif

#if defined(OS_CHROMEOS)
// Keep alphabetized.
const char kChromeUIAccountManagerWelcomeHost[] = "account-manager-welcome";
const char kChromeUIAccountManagerWelcomeURL[] =
    "chrome://account-manager-welcome";
const char kChromeUIAccountMigrationWelcomeHost[] = "account-migration-welcome";
const char kChromeUIAccountMigrationWelcomeURL[] =
    "chrome://account-migration-welcome";
const char kChromeUIActivationMessageHost[] = "activationmessage";
const char kChromeUIAddSupervisionHost[] = "add-supervision";
const char kChromeUIAddSupervisionURL[] = "chrome://add-supervision/";
const char kChromeUIArcGraphicsTracingHost[] = "arc-graphics-tracing";
const char kChromeUIArcGraphicsTracingURL[] = "chrome://arc-graphics-tracing/";
const char kChromeUIAssistantOptInHost[] = "assistant-optin";
const char kChromeUIAssistantOptInURL[] = "chrome://assistant-optin/";
const char kChromeUIBluetoothPairingHost[] = "bluetooth-pairing";
const char kChromeUIBluetoothPairingURL[] = "chrome://bluetooth-pairing/";
const char kChromeUICellularSetupHost[] = "cellular-setup";
const char kChromeUICellularSetupUrl[] = "chrome://cellular-setup";
const char kChromeUICertificateManagerDialogURL[] =
    "chrome://certificate-manager/";
const char kChromeUICertificateManagerHost[] = "certificate-manager";
const char kChromeUICryptohomeHost[] = "cryptohome";
const char kChromeUIDeviceEmulatorHost[] = "device-emulator";
const char kChromeUIDiscoverURL[] = "chrome://oobe/discover";
const char kChromeUIFirstRunHost[] = "first-run";
const char kChromeUIFirstRunURL[] = "chrome://first-run/";
const char kChromeUIIntenetConfigDialogURL[] =
    "chrome://internet-config-dialog/";
const char kChromeUIIntenetDetailDialogURL[] =
    "chrome://internet-detail-dialog/";
const char kChromeUIInternetConfigDialogHost[] = "internet-config-dialog";
const char kChromeUIInternetDetailDialogHost[] = "internet-detail-dialog";
const char kChromeUILinuxCreditsHost[] = "linux-credits";
const char kChromeUILinuxCreditsURL[] = "chrome://linux-credits/";
const char kChromeUIMachineLearningInternalsHost[] =
    "machine-learning-internals";
const char kChromeUIMobileSetupHost[] = "mobilesetup";
const char kChromeUIMobileSetupURL[] = "chrome://mobilesetup/";
const char kChromeUIMultiDeviceSetupHost[] = "multidevice-setup";
const char kChromeUIMultiDeviceSetupUrl[] = "chrome://multidevice-setup";
const char kChromeUINetworkHost[] = "network";
const char kChromeUIOSCreditsHost[] = "os-credits";
const char kChromeUIOSCreditsURL[] = "chrome://os-credits/";
const char kChromeUIOSSettingsHost[] = "os-settings";
const char kChromeUIOSSettingsURL[] = "chrome://os-settings/";
const char kChromeUIOobeHost[] = "oobe";
const char kChromeUIOobeURL[] = "chrome://oobe/";
const char kChromeUIPasswordChangeHost[] = "password-change";
const char kChromeUIPasswordChangeUrl[] = "chrome://password-change";
const char kChromeUIPowerHost[] = "power";
const char kChromeUIScreenlockIconHost[] = "screenlock-icon";
const char kChromeUIScreenlockIconURL[] = "chrome://screenlock-icon/";
const char kChromeUISetTimeHost[] = "set-time";
const char kChromeUISetTimeURL[] = "chrome://set-time/";
const char kChromeUISlowHost[] = "slow";
const char kChromeUISlowTraceHost[] = "slow_trace";
const char kChromeUISlowURL[] = "chrome://slow/";
const char kChromeUISmbShareHost[] = "smb-share-dialog";
const char kChromeUISmbShareURL[] = "chrome://smb-share-dialog/";
const char kChromeUISmbCredentialsHost[] = "smb-credentials-dialog";
const char kChromeUISmbCredentialsURL[] = "chrome://smb-credentials-dialog/";
const char kChromeUISysInternalsHost[] = "sys-internals";
const char kChromeUIUserImageHost[] = "userimage";
const char kChromeUIUserImageURL[] = "chrome://userimage/";
// Keep alphabetized.

bool IsSystemWebUIHost(base::StringPiece host) {
  // Compares host instead of full URL for performance (the strings are
  // shorter).
  static const char* const kHosts[] = {
      kChromeUIAccountManagerWelcomeHost,
      kChromeUIAccountMigrationWelcomeHost,
      kChromeUIActivationMessageHost,
      kChromeUIAddSupervisionHost,
      kChromeUIAssistantOptInHost,
      kChromeUIBluetoothPairingHost,
      kChromeUICellularSetupHost,
      kChromeUICertificateManagerHost,
      kChromeUICryptohomeHost,
      kChromeUIDeviceEmulatorHost,
      kChromeUIFirstRunHost,
      kChromeUIInternetConfigDialogHost,
      kChromeUIInternetDetailDialogHost,
      kChromeUILinuxCreditsHost,
      kChromeUIMobileSetupHost,
      kChromeUIMultiDeviceSetupHost,
      kChromeUINetworkHost,
      kChromeUIOobeHost,
      kChromeUIOSCreditsHost,
      kChromeUIOSSettingsHost,
      kChromeUIPasswordChangeHost,
      kChromeUIPowerHost,
      kChromeUISetTimeHost,
      kChromeUISmbCredentialsHost,
      kChromeUISmbShareHost,
  };
  for (const char* h : kHosts) {
    if (host == h)
      return true;
  }
  return false;
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
const char kChromeUIMetroFlowHost[] = "make-metro";
const char kChromeUIMetroFlowURL[] = "chrome://make-metro/";
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_CHROMEOS)
const char kChromeUICastHost[] = "cast";
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
const char kChromeUIDiscardsHost[] = "discards";
const char kChromeUIDiscardsURL[] = "chrome://discards/";
const char kChromeUIHatsHost[] = "hats";
const char kChromeUIHatsURL[] = "chrome://hats/";
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
const char kChromeUILinuxProxyConfigHost[] = "linux-proxy-config";
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
const char kChromeUISandboxHost[] = "sandbox";
#endif

#if defined(OS_WIN) || defined(OS_MACOSX) || \
    (defined(OS_LINUX) && !defined(OS_CHROMEOS))
const char kChromeUIBrowserSwitchHost[] = "browser-switch";
const char kChromeUIBrowserSwitchURL[] = "chrome://browser-switch/";
#endif

#if (defined(OS_LINUX) && defined(TOOLKIT_VIEWS)) || defined(USE_AURA)
const char kChromeUITabModalConfirmDialogHost[] = "tab-modal-confirm-dialog";
#endif

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
const char kChromeUIPrintHost[] = "print";
#endif

const char kChromeUIWebRtcLogsHost[] = "webrtc-logs";

// Settings sub pages.

// NOTE: Add sub page paths to |kChromeSettingsSubPages| in
// chrome_autocomplete_provider_client.cc to be listed by the built-in
// AutocompleteProvider.

const char kAddressesSubPage[] = "addresses";
const char kAutofillSubPage[] = "autofill";
const char kClearBrowserDataSubPage[] = "clearBrowserData";
const char kContentSettingsSubPage[] = "content";
const char kDeprecatedExtensionsSubPage[] = "extensions";
const char kHandlerSettingsSubPage[] = "handlers";
const char kImportDataSubPage[] = "importData";
const char kLanguageOptionsSubPage[] = "languages";
const char kPasswordManagerSubPage[] = "passwords";
const char kPaymentsSubPage[] = "payments";
const char kPrintingSettingsSubPage[] = "printing";
const char kPrivacySubPage[] = "privacy";
const char kResetProfileSettingsSubPage[] = "resetProfileSettings";
const char kSearchEnginesSubPage[] = "searchEngines";
const char kSignOutSubPage[] = "signOut";
const char kSyncSetupSubPage[] = "syncSetup";
const char kTriggeredResetProfileSettingsSubPage[] =
    "triggeredResetProfileSettings";

#if defined(OS_CHROMEOS)
// NOTE: Add new OS settings to IsOSSettingsSubPage() below.
const char kAccessibilitySubPage[] = "accessibility";
const char kAccountManagerSubPage[] = "accountManager";
const char kAndroidAppsDetailsSubPage[] = "androidApps/details";
const char kAssistantSubPage[] = "googleAssistant";
const char kBluetoothSubPage[] = "bluetoothDevices";
const char kCrostiniSharedUsbDevicesSubPage[] = "crostini/sharedUsbDevices";
const char kDateTimeSubPage[] = "dateTime";
const char kDisplaySubPage[] = "display";
const char kHelpSubPage[] = "help";
const char kInternetSubPage[] = "internet";
// 'multidevice/features' is a child of the 'multidevice' route
const char kConnectedDevicesSubPage[] = "multidevice/features";
const char kLockScreenSubPage[] = "lockScreen";
const char kNativePrintingSettingsSubPage[] = "cupsPrinters";
const char kNetworkDetailSubPage[] = "networkDetail";
const char kNetworksSubPage[] = "networks";
const char kPluginVmDetailsSubPage[] = "pluginVm/details";
const char kPluginVmSharedPathSubPage[] = "pluginVm/sharedPath";
const char kPowerSubPage[] = "power";
const char kSmartLockSettingsSubPage[] = "multidevice/features/smartLock";
const char kSmbSharesSubPage[] = "smbShares";
const char kStorageSubPage[] = "storage";
const char kStylusSubPage[] = "stylus";
// Tether is a child of the 'networks' route.
const char kTetherSettingsSubPage[] = "networks?type=Tether";

bool IsOSSettingsSubPage(const std::string& sub_page) {
  static const char* const kSubPages[] = {kAccessibilitySubPage,
                                          kAccountManagerSubPage,
                                          kAndroidAppsDetailsSubPage,
                                          kAssistantSubPage,
                                          kBluetoothSubPage,
                                          kCrostiniSharedUsbDevicesSubPage,
                                          kDateTimeSubPage,
                                          kDisplaySubPage,
                                          kHelpSubPage,
                                          kInternetSubPage,
                                          kConnectedDevicesSubPage,
                                          kLockScreenSubPage,
                                          kNetworkDetailSubPage,
                                          kNetworksSubPage,
                                          kPowerSubPage,
                                          kSmartLockSettingsSubPage,
                                          kSmbSharesSubPage,
                                          kStorageSubPage,
                                          kStylusSubPage};
  // Sub-pages may have query parameters, e.g. networkDetail?guid=123456.
  std::string sub_page_without_query = sub_page;
  std::string::size_type index = sub_page.find('?');
  if (index != std::string::npos)
    sub_page_without_query.resize(index);

  for (const char* p : kSubPages) {
    if (sub_page_without_query == p)
      return true;
  }
  return false;
}
#else
const char kCreateProfileSubPage[] = "createProfile";
const char kManageProfileSubPage[] = "manageProfile";
const char kPeopleSubPage[] = "people";
#endif  // defined(OS_CHROMEOS)
#if defined(OS_WIN)
const char kCleanupSubPage[] = "cleanup";
#endif  // defined(OS_WIN)

// Extension sub pages.
const char kExtensionConfigureCommandsSubPage[] = "configureCommands";

// Add hosts here to be included in chrome://chrome-urls (about:about).
// These hosts will also be suggested by BuiltinProvider.
const char* const kChromeHostURLs[] = {
    kChromeUIAboutHost,
    kChromeUIAccessibilityHost,
    kChromeUIBluetoothInternalsHost,
    kChromeUIChromeURLsHost,
    kChromeUIComponentsHost,
    kChromeUICrashesHost,
    kChromeUICreditsHost,
#if defined(OS_CHROMEOS) && !defined(OFFICIAL_BUILD)
    kChromeUIDeviceEmulatorHost,
#endif
    kChromeUIDeviceLogHost,
    kChromeUIDownloadInternalsHost,
    kChromeUIFlagsHost,
    kChromeUIGCMInternalsHost,
    kChromeUIHistoryHost,
    kChromeUIInterstitialHost,
    kChromeUIInterventionsInternalsHost,
    kChromeUIInvalidationsHost,
    kChromeUILocalStateHost,
#if !defined(OS_ANDROID)
    kChromeUIManagementHost,
#endif
    kChromeUIMediaEngagementHost,
    kChromeUINetExportHost,
    kChromeUINetInternalsHost,
    kChromeUINewTabHost,
    kChromeUIOmniboxHost,
    kChromeUIPasswordManagerInternalsHost,
    kChromeUIPolicyHost,
    kChromeUIPredictorsHost,
    kChromeUIPrefsInternalsHost,
    kChromeUIQuotaInternalsHost,
    kChromeUISignInInternalsHost,
    kChromeUISiteEngagementHost,
    kChromeUINTPTilesInternalsHost,
    safe_browsing::kChromeUISafeBrowsingHost,
    kChromeUISuggestionsHost,
    kChromeUISupervisedUserInternalsHost,
    kChromeUISyncInternalsHost,
#if !defined(OS_ANDROID)
    kChromeUITermsHost,
    kChromeUIThumbnailListHost,
#endif
    kChromeUITranslateInternalsHost,
    kChromeUIUsbInternalsHost,
    kChromeUIUserActionsHost,
    kChromeUIVersionHost,
    content::kChromeUIAppCacheInternalsHost,
    content::kChromeUIBlobInternalsHost,
    content::kChromeUIDinoHost,
    content::kChromeUIGpuHost,
    content::kChromeUIHistogramHost,
    content::kChromeUIIndexedDBInternalsHost,
    content::kChromeUIMediaInternalsHost,
    content::kChromeUINetworkErrorHost,
    content::kChromeUINetworkErrorsListingHost,
    content::kChromeUIProcessInternalsHost,
    content::kChromeUIServiceWorkerInternalsHost,
#if !defined(OS_ANDROID)
    content::kChromeUITracingHost,
#endif
    content::kChromeUIWebRTCInternalsHost,
#if !defined(OS_ANDROID)
#if !defined(OS_CHROMEOS)
    kChromeUIAppLauncherPageHost,
#endif
    kChromeUIBookmarksHost,
    kChromeUIDownloadsHost,
    kChromeUIHelpHost,
    kChromeUIInspectHost,
    kChromeUISettingsHost,
    kChromeUISystemInfoHost,
    kChromeUIUberHost,
#endif
#if defined(OS_ANDROID)
    kChromeUIExploreSitesInternalsHost,
    kChromeUIOfflineInternalsHost,
    kChromeUISnippetsInternalsHost,
    kChromeUIWebApksHost,
#endif
#if defined(OS_CHROMEOS)
    kChromeUICertificateManagerHost,
    kChromeUICryptohomeHost,
    kChromeUIDriveInternalsHost,
    kChromeUIFirstRunHost,
    kChromeUILinuxCreditsHost,
    kChromeUIMachineLearningInternalsHost,
    kChromeUINetworkHost,
    kChromeUIOobeHost,
    kChromeUIOSCreditsHost,
    kChromeUIPowerHost,
    kChromeUIInternetConfigDialogHost,
    kChromeUIInternetDetailDialogHost,
    kChromeUIAssistantOptInHost,
#endif
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
    kChromeUIDiscardsHost,
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
    kChromeUILinuxProxyConfigHost,
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
    kChromeUISandboxHost,
#endif
#if defined(OS_WIN)
    kChromeUIConflictsHost,
#endif
#if BUILDFLAG(ENABLE_NACL)
    kChromeUINaClHost,
#endif
#if BUILDFLAG(ENABLE_EXTENSIONS)
    kChromeUIExtensionsHost,
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    kChromeUIPrintHost,
#endif
#if BUILDFLAG(ENABLE_SERVICE_DISCOVERY)
    kChromeUIDevicesHost,
#endif
    kChromeUIWebRtcLogsHost,
};
const size_t kNumberOfChromeHostURLs = base::size(kChromeHostURLs);

const char* const kChromeDebugURLs[] = {
    content::kChromeUIBadCastCrashURL,
    content::kChromeUIBrowserCrashURL,
    content::kChromeUICrashURL,
    content::kChromeUIDumpURL,
    content::kChromeUIKillURL,
    content::kChromeUIHangURL,
    content::kChromeUIShorthangURL,
    content::kChromeUIGpuCleanURL,
    content::kChromeUIGpuCrashURL,
    content::kChromeUIGpuHangURL,
    content::kChromeUIMemoryExhaustURL,
    content::kChromeUIPpapiFlashCrashURL,
    content::kChromeUIPpapiFlashHangURL,
#if defined(OS_WIN)
    content::kChromeUIBrowserHeapCorruptionURL,
    content::kChromeUIHeapCorruptionCrashURL,
#endif
#if defined(OS_ANDROID)
    content::kChromeUIGpuJavaCrashURL,
    kChromeUIJavaCrashURL,
#endif
    kChromeUIQuitURL,
    kChromeUIRestartURL};
const size_t kNumberOfChromeDebugURLs = base::size(kChromeDebugURLs);

}  // namespace chrome
