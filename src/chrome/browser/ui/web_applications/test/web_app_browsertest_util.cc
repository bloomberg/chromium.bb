// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"

#include <memory>
#include <string>

#include "base/callback_helpers.h"
#include "base/one_shot_event.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/components/app_icon_manager.h"
#include "chrome/browser/web_applications/components/external_install_options.h"
#include "chrome/browser/web_applications/components/externally_managed_app_manager.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"
#include "chrome/browser/web_applications/components/install_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "chrome/browser/web_applications/test/service_worker_registration_waiter.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/page_transition_types.h"

using ui_test_utils::BrowserChangeObserver;

namespace web_app {

namespace {

void WaitUntilReady(WebAppProvider* provider) {
  if (provider->on_registry_ready().is_signaled())
    return;

  base::RunLoop run_loop;
  provider->on_registry_ready().Post(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

// Waits for |browser| to be removed from BrowserList and then calls |callback|.
class BrowserRemovedWaiter final : public BrowserListObserver {
 public:
  explicit BrowserRemovedWaiter(Browser* browser) : browser_(browser) {}
  ~BrowserRemovedWaiter() override = default;

  void Wait() {
    BrowserList::AddObserver(this);
    run_loop_.Run();
  }

  // BrowserListObserver
  void OnBrowserRemoved(Browser* browser) override {
    if (browser != browser_)
      return;

    BrowserList::RemoveObserver(this);
    // Post a task to ensure the Remove event has been dispatched to all
    // observers.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  run_loop_.QuitClosure());
  }

 private:
  Browser* browser_;
  base::RunLoop run_loop_;
};

void AutoAcceptDialogCallback(
    content::WebContents* initiator_web_contents,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    InstallManager::WebAppInstallationAcceptanceCallback acceptance_callback) {
  web_app_info->open_as_window = true;
  std::move(acceptance_callback)
      .Run(
          /*user_accepted=*/true, std::move(web_app_info));
}

}  // namespace

AppId InstallWebAppFromPage(Browser* browser, const GURL& app_url) {
  NavigateToURLAndWait(browser, app_url);

  AppId app_id;
  base::RunLoop run_loop;

  auto* provider = WebAppProvider::Get(browser->profile());
  DCHECK(provider);
  WaitUntilReady(provider);
  provider->install_manager().InstallWebAppFromManifestWithFallback(
      browser->tab_strip_model()->GetActiveWebContents(),
      /*force_shortcut_app=*/false,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(&AutoAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const AppId& installed_app_id,
                               InstallResultCode code) {
            DCHECK_EQ(code, InstallResultCode::kSuccessNewInstall);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

AppId InstallWebAppFromManifest(Browser* browser, const GURL& app_url) {
  ServiceWorkerRegistrationWaiter registration_waiter(browser->profile(),
                                                      app_url);
  NavigateToURLAndWait(browser, app_url);
  registration_waiter.AwaitRegistration();

  AppId app_id;
  base::RunLoop run_loop;

  auto* provider = WebAppProvider::Get(browser->profile());
  DCHECK(provider);
  WaitUntilReady(provider);
  provider->install_manager().InstallWebAppFromManifestWithFallback(
      browser->tab_strip_model()->GetActiveWebContents(),
      /*force_shortcut_app=*/false,
      webapps::WebappInstallSource::MENU_BROWSER_TAB,
      base::BindOnce(&AutoAcceptDialogCallback),
      base::BindLambdaForTesting(
          [&run_loop, &app_id](const AppId& installed_app_id,
                               InstallResultCode code) {
            DCHECK_EQ(code, InstallResultCode::kSuccessNewInstall);
            app_id = installed_app_id;
            run_loop.Quit();
          }));

  run_loop.Run();
  return app_id;
}

Browser* LaunchWebAppBrowser(Profile* profile, const AppId& app_id) {
  BrowserChangeObserver observer(nullptr,
                                 BrowserChangeObserver::ChangeType::kAdded);
  EXPECT_TRUE(
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(apps::AppLaunchParams(
              app_id, apps::mojom::LaunchContainer::kLaunchContainerWindow,
              WindowOpenDisposition::CURRENT_TAB,
              apps::mojom::AppLaunchSource::kSourceTest)));

  Browser* browser = observer.Wait();
  EXPECT_EQ(browser, chrome::FindLastActive());
  bool is_correct_app_browser =
      browser && GetAppIdFromApplicationName(browser->app_name()) == app_id;
  EXPECT_TRUE(is_correct_app_browser);

  return is_correct_app_browser ? browser : nullptr;
}

// Launches the app, waits for the app url to load.
Browser* LaunchWebAppBrowserAndWait(Profile* profile, const AppId& app_id) {
  ui_test_utils::UrlLoadObserver url_observer(
      WebAppProvider::Get(profile)->registrar().GetAppLaunchUrl(app_id),
      content::NotificationService::AllSources());
  Browser* const app_browser = LaunchWebAppBrowser(profile, app_id);
  url_observer.Wait();
  return app_browser;
}

Browser* LaunchBrowserForWebAppInTab(Profile* profile, const AppId& app_id) {
  content::WebContents* web_contents =
      apps::AppServiceProxyFactory::GetForProfile(profile)
          ->BrowserAppLauncher()
          ->LaunchAppWithParams(apps::AppLaunchParams(
              app_id, apps::mojom::LaunchContainer::kLaunchContainerTab,
              WindowOpenDisposition::NEW_FOREGROUND_TAB,
              apps::mojom::AppLaunchSource::kSourceTest));
  DCHECK(web_contents);

  WebAppTabHelper* tab_helper = WebAppTabHelper::FromWebContents(web_contents);
  DCHECK(tab_helper);
  EXPECT_EQ(app_id, tab_helper->GetAppId());

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  EXPECT_EQ(browser, chrome::FindLastActive());
  EXPECT_EQ(web_contents, browser->tab_strip_model()->GetActiveWebContents());
  return browser;
}

ExternalInstallOptions CreateInstallOptions(const GURL& url) {
  ExternalInstallOptions install_options(
      url, DisplayMode::kStandalone, ExternalInstallSource::kInternalDefault);
  // Avoid creating real shortcuts in tests.
  install_options.add_to_applications_menu = false;
  install_options.add_to_desktop = false;
  install_options.add_to_quick_launch_bar = false;

  return install_options;
}

InstallResultCode ExternallyManagedAppManagerInstall(
    Profile* profile,
    ExternalInstallOptions install_options) {
  DCHECK(profile);
  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);
  WaitUntilReady(provider);
  base::RunLoop run_loop;
  InstallResultCode result_code;

  provider->externally_managed_app_manager().Install(
      std::move(install_options),
      base::BindLambdaForTesting(
          [&result_code, &run_loop](
              const GURL& provided_url,
              ExternallyManagedAppManager::InstallResult result) {
            result_code = result.code;
            run_loop.Quit();
          }));
  run_loop.Run();
  return result_code;
}

void NavigateToURLAndWait(Browser* browser,
                          const GURL& url,
                          bool proceed_through_interstitial) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  {
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    NavigateParams params(browser, url, ui::PAGE_TRANSITION_LINK);
    ui_test_utils::NavigateToURL(&params);
    observer.WaitForNavigationFinished();
  }

  if (!proceed_through_interstitial)
    return;

  {
    // Need a second TestNavigationObserver; the above one is spent.
    content::TestNavigationObserver observer(
        web_contents, content::MessageLoopRunner::QuitMode::DEFERRED);
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            browser->tab_strip_model()->GetActiveWebContents());
    ASSERT_TRUE(
        helper &&
        helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting());
    std::string javascript = "window.certificateErrorPageController.proceed();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, javascript));
    observer.Wait();
  }
}

// Performs a navigation and then checks that the toolbar visibility is as
// expected.
void NavigateAndCheckForToolbar(Browser* browser,
                                const GURL& url,
                                bool expected_visibility,
                                bool proceed_through_interstitial) {
  NavigateToURLAndWait(browser, url, proceed_through_interstitial);
  EXPECT_EQ(expected_visibility,
            browser->app_controller()->ShouldShowCustomTabBar());
}

AppMenuCommandState GetAppMenuCommandState(int command_id, Browser* browser) {
  DCHECK(!browser->app_controller())
      << "This check only applies to regular browser windows.";
  auto app_menu_model = std::make_unique<AppMenuModel>(nullptr, browser);
  app_menu_model->Init();
  ui::MenuModel* model = app_menu_model.get();
  int index = -1;
  if (!app_menu_model->GetModelAndIndexForCommandId(command_id, &model,
                                                    &index)) {
    return kNotPresent;
  }
  return model->IsEnabledAt(index) ? kEnabled : kDisabled;
}

Browser* FindWebAppBrowser(Profile* profile, const AppId& app_id) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile)
      continue;

    if (AppBrowserController::IsForWebApp(browser, app_id))
      return browser;
  }

  return nullptr;
}

void CloseAndWait(Browser* browser) {
  BrowserRemovedWaiter waiter(browser);
  browser->window()->Close();
  waiter.Wait();
}

void WaitForBrowserToBeClosed(Browser* browser) {
  BrowserRemovedWaiter waiter(browser);
  waiter.Wait();
}

bool IsBrowserOpen(const Browser* test_browser) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser == test_browser)
      return true;
  }
  return false;
}

void UninstallWebApp(Profile* profile, const AppId& app_id) {
  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);
  DCHECK(provider->install_finalizer().CanUserUninstallWebApp(app_id));
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu, base::DoNothing());
}

void UninstallWebAppWithCallback(Profile* profile,
                                 const AppId& app_id,
                                 UninstallWebAppCallback callback) {
  auto* provider = WebAppProvider::Get(profile);
  DCHECK(provider);
  DCHECK(provider->install_finalizer().CanUserUninstallWebApp(app_id));
  provider->install_finalizer().UninstallWebApp(
      app_id, webapps::WebappUninstallSource::kAppMenu, std::move(callback));
}

SkColor ReadAppIconPixel(Profile* profile,
                         const AppId& app_id,
                         SquareSizePx size,
                         int x,
                         int y) {
  SkColor result;
  base::RunLoop run_loop;
  WebAppProvider::Get(profile)->icon_manager().ReadIcons(
      app_id, IconPurpose::ANY, {size},
      base::BindLambdaForTesting(
          [&](std::map<SquareSizePx, SkBitmap> icon_bitmaps) {
            run_loop.Quit();
            result = icon_bitmaps.at(size).getColor(x, y);
          }));
  run_loop.Run();
  return result;
}

}  // namespace web_app
