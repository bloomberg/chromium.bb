// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/externally_managed_app_manager_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/external_install_options.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/externally_managed_app_registration_task.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/test/web_app_registration_waiter.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace web_app {

class ExternallyManagedAppManagerImplBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Allow different origins to be handled by the embedded_test_server.
    host_resolver()->AddRule("*", "127.0.0.1");
    os_hooks_suppress_ =
        OsIntegrationManager::ScopedSuppressOsHooksForTesting();
  }

  WebAppRegistrar& registrar() {
    return WebAppProvider::GetForTest(browser()->profile())->registrar();
  }

  ExternallyManagedAppManager& externally_managed_app_manager() {
    return WebAppProvider::GetForTest(browser()->profile())
        ->externally_managed_app_manager();
  }

  void InstallApp(ExternalInstallOptions install_options) {
    result_code_ = ExternallyManagedAppManagerInstall(browser()->profile(),
                                                      install_options);
  }

  void CheckServiceWorkerStatus(const GURL& url,
                                content::ServiceWorkerCapability status) {
    base::RunLoop run_loop;
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::ServiceWorkerContext* service_worker_context =
        web_contents->GetBrowserContext()
            ->GetStoragePartition(web_contents->GetSiteInstance())
            ->GetServiceWorkerContext();
    service_worker_context->CheckHasServiceWorker(
        url, blink::StorageKey(url::Origin::Create(url)),
        base::BindLambdaForTesting(
            [&run_loop, status](content::ServiceWorkerCapability capability) {
              CHECK_EQ(status, capability);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  absl::optional<InstallResultCode> result_code_;

 private:
  ScopedOsHooksSuppress os_hooks_suppress_;
};

// Basic integration test to make sure the whole flow works. Each step in the
// flow is unit tested separately.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       InstallSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  absl::optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
}

// If install URL redirects, install should still succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       InstallSucceedsWithRedirect) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL start_url =
      embedded_test_server()->GetURL("/banners/manifest_test_page.html");
  GURL install_url =
      embedded_test_server()->GetURL("/server-redirect?" + start_url.spec());
  InstallApp(CreateInstallOptions(install_url));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  absl::optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(install_url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(app_id.value()));
  // Same AppID should be in the registrar using start_url from the manifest.
  EXPECT_TRUE(registrar().IsLocallyInstalled(start_url));
  absl::optional<AppId> opt_app_id =
      registrar().FindAppWithUrlInScope(start_url);
  EXPECT_TRUE(opt_app_id.has_value());
  EXPECT_EQ(*opt_app_id, app_id);
}

// If install URL redirects, install should still succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       InstallSucceedsWithRedirectNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL final_url =
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html");
  GURL install_url =
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec());
  InstallApp(CreateInstallOptions(install_url));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  absl::optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(install_url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_EQ("Web app banner test page",
            registrar().GetAppShortName(app_id.value()));
  // Same AppID should be in the registrar using install_url.
  EXPECT_TRUE(registrar().IsLocallyInstalled(install_url));
  absl::optional<AppId> opt_app_id =
      registrar().FindAppWithUrlInScope(install_url);
  ASSERT_TRUE(opt_app_id.has_value());
  EXPECT_EQ(*opt_app_id, app_id);
  EXPECT_EQ(registrar().GetAppStartUrl(*opt_app_id), install_url);
}

// Installing a placeholder app with shortcuts should succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       PlaceholderInstallSucceedsWithShortcuts) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL(
      "other.origin.com", "/banners/manifest_test_page.html");
  // Add a redirect to a different origin, so a placeholder is installed.
  GURL url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));

  ExternalInstallOptions options = CreateInstallOptions(url);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  InstallApp(options);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  absl::optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(registrar().IsPlaceholderApp(app_id.value()));
}

// Installing a placeholder app with shortcuts should succeed.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       PlaceholderInstallSucceedsWithCustomName) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL final_url = embedded_test_server()->GetURL(
      "other.origin.com", "/banners/manifest_test_page.html");
  // Add a redirect to a different origin, so a placeholder is installed.
  GURL url(
      embedded_test_server()->GetURL("/server-redirect?" + final_url.spec()));
  const std::string CUSTOM_NAME = "CUSTOM_NAME";

  ExternalInstallOptions options = CreateInstallOptions(url);
  options.install_placeholder = true;
  options.add_to_applications_menu = true;
  options.add_to_desktop = true;
  options.placeholder_name = CUSTOM_NAME;
  InstallApp(options);

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  absl::optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_TRUE(app_id.has_value());
  EXPECT_TRUE(registrar().IsPlaceholderApp(app_id.value()));
  EXPECT_EQ(CUSTOM_NAME, registrar().GetAppById(app_id.value())->name());
}

// Tests that the browser doesn't crash if it gets shutdown with a pending
// installation.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       ShutdownWithPendingInstallation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ExternalInstallOptions install_options = CreateInstallOptions(
      embedded_test_server()->GetURL("/banners/manifest_test_page.html"));

  // Start an installation but don't wait for it to finish.
  WebAppProvider::GetForTest(browser()->profile())
      ->externally_managed_app_manager()
      .Install(std::move(install_options), base::DoNothing());

  // The browser should shutdown cleanly even if there is a pending
  // installation.
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       BypassServiceWorkerCheck) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.bypass_service_worker_check = true;
  InstallApp(std::move(install_options));
  absl::optional<AppId> app_id = registrar().FindAppWithUrlInScope(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_TRUE(registrar().GetAppScopeInternal(*app_id).has_value());
  EXPECT_EQ("Manifest test app", registrar().GetAppShortName(*app_id));
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       PerformServiceWorkerCheck) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  InstallApp(std::move(install_options));
  absl::optional<AppId> app_id = registrar().FindAppWithUrlInScope(url);
  EXPECT_TRUE(app_id.has_value());
  EXPECT_TRUE(registrar().GetAppScopeInternal(app_id.value()).has_value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       ForceReinstall) {
  ASSERT_TRUE(embedded_test_server()->Start());
  absl::optional<AppId> app_id;
  {
    GURL url(embedded_test_server()->GetURL(
        "/banners/"
        "manifest_test_page.html?manifest=manifest_short_name_only.json"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    app_id = registrar().FindAppWithUrlInScope(url);
    EXPECT_TRUE(app_id.has_value());
    EXPECT_EQ("Manifest", registrar().GetAppShortName(app_id.value()));
  }
  {
    GURL url(
        embedded_test_server()->GetURL("/banners/manifest_test_page.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));

    absl::optional<AppId> new_app_id = registrar().FindAppWithUrlInScope(url);
    EXPECT_TRUE(new_app_id.has_value());
    EXPECT_EQ(new_app_id, app_id);
    EXPECT_EQ("Manifest test app",
              registrar().GetAppShortName(new_app_id.value()));
  }
}

// Test that adding a manifest that points to a chrome:// URL does not actually
// install a web app that points to a chrome:// URL.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       InstallChromeURLFails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=manifest_chrome_url.json"));
  InstallApp(CreateInstallOptions(url));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  absl::optional<AppId> app_id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_TRUE(app_id.has_value());

  // The installer falls back to installing a web app of the original URL.
  EXPECT_EQ(url, registrar().GetAppStartUrl(app_id.value()));
  EXPECT_NE(app_id,
            registrar().FindAppWithUrlInScope(GURL("chrome://settings")));
}

// Test that adding a web app without a manifest while using the
// |require_manifest| flag fails.
IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       RequireManifestFailsIfNoManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(
      embedded_test_server()->GetURL("/banners/no_manifest_test_page.html"));
  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.require_manifest = true;
  InstallApp(std::move(install_options));

  EXPECT_EQ(InstallResultCode::kNotValidManifestForWebApp,
            result_code_.value());
  absl::optional<AppId> id =
      ExternallyInstalledWebAppPrefs(browser()->profile()->GetPrefs())
          .LookupAppId(url);
  ASSERT_FALSE(id.has_value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       RegistrationSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Delay service worker registration to second load to simulate it not loading
  // during the initial install pass.
  GURL install_url(embedded_test_server()->GetURL(
      "/web_apps/service_worker_on_second_load.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.bypass_service_worker_check = true;
  InstallApp(std::move(install_options));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  WebAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(install_url, RegistrationResultCode::kSuccess);
  CheckServiceWorkerStatus(
      install_url,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       RegistrationAlternateUrlSucceeds) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL install_url(
      embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
  GURL registration_url =
      embedded_test_server()->GetURL("/web_apps/basic.html");

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.bypass_service_worker_check = true;
  install_options.service_worker_registration_url = registration_url;
  InstallApp(std::move(install_options));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  WebAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(registration_url,
                             RegistrationResultCode::kSuccess);
  CheckServiceWorkerStatus(
      install_url,
      content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       RegistrationSkipped) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Delay service worker registration to second load to simulate it not loading
  // during the initial install pass.
  GURL install_url(embedded_test_server()->GetURL(
      "/web_apps/service_worker_on_second_load.html"));

  ExternalInstallOptions install_options = CreateInstallOptions(install_url);
  install_options.bypass_service_worker_check = true;
  install_options.load_and_await_service_worker_registration = false;
  WebAppRegistrationWaiter waiter(&externally_managed_app_manager());
  InstallApp(std::move(install_options));
  waiter.AwaitRegistrationsComplete();

  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  CheckServiceWorkerStatus(install_url,
                           content::ServiceWorkerCapability::NO_SERVICE_WORKER);
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       AlreadyRegistered) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Ensure service worker registered for http://embedded_test_server/web_apps/.
  // We don't need to be installing a web app here but it's convenient just to
  // await the service worker registration.
  {
    GURL install_url(embedded_test_server()->GetURL("/web_apps/basic.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(install_url);
    install_options.force_reinstall = true;
    InstallApp(std::move(install_options));
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
    WebAppRegistrationWaiter(&externally_managed_app_manager())
        .AwaitNextNonFailedRegistration(install_url);
    CheckServiceWorkerStatus(
        embedded_test_server()->GetURL("/web_apps/basic.html"),
        content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER);
  }

  // With the service worker registered we install a page that doesn't register
  // a service worker to check that the existing service worker is seen by our
  // service worker registration step.
  {
    GURL install_url(
        embedded_test_server()->GetURL("/web_apps/no_service_worker.html"));
    ExternalInstallOptions install_options = CreateInstallOptions(install_url);
    install_options.force_reinstall = true;
    install_options.bypass_service_worker_check = true;
    InstallApp(std::move(install_options));
    EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
    WebAppRegistrationWaiter(&externally_managed_app_manager())
        .AwaitNextRegistration(install_url,
                               RegistrationResultCode::kAlreadyRegistered);
  }
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       CannotFetchManifest) {
  // With a flaky network connection, clients may request an app whose manifest
  // cannot currently be retrieved. The app display mode is then assumed to be
  // 'browser'.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL app_url(embedded_test_server()->GetURL(
      "/banners/manifest_test_page.html?manifest=does_not_exist.json"));

  std::vector<ExternalInstallOptions> desired_apps_install_options;
  {
    ExternalInstallOptions install_options(
        app_url, DisplayMode::kStandalone,
        ExternalInstallSource::kExternalPolicy);
    install_options.add_to_applications_menu = false;
    install_options.add_to_desktop = false;
    install_options.add_to_quick_launch_bar = false;
    install_options.require_manifest = false;
    desired_apps_install_options.push_back(std::move(install_options));
  }

  base::RunLoop run_loop;
  externally_managed_app_manager().SynchronizeInstalledApps(
      std::move(desired_apps_install_options),
      ExternalInstallSource::kExternalPolicy,
      base::BindLambdaForTesting(
          [&run_loop, &app_url](
              std::map<GURL, ExternallyManagedAppManager::InstallResult>
                  install_results,
              std::map<GURL, bool> uninstall_results) {
            EXPECT_TRUE(uninstall_results.empty());
            EXPECT_EQ(install_results.size(), 1U);
            EXPECT_EQ(install_results[app_url].code,
                      InstallResultCode::kSuccessNewInstall);
            run_loop.Quit();
          }));
  run_loop.Run();

  absl::optional<AppId> app_id = registrar().FindAppWithUrlInScope(app_url);
  DCHECK(app_id.has_value());
  EXPECT_EQ(registrar().GetAppDisplayMode(*app_id), DisplayMode::kBrowser);
  EXPECT_EQ(registrar().GetAppUserDisplayMode(*app_id),
            DisplayMode::kStandalone);
  EXPECT_EQ(registrar().GetAppEffectiveDisplayMode(*app_id),
            DisplayMode::kMinimalUi);
  EXPECT_FALSE(registrar().GetAppThemeColor(*app_id).has_value());
}

IN_PROC_BROWSER_TEST_F(ExternallyManagedAppManagerImplBrowserTest,
                       RegistrationTimeout) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ExternallyManagedAppRegistrationTask::SetTimeoutForTesting(0);
  GURL url(embedded_test_server()->GetURL(
      "/banners/manifest_no_service_worker.html"));
  CheckServiceWorkerStatus(url,
                           content::ServiceWorkerCapability::NO_SERVICE_WORKER);

  ExternalInstallOptions install_options = CreateInstallOptions(url);
  install_options.bypass_service_worker_check = true;
  InstallApp(std::move(install_options));
  EXPECT_EQ(InstallResultCode::kSuccessNewInstall, result_code_.value());
  WebAppRegistrationWaiter(&externally_managed_app_manager())
      .AwaitNextRegistration(url, RegistrationResultCode::kTimeout);
}

}  // namespace web_app
