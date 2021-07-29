// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_controller_browsertest.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_type.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace web_app {

WebAppControllerBrowserTest::WebAppControllerBrowserTest()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
  scoped_feature_list_.InitAndDisableFeature(
      predictors::kSpeculativePreconnectFeature);
}

WebAppControllerBrowserTest::~WebAppControllerBrowserTest() = default;

WebAppProvider& WebAppControllerBrowserTest::provider() {
  auto* provider = WebAppProvider::Get(profile());
  DCHECK(provider);
  return *provider;
}

Profile* WebAppControllerBrowserTest::profile() {
  return browser()->profile();
}

AppId WebAppControllerBrowserTest::InstallPWA(const GURL& start_url) {
  auto web_app_info = std::make_unique<WebApplicationInfo>();
  web_app_info->start_url = start_url;
  web_app_info->scope = start_url.GetWithoutFilename();
  web_app_info->open_as_window = true;
  web_app_info->title = u"A Web App";
  return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
}

AppId WebAppControllerBrowserTest::InstallWebApp(
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  return web_app::test::InstallWebApp(profile(), std::move(web_app_info));
}

Browser* WebAppControllerBrowserTest::LaunchWebAppBrowser(const AppId& app_id) {
  return web_app::LaunchWebAppBrowser(profile(), app_id);
}

Browser* WebAppControllerBrowserTest::LaunchWebAppBrowserAndWait(
    const AppId& app_id) {
  return web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
}

Browser*
WebAppControllerBrowserTest::LaunchWebAppBrowserAndAwaitInstallabilityCheck(
    const AppId& app_id) {
  Browser* browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);
  webapps::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents())
      ->WaitForInstallableCheck();
  return browser;
}

Browser* WebAppControllerBrowserTest::LaunchBrowserForWebAppInTab(
    const AppId& app_id) {
  return web_app::LaunchBrowserForWebAppInTab(profile(), app_id);
}

content::WebContents* WebAppControllerBrowserTest::OpenWindow(
    content::WebContents* contents,
    const GURL& url) {
  content::WebContentsAddedObserver tab_added_observer;
  EXPECT_TRUE(
      content::ExecuteScript(contents, "window.open('" + url.spec() + "');"));
  content::WebContents* new_contents = tab_added_observer.GetWebContents();
  EXPECT_TRUE(new_contents);
  WaitForLoadStop(new_contents);

  EXPECT_EQ(url, new_contents->GetLastCommittedURL());
  EXPECT_EQ(
      content::PAGE_TYPE_NORMAL,
      new_contents->GetController().GetLastCommittedEntry()->GetPageType());
  EXPECT_EQ(contents->GetMainFrame()->GetSiteInstance(),
            new_contents->GetMainFrame()->GetSiteInstance());

  return new_contents;
}

void WebAppControllerBrowserTest::NavigateInRenderer(
    content::WebContents* contents,
    const GURL& url) {
  EXPECT_TRUE(content::ExecuteScript(
      contents, "window.location = '" + url.spec() + "';"));
  content::WaitForLoadStop(contents);
  EXPECT_EQ(url, contents->GetController().GetLastCommittedEntry()->GetURL());
}

// static
bool WebAppControllerBrowserTest::NavigateAndAwaitInstallabilityCheck(
    Browser* browser,
    const GURL& url) {
  auto* manager = webapps::TestAppBannerManagerDesktop::FromWebContents(
      browser->tab_strip_model()->GetActiveWebContents());
  NavigateToURLAndWait(browser, url);
  return manager->WaitForInstallableCheck();
}

Browser*
WebAppControllerBrowserTest::NavigateInNewWindowAndAwaitInstallabilityCheck(
    const GURL& url) {
  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile(), true));
  AddBlankTabAndShow(new_browser);
  NavigateAndAwaitInstallabilityCheck(new_browser, url);
  return new_browser;
}

absl::optional<AppId> WebAppControllerBrowserTest::FindAppWithUrlInScope(
    const GURL& url) {
  return provider().registrar().FindAppWithUrlInScope(url);
}

content::WebContents* WebAppControllerBrowserTest::OpenApplication(
    const AppId& app_id) {
  ui_test_utils::UrlLoadObserver url_observer(
      provider().registrar().GetAppStartUrl(app_id),
      content::NotificationService::AllSources());

  apps::AppLaunchParams params(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW,
      apps::mojom::AppLaunchSource::kSourceTest);
  content::WebContents* contents =
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(std::move(params));
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

void WebAppControllerBrowserTest::SetUp() {
  https_server_.AddDefaultHandlers(GetChromeTestDataDir());
  webapps::TestAppBannerManagerDesktop::SetUp();

  InProcessBrowserTest::SetUp();
}

void WebAppControllerBrowserTest::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  cert_verifier_.SetUpInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::TearDownInProcessBrowserTestFixture() {
  InProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  cert_verifier_.TearDownInProcessBrowserTestFixture();
}

void WebAppControllerBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Browser will both run and display insecure content.
  command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
  cert_verifier_.SetUpCommandLine(command_line);
}

void WebAppControllerBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(https_server()->Start());

  // By default, all SSL cert checks are valid. Can be overridden in tests.
  cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

  os_hooks_suppress_ = OsIntegrationManager::ScopedSuppressOsHooksForTesting();
}

}  // namespace web_app
