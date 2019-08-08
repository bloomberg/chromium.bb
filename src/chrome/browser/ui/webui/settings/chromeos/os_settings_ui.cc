// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/webui/certificates_handler.h"
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
#include "chrome/browser/ui/webui/settings/languages_handler.h"
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
#include "chrome/browser/ui/webui/settings/settings_security_key_handler.h"
#include "chrome/browser/ui/webui/settings/settings_startup_pages_handler.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/settings/site_settings_handler.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/os_settings_resources.h"
#include "chrome/grit/os_settings_resources_map.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/unified_consent/feature.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"

#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/chrome_password_protection_service.h"
#include "chrome/browser/ui/webui/settings/change_password_handler.h"
#endif

namespace chromeos {
namespace settings {

// static
void OSSettingsUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(::prefs::kImportDialogAutofillFormData, true);
  registry->RegisterBooleanPref(::prefs::kImportDialogBookmarks, true);
  registry->RegisterBooleanPref(::prefs::kImportDialogHistory, true);
  registry->RegisterBooleanPref(::prefs::kImportDialogSavedPasswords, true);
  registry->RegisterBooleanPref(::prefs::kImportDialogSearchEngine, true);
}

OSSettingsUI::OSSettingsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui),
      WebContentsObserver(web_ui->GetWebContents()) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIOSSettingsHost);

  ::settings::SettingsUI::InitOSWebUIHandlers(profile, web_ui, html_source);

  // TODO(jamescook): Remove after basic_page.html is forked for OS settings.
  html_source->AddBoolean("showOSSettings", true);

#if BUILDFLAG(OPTIMIZE_WEBUI)
  std::vector<std::string> exclude_from_gzip;
#endif

  AddSettingsPageUIHandler(
      std::make_unique<::settings::AppearanceHandler>(web_ui));

#if defined(USE_NSS_CERTS)
  AddSettingsPageUIHandler(
      std::make_unique<certificate_manager::CertificatesHandler>());
#elif defined(OS_WIN) || defined(OS_MACOSX)
  AddSettingsPageUIHandler(std::make_unique<NativeCertificatesHandler>());
#endif  // defined(USE_NSS_CERTS)

  AddSettingsPageUIHandler(
      std::make_unique<::settings::AccessibilityMainHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::BrowserLifetimeHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ClearBrowsingDataHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<::settings::CookiesViewHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::DownloadsHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ExtensionControlHandler>());
  AddSettingsPageUIHandler(std::make_unique<::settings::FontHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<::settings::ImportDataHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::LanguagesHandler>(web_ui));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::MediaDevicesSelectionHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::OnStartupHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::PeopleHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ProfileInfoHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::ProtocolHandlersHandler>());
  AddSettingsPageUIHandler(
      std::make_unique<::settings::SearchEnginesHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::SiteSettingsHandler>(profile));
  AddSettingsPageUIHandler(
      std::make_unique<::settings::StartupPagesHandler>(web_ui));
  AddSettingsPageUIHandler(std::make_unique<::settings::SecurityKeysHandler>());

  bool password_protection_available = false;
#if defined(FULL_SAFE_BROWSING)
  safe_browsing::ChromePasswordProtectionService* password_protection =
      safe_browsing::ChromePasswordProtectionService::
          GetPasswordProtectionService(profile);
  password_protection_available = !!password_protection;
  if (password_protection) {
    AddSettingsPageUIHandler(
        std::make_unique<::settings::ChangePasswordHandler>(
            profile, password_protection));
  }
#endif

  html_source->AddBoolean("passwordProtectionAvailable",
                          password_protection_available);

  html_source->AddBoolean("unifiedConsentEnabled",
                          unified_consent::IsUnifiedConsentFeatureEnabled());

  html_source->AddBoolean(
      "navigateToGooglePasswordManager",
      ShouldManagePasswordsinGooglePasswordManager(profile));

  html_source->AddBoolean("showImportPasswords",
                          base::FeatureList::IsEnabled(
                              password_manager::features::kPasswordImport));

  AddSettingsPageUIHandler(
      base::WrapUnique(::settings::AboutHandler::Create(html_source, profile)));
  AddSettingsPageUIHandler(base::WrapUnique(
      ::settings::ResetSettingsHandler::Create(html_source, profile)));

  // Add the metrics handler to write uma stats.
  web_ui->AddMessageHandler(std::make_unique<MetricsHandler>());

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

#if BUILDFLAG(OPTIMIZE_WEBUI)
  const bool use_polymer_2 =
      base::FeatureList::IsEnabled(::features::kWebUIPolymer2);
  html_source->AddResourcePath("crisper.js", IDR_OS_SETTINGS_CRISPER_JS);
  html_source->AddResourcePath("lazy_load.crisper.js",
                               IDR_OS_SETTINGS_LAZY_LOAD_CRISPER_JS);
  html_source->AddResourcePath(
      "lazy_load.html", use_polymer_2
                            ? IDR_OS_SETTINGS_LAZY_LOAD_VULCANIZED_P2_HTML
                            : IDR_OS_SETTINGS_LAZY_LOAD_VULCANIZED_HTML);
  html_source->SetDefaultResource(use_polymer_2
                                      ? IDR_OS_SETTINGS_VULCANIZED_P2_HTML
                                      : IDR_OS_SETTINGS_VULCANIZED_HTML);
  html_source->UseGzip(base::BindRepeating(
      [](const std::vector<std::string>& excluded_paths,
         const std::string& path) {
        return !base::ContainsValue(excluded_paths, path);
      },
      std::move(exclude_from_gzip)));
  html_source->AddResourcePath("manifest.json", IDR_OS_SETTINGS_MANIFEST);
#else
  // Add all settings resources.
  for (size_t i = 0; i < kOsSettingsResourcesSize; ++i) {
    html_source->AddResourcePath(kOsSettingsResources[i].name,
                                 kOsSettingsResources[i].value);
  }
  html_source->SetDefaultResource(IDR_OS_SETTINGS_SETTINGS_HTML);
#endif

  ::settings::AddLocalizedStrings(html_source, profile);

  DarkModeHandler::Initialize(web_ui, html_source);
  ManagedUIHandler::Initialize(web_ui, html_source);

  content::WebUIDataSource::Add(web_ui->GetWebContents()->GetBrowserContext(),
                                html_source);
}

OSSettingsUI::~OSSettingsUI() {}

void OSSettingsUI::AddSettingsPageUIHandler(
    std::unique_ptr<content::WebUIMessageHandler> handler) {
  DCHECK(handler);
  web_ui()->AddMessageHandler(std::move(handler));
}

void OSSettingsUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  load_start_time_ = base::Time::Now();
}

void OSSettingsUI::DocumentLoadedInFrame(
    content::RenderFrameHost* render_frame_host) {
  // TODO(crbug/950007): Create new load histograms
}

void OSSettingsUI::DocumentOnLoadCompletedInMainFrame() {
  // TODO(crbug/950007): Create new load histograms
}

}  // namespace settings
}  // namespace chromeos
