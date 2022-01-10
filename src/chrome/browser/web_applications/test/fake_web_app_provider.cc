// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_provider.h"

#include <utility>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/policy/web_app_policy_manager.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/test_system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider_factory.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"

namespace web_app {

// static
std::unique_ptr<KeyedService> FakeWebAppProvider::BuildDefault(
    content::BrowserContext* context) {
  auto provider = std::make_unique<FakeWebAppProvider>(
      Profile::FromBrowserContext(context));

  // Do not call default production StartImpl if in TestingProfile.
  provider->SetRunSubsystemStartupTasks(false);

  // TODO(crbug.com/973324): Replace core subsystems with fakes by default.
  provider->ConnectSubsystems();

  return provider;
}

// static
FakeWebAppProvider* FakeWebAppProvider::Get(Profile* profile) {
  CHECK(profile->AsTestingProfile());
  auto* test_provider = static_cast<FakeWebAppProvider*>(
      WebAppProvider::GetForLocalAppsUnchecked(profile));
  CHECK(!test_provider->started_);

  // Disconnect so that clients are forced to call Start() before accessing any
  // subsystems.
  test_provider->connected_ = false;

  return test_provider;
}

FakeWebAppProvider::FakeWebAppProvider(Profile* profile)
    : WebAppProvider(profile) {}

FakeWebAppProvider::~FakeWebAppProvider() = default;

void FakeWebAppProvider::SetRunSubsystemStartupTasks(
    bool run_subsystem_startup_tasks) {
  run_subsystem_startup_tasks_ = run_subsystem_startup_tasks;
}

void FakeWebAppProvider::SetRegistrar(
    std::unique_ptr<WebAppRegistrar> registrar) {
  CheckNotStarted();
  registrar_ = std::move(registrar);
}

void FakeWebAppProvider::SetSyncBridge(
    std::unique_ptr<WebAppSyncBridge> sync_bridge) {
  CheckNotStarted();
  sync_bridge_ = std::move(sync_bridge);
}

void FakeWebAppProvider::SetInstallManager(
    std::unique_ptr<WebAppInstallManager> install_manager) {
  CheckNotStarted();
  install_manager_ = std::move(install_manager);
}

void FakeWebAppProvider::SetInstallFinalizer(
    std::unique_ptr<WebAppInstallFinalizer> install_finalizer) {
  CheckNotStarted();
  install_finalizer_ = std::move(install_finalizer);
}

void FakeWebAppProvider::SetExternallyManagedAppManager(
    std::unique_ptr<ExternallyManagedAppManager>
        externally_managed_app_manager) {
  CheckNotStarted();
  externally_managed_app_manager_ = std::move(externally_managed_app_manager);
}

void FakeWebAppProvider::SetWebAppUiManager(
    std::unique_ptr<WebAppUiManager> ui_manager) {
  CheckNotStarted();
  ui_manager_ = std::move(ui_manager);
}

void FakeWebAppProvider::SetSystemWebAppManager(
    std::unique_ptr<SystemWebAppManager> system_web_app_manager) {
  CheckNotStarted();
  system_web_app_manager_ = std::move(system_web_app_manager);
}

void FakeWebAppProvider::SetWebAppPolicyManager(
    std::unique_ptr<WebAppPolicyManager> web_app_policy_manager) {
  CheckNotStarted();
  web_app_policy_manager_ = std::move(web_app_policy_manager);
}

void FakeWebAppProvider::SetOsIntegrationManager(
    std::unique_ptr<OsIntegrationManager> os_integration_manager) {
  CheckNotStarted();
  os_integration_manager_ = std::move(os_integration_manager);
}

void FakeWebAppProvider::SkipAwaitingExtensionSystem() {
  CheckNotStarted();
  skip_awaiting_extension_system_ = true;
}

WebAppRegistrarMutable& FakeWebAppProvider::GetRegistrarMutable() const {
  DCHECK(registrar_);
  return *static_cast<WebAppRegistrarMutable*>(registrar_.get());
}

WebAppIconManager& FakeWebAppProvider::GetIconManager() const {
  DCHECK(icon_manager_);
  return *icon_manager_;
}

void FakeWebAppProvider::CheckNotStarted() const {
  CHECK(!started_) << "Attempted to set a WebAppProvider subsystem after "
                      "Start() was called.";
}

void FakeWebAppProvider::StartImpl() {
  if (run_subsystem_startup_tasks_)
    WebAppProvider::StartImpl();
  else
    on_registry_ready_.Signal();
}

FakeWebAppProviderCreator::FakeWebAppProviderCreator(
    CreateWebAppProviderCallback callback)
    : callback_(std::move(callback)) {
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &FakeWebAppProviderCreator::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

FakeWebAppProviderCreator::~FakeWebAppProviderCreator() = default;

void FakeWebAppProviderCreator::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  WebAppProviderFactory::GetInstance()->SetTestingFactory(
      context,
      base::BindRepeating(&FakeWebAppProviderCreator::CreateWebAppProvider,
                          base::Unretained(this)));
}

std::unique_ptr<KeyedService> FakeWebAppProviderCreator::CreateWebAppProvider(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!AreWebAppsEnabled(profile) || !callback_)
    return nullptr;
  return callback_.Run(profile);
}

}  // namespace web_app
