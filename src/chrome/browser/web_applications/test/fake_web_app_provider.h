// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_

#include <memory>

#include "base/callback.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace web_app {

class WebAppRegistrar;
class OsIntegrationManager;
class WebAppInstallFinalizer;
class ExternallyManagedAppManager;
class SystemWebAppManager;
class WebAppInstallManager;
class WebAppPolicyManager;
class WebAppIconManager;

class FakeWebAppProvider : public WebAppProvider {
 public:
  // Used by the TestingProfile in unit tests.
  // Builds a stub WebAppProvider which will not fire subsystem startup tasks.
  // Use FakeWebAppProvider::Get() to replace subsystems.
  static std::unique_ptr<KeyedService> BuildDefault(
      content::BrowserContext* context);

  // Gets a FakeWebAppProvider that can have its subsystems set. This should
  // only be called once during SetUp(), and clients must call Start() before
  // using the subsystems.
  static FakeWebAppProvider* Get(Profile* profile);

  explicit FakeWebAppProvider(Profile* profile);
  ~FakeWebAppProvider() override;

  // |run_subsystem_startup_tasks| is true by default as browser test clients
  // will generally want to construct their FakeWebAppProvider to behave as it
  // would in a production browser.
  //
  // |run_subsystem_startup_tasks| is false by default for FakeWebAppProvider
  // if it's a part of TestingProfile (see BuildDefault() method above).
  void SetRunSubsystemStartupTasks(bool run_subsystem_startup_tasks);

  void SetRegistrar(std::unique_ptr<WebAppRegistrar> registrar);
  void SetSyncBridge(std::unique_ptr<WebAppSyncBridge> sync_bridge);
  void SetOsIntegrationManager(
      std::unique_ptr<OsIntegrationManager> os_integration_manager);
  void SetInstallManager(std::unique_ptr<WebAppInstallManager> install_manager);
  void SetInstallFinalizer(
      std::unique_ptr<WebAppInstallFinalizer> install_finalizer);
  void SetExternallyManagedAppManager(
      std::unique_ptr<ExternallyManagedAppManager>
          externally_managed_app_manager);
  void SetWebAppUiManager(std::unique_ptr<WebAppUiManager> ui_manager);
  void SetSystemWebAppManager(
      std::unique_ptr<SystemWebAppManager> system_web_app_manager);
  void SetWebAppPolicyManager(
      std::unique_ptr<WebAppPolicyManager> web_app_policy_manager);
  void SkipAwaitingExtensionSystem();

  // These getters can be called at any time: no
  // WebAppProvider::CheckIsConnected() check performed. See
  // WebAppProvider::ConnectSubsystems().
  //
  // A mutable view must be accessible only in tests.
  WebAppRegistrarMutable& GetRegistrarMutable() const;
  WebAppIconManager& GetIconManager() const;

 private:
  void CheckNotStarted() const;

  // WebAppProvider:
  void StartImpl() override;

  // If true, when Start()ed the FakeWebAppProvider will call
  // WebAppProvider::StartImpl() and fire startup tasks like a real
  // WebAppProvider.
  bool run_subsystem_startup_tasks_ = true;
};

// Used in BrowserTests to ensure that the WebAppProvider that is create on
// profile startup is the FakeWebAppProvider. Hooks into the
// BrowserContextKeyedService initialization pipeline.
class FakeWebAppProviderCreator {
 public:
  using OnceCreateWebAppProviderCallback =
      base::OnceCallback<std::unique_ptr<KeyedService>(Profile* profile)>;
  using CreateWebAppProviderCallback =
      base::RepeatingCallback<std::unique_ptr<KeyedService>(Profile* profile)>;

  explicit FakeWebAppProviderCreator(OnceCreateWebAppProviderCallback callback);
  explicit FakeWebAppProviderCreator(CreateWebAppProviderCallback callback);
  ~FakeWebAppProviderCreator();

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  std::unique_ptr<KeyedService> CreateWebAppProvider(
      content::BrowserContext* context);

  CreateWebAppProviderCallback callback_;

  base::CallbackListSubscription create_services_subscription_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_FAKE_WEB_APP_PROVIDER_H_
