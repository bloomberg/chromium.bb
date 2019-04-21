// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/page_action/pwa_install_view.h"

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/banners/test_app_banner_manager_desktop.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/page_action/omnibox_page_action_icon_container_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/referrer.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/gfx/color_utils.h"

class PwaInstallViewBrowserTest : public InProcessBrowserTest {
 public:
  PwaInstallViewBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~PwaInstallViewBrowserTest() override {}

  void SetUp() override {
    DCHECK(base::FeatureList::IsEnabled(features::kDesktopPWAWindowing));
    scoped_feature_list_.InitAndEnableFeature(
        features::kDesktopPWAsOmniboxInstall);

    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        network::switches::kUnsafelyTreatInsecureOriginAsSecure,
        GetInstallableAppURL().GetOrigin().spec());
  }

  void SetUpOnMainThread() override {
    pwa_install_view_ =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetOmniboxPageActionIconContainerView()
            ->GetPageActionIconView(PageActionIconType::kPwaInstall);
    EXPECT_FALSE(pwa_install_view_->visible());

    web_contents_ = GetCurrentTab();
    app_banner_manager_ =
        banners::TestAppBannerManagerDesktop::CreateForWebContents(
            web_contents_);
  }

  content::WebContents* GetCurrentTab() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::WebContents* OpenNewTab(const GURL& url,
                                   bool expected_installability) {
    chrome::NewTab(browser());
    content::WebContents* web_contents = GetCurrentTab();
    auto* app_banner_manager =
        banners::TestAppBannerManagerDesktop::CreateForWebContents(
            web_contents);
    DCHECK(!app_banner_manager->WaitForInstallableCheck());

    ui_test_utils::NavigateToURL(browser(), url);
    DCHECK_EQ(app_banner_manager->WaitForInstallableCheck(),
              expected_installability);

    return web_contents;
  }

  GURL GetInstallableAppURL() {
    return https_server_.GetURL("/banners/manifest_test_page.html");
  }

  GURL GetNonInstallableAppURL() {
    return https_server_.GetURL("app.com", "/simple.html");
  }

  void NavigateToURL(const GURL& url) {
    browser()->OpenURL(content::OpenURLParams(
        url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
        ui::PAGE_TRANSITION_TYPED, false /* is_renderer_initiated */));
    app_banner_manager_->WaitForInstallableCheckTearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;

  PageActionIconView* pwa_install_view_ = nullptr;
  content::WebContents* web_contents_ = nullptr;
  banners::TestAppBannerManagerDesktop* app_banner_manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PwaInstallViewBrowserTest);
};

// Tests that the plus icon updates its visibiliy when switching between
// installable/non-installable tabs.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  content::WebContents* installable_web_contents =
      OpenNewTab(GetInstallableAppURL(), true);
  content::WebContents* non_installable_web_contents =
      OpenNewTab(GetNonInstallableAppURL(), false);

  chrome::SelectPreviousTab(browser());
  ASSERT_EQ(installable_web_contents, GetCurrentTab());
  EXPECT_TRUE(pwa_install_view_->visible());

  chrome::SelectNextTab(browser());
  ASSERT_EQ(non_installable_web_contents, GetCurrentTab());
  EXPECT_FALSE(pwa_install_view_->visible());
}

// Tests that the plus icon updates its visibiliy once the installability check
// completes.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       IconVisibilityAfterInstallabilityCheck) {
  NavigateToURL(GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());

  NavigateToURL(GetNonInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_FALSE(pwa_install_view_->visible());
}

// Tests that the plus icon animates its label when the installability check
// passes but doesn't animate more than once for the same installability check.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, LabelAnimation) {
  NavigateToURL(GetInstallableAppURL());
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  chrome::NewTab(browser());
  EXPECT_FALSE(pwa_install_view_->visible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon persists while loading the same scope and omits running
// the label animation again.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, NavigateToSameScope) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_2.html"));
  EXPECT_TRUE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon persists while loading the same scope but goes away when
// the installability check fails.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       NavigateToSameScopeNonInstallable) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/scope_a/bad_manifest.html"));
  EXPECT_TRUE(pwa_install_view_->visible());
  ASSERT_FALSE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_FALSE(pwa_install_view_->visible());
  EXPECT_FALSE(pwa_install_view_->is_animating_label());
}

// Tests that the icon and animation resets while loading a different scope.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, NavigateToDifferentScope) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/scope_b/scope_b.html"));
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());
}

// Tests that the icon and animation resets while loading a different empty
// scope.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest,
                       NavigateToDifferentEmptyScope) {
  NavigateToURL(https_server_.GetURL("/banners/scope_a/page_1.html"));
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  NavigateToURL(https_server_.GetURL("/banners/manifest_test_page.html"));
  EXPECT_FALSE(pwa_install_view_->visible());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());
}

// Tests that the animation is suppressed for navigations within the same scope
// for an exponentially increasing period of time.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, AnimationSuppression) {
  std::vector<bool> animation_shown_for_day = {
      true,  true,  false, true,  false, false, false, true,
      false, false, false, false, false, false, false, true,
  };
  for (size_t day = 0; day < animation_shown_for_day.size(); ++day) {
    SCOPED_TRACE(testing::Message() << "day: " << day);

    banners::AppBannerManager::SetTimeDeltaForTesting(day);

    NavigateToURL(GetInstallableAppURL());
    ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
    EXPECT_EQ(pwa_install_view_->is_animating_label(),
              animation_shown_for_day[day]);
  }
}

// Tests that the icon label is visible against the omnibox background after the
// native widget becomes active.
IN_PROC_BROWSER_TEST_F(PwaInstallViewBrowserTest, TextContrast) {
  NavigateToURL(GetInstallableAppURL());
  ASSERT_TRUE(app_banner_manager_->WaitForInstallableCheck());
  EXPECT_TRUE(pwa_install_view_->visible());
  EXPECT_TRUE(pwa_install_view_->is_animating_label());

  pwa_install_view_->GetWidget()->OnNativeWidgetActivationChanged(true);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  SkColor omnibox_background = browser_view->GetLocationBarView()->GetColor(
      OmniboxPart::LOCATION_BAR_BACKGROUND);
  SkColor label_color = pwa_install_view_->GetLabelColorForTesting();
  EXPECT_EQ(SkColorGetA(label_color), SK_AlphaOPAQUE);
  EXPECT_GT(color_utils::GetContrastRatio(omnibox_background, label_color),
            color_utils::kMinimumReadableContrastRatio);
}
