// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/daily_metrics_helper.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"
#include "chrome/browser/web_applications/file_utils_wrapper.h"
#include "chrome/browser/web_applications/install_bounce_metric.h"
#include "chrome/browser/web_applications/isolation_prefs_utils.h"
#include "chrome/browser/web_applications/manifest_update_manager.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/preinstalled_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/url_handler_manager.h"
#include "chrome/browser/web_applications/url_handler_manager_impl.h"
#include "chrome/browser/web_applications/web_app_audio_focus_id_map.h"
#include "chrome/browser/web_applications/web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app_file_handler_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_mover.h"
#include "chrome/browser/web_applications/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/web_app_protocol_handler_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

WebAppProvider::OsIntegrationManagerFactory
    g_os_integration_manager_factory_for_testing = nullptr;

}  // namespace

// static
WebAppProvider* WebAppProvider::GetDeprecated(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForSystemWebApps(Profile* profile) {
  if (!AreSystemWebAppsSupported())
    return nullptr;

  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForWebApps(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // If features::kWebAppsCrosapi is enabled, Ash browser only manages system
  // web apps (return nullptr here). Otherwise, Ash browser manages all web apps
  // (return WebAppProvider).
  return IsWebAppsCrosapiEnabled()
             ? nullptr
             : WebAppProviderFactory::GetForProfile(profile);
#else
  return WebAppProviderFactory::GetForProfile(profile);
#endif
}

// static
WebAppProvider* WebAppProvider::GetForLocalAppsUnchecked(Profile* profile) {
  return WebAppProviderFactory::GetForProfile(profile);
}

// static
WebAppProvider* WebAppProvider::GetForTest(Profile* profile) {
  WebAppProvider* provider = GetForLocalAppsUnchecked(profile);

  if (provider->on_registry_ready().is_signaled())
    return provider;

  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  return provider;
}

// static
WebAppProvider* WebAppProvider::GetForWebContents(
    content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  DCHECK(profile);
  return WebAppProvider::GetForLocalAppsUnchecked(profile);
}

// static
void WebAppProvider::SetOsIntegrationManagerFactoryForTesting(
    OsIntegrationManagerFactory factory) {
  g_os_integration_manager_factory_for_testing = factory;
}

WebAppProvider::WebAppProvider(Profile* profile) : profile_(profile) {
  DCHECK(AreWebAppsEnabled(profile_));
  // WebApp System must have only one instance in original profile.
  // Exclude secondary off-the-record profiles.
  DCHECK(!profile_->IsOffTheRecord());

  CreateSubsystems(profile_);
}

WebAppProvider::~WebAppProvider() = default;

void WebAppProvider::Start() {
  CHECK(!started_);

  ConnectSubsystems();
  started_ = true;

  StartImpl();
}

WebAppRegistrar& WebAppProvider::registrar() {
  CheckIsConnected();
  return *registrar_;
}

const WebAppRegistrar& WebAppProvider::registrar() const {
  CheckIsConnected();
  return *registrar_;
}

WebAppSyncBridge& WebAppProvider::sync_bridge() {
  CheckIsConnected();
  return *sync_bridge_;
}

WebAppInstallManager& WebAppProvider::install_manager() {
  CheckIsConnected();
  return *install_manager_;
}

WebAppInstallFinalizer& WebAppProvider::install_finalizer() {
  CheckIsConnected();
  return *install_finalizer_;
}

ManifestUpdateManager& WebAppProvider::manifest_update_manager() {
  CheckIsConnected();
  return *manifest_update_manager_;
}

ExternallyManagedAppManager& WebAppProvider::externally_managed_app_manager() {
  CheckIsConnected();
  return *externally_managed_app_manager_;
}

WebAppPolicyManager& WebAppProvider::policy_manager() {
  CheckIsConnected();
  return *web_app_policy_manager_;
}

WebAppUiManager& WebAppProvider::ui_manager() {
  CheckIsConnected();
  return *ui_manager_;
}

WebAppAudioFocusIdMap& WebAppProvider::audio_focus_id_map() {
  CheckIsConnected();
  return *audio_focus_id_map_;
}

WebAppIconManager& WebAppProvider::icon_manager() {
  CheckIsConnected();
  return *icon_manager_;
}

SystemWebAppManager& WebAppProvider::system_web_app_manager() {
  CheckIsConnected();
  return *system_web_app_manager_;
}

OsIntegrationManager& WebAppProvider::os_integration_manager() {
  CheckIsConnected();
  return *os_integration_manager_;
}

void WebAppProvider::Shutdown() {
  ui_manager_->Shutdown();
  externally_managed_app_manager_->Shutdown();
  manifest_update_manager_->Shutdown();
  system_web_app_manager_->Shutdown();
  install_manager_->Shutdown();
  icon_manager_->Shutdown();
  install_finalizer_->Shutdown();
  registrar_->Shutdown();
  if (web_app_mover_)
    web_app_mover_->Shutdown();
}

void WebAppProvider::StartImpl() {
  if (!skip_awaiting_extension_system_) {
    // Basically the WebAppUiManagerImpl is dependent on ExtensionSystem
    // initialization.
    // TODO(crbug.com/1201878): Make WebAppUiManagerImpl lazily check
    // ExtensionSystem readiness.
    WaitForExtensionSystemReady();
  } else {
    OnExtensionSystemReady();
  }
}

void WebAppProvider::OnExtensionSystemReady() {
  StartSyncBridge();
}

void WebAppProvider::CreateSubsystems(Profile* profile) {
  audio_focus_id_map_ = std::make_unique<WebAppAudioFocusIdMap>();
  ui_manager_ = WebAppUiManager::Create(profile);
  install_manager_ = std::make_unique<WebAppInstallManager>(profile);
  manifest_update_manager_ = std::make_unique<ManifestUpdateManager>();
  externally_managed_app_manager_ =
      std::make_unique<ExternallyManagedAppManagerImpl>(profile);
  preinstalled_web_app_manager_ =
      std::make_unique<PreinstalledWebAppManager>(profile);
  system_web_app_manager_ = std::make_unique<SystemWebAppManager>(profile);
  web_app_policy_manager_ = std::make_unique<WebAppPolicyManager>(profile);

  database_factory_ = std::make_unique<WebAppDatabaseFactory>(profile);

  std::unique_ptr<WebAppRegistrar> registrar;
  std::unique_ptr<WebAppSyncBridge> sync_bridge;

  // Only WebAppSyncBridge must have an access to mutable WebAppRegistrar.
  {
    auto mutable_registrar = std::make_unique<WebAppRegistrarMutable>(profile);

    sync_bridge = std::make_unique<WebAppSyncBridge>(database_factory_.get(),
                                                     mutable_registrar.get(),
                                                     install_manager_.get());

    // Upcast to read-only WebAppRegistrar.
    registrar = std::move(mutable_registrar);
  }

  auto icon_manager = std::make_unique<WebAppIconManager>(
      profile, *registrar, base::MakeRefCounted<FileUtilsWrapper>());
  install_finalizer_ = std::make_unique<WebAppInstallFinalizer>(
      profile, icon_manager.get(), web_app_policy_manager_.get());

  if (g_os_integration_manager_factory_for_testing) {
    os_integration_manager_ =
        g_os_integration_manager_factory_for_testing(profile);
  } else {
    auto file_handler_manager =
        std::make_unique<WebAppFileHandlerManager>(profile);
    auto protocol_handler_manager =
        std::make_unique<WebAppProtocolHandlerManager>(profile);
    auto shortcut_manager = std::make_unique<WebAppShortcutManager>(
        profile, icon_manager.get(), file_handler_manager.get(),
        protocol_handler_manager.get());

    std::unique_ptr<UrlHandlerManager> url_handler_manager;
#if defined(OS_WIN) || defined(OS_MAC) || \
    (defined(OS_LINUX) && !BUILDFLAG(IS_CHROMEOS_LACROS))
    url_handler_manager = std::make_unique<UrlHandlerManagerImpl>(profile);
#endif

    os_integration_manager_ = std::make_unique<OsIntegrationManager>(
        profile, std::move(shortcut_manager), std::move(file_handler_manager),
        std::move(protocol_handler_manager), std::move(url_handler_manager));
  }

  web_app_mover_ = WebAppMover::CreateIfNeeded(
      profile, registrar.get(), install_finalizer_.get(),
      install_manager_.get(), sync_bridge.get());

  registrar_ = std::move(registrar);
  sync_bridge_ = std::move(sync_bridge);
  icon_manager_ = std::move(icon_manager);
}

void WebAppProvider::ConnectSubsystems() {
  DCHECK(!started_);

  install_finalizer_->SetSubsystems(registrar_.get(), ui_manager_.get(),
                                    sync_bridge_.get(),
                                    os_integration_manager_.get());
  install_manager_->SetSubsystems(registrar_.get(),
                                  os_integration_manager_.get(),
                                  install_finalizer_.get());
  manifest_update_manager_->SetSubsystems(
      registrar_.get(), icon_manager_.get(), ui_manager_.get(),
      install_finalizer_.get(), system_web_app_manager_.get(),
      os_integration_manager_.get(), sync_bridge_.get());
  externally_managed_app_manager_->SetSubsystems(
      registrar_.get(), os_integration_manager_.get(), ui_manager_.get(),
      install_finalizer_.get(), install_manager_.get());
  preinstalled_web_app_manager_->SetSubsystems(
      registrar_.get(), ui_manager_.get(),
      externally_managed_app_manager_.get());
  system_web_app_manager_->SetSubsystems(
      externally_managed_app_manager_.get(), registrar_.get(),
      sync_bridge_.get(), ui_manager_.get(), web_app_policy_manager_.get());
  web_app_policy_manager_->SetSubsystems(externally_managed_app_manager_.get(),
                                         registrar_.get(), sync_bridge_.get(),
                                         system_web_app_manager_.get(),
                                         os_integration_manager_.get());
  ui_manager_->SetSubsystems(sync_bridge_.get(), os_integration_manager_.get());
  os_integration_manager_->SetSubsystems(sync_bridge_.get(), registrar_.get(),
                                         ui_manager_.get(),
                                         icon_manager_.get());

  connected_ = true;
}

void WebAppProvider::StartSyncBridge() {
  sync_bridge_->Init(base::BindOnce(&WebAppProvider::OnSyncBridgeReady,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void WebAppProvider::OnSyncBridgeReady() {
  DCHECK(!on_registry_ready_.is_signaled());

  registrar_->Start();
  install_finalizer_->Start();
  icon_manager_->Start();
  install_manager_->Start();
  preinstalled_web_app_manager_->Start();
  web_app_policy_manager_->Start();
  system_web_app_manager_->Start();
  manifest_update_manager_->Start();
  os_integration_manager_->Start();
  ui_manager_->Start();
  if (web_app_mover_)
    web_app_mover_->Start();

  on_registry_ready_.Signal();
}

void WebAppProvider::CheckIsConnected() const {
  DCHECK(connected_) << "Attempted to access Web App subsystem while "
                        "WebAppProvider is not connected.";
}

// static
void WebAppProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  ExternallyInstalledWebAppPrefs::RegisterProfilePrefs(registry);
  PreinstalledWebAppManager::RegisterProfilePrefs(registry);
  WebAppPolicyManager::RegisterProfilePrefs(registry);
  SystemWebAppManager::RegisterProfilePrefs(registry);
  WebAppPrefsUtilsRegisterProfilePrefs(registry);
  IsolationPrefsUtilsRegisterProfilePrefs(registry);
  RegisterInstallBounceMetricProfilePrefs(registry);
  RegisterDailyWebAppMetricsProfilePrefs(registry);
}

}  // namespace web_app
