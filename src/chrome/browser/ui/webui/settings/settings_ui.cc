// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/webui/dark_mode_handler.h"
#include "chrome/browser/ui/webui/managed_ui_handler.h"
#include "chrome/browser/ui/webui/metrics_handler.h"
#include "chrome/browser/ui/webui/settings/about_handler.h"
#include "chrome/browser/ui/webui/settings/accessibility_main_handler.h"
#include "chrome/browser/ui/webui/settings/appearance_handler.h"
#include "chrome/browser/ui/webui/settings/browser_lifetime_handler.h"
#include "chrome/browser/ui/webui/settings/downloads_handler.h"
#include "chrome/browser/ui/webui/settings/extension_control_handler.h"
#include "chrome/browser/ui/webui/settings/font_handler.h"
#include "chrome/browser/ui/webui/settings/metrics_reporting_handler.h"
#include "chrome/browser/ui/webui/settings/on_startup_handler.h"
#include "chrome/browser/ui/webui/settings/people_handler.h"
#include "chrome/browser/ui/webui/settings/profile_info_handler.h"
#include "chrome/browser/ui/webui/settings/protocol_handlers_handler.h"
#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"
#include "chrome/browser/ui/webui/settings/search_engines_handler.h"
#include "chrome/browser/ui/webui/settings/settings_clear_browsing_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_cookies_view_handler.h"
#include "chrome/browser/ui/webui/settings/settings_import_data_handler.h"
#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/settings_media_devices_selection_handler.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"
#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"
#include "chrome/browser/ui/webui/settings/site_settings_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/settings_resources.h"
#include "chrome/grit/settings_resources_map.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/unified_consent/feature.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_WIN)
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/webui/settings/chrome_cleanup_handler.h"
#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/conflicts/incompatible_applications_updater_win.h"
#include "chrome/browser/conflicts/token_util_win.h"
#include "chrome/browser/ui/webui/settings/incompatible_applications_handler_win.h"
#endif
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/languages_handler.h"
#include "chrome/browser/ui/webui/settings/tts_handler.h"
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/stylus_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/account_manager/account_manager_util.h"
#include "chrome/browser/chromeos/android_sms/android_sms_app_manager.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/multidevice_setup/multidevice_setup_client_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/ui/webui/chromeos/smb_shares/smb_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/accessibility_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/account_manager_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/android_apps_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/change_picture_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/crostini_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/date_time_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_keyboard_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_pointer_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_power_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/device_stylus_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/fingerprint_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/google_assistant_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/multidevice_handler.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/components/account_manager/account_manager.h"
#include "chromeos/components/account_manager/account_manager_factory.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_pref_names.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/services/multidevice_setup/public/cpp/prefs.h"
#include "components/arc/arc_util.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_features.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#else  // !defined(OS_CHROMEOS)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/webui/settings/settings_default_browser_handler.h"
#include "chrome/browser/ui/webui/settings/settings_manage_profile_handler.h"
#include "chrome/browser/ui/webui/settings/system_handler.h"
#endif  // defined(OS_CHROMEOS)

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/webui/certificates_handler.h"
#elif defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/ui/webui/settings/native_certificates_handler.h"
#endif  // defined(USE_NSS_CERTS)

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/settings/printing_handler.h"
#endif

#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/webui/settings/change_password_handler.h"
#endif

namespace settings {

// static
void SettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kImportDialogAutofillFormData, true);
  registry->RegisterBooleanPref(prefs::kImportDialogBookmarks, true);
  registry->RegisterBooleanPref(prefs::kImportDialogHistory, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSavedPasswords, true);
  registry->RegisterBooleanPref(prefs::kImportDialogSearchEngine, true);
}

SettingsUI::SettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()) {
#if BUILDFLAG(OPTIMIZE_WEBUI)
  std::vector<std::string> exclude_from_gzip;
#endif

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUISettingsHost);

  AddSettingsPageUIHandler(std::make_unique<AppearanceHandler>(web_ui));

#if defined(USE_NSS_CERTS)
  AddSettingsPageUIHandler(
      std::make_unique<certificate_manager::CertificatesHandler>());
#elif defined(OS_WIN) || defined(OS_MACOSX)
  AddSettingsPageUIHandler(std::make_unique<NativeCertificatesHandler>());
#endif  // defined(USE_NSS_CERTS)

  AddSettingsPageUIHandler(std::make_unique<AccessibilityMainHandler>());
  AddSettingsPageUIHandler(std::make_unique<BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(std::make_unique<ClearBrowsingDataHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<CookiesViewHandler>());
  AddSettingsPageUIHandler(std::make_unique<DownloadsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ExtensionControlHandler>());
  AddSettingsPageUIHandler(std::make_unique<FontHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<ImportDataHandler>());

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // TODO(jamescook): Sort out how language is split between Chrome OS and
  // and browser settings.
  AddSettingsPageUIHandler(std::make_unique<LanguagesHandler>(web_ui));
#endif  // defined(OS_WIN) || defined(OS_CHROMEOS)

  AddSettingsPageUIHandler(
      std::make_unique<MediaDevicesSelectionHandler>(profile));
#if defined(GOOGLE_CHROME_BUILD) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<MetricsReportingHandler>());
#endif
  AddSettingsPageUIHandler(std::make_unique<OnStartupHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<PeopleHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(std::make_unique<SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SiteSettingsHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<StartupPagesHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<SecurityKeysHandler>());

#if defined(OS_CHROMEOS)
  // TODO(950007): Remove this when SplitSettings is the default and there are
  // no Chrome OS settings in the browser settings page.
  InitOSWebUIHandlers(profile, web_ui, html_source);

  // TODO(jamescook): Sort out how account management is split between Chrome OS
  // and browser settings.
  if (chromeos::IsAccountManagerAvailable(profile)) {
    chromeos::AccountManagerFactory* factory =
        g_browser_process->platform_part()->GetAccountManagerFactory();
    chromeos::AccountManager* account_manager =
        factory->GetAccountManager(profile->GetPath().value());
    DCHECK(account_manager);

    AddSettingsPageUIHandler(
        std::make_unique<chromeos::settings::AccountManagerUIHandler>(
            account_manager, IdentityManagerFactory::GetForProfile(profile)));
    html_source->AddBoolean(
        "secondaryGoogleAccountSigninAllowed",
        profile->GetPrefs()->GetBoolean(
            chromeos::prefs::kSecondaryGoogleAccountSigninAllowed));
  }
  AddSettingsPageUIHandler(
      std::make_unique<chromeos::settings::ChangePictureHandler>());
#else
  AddSettingsPageUIHandler(std::make_unique<DefaultBrowserHandler>());
  AddSettingsPageUIHandler(std::make_unique<ManageProfileHandler>(profile));
  AddSettingsPageUIHandler(std::make_unique<SystemHandler>());
#endif

#if BUILDFLAG(ENABLE_PRINTING) && !defined(OS_CHROMEOS)
  AddSettingsPageUIHandler(std::make_unique<PrintingHandler>());
#endif

#if defined(OS_WIN)
  AddSettingsPageUIHandler(std::make_unique<ChromeCleanupHandler>(profile));
#endif  // defined(OS_WIN)

#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  bool has_incompatible_applications =
      IncompatibleApplicationsUpdater::HasCachedApplications();
  html_source->AddBoolean("showIncompatibleApplications",
                          has_incompatible_applications);
  html_source->AddBoolean("hasAdminRights", HasAdminRights());

  if (has_incompatible_applications)
    AddSettingsPageUIHandler(
        std::make_unique<IncompatibleApplicationsHandler>());
#endif  // OS_WIN && defined(GOOGLE_CHROME_BUILD)

  bool password_protection_available = false;
#if defined(FULL_SAFE_BROWSING)
  safe_browsing::ChromePasswordProtectionService* password_protection =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(profile);
  password_protection_available = !!password_protection;
  if (password_protection) {
    AddSettingsPageUIHandler(
        std::make_unique<ChangePasswordHandler>(profile, password_protection));
  }
#endif
  html_source->AddBoolean("passwordProtectionAvailable",
                          password_protection_available);

#if !defined(OS_CHROMEOS)
  html_source->AddBoolean(
      "diceEnabled",
      AccountConsistencyModeManager::IsDiceEnabledForProfile(profile));
#endif  // !defined(OS_CHROMEOS)

  html_source->AddBoolean("unifiedConsentEnabled",
                          unified_consent::IsUnifiedConsentFeatureEnabled());

  html_source->AddBoolean(
      "navigateToGooglePasswordManager",
      ShouldManagePasswordsinGooglePasswordManager(profile));

  html_source->AddBoolean("showImportPasswords",
                          base::FeatureList::IsEnabled(
                              password_manager::features::kPasswordImport));
#if defined(OS_CHROMEOS)
  html_source->AddBoolean(
      "showOSSettings",
      !base::FeatureList::IsEnabled(chromeos::features::kSplitSettings));
#else
  html_source->AddBoolean("showOSSettings", false);
#endif

  AddSettingsPageUIHandler(
      base::WrapUnique(AboutHandler::Create(html_source, profile)));
  AddSettingsPageUIHandler(
      base::WrapUnique(ResetSettingsHandler::Create(html_source, profile)));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

#if defined(OS_CHROMEOS)
  // Add the System Web App resources for Settings.
  // TODO(jamescook|calamity): Migrate to chromeos::settings::OSSettingsUI.
  if (web_app::SystemWebAppManager::IsEnabled()) {
    html_source->AddResourcePath("icon-192.png", IDR_SETTINGS_LOGO_192);
    html_source->AddResourcePath("pwa.html", IDR_PWA_HTML);
#if BUILDFLAG(OPTIMIZE_WEBUI)
    exclude_from_gzip.push_back("icon-192.png");
    exclude_from_gzip.push_back("pwa.html");
#endif  // BUILDFLAG(OPTIMIZE_WEBUI)
  }
#endif  // defined (OS_CHROMEOS)

#if BUILDFLAG(OPTIMIZE_WEBUI)
  const bool use_polymer_2 =
      base::FeatureList::IsEnabled(features::kWebUIPolymer2);
  html_source->AddResourcePath("crisper.js", IDR_SETTINGS_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.crisper.js",
                               IDR_SETTINGS_LAZY_LOAD_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.html",
                               use_polymer_2
                                   ? IDR_SETTINGS_LAZY_LOAD_VULCANIZED_P2_HTML
                                   : IDR_SETTINGS_LAZY_LOAD_VULCANIZED_HTML);
  html_source->SetDefaultResource(use_polymer_2
                                      ? IDR_SETTINGS_VULCANIZED_P2_HTML
                                      : IDR_SETTINGS_VULCANIZED_HTML);
  html_source->UseGzip(base::BindRepeating(
      [](const std::vector<std::string>& excluded_paths,
         const std::string& path) {
        return !base::ContainsValue(excluded_paths, path);
      },
      std::move(exclude_from_gzip)));
#if defined(OS_CHROMEOS)
  html_source->AddResourcePath("manifest.json", IDR_SETTINGS_MANIFEST);
#endif  // defined (OS_CHROMEOS)
#else
  // Add all settings resources.
  for (size_t i = 0; i < kSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kSettingsResources[i].name,
                                 kSettingsResources[i].value);
  }
  html_source->SetDefaultResource(IDR_SETTINGS_SETTINGS_HTML);
#endif

  AddLocalizedStrings(html_source, profile);

  DarkModeHandler::Initialize(web_ui, html_source);
  ManagedUIHandler::Initialize(web_ui, html_source);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
}

SettingsUI::~SettingsUI() {}

void SettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<content::WebUIMessageHandler> handler) {
  DCHECK(handler);
  web_ui()->AddMessageHandler(std::move(handler));
}

void SettingsUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  load_start_time_ = base::Time::Now();
}

void SettingsUI::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  UMA_HISTOGRAM_TIMES("Settings.LoadDocumentTime.MD",
                      base::Time::Now() - load_start_time_);
}

void SettingsUI::DocumentOnLoadCompletedInMainFrame() {
  UMA_HISTOGRAM_TIMES("Settings.LoadCompletedTime.MD",
                      base::Time::Now() - load_start_time_);
}

#if defined(OS_CHROMEOS)
// static
void SettingsUI::InitOSWebUIHandlers(Profile* profile,
                                     content::WebUI* web_ui,
                                     content::WebUIDataSource* html_source) {
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::AccessibilityHandler>(web_ui));
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::AndroidAppsHandler>(profile));
  if (crostini::IsCrostiniUIAllowedForProfile(profile,
                                              false /* check_policy */)) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::CrostiniHandler>(profile));
  }
  web_ui->AddMessageHandler(
      chromeos::settings::CupsPrintersHandler::Create(web_ui));
  web_ui->AddMessageHandler(base::WrapUnique(
      chromeos::settings::DateTimeHandler::Create(html_source)));
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::FingerprintHandler>(profile));
  if (chromeos::switches::IsAssistantEnabled()) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::GoogleAssistantHandler>(profile));
  }
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::KeyboardHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::PointerHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::StorageHandler>(profile));
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::StylusHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::settings::InternetHandler>(profile));
  web_ui->AddMessageHandler(std::make_unique<TtsHandler>());
  web_ui->AddMessageHandler(
      std::make_unique<chromeos::smb_dialog::SmbHandler>(profile));

  if (!profile->IsGuestSession()) {
    chromeos::android_sms::AndroidSmsService* android_sms_service =
        chromeos::android_sms::AndroidSmsServiceFactory::GetForBrowserContext(
            profile);
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::MultideviceHandler>(
            profile->GetPrefs(),
            chromeos::multidevice_setup::MultiDeviceSetupClientFactory::
                GetForProfile(profile),
            android_sms_service
                ? android_sms_service->android_sms_pairing_state_tracker()
                : nullptr,
            android_sms_service ? android_sms_service->android_sms_app_manager()
                                : nullptr));
  }

  html_source->AddBoolean(
      "multideviceAllowedByPolicy",
      chromeos::multidevice_setup::AreAnyMultiDeviceFeaturesAllowed(
          profile->GetPrefs()));
  html_source->AddBoolean(
      "quickUnlockEnabled",
      chromeos::quick_unlock::IsPinEnabled(profile->GetPrefs()));
  html_source->AddBoolean(
      "quickUnlockDisabledByPolicy",
      chromeos::quick_unlock::IsPinDisabledByPolicy(profile->GetPrefs()));
  const bool fingerprint_unlock_enabled =
      chromeos::quick_unlock::IsFingerprintEnabled(profile);
  html_source->AddBoolean("fingerprintUnlockEnabled",
                          fingerprint_unlock_enabled);
  if (fingerprint_unlock_enabled) {
    html_source->AddBoolean(
        "isFingerprintReaderOnKeyboard",
        chromeos::quick_unlock::IsFingerprintReaderOnKeyboard());
  }
  html_source->AddBoolean("lockScreenNotificationsEnabled",
                          ash::features::IsLockScreenNotificationsEnabled());
  html_source->AddBoolean(
      "lockScreenHideSensitiveNotificationsSupported",
      ash::features::IsLockScreenHideSensitiveNotificationsSupported());
  html_source->AddBoolean("showTechnologyBadge",
                          !ash::features::IsSeparateNetworkIconsEnabled());
  html_source->AddBoolean("hasInternalStylus",
                          ash::stylus_utils::HasInternalStylus());

  html_source->AddBoolean(
      "showKioskNextShell",
      base::FeatureList::IsEnabled(ash::features::kKioskNextShell));

  html_source->AddBoolean("showCrostini",
                          crostini::IsCrostiniUIAllowedForProfile(
                              profile, false /* check_policy */));

  html_source->AddBoolean("allowCrostini",
                          crostini::IsCrostiniUIAllowedForProfile(profile));

  html_source->AddBoolean("showPluginVm",
                          plugin_vm::IsPluginVmEnabled(profile));

  html_source->AddBoolean("isDemoSession",
                          chromeos::DemoSession::IsDeviceInDemoMode());

  html_source->AddBoolean("assistantEnabled",
                          chromeos::switches::IsAssistantEnabled());

  // We have 2 variants of Android apps settings. Default case, when the Play
  // Store app exists we show expandable section that allows as to
  // enable/disable the Play Store and link to Android settings which is
  // available once settings app is registered in the system.
  // For AOSP images we don't have the Play Store app. In last case we Android
  // apps settings consists only from root link to Android settings and only
  // visible once settings app is registered.
  html_source->AddBoolean("androidAppsVisible",
                          arc::IsArcAllowedForProfile(profile));
  html_source->AddBoolean("havePlayStoreApp", arc::IsPlayStoreAvailable());

  // TODO(mash): Support Chrome power settings in Mash. https://crbug.com/644348
  bool enable_power_settings = !::features::IsMultiProcessMash();
  html_source->AddBoolean("enablePowerSettings", enable_power_settings);
  if (enable_power_settings) {
    web_ui->AddMessageHandler(
        std::make_unique<chromeos::settings::PowerHandler>(
            profile->GetPrefs()));
  }
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
