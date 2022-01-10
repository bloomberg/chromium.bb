// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_ui_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_observer.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view_test_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/test/button_test_api.h"

class DummyEvent : public ui::Event {
 public:
  DummyEvent() : Event(ui::ET_UNKNOWN, base::TimeTicks(), 0) {}
  ~DummyEvent() override = default;
};

// Test suite covering the interaction between browser bookmarks and
// `Sec-Fetch-*` headers that can't be covered by Web Platform Tests (yet).
// See https://mikewest.github.io/sec-metadata/#directly-user-initiated and
// https://github.com/web-platform-tests/wpt/issues/16019.
class BookmarkBarNavigationTest : public InProcessBrowserTest {
 public:
  BookmarkBarNavigationTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Setup HTTPS server serving files from standard test directory.
    static constexpr base::FilePath::CharType kDocRoot[] =
        FILE_PATH_LITERAL("chrome/test/data");
    https_test_server_.AddDefaultHandlers(base::FilePath(kDocRoot));
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_test_server_.Start());

    // Setup the mock host resolver
    host_resolver()->AddRule("*", "127.0.0.1");

    browser()->profile()->GetPrefs()->SetBoolean(
        bookmarks::prefs::kShowBookmarkBar, true);

    test_helper_ = std::make_unique<BookmarkBarViewTestHelper>(bookmark_bar());
  }

  views::LabelButton* GetBookmarkButton(size_t index) {
    return test_helper_->GetBookmarkButton(index);
  }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  BookmarkBarView* bookmark_bar() { return browser_view()->bookmark_bar(); }

  std::string GetContent() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return content::EvalJs(web_contents, "document.body.textContent")
        .ExtractString();
  }

  void CreateBookmarkForHeader(const std::string& header) {
    // Populate bookmark bar with a single bookmark to
    // `/echoheader?` + |header|.
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser()->profile());
    bookmarks::test::WaitForBookmarkModelToLoad(model);
    model->ClearStore();
    std::string url = "/echoheader?";
    model->AddURL(model->bookmark_bar_node(), 0, u"Example",
                  https_test_server_.GetURL(url + header));
  }

  void NavigateToBookmark() {
    // Click on the 0th bookmark after setting up a navigation observer that
    // waits for a single navigation to complete successfully.
    content::TestNavigationObserver observer(web_contents(), 1);
    views::LabelButton* button = GetBookmarkButton(0);
    views::test::ButtonTestApi clicker(button);
    DummyEvent click_event;
    clicker.NotifyClick(click_event);
    observer.Wait();

    // All bookmark navigations should have a null initiator, as there's no
    // web origin from which the navigation is triggered.
    ASSERT_EQ(absl::nullopt, observer.last_initiator_origin());
  }

 private:
  net::EmbeddedTestServer https_test_server_;
  std::unique_ptr<BookmarkBarViewTestHelper> test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BookmarkBarNavigationTest, SecFetchFromEmptyTab) {
  // Navigate to an empty tab
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  {
    // Sec-Fetch-Dest: document
    CreateBookmarkForHeader("Sec-Fetch-Dest");
    NavigateToBookmark();
    EXPECT_EQ("document", GetContent());
  }

  {
    // Sec-Fetch-Mode: navigate
    CreateBookmarkForHeader("Sec-Fetch-Mode");
    NavigateToBookmark();
    EXPECT_EQ("navigate", GetContent());
  }

  {
    // Sec-Fetch-Site: none
    CreateBookmarkForHeader("Sec-Fetch-Site");
    NavigateToBookmark();
    EXPECT_EQ("none", GetContent());
  }

  {
    // Sec-Fetch-User: ?1
    CreateBookmarkForHeader("Sec-Fetch-User");
    NavigateToBookmark();
    EXPECT_EQ("?1", GetContent());
  }
}

#if defined(OS_MAC) || defined(OS_WIN)
//  TODO(crbug.com/1006033): Test flaky on Mac and Windows.
#define MAYBE_SecFetchSiteNoneFromNonEmptyTab \
  DISABLED_SecFetchSiteNoneFromNonEmptyTab
#else
#define MAYBE_SecFetchSiteNoneFromNonEmptyTab SecFetchSiteNoneFromNonEmptyTab
#endif
IN_PROC_BROWSER_TEST_F(BookmarkBarNavigationTest,
                       MAYBE_SecFetchSiteNoneFromNonEmptyTab) {
  // Navigate to an non-empty tab
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("http://example.com/")));

  {
    // Sec-Fetch-Dest: document
    CreateBookmarkForHeader("Sec-Fetch-Dest");
    NavigateToBookmark();
    EXPECT_EQ("document", GetContent());
  }

  {
    // Sec-Fetch-Mode: navigate
    CreateBookmarkForHeader("Sec-Fetch-Mode");
    NavigateToBookmark();
    EXPECT_EQ("navigate", GetContent());
  }

  {
    // Sec-Fetch-Site: none
    CreateBookmarkForHeader("Sec-Fetch-Site");
    NavigateToBookmark();
    EXPECT_EQ("none", GetContent());
  }

  {
    // Sec-Fetch-User: ?1
    CreateBookmarkForHeader("Sec-Fetch-User");
    NavigateToBookmark();
    EXPECT_EQ("?1", GetContent());
  }
}

// Class intercepts invocations of external protocol handlers.
class FakeProtocolHandlerDelegate : public ExternalProtocolHandler::Delegate {
 public:
  FakeProtocolHandlerDelegate() = default;

  // Wait until an external handler url is received by the class method
  // |RunExternalProtocolDialog|.
  const GURL& WaitForUrl() {
    run_loop_.Run();
    return url_invoked_;
  }

  FakeProtocolHandlerDelegate(const FakeProtocolHandlerDelegate& other) =
      delete;
  FakeProtocolHandlerDelegate& operator=(
      const FakeProtocolHandlerDelegate& other) = delete;
  class FakeDefaultProtocolClientWorker
      : public shell_integration::DefaultProtocolClientWorker {
   public:
    explicit FakeDefaultProtocolClientWorker(const std::string& protocol)
        : DefaultProtocolClientWorker(protocol) {}
    FakeDefaultProtocolClientWorker(
        const FakeDefaultProtocolClientWorker& other) = delete;
    FakeDefaultProtocolClientWorker& operator=(
        const FakeDefaultProtocolClientWorker& other) = delete;

   private:
    ~FakeDefaultProtocolClientWorker() override = default;
    shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
      return shell_integration::DefaultWebClientState::NOT_DEFAULT;
    }

    void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(on_finished_callback));
    }
  };

 private:
  scoped_refptr<shell_integration::DefaultProtocolClientWorker>
  CreateShellWorker(const std::string& protocol) override {
    return base::MakeRefCounted<FakeDefaultProtocolClientWorker>(protocol);
  }

  void BlockRequest() override { FAIL(); }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::GetBlockState(scheme, nullptr, profile);
  }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const absl::optional<url::Origin>& initiating_origin) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    EXPECT_TRUE(url_invoked_.is_empty());
    url_invoked_ = url;
    run_loop_.Quit();
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    FAIL();
  }

  void FinishedProcessingCheck() override {}

  GURL url_invoked_;
  base::RunLoop run_loop_;
  SEQUENCE_CHECKER(sequence_checker_);
};

// Checks that opening a bookmark to a URL handled by an external handler is not
// blocked by anti-flood protection. Regression test for
// https://crbug.com/1156651
IN_PROC_BROWSER_TEST_F(BookmarkBarNavigationTest, ExternalHandlerAllowed) {
  const char external_protocol[] = "fake";
  const GURL external_url = GURL("fake://path");

  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(browser()->profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);
  model->ClearStore();
  model->AddURL(model->bookmark_bar_node(), 0, u"Example", external_url);

  // First, get into a known (unblocked) state.
  ExternalProtocolHandler::PermitLaunchUrl();
  EXPECT_NE(ExternalProtocolHandler::BLOCK,
            ExternalProtocolHandler::GetBlockState(external_protocol, nullptr,
                                                   browser()->profile()));

  // Next, try to launch a bookmark pointed at the url of an external handler.
  {
    FakeProtocolHandlerDelegate external_handler_delegate;
    ExternalProtocolHandler::SetDelegateForTesting(&external_handler_delegate);
    NavigateToBookmark();
    auto launched_url = external_handler_delegate.WaitForUrl();
    EXPECT_EQ(external_url, launched_url);
    // Verify that the state has returned to block.
    EXPECT_EQ(ExternalProtocolHandler::BLOCK,
              ExternalProtocolHandler::GetBlockState(external_protocol, nullptr,
                                                     browser()->profile()));
  }
  // Finally, without first calling PermitLaunchUrl, try to launch the bookmark.
  {
    FakeProtocolHandlerDelegate external_handler_delegate;
    ExternalProtocolHandler::SetDelegateForTesting(&external_handler_delegate);
    NavigateToBookmark();
    auto launched_url = external_handler_delegate.WaitForUrl();
    EXPECT_EQ(external_url, launched_url);
    // Verify the launch state has changed back.
    EXPECT_EQ(ExternalProtocolHandler::BLOCK,
              ExternalProtocolHandler::GetBlockState(external_protocol, nullptr,
                                                     browser()->profile()));
  }
}
