// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/fake_web_app_registry_controller.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_database_factory.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "third_party/skia/include/core/SkColor.h"

namespace web_app {

FakeWebAppRegistryController::FakeWebAppRegistryController() = default;

FakeWebAppRegistryController::~FakeWebAppRegistryController() = default;

void FakeWebAppRegistryController::SetUp(Profile* profile) {
  database_factory_ = std::make_unique<FakeWebAppDatabaseFactory>();
  mutable_registrar_ = std::make_unique<WebAppRegistrarMutable>(profile);

  os_integration_manager_ = std::make_unique<FakeOsIntegrationManager>(
      profile, /*app_shortcut_manager=*/nullptr,
      /*file_handler_manager=*/nullptr,
      /*protocol_handler_manager=*/nullptr,
      /*url_handler_manager=*/nullptr);

  sync_bridge_ = std::make_unique<WebAppSyncBridge>(
      database_factory_.get(), mutable_registrar_.get(), this,
      mock_processor_.CreateForwardingProcessor());

  ON_CALL(processor(), IsTrackingMetadata())
      .WillByDefault(testing::Return(true));
}

void FakeWebAppRegistryController::Init() {
  base::RunLoop run_loop;
  sync_bridge_->Init(run_loop.QuitClosure());
  run_loop.Run();
}

void FakeWebAppRegistryController::RegisterApp(
    std::unique_ptr<WebApp> web_app) {
  ScopedRegistryUpdate update(sync_bridge_.get());
  update->CreateApp(std::move(web_app));
}

void FakeWebAppRegistryController::UnregisterApp(const AppId& app_id) {
  ScopedRegistryUpdate update(sync_bridge_.get());
  update->DeleteApp(app_id);
}

void FakeWebAppRegistryController::UnregisterAll() {
  ScopedRegistryUpdate update(sync_bridge_.get());
  for (const AppId& app_id : registrar().GetAppIds())
    update->DeleteApp(app_id);
}

void FakeWebAppRegistryController::SetInstallWebAppsAfterSyncDelegate(
    InstallWebAppsAfterSyncDelegate delegate) {
  install_web_apps_after_sync_delegate_ = delegate;
}

void FakeWebAppRegistryController::
    SetUninstallWithoutRegistryUpdateFromSyncDelegate(
        UninstallWithoutRegistryUpdateFromSyncDelegate delegate) {
  uninstall_from_sync_before_registry_update_delegate_ = delegate;
}

void FakeWebAppRegistryController::SetRetryIncompleteUninstallsDelegate(
    RetryIncompleteUninstallsDelegate delegate) {
  retry_incomplete_uninstalls_delegate_ = delegate;
}

void FakeWebAppRegistryController::InstallWebAppsAfterSync(
    std::vector<WebApp*> web_apps,
    RepeatingInstallCallback callback) {
  if (install_web_apps_after_sync_delegate_) {
    install_web_apps_after_sync_delegate_.Run(std::move(web_apps), callback);
  } else {
    for (WebApp* web_app : web_apps)
      callback.Run(web_app->app_id(), InstallResultCode::kSuccessNewInstall);
  }
}

void FakeWebAppRegistryController::UninstallWithoutRegistryUpdateFromSync(
    const std::vector<AppId>& web_apps,
    RepeatingUninstallCallback callback) {
  if (uninstall_from_sync_before_registry_update_delegate_) {
    uninstall_from_sync_before_registry_update_delegate_.Run(web_apps,
                                                             callback);
  } else {
    for (const AppId& web_app : web_apps) {
      callback.Run(web_app, /*uninstalled=*/true);
    }
  }
}

void FakeWebAppRegistryController::RetryIncompleteUninstalls(
    const std::vector<AppId>& apps_to_uninstall) {
  if (retry_incomplete_uninstalls_delegate_)
    retry_incomplete_uninstalls_delegate_.Run(apps_to_uninstall);
}

void FakeWebAppRegistryController::DestroySubsystems() {
  mutable_registrar_.reset();
  sync_bridge_.reset();
  database_factory_.reset();
  os_integration_manager_.reset();
}

}  // namespace web_app
