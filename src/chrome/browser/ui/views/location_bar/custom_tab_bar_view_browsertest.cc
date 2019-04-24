// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_observer.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/ssl/chrome_mock_cert_verifier.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

namespace {

// Waits until the title of any tab in the browser for |contents| has the title
// |target_title|.
class TestTitleObserver : public TabStripModelObserver {
 public:
  // Create a new TitleObserver for the browser of |contents|, waiting for
  // |target_title|.
  TestTitleObserver(content::WebContents* contents, base::string16 target_title)
      : contents_(contents),
        target_title_(target_title),
        tab_strip_model_observer_(this) {
    browser_ = chrome::FindBrowserWithWebContents(contents_);
    tab_strip_model_observer_.Add(browser_->tab_strip_model());
  }

  // Run a loop, blocking until a tab has the title |target_title|.
  void Wait() {
    if (seen_target_title_)
      return;

    awaiter_.Run();
  }

  // TabstripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    content::NavigationEntry* entry =
        contents->GetController().GetVisibleEntry();
    base::string16 title = entry ? entry->GetTitle() : base::string16();

    if (title != target_title_)
      return;

    seen_target_title_ = true;
    awaiter_.Quit();
  }

 private:
  bool seen_target_title_ = false;

  content::WebContents* contents_;
  Browser* browser_;
  base::string16 target_title_;
  base::RunLoop awaiter_;
  ScopedObserver<TabStripModel, TestTitleObserver> tab_strip_model_observer_;
};

// Opens a new popup window from |web_contents| on |target_url| and returns
// the Browser it opened in.
Browser* OpenPopup(content::WebContents* web_contents, const GURL& target_url) {
  content::TestNavigationObserver nav_observer(target_url);
  nav_observer.StartWatchingNewWebContents();

  std::string script = "window.open('" + target_url.spec() +
                       "', 'popup', 'width=400 height=400');";
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));
  nav_observer.Wait();

  return chrome::FindLastActive();
}

// Navigates to |target_url| and waits for navigation to complete.
void NavigateAndWait(content::WebContents* web_contents,
                     const GURL& target_url) {
  content::TestNavigationObserver nav_observer(web_contents);

  std::string script = "window.location = '" + target_url.spec() + "';";
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));
  nav_observer.Wait();
}

// Navigates |web_contents| to |location|, waits for navigation to complete
// and then sets document.title to be |title| and waits for the change
// to propogate.
void SetTitleAndLocation(content::WebContents* web_contents,
                         const base::string16 title,
                         const GURL& location) {
  NavigateAndWait(web_contents, location);

  TestTitleObserver title_observer(web_contents, title);

  std::string script = "document.title = '" + base::UTF16ToASCII(title) + "';";
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));

  title_observer.Wait();
}

}  // namespace

class CustomTabBarViewBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  CustomTabBarViewBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}
  ~CustomTabBarViewBrowserTest() override {}

 protected:
  void SetUpInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    extensions::ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
    cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    // Browser will both run and display insecure content.
    command_line->AppendSwitch(switches::kAllowRunningInsecureContent);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kDesktopPWAsStayInWindow, features::kDesktopPWAWindowing,
         features::kDesktopPWAsCustomTabUI},
        {});
    https_server_.AddDefaultHandlers(base::FilePath(kDocRoot));

    // Everything should be redirected to the http server.
    host_resolver()->AddRule("*", "127.0.0.1");
    // All SSL cert checks should be valid.
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    browser_view_ = BrowserView::GetBrowserViewForBrowser(browser());

    location_bar_ = browser_view_->GetLocationBarView();
    custom_tab_bar_ = browser_view_->toolbar()->custom_tab_bar();
  }

  void InstallPWA(const GURL& app_url) {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = app_url;
    web_app_info.scope = app_url.GetWithoutFilename();
    web_app_info.open_as_window = true;
    Install(web_app_info);
  }

  void InstallBookmark(const GURL& app_url) {
    WebApplicationInfo web_app_info;
    web_app_info.app_url = app_url;
    web_app_info.open_as_window = true;
    Install(web_app_info);
  }

  Browser* app_browser_;
  BrowserView* browser_view_;
  LocationBarView* location_bar_;
  CustomTabBarView* custom_tab_bar_;
  extensions::HostedAppBrowserController* hosted_app_controller_;

  net::EmbeddedTestServer* https_server() { return &https_server_; }

 private:
  void Install(const WebApplicationInfo& web_app_info) {
    auto* app = InstallBookmarkApp(web_app_info);

    ui_test_utils::UrlLoadObserver url_observer(
        web_app_info.app_url, content::NotificationService::AllSources());
    app_browser_ = LaunchAppBrowser(app);
    url_observer.Wait();

    DCHECK(app_browser_);
    DCHECK(app_browser_ != browser());

    hosted_app_controller_ = app_browser_->hosted_app_controller();
    DCHECK(hosted_app_controller_);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService. This is needed for when tests run with
  // the NetworkService enabled.
  ChromeMockCertVerifier cert_verifier_;

  DISALLOW_COPY_AND_ASSIGN(CustomTabBarViewBrowserTest);
};

// Check the custom tab bar is not instantiated for a tabbed browser window.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       IsNotCreatedInTabbedBrowser) {
  EXPECT_TRUE(browser_view_->IsBrowserTypeNormal());
  EXPECT_FALSE(custom_tab_bar_);
}

// Check the custom tab bar is not instantiated for a popup window.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, IsNotCreatedInPopup) {
  Browser* popup = OpenPopup(browser_view_->GetActiveWebContents(),
                             GURL("http://example.com"));
  EXPECT_TRUE(popup);

  BrowserView* popup_view = BrowserView::GetBrowserViewForBrowser(popup);

  // The popup should be in a new window.
  EXPECT_NE(browser_view_, popup_view);

  // Popups are not the normal browser view.
  EXPECT_FALSE(popup_view->IsBrowserTypeNormal());
  // Popups should not have a custom tab bar view.
  EXPECT_FALSE(popup_view->toolbar()->custom_tab_bar());
}

// Check the custom tab will be used for a Desktop PWA.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, IsUsedForDesktopPWA) {
  ASSERT_TRUE(https_server()->Start());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL& url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  EXPECT_FALSE(app_view->IsBrowserTypeNormal());

  // Custom tab bar should be created.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar());
}

// The custom tab bar should update with the title and location of the current
// page.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, TitleAndLocationUpdate) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  const GURL& navigate_to =
      https_server()->GetURL("app.com", "/ssl/blank_page.html");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  SetTitleAndLocation(app_view->GetActiveWebContents(),
                      base::ASCIIToUTF16("FooBar"), navigate_to);

  EXPECT_EQ(base::ASCIIToUTF16(navigate_to.spec()),
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(base::ASCIIToUTF16("FooBar"),
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

// If the page doesn't specify a title, we should use the origin.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       UsesLocationInsteadOfEmptyTitles) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  // Empty title should use location.
  SetTitleAndLocation(app_view->GetActiveWebContents(), base::string16(),
                      GURL("http://example.test/"));
  EXPECT_EQ(base::ASCIIToUTF16("example.test"),
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(base::ASCIIToUTF16("example.test"),
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

// Closing the CCT should take you back to the last in scope url.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       OutOfScopeUrlShouldBeClosable) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  auto* web_contents = app_view->GetActiveWebContents();
  EXPECT_NE(app_view, browser_view_);

  // Perform an inscope navigation.
  const GURL& other_app_url =
      https_server()->GetURL("app.com", "/ssl/blank_page.html");
  NavigateAndWait(web_contents, other_app_url);
  EXPECT_FALSE(hosted_app_controller_->ShouldShowToolbar());

  // Navigate out of scope.
  NavigateAndWait(web_contents, GURL("http://example.test/"));
  EXPECT_TRUE(hosted_app_controller_->ShouldShowToolbar());

  // Simulate clicking the close button and wait for navigation to finish.
  content::TestNavigationObserver nav_observer(web_contents);
  app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
  nav_observer.Wait();

  // The app should be on the last in scope url we visited.
  EXPECT_EQ(other_app_url, web_contents->GetLastCommittedURL());
}

// Paths above the launch url should be out of scope and should be closable from
// the CustomTabBar.
IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       ScopeAboveLaunchURLShouldBeOutOfScopeAndClosable) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  auto* web_contents = app_view->GetActiveWebContents();
  EXPECT_NE(app_view, browser_view_);

  // Navigate to a different page in the app scope, so we have something to come
  // back to.
  const GURL& other_app_url =
      https_server()->GetURL("app.com", "/ssl/blank_page.html");
  NavigateAndWait(web_contents, other_app_url);
  EXPECT_FALSE(hosted_app_controller_->ShouldShowToolbar());

  // Navigate above the scope of the app, on the same origin.
  NavigateAndWait(web_contents, https_server()->GetURL(
                                    "app.com", "/accessibility_fail.html"));
  EXPECT_TRUE(hosted_app_controller_->ShouldShowToolbar());

  // Simulate clicking the close button and wait for navigation to finish.
  content::TestNavigationObserver nav_observer(web_contents);
  app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
  nav_observer.Wait();

  // The app should be on the last in scope url we visited.
  EXPECT_EQ(other_app_url, web_contents->GetLastCommittedURL());
}

// When there are no in scope urls to navigate back to, closing the custom tab
// bar should navigate to the app's launch url.
IN_PROC_BROWSER_TEST_F(
    CustomTabBarViewBrowserTest,
    WhenNoHistoryIsInScopeCloseShouldNavigateToAppLaunchURL) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  auto* web_contents = app_view->GetActiveWebContents();
  EXPECT_NE(app_view, browser_view_);

  {
    // Do a state replacing navigation, so we don't have any in scope urls in
    // history.
    content::TestNavigationObserver nav_observer(web_contents);
    EXPECT_TRUE(content::ExecuteScript(
        web_contents, "window.location.replace('http://example.com');"));
    nav_observer.Wait();
    EXPECT_TRUE(hosted_app_controller_->ShouldShowToolbar());
  }
  {
    // Simulate clicking the close button and wait for navigation to finish.
    content::TestNavigationObserver nav_observer(web_contents);
    app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
    nav_observer.Wait();
  }
  // The app should be on the last in scope url we visited.
  EXPECT_EQ(app_url, web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest, URLsWithEmojiArePunyCoded) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  const GURL& navigate_to = GURL("https://🔒.example/ssl/blank_page.html");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  SetTitleAndLocation(app_view->GetActiveWebContents(),
                      base::ASCIIToUTF16("FooBar"), navigate_to);

  EXPECT_EQ(base::UTF8ToUTF16("https://xn--lv8h.example/ssl/blank_page.html"),
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(base::ASCIIToUTF16("FooBar"),
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       URLsWithNonASCIICharactersDisplayNormally) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  const GURL& navigate_to = GURL("https://ΐ.example/ssl/blank_page.html");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  SetTitleAndLocation(app_view->GetActiveWebContents(),
                      base::ASCIIToUTF16("FooBar"), navigate_to);

  EXPECT_EQ(base::UTF8ToUTF16("https://ΐ.example/ssl/blank_page.html"),
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(base::ASCIIToUTF16("FooBar"),
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       BannedCharactersAreURLEncoded) {
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url = https_server()->GetURL("app.com", "/ssl/google.html");
  const GURL& navigate_to = GURL("https://ΐ.example/🔒/blank_page.html");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  SetTitleAndLocation(app_view->GetActiveWebContents(),
                      base::ASCIIToUTF16("FooBar"), navigate_to);

  EXPECT_EQ(base::UTF8ToUTF16("https://ΐ.example/%F0%9F%94%92/blank_page.html"),
            app_view->toolbar()->custom_tab_bar()->location_for_testing());
  EXPECT_EQ(base::ASCIIToUTF16("FooBar"),
            app_view->toolbar()->custom_tab_bar()->title_for_testing());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       BackToAppButtonIsNotVisibleInScope) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server()->Start());

  // We install over http because it's the easiest way to get a custom tab bar
  // in scope. A PWA won't be installed over http in the real world (it'd make a
  // shortcut app instead).
  const GURL& app_url =
      embedded_test_server()->GetURL("app.com", "/ssl/google.html");
  const GURL& out_of_scope_url = GURL("https://example.com");

  InstallPWA(app_url);

  EXPECT_TRUE(app_browser_);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);
  content::WebContents* web_contents = app_view->GetActiveWebContents();

  // Insecure site, so should show custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->visible());
  // In scope, so don't show close button.
  EXPECT_FALSE(app_view->toolbar()
                   ->custom_tab_bar()
                   ->close_button_for_testing()
                   ->visible());

  NavigateAndWait(web_contents, out_of_scope_url);

  // Out of scope, show the custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->visible());
  // Out of scope, show the close button.
  EXPECT_TRUE(app_view->toolbar()
                  ->custom_tab_bar()
                  ->close_button_for_testing()
                  ->visible());

  // Simulate clicking the close button and wait for navigation to finish.
  content::TestNavigationObserver nav_observer(web_contents);
  app_view->toolbar()->custom_tab_bar()->GoBackToAppForTesting();
  nav_observer.Wait();

  // Insecure site, show the custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->visible());
  // In scope, hide the close button.
  EXPECT_FALSE(app_view->toolbar()
                   ->custom_tab_bar()
                   ->close_button_for_testing()
                   ->visible());
}

IN_PROC_BROWSER_TEST_F(CustomTabBarViewBrowserTest,
                       BackToAppButtonIsNotVisibleInBookmarkAppOnOrigin) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(https_server()->Start());

  const GURL& app_url =
      embedded_test_server()->GetURL("app.com", "/ssl/google.html");
  const GURL& out_of_scope_url = GURL("https://example.com");

  InstallBookmark(app_url);

  BrowserView* app_view = BrowserView::GetBrowserViewForBrowser(app_browser_);
  EXPECT_NE(app_view, browser_view_);

  // Insecure site, so should show custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->visible());
  // On origin, so don't show close button.
  EXPECT_FALSE(app_view->toolbar()
                   ->custom_tab_bar()
                   ->close_button_for_testing()
                   ->visible());

  NavigateAndWait(app_view->GetActiveWebContents(), out_of_scope_url);

  // Off origin, show the custom tab bar.
  EXPECT_TRUE(app_view->toolbar()->custom_tab_bar()->visible());
  // Off origin, show the close button.
  EXPECT_TRUE(app_view->toolbar()
                  ->custom_tab_bar()
                  ->close_button_for_testing()
                  ->visible());
}
