// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "url/gurl.h"

namespace {

class SettingsWindowTestObserver
    : public chrome::SettingsWindowManagerObserver {
 public:
  SettingsWindowTestObserver() = default;
  ~SettingsWindowTestObserver() override = default;

  void OnNewSettingsWindow(Browser* settings_browser) override {
    browser_ = settings_browser;
    ++new_settings_count_;
  }

  Browser* browser() { return browser_; }
  size_t new_settings_count() const { return new_settings_count_; }

 private:
  Browser* browser_ = nullptr;
  size_t new_settings_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowTestObserver);
};

}  // namespace

class SettingsWindowManagerTest : public InProcessBrowserTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  SettingsWindowManagerTest()
      : settings_manager_(chrome::SettingsWindowManager::GetInstance()) {
    settings_manager_->AddObserver(&observer_);
    if (EnableSystemWebApps())
      scoped_feature_list_.InitAndEnableFeature(features::kSystemWebApps);
    else
      scoped_feature_list_.InitAndDisableFeature(features::kSystemWebApps);
  }

  void SetUpOnMainThread() override {
    if (!EnableSystemWebApps())
      return;

    web_app::WebAppProvider::Get(browser()->profile())
        ->system_web_app_manager()
        .InstallSystemAppsForTesting();
  }

  bool EnableSystemWebApps() { return GetParam(); }

  ~SettingsWindowManagerTest() override {
    settings_manager_->RemoveObserver(&observer_);
  }

  void ShowSettingsForProfile(Profile* profile) {
    settings_manager_->ShowChromePageForProfile(
        profile, GURL(chrome::kChromeUISettingsURL));
  }

  void CloseNonDefaultBrowsers() {
    std::list<Browser*> browsers_to_close;
    for (auto* b : *BrowserList::GetInstance()) {
      if (b != browser())
        browsers_to_close.push_back(b);
    }
    for (std::list<Browser*>::iterator iter = browsers_to_close.begin();
         iter != browsers_to_close.end(); ++iter) {
      CloseBrowserSynchronously(*iter);
    }
  }

 protected:
  chrome::SettingsWindowManager* settings_manager_;
  SettingsWindowTestObserver observer_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(SettingsWindowManagerTest);
};

IN_PROC_BROWSER_TEST_P(SettingsWindowManagerTest, OpenSettingsWindow) {
  // Open a settings window.
  ShowSettingsForProfile(browser()->profile());
  Browser* settings_browser =
      settings_manager_->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser);
  // Ensure the observer fired correctly.
  EXPECT_EQ(1u, observer_.new_settings_count());
  EXPECT_EQ(settings_browser, observer_.browser());

  // Open the settings again: no new window.
  ShowSettingsForProfile(browser()->profile());
  EXPECT_EQ(settings_browser,
            settings_manager_->FindBrowserForProfile(browser()->profile()));
  EXPECT_EQ(1u, observer_.new_settings_count());

  // Close the settings window.
  CloseBrowserSynchronously(settings_browser);
  EXPECT_FALSE(settings_manager_->FindBrowserForProfile(browser()->profile()));

  // Open a new settings window.
  ShowSettingsForProfile(browser()->profile());
  Browser* settings_browser2 =
      settings_manager_->FindBrowserForProfile(browser()->profile());
  ASSERT_TRUE(settings_browser2);
  EXPECT_EQ(2u, observer_.new_settings_count());

  CloseBrowserSynchronously(settings_browser2);
}

IN_PROC_BROWSER_TEST_P(SettingsWindowManagerTest, OpenChromePages) {
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // History should open in the existing browser window.
  chrome::ShowHistory(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Settings should open a new browser window.
  chrome::ShowSettings(browser());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // About should reuse the existing Settings window.
  chrome::ShowAboutChrome(browser());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // Extensions should open in an existing browser window.
  CloseNonDefaultBrowsers();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  std::string extension_to_highlight;  // none
  chrome::ShowExtensions(browser(), extension_to_highlight);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Downloads should open in an existing browser window.
  chrome::ShowDownloads(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // About should open a new browser window.
  chrome::ShowAboutChrome(browser());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
}

IN_PROC_BROWSER_TEST_P(SettingsWindowManagerTest, SplitSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(chromeos::features::kSplitSettings);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Browser settings opens in the existing browser window.
  chrome::ShowSettings(browser());
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // OS settings opens in a new window.
  settings_manager_->ShowOSSettings(browser()->profile());
  EXPECT_EQ(1u, observer_.new_settings_count());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  content::WebContents* web_contents =
      observer_.browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(chrome::kChromeUIOSSettingsHost, web_contents->GetURL().host());

  // Showing an OS sub-page reuses the OS settings window.
  settings_manager_->ShowOSSettings(browser()->profile(),
                                    chrome::kBluetoothSubPage);
  EXPECT_EQ(1u, observer_.new_settings_count());
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());

  // Close the settings window.
  CloseNonDefaultBrowsers();
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

  // Showing a browser setting sub-page reuses the browser window.
  chrome::ShowSettingsSubPage(browser(), chrome::kAutofillSubPage);
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SettingsWindowManagerTest,
    ::testing::Bool());
