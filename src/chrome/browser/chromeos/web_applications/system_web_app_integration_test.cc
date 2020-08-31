// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/web_applications/system_web_app_integration_test.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"

SystemWebAppIntegrationTest::SystemWebAppIntegrationTest()
    : SystemWebAppManagerBrowserTest(false /* install_mock */) {}

SystemWebAppIntegrationTest::~SystemWebAppIntegrationTest() = default;

Profile* SystemWebAppIntegrationTest::profile() {
  return browser()->profile();
}

void SystemWebAppIntegrationTest::ExpectSystemWebAppValid(
    web_app::SystemAppType app_type,
    const GURL& url,
    const std::string& title) {
  Browser* app_browser = WaitForSystemAppInstallAndLaunch(app_type);

  web_app::AppId app_id = app_browser->app_controller()->GetAppId();
  EXPECT_EQ(GetManager().GetAppIdForSystemApp(app_type), app_id);
  EXPECT_TRUE(GetManager().IsSystemWebApp(app_id));

  web_app::AppRegistrar& registrar =
      web_app::WebAppProviderBase::GetProviderBase(profile())->registrar();
  EXPECT_EQ(title, registrar.GetAppShortName(app_id));
  EXPECT_EQ(base::ASCIIToUTF16(title),
            app_browser->window()->GetNativeWindow()->GetTitle());
  EXPECT_TRUE(registrar.HasExternalAppWithInstallSource(
      app_id, web_app::ExternalInstallSource::kSystemInstalled));

  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();

  // The opened window should be showing the url with attached WebUI.
  EXPECT_EQ(url, web_contents->GetVisibleURL());

  content::TestNavigationObserver observer(web_contents);
  observer.WaitForNavigationFinished();
  EXPECT_EQ(url, web_contents->GetLastCommittedURL());

  content::WebUI* web_ui = web_contents->GetCommittedWebUI();
  ASSERT_TRUE(web_ui);
  EXPECT_TRUE(web_ui->GetController());

  // A completed navigation could change the window title. Check again.
  EXPECT_EQ(base::ASCIIToUTF16(title),
            app_browser->window()->GetNativeWindow()->GetTitle());
}
