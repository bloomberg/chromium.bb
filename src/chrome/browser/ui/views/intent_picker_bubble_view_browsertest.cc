// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_navigation_browsertest.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/prerender_test_util.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/blink/public/common/features.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/gurl.h"

class IntentPickerBubbleViewBrowserTest
    : public web_app::WebAppNavigationBrowserTest,
      public ::testing::WithParamInterface<std::string> {
 public:
  IntentPickerBubbleViewBrowserTest() {
    // TODO(schenney): Stop disabling Paint Holding. crbug.com/1001189
    scoped_feature_list_.InitAndDisableFeature(blink::features::kPaintHolding);
  }

  void OpenNewTab(const GURL& url) {
    chrome::NewTab(browser());
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    NavigateToLaunchingPage(browser());
    TestTabActionDoesNotOpenAppWindow(
        url, base::BindOnce(&ClickLinkAndWait, web_contents, url,
                            LinkTarget::SELF, GetParam()));
  }

  // Inserts an iframe in the main frame of |web_contents|.
  bool InsertIFrame(content::WebContents* web_contents) {
    return content::ExecuteScript(
        web_contents,
        "let iframe = document.createElement('iframe');"
        "iframe.id = 'iframe';"
        "document.body.appendChild(iframe);");
  }

  PageActionIconView* GetIntentPickerIcon() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kIntentPicker);
  }

  IntentPickerBubbleView* intent_picker_bubble() {
    return IntentPickerBubbleView::intent_picker_bubble();
  }

  void VerifyBubbleWithTestWebApp() {
    EXPECT_EQ(1U, intent_picker_bubble()->GetScrollViewSize());
    auto& app_info = intent_picker_bubble()->app_info_for_testing();
    ASSERT_EQ(1U, app_info.size());
    EXPECT_EQ(test_web_app_id(), app_info[0].launch_name);
    EXPECT_EQ(GetAppName(), app_info[0].display_name);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that clicking a link from a tabbed browser to within the scope of an
// installed app shows the intent picker icon in Omnibox. The intent picker
// bubble will only show up for android apps which is too hard to test.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToInScopeLinkShowsIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  NavigateToLaunchingPage(browser());

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  TestTabActionDoesNotOpenAppWindow(
      in_scope_url, base::BindOnce(&ClickLinkAndWait, web_contents,
                                   in_scope_url, LinkTarget::SELF, GetParam()));

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();
  EXPECT_TRUE(intent_picker_view->GetVisible());

  if (!base::FeatureList::IsEnabled(features::kIntentPickerPWAPersistence)) {
    EXPECT_FALSE(intent_picker_bubble());
    GetIntentPickerIcon()->ExecuteForTesting();
  }

  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());

  VerifyBubbleWithTestWebApp();

  intent_picker_bubble()->AcceptDialog();

  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser,
                                                         test_web_app_id()));
}

// Tests that clicking a link from a tabbed browser to outside the scope of an
// installed app does not show the intent picker.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       NavigationToOutofScopeLinkDoesNotShowIntentPicker) {
  InstallTestWebApp();

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  NavigateToLaunchingPage(browser());
  TestTabActionDoesNotOpenAppWindow(
      out_of_scope_url,
      base::BindOnce(&ClickLinkAndWait,
                     browser()->tab_strip_model()->GetActiveWebContents(),
                     out_of_scope_url, LinkTarget::SELF, GetParam()));

  EXPECT_EQ(nullptr, intent_picker_bubble());
}

// Tests that clicking a link from an app browser to either within or outside
// the scope of an installed app does not show the intent picker, even when an
// outside of scope link is opened within the context of the PWA.
// Flaky on Linux: https://crbug.com/1186613
#if defined(OS_LINUX)
#define MAYBE_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker \
  DISABLED_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker
#else
#define MAYBE_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker \
  NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker
#endif
IN_PROC_BROWSER_TEST_P(
    IntentPickerBubbleViewBrowserTest,
    MAYBE_NavigationInAppWindowToInScopeLinkDoesNotShowIntentPicker) {
  InstallTestWebApp();

  // No intent picker should be seen when first opening the web app.
  Browser* app_browser = OpenTestWebApp();
  EXPECT_EQ(nullptr, intent_picker_bubble());

  {
    const GURL in_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
    TestActionDoesNotOpenAppWindow(
        app_browser, in_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       in_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, intent_picker_bubble());
  }

  {
    const GURL out_of_scope_url =
        https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
    TestActionDoesNotOpenAppWindow(
        app_browser, out_of_scope_url,
        base::BindOnce(&ClickLinkAndWait,
                       app_browser->tab_strip_model()->GetActiveWebContents(),
                       out_of_scope_url, LinkTarget::SELF, GetParam()));

    EXPECT_EQ(nullptr, intent_picker_bubble());
  }
}

// Tests that the intent icon updates its visibiliy when switching between
// tabs.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       IconVisibilityAfterTabSwitching) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  // OpenNewTab opens a new tab and focus on the new tab.
  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_view->GetVisible());
  OpenNewTab(out_of_scope_url);
  EXPECT_FALSE(intent_picker_view->GetVisible());

  chrome::SelectPreviousTab(browser());
  EXPECT_TRUE(intent_picker_view->GetVisible());

  chrome::SelectNextTab(browser());
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

// Tests that the navigation in iframe doesn't affect intent picker icon
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       IframeNavigationDoesNotAffectIntentPicker) {
  InstallTestWebApp();

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  OpenNewTab(out_of_scope_url);
  content::WebContents* initial_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(InsertIFrame(initial_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", in_scope_url));
  EXPECT_FALSE(intent_picker_view->GetVisible());

  OpenNewTab(in_scope_url);
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(InsertIFrame(new_tab));

  EXPECT_TRUE(
      content::NavigateIframeToURL(initial_tab, "iframe", out_of_scope_url));
  EXPECT_TRUE(intent_picker_view->GetVisible());
}

// Tests that the intent picker icon is not visible if the navigatation
// redirects to a URL that doesn't have an installed PWA.
IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewBrowserTest,
                       DoesNotShowIntentPickerWhenRedirectedOutOfScope) {
  InstallTestWebApp(GetOtherAppUrlHost(), /*app_scope=*/"/");

  const GURL out_of_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetOutOfScopeUrlPath());
  const GURL in_scope_url = https_server().GetURL(GetOtherAppUrlHost(), "/");
  const GURL redirect_url = https_server().GetURL(
      GetOtherAppUrlHost(), CreateServerRedirect(out_of_scope_url));

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  OpenNewTab(in_scope_url);
  EXPECT_TRUE(intent_picker_view->GetVisible());

  ClickLinkAndWaitForURL(browser()->tab_strip_model()->GetActiveWebContents(),
                         redirect_url, out_of_scope_url, LinkTarget::SELF,
                         GetParam());
  EXPECT_FALSE(intent_picker_view->GetVisible());
}

IN_PROC_BROWSER_TEST_F(IntentPickerBubbleViewBrowserTest, DoubleClickOpensApp) {
  auto app_id = InstallTestWebApp(GetAppUrlHost(), GetAppScopePath());

  const GURL in_scope_url =
      https_server().GetURL(GetAppUrlHost(), GetInScopeUrlPath());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), in_scope_url));

  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "IntentPickerBubbleView");
  GetIntentPickerIcon()->ExecuteForTesting();
  waiter.WaitIfNeededAndGet();
  ASSERT_TRUE(intent_picker_bubble());
  EXPECT_TRUE(intent_picker_bubble()->GetVisible());

  intent_picker_bubble()->PressButtonForTesting(
      /* index= */ 0,
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0));
  intent_picker_bubble()->PressButtonForTesting(
      /* index= */ 0,
      ui::MouseEvent(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), ui::EF_IS_DOUBLE_CLICK, 0));

  Browser* app_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerBubbleViewBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

class IntentPickerBubbleViewPrerenderingBrowserTest
    : public IntentPickerBubbleViewBrowserTest {
 public:
  IntentPickerBubbleViewPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &IntentPickerBubbleViewPrerenderingBrowserTest::GetWebContents,
            base::Unretained(this))) {}
  ~IntentPickerBubbleViewPrerenderingBrowserTest() override = default;
  IntentPickerBubbleViewPrerenderingBrowserTest(
      const IntentPickerBubbleViewPrerenderingBrowserTest&) = delete;

  IntentPickerBubbleViewPrerenderingBrowserTest& operator=(
      const IntentPickerBubbleViewPrerenderingBrowserTest&) = delete;

  void SetUp() override {
    prerender_helper_.SetUp(embedded_test_server());
    IntentPickerBubbleViewBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
    IntentPickerBubbleViewBrowserTest::SetUpOnMainThread();
  }

  content::test::PrerenderTestHelper& prerender_test_helper() {
    return prerender_helper_;
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

IN_PROC_BROWSER_TEST_P(IntentPickerBubbleViewPrerenderingBrowserTest,
                       PrerenderingShouldNotShowIntentPicker) {
  InstallTestWebApp();

  PageActionIconView* intent_picker_view = GetIntentPickerIcon();

  const GURL initial_url =
      https_server().GetURL(GetAppUrlHost(), "/empty.html");
  OpenNewTab(initial_url);
  EXPECT_FALSE(intent_picker_view->GetVisible());

  // Load a prerender page and prerendering should not try to show the
  // intent picker.
  const GURL prerender_url = https_server().GetURL(
      GetAppUrlHost(), std::string(GetAppScopePath()) + "index1.html");
  int host_id = prerender_test_helper().AddPrerender(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     host_id);
  EXPECT_FALSE(host_observer.was_activated());
  EXPECT_FALSE(intent_picker_view->GetVisible());

  // Activate the prerender page.
  prerender_test_helper().NavigatePrimaryPage(prerender_url);
  EXPECT_TRUE(host_observer.was_activated());

  // After activation, IntentPickerTabHelper should show the
  // intent picker.
  EXPECT_TRUE(intent_picker_view->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    IntentPickerBubbleViewPrerenderingBrowserTest,
    testing::Values("", "noopener", "noreferrer", "nofollow"));

class IntentPickerDialogTest : public DialogBrowserTest {
 public:
  IntentPickerDialogTest() = default;
  IntentPickerDialogTest(const IntentPickerDialogTest&) = delete;
  IntentPickerDialogTest& operator=(const IntentPickerDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    PageActionIconView* anchor =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar_button_provider()
            ->GetPageActionIconView(PageActionIconType::kIntentPicker);
    std::vector<apps::IntentPickerAppInfo> app_info;
    const auto add_entry = [&app_info](const std::string& str) {
      app_info.emplace_back(
          apps::PickerEntryType::kUnknown,
          ui::ImageModel::FromImage(
              gfx::Image::CreateFrom1xBitmap(favicon::GenerateMonogramFavicon(
                  GURL("https://" + str + ".com"), gfx::kFaviconSize,
                  gfx::kFaviconSize))),
          "Launch name " + str, "Display name " + str);
    };
    add_entry("a");
    add_entry("b");
    add_entry("c");
    add_entry("d");
    IntentPickerBubbleView::ShowBubble(
        anchor, anchor, PageActionIconType::kIntentPicker,
        browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(app_info), true, true,
        url::Origin::Create(GURL("https://c.com")), base::DoNothing());
  }
};

IN_PROC_BROWSER_TEST_F(IntentPickerDialogTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
