// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/find_in_page_observer.h"
#include "components/app_modal/app_modal_dialog_queue.h"
#include "components/app_modal/javascript_app_modal_dialog.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/download/public/common/download_item.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;

namespace ui_test_utils {

namespace {

Browser* WaitForBrowserNotInSet(std::set<Browser*> excluded_browsers) {
  Browser* new_browser = GetBrowserNotInSet(excluded_browsers);
  if (new_browser == NULL) {
    BrowserAddedObserver observer;
    new_browser = observer.WaitForSingleNewBrowser();
    // The new browser should never be in |excluded_browsers|.
    DCHECK(!base::ContainsKey(excluded_browsers, new_browser));
  }
  return new_browser;
}

class AppModalDialogWaiter : public app_modal::AppModalDialogObserver {
 public:
  AppModalDialogWaiter() : dialog_(nullptr) {}
  ~AppModalDialogWaiter() override {}

  app_modal::JavaScriptAppModalDialog* Wait() {
    if (dialog_)
      return dialog_;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    EXPECT_TRUE(dialog_);
    return dialog_;
  }

  // AppModalDialogObserver:
  void Notify(app_modal::JavaScriptAppModalDialog* dialog) override {
    DCHECK(!dialog_);
    dialog_ = dialog;
    CheckForHangMonitorDisabling(dialog);
    if (message_loop_runner_.get() && message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }

  static void CheckForHangMonitorDisabling(
      app_modal::JavaScriptAppModalDialog* dialog) {
    // If a test waits for a beforeunload dialog but hasn't disabled the
    // beforeunload hang timer before triggering it, there will be a race
    // between the dialog and the timer and the test will be flaky. We can't
    // disable the timer here, as it's too late, but we can tell when we've won
    // a race that we shouldn't have been in.
    if (!dialog->is_before_unload_dialog())
      return;

    // Unfortunately we don't know which frame spawned this dialog and should
    // have the hang monitor disabled, so we cheat a bit and search for *a*
    // frame with the hang monitor disabled. The failure case that's worrisome
    // is someone who doesn't know the requirement to disable the hang monitor,
    // and this will catch that case.
    auto* contents = dialog->web_contents();
    for (auto* frame : contents->GetAllFrames())
      if (frame->IsBeforeUnloadHangMonitorDisabledForTesting())
        return;

    FAIL() << "If waiting for a beforeunload dialog, the beforeunload timer "
              "must be disabled on the spawning frame to avoid flakiness.";
  }

 private:
  app_modal::JavaScriptAppModalDialog* dialog_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(AppModalDialogWaiter);
};

}  // namespace

bool GetCurrentTabTitle(const Browser* browser, base::string16* title) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;
  NavigationEntry* last_entry = web_contents->GetController().GetActiveEntry();
  if (!last_entry)
    return false;
  title->assign(last_entry->GetTitleForDisplay());
  return true;
}

void NavigateToURL(NavigateParams* params) {
  Navigate(params);
  content::WaitForLoadStop(params->navigated_or_inserted_contents);
}

void NavigateToURLWithPost(Browser* browser, const GURL& url) {
  NavigateParams params(browser, url, ui::PAGE_TRANSITION_FORM_SUBMIT);

  std::string post_data("test=body");
  params.post_data = network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());
  params.uses_post = true;

  NavigateToURL(&params);
}

void NavigateToURL(Browser* browser, const GURL& url) {
  NavigateToURLWithDisposition(browser, url, WindowOpenDisposition::CURRENT_TAB,
                               BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

void NavigateToURLWithDispositionBlockUntilNavigationsComplete(
    Browser* browser,
    const GURL& url,
    int number_of_navigations,
    WindowOpenDisposition disposition,
    int browser_test_flags) {
  TabStripModel* tab_strip = browser->tab_strip_model();
  if (disposition == WindowOpenDisposition::CURRENT_TAB &&
      tab_strip->GetActiveWebContents())
    content::WaitForLoadStop(tab_strip->GetActiveWebContents());
  content::TestNavigationObserver same_tab_observer(
      tab_strip->GetActiveWebContents(), number_of_navigations,
      content::MessageLoopRunner::QuitMode::DEFERRED);

  std::set<Browser*> initial_browsers;
  for (auto* browser : *BrowserList::GetInstance())
    initial_browsers.insert(browser);

  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  browser->OpenURL(OpenURLParams(
      url, Referrer(), disposition, ui::PAGE_TRANSITION_TYPED, false));
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_BROWSER)
    browser = WaitForBrowserNotInSet(initial_browsers);
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_TAB)
    tab_added_observer.Wait();
  if (!(browser_test_flags & BROWSER_TEST_WAIT_FOR_NAVIGATION)) {
    // Some other flag caused the wait prior to this.
    return;
  }
  WebContents* web_contents = NULL;
  if (disposition == WindowOpenDisposition::NEW_BACKGROUND_TAB) {
    // We've opened up a new tab, but not selected it.
    TabStripModel* tab_strip = browser->tab_strip_model();
    web_contents = tab_strip->GetWebContentsAt(tab_strip->active_index() + 1);
    EXPECT_TRUE(web_contents != NULL)
        << " Unable to wait for navigation to \"" << url.spec()
        << "\" because the new tab is not available yet";
    if (!web_contents)
      return;
  } else if ((disposition == WindowOpenDisposition::CURRENT_TAB) ||
             (disposition == WindowOpenDisposition::NEW_FOREGROUND_TAB) ||
             (disposition == WindowOpenDisposition::SINGLETON_TAB)) {
    // The currently selected tab is the right one.
    web_contents = browser->tab_strip_model()->GetActiveWebContents();
  }
  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    same_tab_observer.Wait();
    return;
  } else if (web_contents) {
    content::TestNavigationObserver observer(
        web_contents, number_of_navigations,
        content::MessageLoopRunner::QuitMode::DEFERRED);
    observer.Wait();
    return;
  }
  EXPECT_TRUE(NULL != web_contents) << " Unable to wait for navigation to \""
                                    << url.spec() << "\""
                                    << " because we can't get the tab contents";
}

void NavigateToURLWithDisposition(Browser* browser,
                                  const GURL& url,
                                  WindowOpenDisposition disposition,
                                  int browser_test_flags) {
  NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser,
      url,
      1,
      disposition,
      browser_test_flags);
}

void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations) {
  NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser, url, number_of_navigations, WindowOpenDisposition::CURRENT_TAB,
      BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

base::FilePath GetTestFilePath(const base::FilePath& dir,
                               const base::FilePath& file) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.Append(dir).Append(file);
}

GURL GetTestUrl(const base::FilePath& dir, const base::FilePath& file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

bool GetRelativeBuildDirectory(base::FilePath* build_dir) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // This function is used to find the build directory so TestServer can serve
  // built files (nexes, etc).  TestServer expects a path relative to the source
  // root.
  base::FilePath exe_dir =
      base::CommandLine::ForCurrentProcess()->GetProgram().DirName();
  base::FilePath src_dir;
  if (!base::PathService::Get(base::DIR_SOURCE_ROOT, &src_dir))
    return false;

  // We must first generate absolute paths to SRC and EXE and from there
  // generate a relative path.
  if (!exe_dir.IsAbsolute())
    exe_dir = base::MakeAbsoluteFilePath(exe_dir);
  if (!src_dir.IsAbsolute())
    src_dir = base::MakeAbsoluteFilePath(src_dir);
  if (!exe_dir.IsAbsolute())
    return false;
  if (!src_dir.IsAbsolute())
    return false;

  size_t match, exe_size, src_size;
  std::vector<base::FilePath::StringType> src_parts, exe_parts;

  // Determine point at which src and exe diverge.
  exe_dir.GetComponents(&exe_parts);
  src_dir.GetComponents(&src_parts);
  exe_size = exe_parts.size();
  src_size = src_parts.size();
  for (match = 0; match < exe_size && match < src_size; ++match) {
    if (exe_parts[match] != src_parts[match])
      break;
  }

  // Create a relative path.
  *build_dir = base::FilePath();
  for (size_t tmp_itr = match; tmp_itr < src_size; ++tmp_itr)
    *build_dir = build_dir->Append(FILE_PATH_LITERAL(".."));
  for (; match < exe_size; ++match)
    *build_dir = build_dir->Append(exe_parts[match]);
  return true;
}

app_modal::JavaScriptAppModalDialog* WaitForAppModalDialog() {
  app_modal::AppModalDialogQueue* dialog_queue =
      app_modal::AppModalDialogQueue::GetInstance();
  if (dialog_queue->HasActiveDialog()) {
    AppModalDialogWaiter::CheckForHangMonitorDisabling(
        dialog_queue->active_dialog());
    return dialog_queue->active_dialog();
  }
  AppModalDialogWaiter waiter;
  return waiter.Wait();
}

int FindInPage(WebContents* tab,
               const base::string16& search_string,
               bool forward,
               bool match_case,
               int* ordinal,
               gfx::Rect* selection_rect) {
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(tab);
  find_tab_helper->StartFinding(search_string, forward, match_case,
                                true /* run_synchronously_for_testing */);
  FindInPageNotificationObserver observer(tab);
  observer.Wait();
  if (ordinal)
    *ordinal = observer.active_match_ordinal();
  if (selection_rect)
    *selection_rect = observer.selection_rect();
  return observer.number_of_matches();
}

void DownloadURL(Browser* browser, const GURL& download_url) {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser->profile());
  std::unique_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

  ui_test_utils::NavigateToURL(browser, download_url);
  observer->WaitForFinished();
}

void SendToOmniboxAndSubmit(LocationBar* location_bar,
                            const std::string& input,
                            base::TimeTicks match_selection_timestamp) {
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  omnibox->model()->OnSetFocus(false);
  omnibox->SetUserText(base::ASCIIToUTF16(input));
  location_bar->AcceptInput(match_selection_timestamp);
  while (!omnibox->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::NotificationService::AllSources());
    observer.Wait();
  }
}

Browser* GetBrowserNotInSet(const std::set<Browser*>& excluded_browsers) {
  for (auto* browser : *BrowserList::GetInstance()) {
    if (excluded_browsers.find(browser) == excluded_browsers.end())
      return browser;
  }
  return nullptr;
}

namespace {

void GetCookieCallback(base::RepeatingClosure callback,
                       net::CookieList* cookies,
                       const net::CookieList& cookie_list) {
  *cookies = cookie_list;
  callback.Run();
}

}  // namespace

void GetCookies(const GURL& url,
                WebContents* contents,
                int* value_size,
                std::string* value) {
  *value_size = -1;
  if (url.is_valid() && contents) {
    base::RunLoop loop;
    auto* storage_partition =
        contents->GetMainFrame()->GetProcess()->GetStoragePartition();
    net::CookieOptions options;
    net::CookieList cookie_list;
    storage_partition->GetCookieManagerForBrowserProcess()->GetCookieList(
        url, options,
        base::BindOnce(GetCookieCallback, loop.QuitClosure(), &cookie_list));
    loop.Run();

    *value = net::CanonicalCookie::BuildCookieLine(cookie_list);
    *value_size = static_cast<int>(value->size());
  }
}

WindowedTabAddedNotificationObserver::WindowedTabAddedNotificationObserver(
    const content::NotificationSource& source)
    : WindowedNotificationObserver(chrome::NOTIFICATION_TAB_ADDED, source),
      added_tab_(NULL) {
}

void WindowedTabAddedNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  added_tab_ = content::Details<WebContents>(details).ptr();
  content::WindowedNotificationObserver::Observe(type, source, details);
}

UrlLoadObserver::UrlLoadObserver(const GURL& url,
                                 const content::NotificationSource& source)
    : WindowedNotificationObserver(content::NOTIFICATION_LOAD_STOP, source),
      url_(url) {
}

UrlLoadObserver::~UrlLoadObserver() {}

void UrlLoadObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  NavigationController* controller =
      content::Source<NavigationController>(source).ptr();
  if (controller->GetWebContents()->GetURL() != url_)
    return;

  WindowedNotificationObserver::Observe(type, source, details);
}

BrowserAddedObserver::BrowserAddedObserver()
    : notification_observer_(
          chrome::NOTIFICATION_BROWSER_OPENED,
          content::NotificationService::AllSources()) {
  for (auto* browser : *BrowserList::GetInstance())
    original_browsers_.insert(browser);
}

BrowserAddedObserver::~BrowserAddedObserver() {
}

Browser* BrowserAddedObserver::WaitForSingleNewBrowser() {
  notification_observer_.Wait();
  // Ensure that only a single new browser has appeared.
  EXPECT_EQ(original_browsers_.size() + 1, chrome::GetTotalBrowserCount());
  return GetBrowserNotInSet(original_browsers_);
}

HistoryEnumerator::HistoryEnumerator(Profile* profile) {
  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  hs->QueryHistory(base::string16(),
                   history::QueryOptions(),
                   base::Bind(&HistoryEnumerator::HistoryQueryComplete,
                              base::Unretained(this),
                              message_loop_runner->QuitClosure()),
                   &tracker_);
  message_loop_runner->Run();
}

HistoryEnumerator::~HistoryEnumerator() {}

void HistoryEnumerator::HistoryQueryComplete(
    const base::Closure& quit_task,
    history::QueryResults* results) {
  for (size_t i = 0; i < results->size(); ++i)
    urls_.push_back((*results)[i].url());
  quit_task.Run();
}

// Wait for HistoryService to load.
class WaitHistoryLoadedObserver : public history::HistoryServiceObserver {
 public:
  explicit WaitHistoryLoadedObserver(content::MessageLoopRunner* runner);
  ~WaitHistoryLoadedObserver() override;

  // history::HistoryServiceObserver:
  void OnHistoryServiceLoaded(history::HistoryService* service) override;

 private:
  // weak
  content::MessageLoopRunner* runner_;
};

WaitHistoryLoadedObserver::WaitHistoryLoadedObserver(
    content::MessageLoopRunner* runner)
    : runner_(runner) {
}

WaitHistoryLoadedObserver::~WaitHistoryLoadedObserver() {
}

void WaitHistoryLoadedObserver::OnHistoryServiceLoaded(
    history::HistoryService* service) {
  runner_->Quit();
}

void WaitForHistoryToLoad(history::HistoryService* history_service) {
  if (!history_service->BackendLoaded()) {
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    WaitHistoryLoadedObserver observer(runner.get());
    ScopedObserver<history::HistoryService, history::HistoryServiceObserver>
        scoped_observer(&observer);
    scoped_observer.Add(history_service);
    runner->Run();
  }
}

}  // namespace ui_test_utils
