// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"

#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "net/dns/mock_host_resolver.h"

namespace web_app {

std::string ControllerTypeParamToString(
    const ::testing::TestParamInfo<ControllerType>& controller_type) {
  switch (controller_type.param) {
    case ControllerType::kHostedAppController:
      return "HostedAppController";
    case ControllerType::kUnifiedControllerWithBookmarkApp:
      return "UnifiedControllerWithBookmarkApp";
    case ControllerType::kUnifiedControllerWithWebApp:
      return "UnifiedControllerWithWebApp";
  }
}

WebAppControllerBrowserTestBase::WebAppControllerBrowserTestBase() {
  if (GetParam() == ControllerType::kUnifiedControllerWithWebApp) {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsWithoutExtensions}, {});
  } else if (GetParam() == ControllerType::kUnifiedControllerWithBookmarkApp) {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsUnifiedUiController},
        {features::kDesktopPWAsWithoutExtensions});
  } else {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kDesktopPWAsUnifiedUiController,
             features::kDesktopPWAsWithoutExtensions});
  }
}

WebAppControllerBrowserTestBase::~WebAppControllerBrowserTestBase() = default;

WebAppProviderBase& WebAppControllerBrowserTestBase::provider() {
  auto* provider = WebAppProviderBase::GetProviderBase(profile());
  DCHECK(provider);
  return *provider;
}

AppId WebAppControllerBrowserTestBase::InstallPWA(const GURL& app_url) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->app_url = app_url;
  web_app_info->scope = app_url.GetWithoutFilename();
  web_app_info->open_as_window = true;
  web_app_info->title = base::ASCIIToUTF16("A Web App");
  return web_app::InstallWebApp(profile(), std::move(web_app_info));
}

AppId WebAppControllerBrowserTestBase::InstallWebApp(
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  return web_app::InstallWebApp(profile(), std::move(web_app_info));
}

Browser* WebAppControllerBrowserTestBase::LaunchWebAppBrowser(
    const AppId& app_id) {
  return web_app::LaunchWebAppBrowser(profile(), app_id);
}

Browser* WebAppControllerBrowserTestBase::LaunchWebAppBrowserAndWait(
    const AppId& app_id) {
  return web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
}

Browser*
WebAppControllerBrowserTestBase::LaunchWebAppBrowserAndAwaitInstallabilityCheck(
    const AppId& app_id) {
  Browser* browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  banners::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents())
      ->WaitForInstallableCheck();
  return browser;
}

Browser* WebAppControllerBrowserTestBase::LaunchBrowserForWebAppInTab(
    const AppId& app_id) {
  return web_app::LaunchBrowserForWebAppInTab(profile(), app_id);
}

// static
bool WebAppControllerBrowserTestBase::NavigateAndAwaitInstallabilityCheck(
    Browser* browser,
    const GURL& url) {
  auto* manager = banners::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  NavigateToURLAndWait(browser, url);
  return manager->WaitForInstallableCheck();
}

Browser*
WebAppControllerBrowserTestBase::NavigateInNewWindowAndAwaitInstallabilityCheck(
    const GURL& url) {
  Browser* new_browser =
      new Browser(Browser::CreateParams(Browser::TYPE_NORMAL, profile(), true));
  AddBlankTabAndShow(new_browser);
  NavigateAndAwaitInstallabilityCheck(new_browser, url);
  return new_browser;
}

base::Optional<AppId> WebAppControllerBrowserTestBase::FindAppWithUrlInScope(
    const GURL& url) {
  return provider().registrar().FindAppWithUrlInScope(url);
}

WebAppControllerBrowserTest::WebAppControllerBrowserTest()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  scoped_feature_list_.InitWithFeatures(
      {}, {predictors::kSpeculativePreconnectFeature});
}

WebAppControllerBrowserTest::~WebAppControllerBrowserTest() = default;

void WebAppControllerBrowserTest::SetUp() {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  banners::TestAppBannerManagerDesktop::SetUp();

  extensions::ExtensionBrowserTest::SetUp();
}

content::WebContents* WebAppControllerBrowserTest::OpenApplication(
    const AppId& app_id) {
  ui_test_utils::UrlLoadObserver url_observer(
      provider().registrar().GetAppLaunchURL(app_id),
      content::NotificationService::AllSources());

  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceTest);
  content::WebContents* contents =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->BrowserAppLauncher()
          .LaunchAppWithParams(params);
  url_observer.Wait();
  return contents;
}

GURL WebAppControllerBrowserTest::GetInstallableAppURL() {
  return https_server()->GetURL("/banners/manifest_test_page.html");
}

// static
const char* WebAppControllerBrowserTest::GetInstallableAppName() {
  return "Manifest test app";
}

void WebAppControllerBrowserTest::SetUpInProcessBrowserTestFixture() {
  extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::TearDownInProcessBrowserTestFixture() {
  extensions::ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
  cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
  // Browser will both run and display insecure content.
  command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  cert_verifier_.SetUpCommandLine(command_line);
}

void WebAppControllerBrowserTest::SetUpOnMainThread() {
  extensions::ExtensionBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(https_server()->Start());

  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

  provider().shortcut_manager().SuppressShortcutsForTesting();
}

}  // namespace web_app
