// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/persistence/browser_persister.h"

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "components/sessions/core/command_storage_manager_test_helper.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "weblayer/browser/browser_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/common/weblayer_paths.h"
#include "weblayer/public/navigation.h"
#include "weblayer/public/navigation_controller.h"
#include "weblayer/public/navigation_observer.h"
#include "weblayer/public/tab.h"
#include "weblayer/shell/browser/shell.h"
#include "weblayer/test/interstitial_utils.h"
#include "weblayer/test/test_navigation_observer.h"
#include "weblayer/test/weblayer_browser_test.h"
#include "weblayer/test/weblayer_browser_test_utils.h"

namespace weblayer {

class BrowserPersisterTestHelper {
 public:
  static sessions::CommandStorageManager* GetCommandStorageManager(
      BrowserPersister* persister) {
    return persister->command_storage_manager_.get();
  }
};

namespace {

class OneShotNavigationObserver : public NavigationObserver {
 public:
  explicit OneShotNavigationObserver(Shell* shell) : tab_(shell->tab()) {
    tab_->GetNavigationController()->AddObserver(this);
  }

  ~OneShotNavigationObserver() override {
    tab_->GetNavigationController()->RemoveObserver(this);
  }

  void WaitForNavigation() { run_loop_.Run(); }

  bool completed() { return completed_; }
  bool is_error_page() { return is_error_page_; }
  Navigation::LoadError load_error() { return load_error_; }
  int http_status_code() { return http_status_code_; }
  NavigationState navigation_state() { return navigation_state_; }

 private:
  // NavigationObserver implementation:
  void NavigationCompleted(Navigation* navigation) override {
    completed_ = true;
    Finish(navigation);
  }

  void NavigationFailed(Navigation* navigation) override { Finish(navigation); }

  void Finish(Navigation* navigation) {
    is_error_page_ = navigation->IsErrorPage();
    load_error_ = navigation->GetLoadError();
    http_status_code_ = navigation->GetHttpStatusCode();
    navigation_state_ = navigation->GetState();
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  Tab* tab_;
  bool completed_ = false;
  bool is_error_page_ = false;
  Navigation::LoadError load_error_ = Navigation::kNoError;
  int http_status_code_ = 0;
  NavigationState navigation_state_ = NavigationState::kWaitingResponse;
};

class BrowserObserverImpl : public BrowserObserver {
 public:
  static void WaitForNewTab(Browser* browser) {
    BrowserObserverImpl observer(browser);
    observer.Wait();
  }

 private:
  explicit BrowserObserverImpl(Browser* browser) : browser_(browser) {
    browser_->AddObserver(this);
  }
  ~BrowserObserverImpl() override { browser_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  // BrowserObserver:
  void OnTabAdded(Tab* tab) override { run_loop_.Quit(); }

  Browser* browser_;
  base::RunLoop run_loop_;
};

class BrowserNavigationObserverImpl : public BrowserObserver,
                                      public NavigationObserver {
 public:
  static void WaitForNewTabToCompleteNavigation(Browser* browser,
                                                const GURL& url,
                                                int tab_to_wait_for = 1) {
    BrowserNavigationObserverImpl observer(browser, url, tab_to_wait_for);
    observer.Wait();
  }

 private:
  BrowserNavigationObserverImpl(Browser* browser,
                                const GURL& url,
                                int tab_to_wait_for)
      : browser_(browser), url_(url), tab_to_wait_for_(tab_to_wait_for) {
    browser_->AddObserver(this);
  }
  ~BrowserNavigationObserverImpl() override {
    tab_->GetNavigationController()->RemoveObserver(this);
  }

  void Wait() { run_loop_.Run(); }

  // NavigationObserver;
  void NavigationCompleted(Navigation* navigation) override {
    if (navigation->GetURL() == url_)
      run_loop_.Quit();
  }

  // BrowserObserver:
  void OnTabAdded(Tab* tab) override {
    if (--tab_to_wait_for_ != 0)
      return;

    browser_->RemoveObserver(this);
    tab_ = tab;
    tab_->GetNavigationController()->AddObserver(this);
  }

  Browser* browser_;
  const GURL& url_;
  Tab* tab_ = nullptr;
  int tab_to_wait_for_;
  std::unique_ptr<TestNavigationObserver> navigation_observer_;
  base::RunLoop run_loop_;
};

void ShutdownBrowserPersisterAndWait(BrowserImpl* browser) {
  auto task_runner = sessions::CommandStorageManagerTestHelper(
                         BrowserPersisterTestHelper::GetCommandStorageManager(
                             browser->browser_persister()))
                         .GetBackendTaskRunner();
  browser->PrepareForShutdown();
  base::RunLoop run_loop;
  task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                run_loop.QuitClosure());
  run_loop.Run();
}

std::unique_ptr<BrowserImpl> CreateBrowser(ProfileImpl* profile,
                                           const std::string& persistence_id) {
  Browser::PersistenceInfo info;
  info.id = persistence_id;
  auto browser = Browser::Create(profile, &info);
  return std::unique_ptr<BrowserImpl>(
      static_cast<BrowserImpl*>(browser.release()));
}

}  // namespace

using BrowserPersisterTest = WebLayerBrowserTest;

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, SingleTab) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab = browser->AddTab(Tab::Create(GetProfile()));
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url, tab);
  ShutdownBrowserPersisterAndWait(browser.get());
  tab = nullptr;
  browser.reset();

  browser = CreateBrowser(GetProfile(), "x");
  // Should be no tabs while waiting for restore.
  EXPECT_TRUE(browser->GetTabs().empty());
  // Wait for the restore and navigation to complete.
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser.get(), url);

  ASSERT_EQ(1u, browser->GetTabs().size());
  EXPECT_EQ(browser->GetTabs()[0], browser->GetActiveTab());
  EXPECT_EQ(1, browser->GetTabs()[0]
                   ->GetNavigationController()
                   ->GetNavigationListSize());
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, RestoresGuid) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab = browser->AddTab(Tab::Create(GetProfile()));
  const std::string original_guid = tab->GetGuid();
  EXPECT_FALSE(original_guid.empty());
  EXPECT_TRUE(base::IsValidGUID(original_guid));
  const GURL url = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url, tab);
  ShutdownBrowserPersisterAndWait(browser.get());
  tab = nullptr;
  browser.reset();

  browser = CreateBrowser(GetProfile(), "x");
  // Should be no tabs while waiting for restore.
  EXPECT_TRUE(browser->GetTabs().empty());
  // Wait for the restore and navigation to complete.
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser.get(), url);

  ASSERT_EQ(1u, browser->GetTabs().size());
  EXPECT_EQ(browser->GetTabs()[0], browser->GetActiveTab());
  EXPECT_EQ(original_guid, browser->GetTabs()[0]->GetGuid());
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, TwoTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::unique_ptr<BrowserImpl> browser = CreateBrowser(GetProfile(), "x");
  Tab* tab1 = browser->AddTab(Tab::Create(GetProfile()));
  const GURL url1 = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url1, tab1);

  Tab* tab2 = browser->AddTab(Tab::Create(GetProfile()));
  const GURL url2 = embedded_test_server()->GetURL("/simple_page2.html");
  NavigateAndWaitForCompletion(url2, tab2);
  browser->SetActiveTab(tab2);

  // Shut down the service.
  ShutdownBrowserPersisterAndWait(browser.get());
  tab1 = tab2 = nullptr;
  browser.reset();

  // Recreate the browser and run the assertions twice to ensure we handle
  // correctly storing state of tabs that need to be reloaded.
  for (int i = 0; i < 2; ++i) {
    browser = CreateBrowser(GetProfile(), "x");
    // Should be no tabs while waiting for restore.
    EXPECT_TRUE(browser->GetTabs().empty()) << "iteration " << i;
    // Wait for the restore and navigation to complete. This waits for the
    // second tab as that was the active one.
    BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
        browser.get(), url2, 2);

    ASSERT_EQ(2u, browser->GetTabs().size()) << "iteration " << i;
    // The first tab shouldn't have loaded yet, as it's not active.
    EXPECT_TRUE(static_cast<TabImpl*>(browser->GetTabs()[0])
                    ->web_contents()
                    ->GetController()
                    .NeedsReload())
        << "iteration " << i;
    EXPECT_EQ(browser->GetTabs()[1], browser->GetActiveTab())
        << "iteration " << i;
    EXPECT_EQ(1, browser->GetTabs()[1]
                     ->GetNavigationController()
                     ->GetNavigationListSize())
        << "iteration " << i;

    ShutdownBrowserPersisterAndWait(browser.get());
  }
}

IN_PROC_BROWSER_TEST_F(BrowserPersisterTest, MoveBetweenBrowsers) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Create a browser with two tabs.
  std::unique_ptr<BrowserImpl> browser1 = CreateBrowser(GetProfile(), "x");
  Tab* tab1 = browser1->AddTab(Tab::Create(GetProfile()));
  const GURL url1 = embedded_test_server()->GetURL("/simple_page.html");
  NavigateAndWaitForCompletion(url1, tab1);

  Tab* tab2 = browser1->AddTab(Tab::Create(GetProfile()));
  const GURL url2 = embedded_test_server()->GetURL("/simple_page2.html");
  NavigateAndWaitForCompletion(url2, tab2);
  browser1->SetActiveTab(tab2);

  // Create another browser with a single tab.
  std::unique_ptr<BrowserImpl> browser2 = CreateBrowser(GetProfile(), "y");
  Tab* tab3 = browser2->AddTab(Tab::Create(GetProfile()));
  const GURL url3 = embedded_test_server()->GetURL("/simple_page3.html");
  NavigateAndWaitForCompletion(url3, tab3);

  // Move |tab2| to |browser2|.
  browser2->AddTab(browser1->RemoveTab(tab2));
  browser2->SetActiveTab(tab2);

  ShutdownBrowserPersisterAndWait(browser1.get());
  ShutdownBrowserPersisterAndWait(browser2.get());
  tab1 = nullptr;
  browser1.reset();

  tab2 = tab3 = nullptr;
  browser2.reset();

  // Restore the browsers.
  browser1 = CreateBrowser(GetProfile(), "x");
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser1.get(), url1, 1);
  ASSERT_EQ(1u, browser1->GetTabs().size());
  EXPECT_EQ(1, browser1->GetTabs()[0]
                   ->GetNavigationController()
                   ->GetNavigationListSize());

  browser2 = CreateBrowser(GetProfile(), "y");
  BrowserNavigationObserverImpl::WaitForNewTabToCompleteNavigation(
      browser2.get(), url2, 2);
  ASSERT_EQ(2u, browser2->GetTabs().size());
  EXPECT_EQ(1, browser2->GetTabs()[1]
                   ->GetNavigationController()
                   ->GetNavigationListSize());

  // As |tab3| isn't active it needs to be loaded. Force that now.
  TabImpl* restored_tab_3 = static_cast<TabImpl*>(browser2->GetTabs()[0]);
  EXPECT_TRUE(restored_tab_3->web_contents()->GetController().NeedsReload());
  restored_tab_3->web_contents()->GetController().LoadIfNecessary();
  content::WaitForLoadStop(restored_tab_3->web_contents());
}

}  // namespace weblayer
