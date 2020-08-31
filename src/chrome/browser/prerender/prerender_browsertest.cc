// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/net/profile_network_context_service.h"
#include "chrome/browser/net/profile_network_context_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/prerender/prerender_tab_helper.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/task_manager/providers/web_contents/web_contents_tags_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/favicon/core/favicon_driver_observer.h"
#include "components/nacl/common/buildflags.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/page_load_metrics/browser/metrics_web_contents_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "components/page_load_metrics/common/test/page_load_metrics_test_util.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/safe_browsing/core/features.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/ppapi_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/escape.h"
#include "net/cert/x509_certificate.h"
#include "net/dns/mock_host_resolver.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "url/gurl.h"

using content::BrowserThread;
using content::OpenURLParams;
using content::Referrer;
using content::RenderFrameHost;
using content::TestNavigationObserver;
using content::WebContents;
using content::WebContentsObserver;
using prerender::test_utils::TestPrerender;
using prerender::test_utils::TestPrerenderContents;
using task_manager::browsertest_util::WaitForTaskManagerRows;

// crbug.com/708158
#if !defined(OS_MACOSX) || !defined(ADDRESS_SANITIZER)

// Prerender tests work as follows:
//
// A page with a prefetch link to the test page is loaded.  Once prerendered,
// its Javascript function DidPrerenderPass() is called, which returns true if
// the page behaves as expected when prerendered.
//
// The prerendered page is then displayed on a tab.  The Javascript function
// DidDisplayPass() is called, and returns true if the page behaved as it
// should while being displayed.

namespace prerender {

namespace {

const char kPrefetchJpeg[] = "/prerender/image.jpeg";

class FaviconUpdateWatcher : public favicon::FaviconDriverObserver {
 public:
  explicit FaviconUpdateWatcher(content::WebContents* web_contents) {
    scoped_observer_.Add(
        favicon::ContentFaviconDriver::FromWebContents(web_contents));
  }

  void Wait() {
    if (seen_)
      return;

    running_ = true;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override {
    seen_ = true;
    if (!running_)
      return;

    message_loop_runner_->Quit();
    running_ = false;
  }

  bool seen_ = false;
  bool running_ = false;
  ScopedObserver<favicon::FaviconDriver, favicon::FaviconDriverObserver>
      scoped_observer_{this};
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(FaviconUpdateWatcher);
};

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "/server-redirect?";
  return kServerRedirectBase + net::EscapeQueryParamValue(dest_url, false);
}

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Browser* browser, int remove_mask) {
  content::BrowsingDataRemover* remover =
      content::BrowserContext::GetBrowsingDataRemover(browser->profile());
  content::BrowsingDataRemoverCompletionObserver observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), remove_mask,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB, &observer);
  observer.BlockUntilCompletion();
  // BrowsingDataRemover deletes itself.
}

// Returns true if the prerender is expected to abort on its own, before
// attempting to swap it.
bool ShouldAbortPrerenderBeforeSwap(FinalStatus status) {
  switch (status) {
    case FINAL_STATUS_USED:
    case FINAL_STATUS_APP_TERMINATING:
    case FINAL_STATUS_PROFILE_DESTROYED:
    case FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
    // We'll crash the renderer after it's loaded.
    case FINAL_STATUS_RENDERER_CRASHED:
    case FINAL_STATUS_CANCELLED:
      return false;
    default:
      return true;
  }
}

// Waits for the destruction of a RenderProcessHost's IPC channel.
// Used to make sure the PrerenderLinkManager's OnChannelClosed function has
// been called, before checking its state.
class ChannelDestructionWatcher {
 public:
  ChannelDestructionWatcher() : channel_destroyed_(false) {}

  ~ChannelDestructionWatcher() = default;

  void WatchChannel(content::RenderProcessHost* host) {
    host->AddFilter(new DestructionMessageFilter(this));
  }

  void WaitForChannelClose() {
    run_loop_.Run();
    EXPECT_TRUE(channel_destroyed_);
  }

 private:
  // When destroyed, calls ChannelDestructionWatcher::OnChannelDestroyed.
  // Ignores all messages.
  class DestructionMessageFilter : public content::BrowserMessageFilter {
   public:
    explicit DestructionMessageFilter(ChannelDestructionWatcher* watcher)
        : BrowserMessageFilter(0), watcher_(watcher) {}

   private:
    ~DestructionMessageFilter() override {
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(&ChannelDestructionWatcher::OnChannelDestroyed,
                         base::Unretained(watcher_)));
    }

    bool OnMessageReceived(const IPC::Message& message) override {
      return false;
    }

    ChannelDestructionWatcher* watcher_;

    DISALLOW_COPY_AND_ASSIGN(DestructionMessageFilter);
  };

  void OnChannelDestroyed() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    EXPECT_FALSE(channel_destroyed_);
    channel_destroyed_ = true;
    run_loop_.Quit();
  }

  bool channel_destroyed_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(ChannelDestructionWatcher);
};

// A navigation observer to wait on either a new load or a swap of a
// WebContents. On swap, if the new WebContents is still loading, wait for that
// load to complete as well. Note that the load must begin after the observer is
// attached.
class NavigationOrSwapObserver : public WebContentsObserver,
                                 public TabStripModelObserver {
 public:
  // Waits for either a new load or a swap of |tab_strip_model|'s active
  // WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(1) {
    EXPECT_NE(TabStripModel::kNoTab,
              tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  // Waits for either |number_of_loads| loads or a swap of |tab_strip_model|'s
  // active WebContents.
  NavigationOrSwapObserver(TabStripModel* tab_strip_model,
                           WebContents* web_contents,
                           int number_of_loads)
      : WebContentsObserver(web_contents),
        tab_strip_model_(tab_strip_model),
        did_start_loading_(false),
        number_of_loads_(number_of_loads) {
    EXPECT_NE(TabStripModel::kNoTab,
              tab_strip_model->GetIndexOfWebContents(web_contents));
    tab_strip_model_->AddObserver(this);
  }

  ~NavigationOrSwapObserver() override {
    tab_strip_model_->RemoveObserver(this);
  }

  void set_did_start_loading() { did_start_loading_ = true; }

  void Wait() { loop_.Run(); }

  // WebContentsObserver implementation:
  void DidStartLoading() override { did_start_loading_ = true; }
  void DidStopLoading() override {
    if (!did_start_loading_)
      return;
    number_of_loads_--;
    if (number_of_loads_ == 0)
      loop_.Quit();
  }

  // TabStripModelObserver implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kReplaced)
      return;

    auto* replace = change.GetReplace();
    if (replace->old_contents != web_contents())
      return;

    // Switch to observing the new WebContents.
    Observe(replace->new_contents);
    if (replace->new_contents->IsLoading()) {
      // If the new WebContents is still loading, wait for it to complete.
      // Only one load post-swap is supported.
      did_start_loading_ = true;
      number_of_loads_ = 1;
    } else {
      loop_.Quit();
    }
  }

 private:
  TabStripModel* tab_strip_model_;
  bool did_start_loading_;
  int number_of_loads_;
  base::RunLoop loop_;
};

// Waits for a new tab to open and a navigation or swap in it.
class NewTabNavigationOrSwapObserver : public TabStripModelObserver,
                                       public BrowserListObserver {
 public:
  NewTabNavigationOrSwapObserver() {
    BrowserList::AddObserver(this);
    for (const Browser* browser : *BrowserList::GetInstance())
      browser->tab_strip_model()->AddObserver(this);
  }

  ~NewTabNavigationOrSwapObserver() override {
    BrowserList::RemoveObserver(this);
  }

  void Wait() {
    new_tab_run_loop_.Run();
    swap_observer_->Wait();
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() != TabStripModelChange::kInserted || swap_observer_)
      return;

    WebContents* new_tab = change.GetInsert()->contents[0].contents;
    swap_observer_ =
        std::make_unique<NavigationOrSwapObserver>(tab_strip_model, new_tab);
    swap_observer_->set_did_start_loading();

    new_tab_run_loop_.Quit();
  }

  // BrowserListObserver:
  void OnBrowserAdded(Browser* browser) override {
    browser->tab_strip_model()->AddObserver(this);
  }

 private:
  base::RunLoop new_tab_run_loop_;
  std::unique_ptr<NavigationOrSwapObserver> swap_observer_;

  DISALLOW_COPY_AND_ASSIGN(NewTabNavigationOrSwapObserver);
};

}  // namespace

class PrerenderBrowserTest : public test_utils::PrerenderInProcessBrowserTest {
 public:
  PrerenderBrowserTest()
      : call_javascript_(true),
        check_load_events_(true),
        loader_path_("/prerender/prerender_loader.html") {}

  ~PrerenderBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kNoUserGestureRequiredPolicy);

    test_utils::PrerenderInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const std::string& html_file,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    GURL url = src_server()->GetURL(MakeAbsolute(html_file));
    return PrerenderTestURL(url, expected_final_status,
                            expected_number_of_loads);
  }

  std::unique_ptr<TestPrerender> PrerenderTestURL(
      const GURL& url,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    std::vector<FinalStatus> expected_final_status_queue(1,
                                                         expected_final_status);
    auto prerenders = PrerenderTestURLImpl(url, expected_final_status_queue,
                                           expected_number_of_loads);
    CHECK_EQ(1u, prerenders.size());
    return std::move(prerenders[0]);
  }

  std::vector<std::unique_ptr<TestPrerender>> PrerenderTestURL(
      const std::string& html_file,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    GURL url = src_server()->GetURL(MakeAbsolute(html_file));
    return PrerenderTestURLImpl(url, expected_final_status_queue,
                                expected_number_of_loads);
  }

  void SetUpOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::SetUpOnMainThread();
    prerender::PrerenderManager::SetMode(
        prerender::PrerenderManager::DEPRECATED_PRERENDER_MODE_ENABLED);
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    // This one test fails with the host resolver redirecting all hosts.
    if (std::string(test_info->name()) != "PrerenderServerRedirectInIframe")
      host_resolver()->AddRule("*", "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    test_utils::PrerenderInProcessBrowserTest::TearDownOnMainThread();
    interceptor_.reset();
  }

  void NavigateToDestURL() const {
    NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, true);
  }

  // Opens the url in a new tab, with no opener.
  void NavigateToDestURLWithDisposition(WindowOpenDisposition disposition,
                                        bool expect_swap_to_succeed) const {
    NavigateToURLWithParams(
        content::OpenURLParams(dest_url_, Referrer(), disposition,
                               ui::PAGE_TRANSITION_TYPED, false),
        expect_swap_to_succeed);
  }

  void NavigateToURL(const std::string& dest_html_file) const {
    NavigateToURLWithDisposition(dest_html_file,
                                 WindowOpenDisposition::CURRENT_TAB, true);
  }

  void NavigateToURLWithDisposition(const std::string& dest_html_file,
                                    WindowOpenDisposition disposition,
                                    bool expect_swap_to_succeed) const {
    GURL dest_url = embedded_test_server()->GetURL(dest_html_file);
    NavigateToURLWithDisposition(dest_url, disposition, expect_swap_to_succeed);
  }

  void NavigateToURLWithDisposition(const GURL& dest_url,
                                    WindowOpenDisposition disposition,
                                    bool expect_swap_to_succeed) const {
    NavigateToURLWithParams(
        content::OpenURLParams(dest_url, Referrer(), disposition,
                               ui::PAGE_TRANSITION_TYPED, false),
        expect_swap_to_succeed);
  }

  void NavigateToURLWithParams(const content::OpenURLParams& params,
                               bool expect_swap_to_succeed) const {
    NavigateToURLImpl(params, expect_swap_to_succeed);
  }

  void OpenDestURLViaClick() const { OpenURLViaClick(dest_url_); }

  void OpenURLViaClick(const GURL& url) const {
    OpenURLWithJSImpl("Click", url, GURL(), false);
  }

  void OpenDestURLViaClickTarget() const {
    OpenURLWithJSImpl("ClickTarget", dest_url_, GURL(), true);
  }

  void OpenDestURLViaClickPing(const GURL& ping_url) const {
    OpenURLWithJSImpl("ClickPing", dest_url_, ping_url, false);
  }

  void OpenDestURLViaClickNewWindow() const {
    OpenURLWithJSImpl("ShiftClick", dest_url_, GURL(), true);
  }

  void OpenDestURLViaClickNewForegroundTab() const {
#if defined(OS_MACOSX)
    OpenURLWithJSImpl("MetaShiftClick", dest_url_, GURL(), true);
#else
    OpenURLWithJSImpl("CtrlShiftClick", dest_url_, GURL(), true);
#endif
  }

  void OpenDestURLViaWindowOpen() const { OpenURLViaWindowOpen(dest_url_); }

  void OpenURLViaWindowOpen(const GURL& url) const {
    OpenURLWithJSImpl("WindowOpen", url, GURL(), true);
  }

  void RemoveLinkElement(int i) const {
    GetActiveWebContents()->GetMainFrame()->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16(base::StringPrintf("RemoveLinkElement(%d)", i)),
        base::NullCallback());
  }

  void ClickToNextPageAfterPrerender() {
    TestNavigationObserver nav_observer(GetActiveWebContents());
    RenderFrameHost* render_frame_host = GetActiveWebContents()->GetMainFrame();
    render_frame_host->ExecuteJavaScriptForTests(
        base::ASCIIToUTF16("ClickOpenLink()"), base::NullCallback());
    nav_observer.Wait();
  }

  void NavigateToNextPageAfterPrerender() const {
    ui_test_utils::NavigateToURL(
        current_browser(),
        embedded_test_server()->GetURL("/prerender/prerender_page.html"));
  }

  // Called after the prerendered page has been navigated to and then away from.
  // Navigates back through the history to the prerendered page.
  void GoBackToPrerender() {
    TestNavigationObserver back_nav_observer(GetActiveWebContents());
    chrome::GoBack(current_browser(), WindowOpenDisposition::CURRENT_TAB);
    back_nav_observer.Wait();
    bool original_prerender_page = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(IsOriginalPrerenderPage())",
        &original_prerender_page));
    EXPECT_TRUE(original_prerender_page);
  }

  void DisableJavascriptCalls() { call_javascript_ = false; }

  void EnableJavascriptCalls() { call_javascript_ = true; }

  void DisableLoadEventCheck() { check_load_events_ = false; }

  const PrerenderLinkManager* GetPrerenderLinkManager() const {
    PrerenderLinkManager* prerender_link_manager =
        PrerenderLinkManagerFactory::GetForBrowserContext(
            current_browser()->profile());
    return prerender_link_manager;
  }

  // Synchronization note: The IPCs used to communicate DOM events back to the
  // referring web page (see blink::mojom::PrerenderHandleClient) may race w/
  // the IPCs used here to inject script. The WaitFor* variants should be used
  // when an event was expected to happen or to happen soon.

  int GetPrerenderEventCount(int index, const std::string& type) const {
    int event_count;
    std::string expression = base::StringPrintf(
        "window.domAutomationController.send("
        "    GetPrerenderEventCount(%d, '%s'))",
        index, type.c_str());

    CHECK(content::ExecuteScriptAndExtractInt(GetActiveWebContents(),
                                              expression, &event_count));
    return event_count;
  }

  bool DidReceivePrerenderStartEventForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderstart") > 0;
  }

  int GetPrerenderLoadEventCountForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderload");
  }

  bool DidReceivePrerenderStopEventForLinkNumber(int index) const {
    return GetPrerenderEventCount(index, "webkitprerenderstop") > 0;
  }

  void WaitForPrerenderEventCount(int index,
                                  const std::string& type,
                                  int count) const {
    int dummy;
    std::string expression = base::StringPrintf(
        "WaitForPrerenderEventCount(%d, '%s', %d,"
        "    window.domAutomationController.send.bind("
        "        window.domAutomationController, 0))",
        index, type.c_str(), count);

    CHECK(content::ExecuteScriptAndExtractInt(GetActiveWebContents(),
                                              expression, &dummy));
    CHECK_EQ(0, dummy);
  }

  void WaitForPrerenderStartEventForLinkNumber(int index) const {
    WaitForPrerenderEventCount(index, "webkitprerenderstart", 1);
  }

  void WaitForPrerenderStopEventForLinkNumber(int index) const {
    WaitForPrerenderEventCount(index, "webkitprerenderstart", 1);
  }

  bool HadPrerenderEventErrors() const {
    bool had_prerender_event_errors;
    CHECK(content::ExecuteScriptAndExtractBool(
        GetActiveWebContents(),
        "window.domAutomationController.send(Boolean("
        "    hadPrerenderEventErrors))",
        &had_prerender_event_errors));
    return had_prerender_event_errors;
  }

  // Asserting on this can result in flaky tests.  PrerenderHandles are
  // removed from the PrerenderLinkManager when the prerender is canceled from
  // the browser, when the prerenders are cancelled from the renderer process,
  // or the channel for the renderer process is closed on the IO thread.  In the
  // last case, the code must be careful to wait for the channel to close, as it
  // is done asynchronously after swapping out the old process.  See
  // ChannelDestructionWatcher.
  bool IsEmptyPrerenderLinkManager() const {
    return GetPrerenderLinkManager()->IsEmpty();
  }

  size_t GetLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->prerenders_.size();
  }

  size_t GetRunningLinkPrerenderCount() const {
    return GetPrerenderLinkManager()->CountRunningPrerenders();
  }

  // Returns length of |prerender_manager_|'s history, or SIZE_MAX on failure.
  size_t GetHistoryLength() const {
    std::unique_ptr<base::DictionaryValue> prerender_dict =
        GetPrerenderManager()->CopyAsValue();
    if (!prerender_dict)
      return std::numeric_limits<size_t>::max();
    base::ListValue* history_list;
    if (!prerender_dict->GetList("history", &history_list))
      return std::numeric_limits<size_t>::max();
    return history_list->GetSize();
  }

  void SetLoaderHostOverride(const std::string& host) {
    loader_host_override_ = host;
  }

  void set_loader_path(const std::string& path) { loader_path_ = path; }

  void set_loader_query(const std::string& query) { loader_query_ = query; }

  GURL GetCrossDomainTestUrl(const std::string& path) {
    static const std::string secondary_domain = "www.foo.com";
    std::string url_str(base::StringPrintf(
        "http://%s:%d/%s", secondary_domain.c_str(),
        embedded_test_server()->host_port_pair().port(), path.c_str()));
    return GURL(url_str);
  }

  const GURL& dest_url() const { return dest_url_; }

  bool DidPrerenderPass(WebContents* web_contents) const {
    bool prerender_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidPrerenderPass())",
            &prerender_test_result))
      return false;
    return prerender_test_result;
  }

  bool DidDisplayPass(WebContents* web_contents) const {
    bool display_test_result = false;
    if (!content::ExecuteScriptAndExtractBool(
            web_contents,
            "window.domAutomationController.send(DidDisplayPass())",
            &display_test_result))
      return false;
    return display_test_result;
  }

  std::unique_ptr<TestPrerender> ExpectPrerender(
      FinalStatus expected_final_status) {
    return prerender_contents_factory()->ExpectPrerenderContents(
        expected_final_status);
  }

  void AddPrerender(const GURL& url, int index) {
    std::string javascript =
        base::StringPrintf("AddPrerender('%s', %d)", url.spec().c_str(), index);
    RenderFrameHost* render_frame_host = GetActiveWebContents()->GetMainFrame();
    render_frame_host->ExecuteJavaScriptForTests(base::ASCIIToUTF16(javascript),
                                                 base::NullCallback());
  }

  base::SimpleTestTickClock* OverridePrerenderManagerTimeTicks() {
    // The default zero time causes the prerender manager to do strange things.
    clock_.Advance(base::TimeDelta::FromSeconds(1));
    GetPrerenderManager()->SetTickClockForTesting(&clock_);
    return &clock_;
  }

  // Makes |url| never respond on the first load, and then with the contents of
  // |file| afterwards. When the first load has been scheduled, runs
  // |callback_io| on the IO thread.
  void CreateHangingFirstRequestInterceptor(const GURL& url,
                                            const base::FilePath& file,
                                            base::Closure closure) {
    DCHECK(!interceptor_);
    interceptor_ = std::make_unique<content::URLLoaderInterceptor>(
        base::BindLambdaForTesting(
            [=](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url == url) {
                static bool first = true;
                if (first) {
                  first = false;
                  // Need to leak the client pipe, or else the renderer will
                  // get a disconnect error and load the error page.
                  (void)params->client.Unbind().PassPipe().release();
                  closure.Run();
                  return true;
                }
              }
              return false;
            }));
  }

 private:
  // TODO(davidben): Remove this altogether so the tests don't globally assume
  // only one prerender.
  TestPrerenderContents* GetPrerenderContents() const {
    return GetPrerenderContentsFor(dest_url_);
  }

  std::vector<std::unique_ptr<TestPrerender>> PrerenderTestURLImpl(
      const GURL& prerender_url,
      const std::vector<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    dest_url_ = prerender_url;

    GURL loader_url = ServeLoaderURL(loader_path_, "REPLACE_WITH_PRERENDER_URL",
                                     prerender_url, "&" + loader_query_);
    GURL::Replacements loader_replacements;
    if (!loader_host_override_.empty())
      loader_replacements.SetHostStr(loader_host_override_);
    loader_url = loader_url.ReplaceComponents(loader_replacements);

    std::vector<std::unique_ptr<TestPrerender>> prerenders =
        NavigateWithPrerenders(loader_url, expected_final_status_queue);
    prerenders[0]->WaitForLoads(expected_number_of_loads);

    // Ensure that the referring page receives the right start and load events.
    WaitForPrerenderStartEventForLinkNumber(0);
    if (check_load_events_) {
      EXPECT_EQ(expected_number_of_loads, prerenders[0]->number_of_loads());
      WaitForPrerenderEventCount(0, "webkitprerenderload",
                                 expected_number_of_loads);
    }

    FinalStatus expected_final_status = expected_final_status_queue.front();
    if (ShouldAbortPrerenderBeforeSwap(expected_final_status)) {
      // The prerender will abort on its own. Assert it does so correctly.
      prerenders[0]->WaitForStop();
      EXPECT_FALSE(prerenders[0]->contents());
      WaitForPrerenderStopEventForLinkNumber(0);
    } else {
      // Otherwise, check that it prerendered correctly.
      TestPrerenderContents* prerender_contents = prerenders[0]->contents();
      CHECK(prerender_contents);
      EXPECT_EQ(FINAL_STATUS_UNKNOWN, prerender_contents->final_status());
      EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));

      if (call_javascript_) {
        // Check if page behaves as expected while in prerendered state.
        EXPECT_TRUE(DidPrerenderPass(prerender_contents->prerender_contents()));
      }
    }

    // Test for proper event ordering.
    EXPECT_FALSE(HadPrerenderEventErrors());

    return prerenders;
  }

  void NavigateToURLImpl(const content::OpenURLParams& params,
                         bool expect_swap_to_succeed) const {
    ASSERT_TRUE(GetPrerenderManager());
    // Make sure in navigating we have a URL to use in the PrerenderManager.
    ASSERT_TRUE(GetPrerenderContents());

    WebContents* web_contents = GetPrerenderContents()->prerender_contents();

    // Navigate and wait for either the load to finish normally or for a swap to
    // occur.
    // TODO(davidben): The only handles CURRENT_TAB navigations, which is the
    // only case tested or prerendered right now.
    CHECK_EQ(WindowOpenDisposition::CURRENT_TAB, params.disposition);
    NavigationOrSwapObserver swap_observer(current_browser()->tab_strip_model(),
                                           GetActiveWebContents());
    WebContents* target_web_contents = current_browser()->OpenURL(params);
    swap_observer.Wait();

    if (web_contents && expect_swap_to_succeed) {
      EXPECT_EQ(web_contents, target_web_contents);
      if (call_javascript_)
        EXPECT_TRUE(DidDisplayPass(web_contents));
    }
  }

  // Opens the prerendered page using javascript functions in the loader
  // page. |javascript_function_name| should be a 0 argument function which is
  // invoked. |new_web_contents| is true if the navigation is expected to
  // happen in a new WebContents via OpenURL.
  void OpenURLWithJSImpl(const std::string& javascript_function_name,
                         const GURL& url,
                         const GURL& ping_url,
                         bool new_web_contents) const {
    WebContents* web_contents = GetActiveWebContents();
    RenderFrameHost* render_frame_host = web_contents->GetMainFrame();
    // Extra arguments in JS are ignored.
    std::string javascript =
        base::StringPrintf("%s('%s', '%s')", javascript_function_name.c_str(),
                           url.spec().c_str(), ping_url.spec().c_str());

    if (new_web_contents) {
      NewTabNavigationOrSwapObserver observer;
      render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
          base::ASCIIToUTF16(javascript));
      observer.Wait();
    } else {
      NavigationOrSwapObserver observer(current_browser()->tab_strip_model(),
                                        web_contents);
      render_frame_host->ExecuteJavaScriptForTests(
          base::ASCIIToUTF16(javascript), base::NullCallback());
      observer.Wait();
    }
  }

  // Test TickClock that is set by OverridePrerenderManagerTimeTicks().
  base::SimpleTestTickClock clock_;

  GURL dest_url_;
  bool call_javascript_;
  bool check_load_events_;
  std::string loader_host_override_;
  std::string loader_path_;
  std::string loader_query_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<content::URLLoaderInterceptor> interceptor_;
};

// Checks that the correct page load metrics observers are produced without a
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PageLoadMetricsSimple) {
  // The prefetch page is used as a simple page with a nonempty layout; no
  // prefetching is performed.
  test_utils::FirstContentfulPaintManagerWaiter* simple_fcp_waiter =
      test_utils::FirstContentfulPaintManagerWaiter::Create(
          GetPrerenderManager());
  ui_test_utils::NavigateToURL(
      current_browser(), src_server()->GetURL("/prerender/prefetch_page.html"));
  simple_fcp_waiter->Wait();

  histogram_tester().ExpectTotalCount(
      "Prerender.none_PrefetchTTFCP.Reference.Cacheable.Visible", 1);
  histogram_tester().ExpectTotalCount(
      "PageLoad.PaintTiming.ParseStartToFirstContentfulPaint", 1);

  // Histogram only emitted during a prerender, which should not happen here.
  histogram_tester().ExpectTotalCount(
      "Prerender.websame_PrefetchTTFCP.Warm.Cacheable.Visible", 0);
}

// Checks that pending prerenders which are canceled before they are launched
// never get started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageRemovesPending) {
  PrerenderTestURL("/prerender/prerender_page_removes_pending.html",
                   FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetMainFrame()->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  EXPECT_FALSE(DidReceivePrerenderStartEventForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageRemovingLink) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);

  // No ChannelDestructionWatcher is needed here, since prerenders in the
  // PrerenderLinkManager should be deleted by removing the links, rather than
  // shutting down the renderer process.
  RemoveLinkElement(0);
  prerender->WaitForStop();

  WaitForPrerenderStartEventForLinkNumber(0);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageRemovingLinkWithTwoLinks) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  set_loader_query("links_to_insert=2");
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);
  WaitForPrerenderStartEventForLinkNumber(0);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  WaitForPrerenderStartEventForLinkNumber(1);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  RemoveLinkElement(1);
  prerender->WaitForStop();

  WaitForPrerenderStartEventForLinkNumber(0);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  WaitForPrerenderStartEventForLinkNumber(1);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());

  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageRemovingLinkWithTwoLinksOneLate) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;

  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(url, FINAL_STATUS_CANCELLED, 1);

  // Add a second prerender for the same link. It reuses the prerender, so only
  // the start event fires here.
  AddPrerender(url, 1);
  WaitForPrerenderStartEventForLinkNumber(1);
  EXPECT_EQ(0, GetPrerenderLoadEventCountForLinkNumber(1));
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  RemoveLinkElement(1);
  prerender->WaitForStop();

  WaitForPrerenderStartEventForLinkNumber(0);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  WaitForPrerenderStartEventForLinkNumber(1);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageRemovingLinkWithTwoLinksRemovingOne) {
  GetPrerenderManager()->mutable_config().max_link_concurrency = 2;
  GetPrerenderManager()->mutable_config().max_link_concurrency_per_launcher = 2;
  set_loader_query("links_to_insert=2");
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  WaitForPrerenderStartEventForLinkNumber(0);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  WaitForPrerenderStartEventForLinkNumber(1);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));

  RemoveLinkElement(0);
  WaitForPrerenderStartEventForLinkNumber(0);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(0));
  WaitForPrerenderStartEventForLinkNumber(1);
  EXPECT_FALSE(DidReceivePrerenderStopEventForLinkNumber(1));
  EXPECT_FALSE(HadPrerenderEventErrors());
  // IsEmptyPrerenderLinkManager() is not racy because the earlier DidReceive*
  // calls did a thread/process hop to the renderer which insured pending
  // renderer events have arrived.
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(
      GetActiveWebContents()->GetMainFrame()->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("/prerender/prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED, 0);
}

// Checks that server-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderServerRedirectInIframe) {
  std::string redirect_path =
      CreateServerRedirect("//prerender/prerender_embedded_content.html");
  base::StringPairs replacement_text;
  replacement_text.push_back(std::make_pair("REPLACE_WITH_URL", redirect_path));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text);
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(
      UrlIsInPrerenderManager("/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that the referrer is set when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrer) {
  PrerenderTestURL("/prerender/prerender_referrer.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoSSLReferrer) {
  // Use http:// url for the prerendered page main resource.
  GURL url(
      embedded_test_server()->GetURL("/prerender/prerender_no_referrer.html"));

  // Use https:// for all other resources.
  UseHttpsSrcServer();

  PrerenderTestURL(url, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that renderers using excessive memory will be terminated.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExcessiveMemory) {
  ASSERT_TRUE(GetPrerenderManager());
  GetPrerenderManager()->mutable_config().max_bytes = 30 * 1024 * 1024;
  // The excessive memory kill may happen before or after the load event as it
  // happens asynchronously with IPC calls. Even if the test does not start
  // allocating until after load, the browser process might notice before the
  // message gets through. This happens on XP debug bots because they're so
  // slow. Instead, don't bother checking the load event count.
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_excessive_memory.html",
                   FINAL_STATUS_MEMORY_LIMIT_EXCEEDED, 0);
}

// Checks shutdown code while a prerender is active.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderQuickQuit) {
  DisableJavascriptCalls();
  DisableLoadEventCheck();
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 0);
}

// Checks that pending prerenders are aborted (and never launched) when launched
// by a prerender that itself gets aborted.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAbortPendingOnCancel) {
  const char* const kHtmlFileA = "/prerender/prerender_infinite_a.html";
  const char* const kHtmlFileB = "/prerender/prerender_infinite_b.html";

  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL(kHtmlFileA, FINAL_STATUS_CANCELLED, 1);
  ASSERT_TRUE(prerender->contents());
  // Assert that the pending prerender is in there already. This relies on the
  // fact that the renderer sends out the AddLinkRelPrerender IPC before sending
  // the page load one.
  EXPECT_EQ(2U, GetLinkPrerenderCount());
  EXPECT_EQ(1U, GetRunningLinkPrerenderCount());

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));

  // Cancel the prerender.
  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  // All prerenders are now gone.
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenTaskManagerBeforePrerender) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 original = MatchTaskManagerTab("Preloader");
  const base::string16 prerender = MatchTaskManagerPrerender("Prerender Page");
  const base::string16 final = MatchTaskManagerTab("Prerender Page");

  // Show the task manager. This populates the model.
  chrome::OpenTaskManager(current_browser());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));

  // Prerender a page in addition to the original tab.
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // A TaskManager entry should appear like "Prerender: Prerender Page"
  // alongside the original tab entry. There should be just these two entries.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));

  // Swap in the prerendered content.
  NavigateToDestURL();

  // The "Prerender: " TaskManager entry should disappear, being replaced by a
  // "Tab: Prerender Page" entry, and nothing else.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, OpenTaskManagerAfterPrerender) {
  const base::string16 any_prerender = MatchTaskManagerPrerender("*");
  const base::string16 any_tab = MatchTaskManagerTab("*");
  const base::string16 original = MatchTaskManagerTab("Preloader");
  const base::string16 prerender = MatchTaskManagerPrerender("Prerender Page");
  const base::string16 final = MatchTaskManagerTab("Prerender Page");

  // Start with two resources.
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // Show the task manager. This populates the model. Importantly, we're doing
  // this after the prerender WebContents already exists - the task manager
  // needs to find it, it can't just listen for creation.
  chrome::OpenTaskManager(current_browser());

  // A TaskManager entry should appear like "Prerender: Prerender Page"
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));

  // Swap in the tab.
  NavigateToDestURL();

  // The "Prerender: Prerender Page" TaskManager row should disappear, being
  // replaced by "Tab: Prerender Page"
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, prerender));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, original));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, final));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, any_tab));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, any_prerender));
}

// TODO(jam): http://crbug.com/350550
#if !(defined(OS_CHROMEOS) && defined(ADDRESS_SANITIZER))

// Checks that prerenderers will terminate when the RenderView crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRendererCrash) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_RENDERER_CRASHED, 1);

  // Navigate to about:crash and then wait for the renderer to crash.
  content::ScopedAllowRendererCrashes allow_renderer_crashes(
      prerender->contents()->web_contents());
  ASSERT_TRUE(prerender->contents());
  ASSERT_TRUE(prerender->contents()->prerender_contents());
  prerender->contents()->prerender_contents()->GetController().LoadURL(
      GURL(content::kChromeUICrashURL), content::Referrer(),
      ui::PAGE_TRANSITION_TYPED, std::string());
  prerender->WaitForStop();
}
#endif

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageWithFragment) {
  PrerenderTestURL("/prerender/prerender_page.html#fragment", FINAL_STATUS_USED,
                   1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetMainFrame()
                                         ->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// Checks that we do not use a prerendered page when navigating from
// the main page to a fragment.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageNavigateFragment) {
  PrerenderTestURL("/prerender/no_prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html#fragment",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when we prerender a fragment
// but navigate to the main page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderFragmentNavigatePage) {
  PrerenderTestURL("/prerender/no_prerender_page.html#fragment",
                   FINAL_STATUS_APP_TERMINATING, 1);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Checks that we do not use a prerendered page when we prerender a fragment
// but navigate to a different fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderFragmentNavigateFragment) {
  PrerenderTestURL("/prerender/no_prerender_page.html#other_fragment",
                   FINAL_STATUS_APP_TERMINATING, 1);
  NavigateToURLWithDisposition("/prerender/no_prerender_page.html#fragment",
                               WindowOpenDisposition::CURRENT_TAB, false);
}

// Sets up HTTPS server for prerendered page, and checks that an SSL error will
// cancel the prerender. The prerenderer loader will be served through HTTP.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorTopLevel) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(https_url, FINAL_STATUS_SSL_ERROR, 0);
}

class TestClientCertStore : public net::ClientCertStore {
 public:
  explicit TestClientCertStore(const net::CertificateList& certs)
      : certs_(certs) {}
  ~TestClientCertStore() override {}

  // net::ClientCertStore:
  void GetClientCerts(const net::SSLCertRequestInfo& cert_request_info,
                      ClientCertListCallback callback) override {
    std::move(callback).Run(
        FakeClientCertIdentityListFromCertificateList(certs_));
  }

 private:
  net::CertificateList certs_;
};

std::unique_ptr<net::ClientCertStore> CreateCertStore(
    scoped_refptr<net::X509Certificate> available_cert) {
  return std::unique_ptr<net::ClientCertStore>(
      new TestClientCertStore(net::CertificateList(1, available_cert)));
}

// Checks that a top-level page which would normally request an SSL client
// certificate will never be seen since it's an https top-level resource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLClientCertTopLevel) {
  ProfileNetworkContextServiceFactory::GetForContext(
      current_browser()->profile())
      ->set_client_cert_store_factory_for_testing(base::BindRepeating(
          &CreateCertStore, net::ImportCertFromFile(
                                net::GetTestCertsDirectory(), "ok_cert.pem")));
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("/prerender/prerender_page.html");
  PrerenderTestURL(https_url, FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED, 0);
}

// Checks that an SSL Client Certificate request that originates from a
// subresource will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLClientCertSubresource) {
  ProfileNetworkContextServiceFactory::GetForContext(
      current_browser()->profile())
      ->set_client_cert_store_factory_for_testing(base::BindRepeating(
          &CreateCertStore, net::ImportCertFromFile(
                                net::GetTestCertsDirectory(), "ok_cert.pem")));
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(kPrefetchJpeg);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text);
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED, 0);
}

// Checks that an SSL Client Certificate request that originates from an
// iframe will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLClientCertIframe) {
  ProfileNetworkContextServiceFactory::GetForContext(
      current_browser()->profile())
      ->set_client_cert_store_factory_for_testing(base::BindRepeating(
          &CreateCertStore, net::ImportCertFromFile(
                                net::GetTestCertsDirectory(), "ok_cert.pem")));
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  https_server.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
  ASSERT_TRUE(https_server.Start());
  GURL https_url =
      https_server.GetURL("/prerender/prerender_embedded_content.html");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text);
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED, 0);
}

// Checks that the favicon is properly loaded on prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderFavicon) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_favicon.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();

  favicon::FaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(GetActiveWebContents());
  if (!favicon_driver->FaviconIsValid()) {
    // If the favicon has not been set yet, wait for it to be.
    FaviconUpdateWatcher favicon_update_watcher(GetActiveWebContents());
    favicon_update_watcher.Wait();
  }
  EXPECT_TRUE(favicon_driver->FaviconIsValid());
}

// Checks that when the history is cleared, prerendering is cancelled and
// prerendering history is cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClearHistory) {
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL("/prerender/prerender_page.html",
                       FINAL_STATUS_CACHE_OR_HISTORY_CLEARED, 1);

  ClearBrowsingData(current_browser(),
                    ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  prerender->WaitForStop();

  // Make sure prerender history was cleared.
  EXPECT_EQ(0U, GetHistoryLength());
}

// Checks that when the cache is cleared, prerenders are cancelled but
// prerendering history is not cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClearCache) {
  std::unique_ptr<TestPrerender> prerender =
      PrerenderTestURL("/prerender/prerender_page.html",
                       FINAL_STATUS_CACHE_OR_HISTORY_CLEARED, 1);

  ClearBrowsingData(current_browser(),
                    content::BrowsingDataRemover::DATA_TYPE_CACHE);
  prerender->WaitForStop();

  // Make sure prerender history was not cleared.  Not a vital behavior, but
  // used to compare with PrerenderClearHistory test.
  EXPECT_EQ(1U, GetHistoryLength());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelAll) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderEvents) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_CANCELLED, 1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  WaitForPrerenderStartEventForLinkNumber(0);
  WaitForPrerenderStopEventForLinkNumber(0);
  EXPECT_FALSE(HadPrerenderEventErrors());
}

// Cancels the prerender of a page with its own prerender.  The second prerender
// should never be started.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelPrerenderWithPrerender) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_infinite_a.html", FINAL_STATUS_CANCELLED, 1);

  GetPrerenderManager()->CancelAllPrerenders();
  prerender->WaitForStop();

  EXPECT_FALSE(prerender->contents());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewWindow) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_APP_TERMINATING, 1);
  OpenDestURLViaClickNewWindow();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewForegroundTab) {
  PrerenderTestURL("/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_APP_TERMINATING, 1);
  OpenDestURLViaClickNewForegroundTab();
}

// Checks that the referrer policy is used when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrerPolicy) {
  set_loader_path("/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the referrer policy is used when prerendering on HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLReferrerPolicy) {
  UseHttpsSrcServer();
  set_loader_path("/prerender/prerender_loader_with_referrer_policy.html");
  PrerenderTestURL("/prerender/prerender_referrer_policy.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Test interaction of Safe Browsing with prerender. Parametrized to enable
// SafeBrowsing Delayed Warnings experiment. The experiment shouldn't delay
// prerender page loads. Otherwise, the tests will crash or timeout.
// The experiment only delays phishing warnings so the tests must use a phishing
// resource.
class PrerenderSafeBrowsingTest
    : public PrerenderBrowserTest,
      public testing::WithParamInterface<
          testing::tuple<bool /* Enable delayed warnings experiment */>> {
 public:
  PrerenderSafeBrowsingTest() {
    if (testing::get<0>(GetParam())) {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{safe_browsing::kDelayedWarnings},
          /*disabled_features=*/{});
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that we do not prerender pages with a safe browsing
// interstitial.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, TopLevel) {
  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_SAFE_BROWSING,
                   0);
}

// Ensures that server redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, ServerRedirect) {
  GURL url = embedded_test_server()->GetURL("/prerender/prerender_page.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  PrerenderTestURL(CreateServerRedirect("/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING, 0);
}

// Ensures that we do not prerender pages which have a malware subresource.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, Subresource) {
  GURL image_url = embedded_test_server()->GetURL(kPrefetchJpeg);
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      image_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text);
  PrerenderTestURL(replacement_path, FINAL_STATUS_SAFE_BROWSING, 0);
}

// Ensures that we do not prerender pages which have a malware iframe.
IN_PROC_BROWSER_TEST_P(PrerenderSafeBrowsingTest, Iframe) {
  GURL iframe_url = embedded_test_server()->GetURL(
      "/prerender/prerender_embedded_content.html");
  GetFakeSafeBrowsingDatabaseManager()->SetThreatTypeForUrl(
      iframe_url, safe_browsing::SB_THREAT_TYPE_URL_PHISHING);
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", iframe_url.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_iframe.html", replacement_text);
  PrerenderTestURL(replacement_path, FINAL_STATUS_SAFE_BROWSING, 0);
}

INSTANTIATE_TEST_SUITE_P(PrerenderSafeBrowsingTest,
                         PrerenderSafeBrowsingTest,
                         testing::Combine(testing::Values(
                             false,
                             true)) /* Enable delayed warnings experiment */
);

// Test interaction of the webNavigation and tabs API with prerender.
class PrerenderBrowserTestWithExtensions : public PrerenderBrowserTest,
                                           public extensions::ExtensionApiTest {
 public:
  PrerenderBrowserTestWithExtensions() {
    // The individual tests start the test server through ExtensionApiTest, so
    // the port number can be passed through to the extension.
    set_autostart_test_server(false);
  }

  void SetUp() override { PrerenderBrowserTest::SetUp(); }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PrerenderBrowserTest::SetUpInProcessBrowserTestFixture();
    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    PrerenderBrowserTest::TearDownInProcessBrowserTestFixture();
    extensions::ExtensionApiTest::TearDownInProcessBrowserTestFixture();
  }

  void TearDownOnMainThread() override {
    PrerenderBrowserTest::TearDownOnMainThread();
    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  void SetUpOnMainThread() override {
    PrerenderBrowserTest::SetUpOnMainThread();
    extensions::ExtensionApiTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithExtensions, WebNavigation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/prerender")) << message_;

  extensions::ResultCatcher catcher;

  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetMainFrame()
                                         ->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithExtensions, TabsApi) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::FrameNavigationState::set_allow_extension_scheme(true);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("tabs/on_replaced")) << message_;

  extensions::ResultCatcher catcher;

  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  ChannelDestructionWatcher channel_close_watcher;
  channel_close_watcher.WatchChannel(browser()
                                         ->tab_strip_model()
                                         ->GetActiveWebContents()
                                         ->GetMainFrame()
                                         ->GetProcess());
  NavigateToDestURL();
  channel_close_watcher.WaitForChannelClose();

  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// Checks that non-http/https/chrome-extension subresource cancels the
// prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelSubresourceUnsupportedScheme) {
  GURL image_url = GURL("invalidscheme://www.google.com/test.jpg");
  base::StringPairs replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path = net::test_server::GetFilePathWithReplacements(
      "/prerender/prerender_with_image.html", replacement_text);
  // Disable load event checks because they race with cancellation.
  DisableLoadEventCheck();
  PrerenderTestURL(replacement_path, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Checks that non-http/https main page redirects cancel the prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderCancelMainFrameRedirectUnsupportedScheme) {
  // Disable load event checks because they race with cancellation.
  DisableLoadEventCheck();
  GURL url = embedded_test_server()->GetURL(
      CreateServerRedirect("invalidscheme://www.google.com/test.html"));
  PrerenderTestURL(url, FINAL_STATUS_UNSUPPORTED_SCHEME, 0);
}

// Checks that prerenders are not swapped for navigations with extra headers.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExtraHeadersNoSwap) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  content::OpenURLParams params(dest_url(), Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_TYPED, false);
  params.extra_headers = "X-Custom-Header: 42\r\n";
  NavigateToURLWithParams(params, false);
}

// Checks that prerenders are not swapped for navigations with browser-initiated
// POST data.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderBrowserInitiatedPostNoSwap) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  std::string post_data = "DATA";
  content::OpenURLParams params(dest_url(), Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_TYPED, false);
  params.post_data = network::ResourceRequestBody::CreateFromBytes(
      post_data.data(), post_data.size());
  NavigateToURLWithParams(params, false);
}

// Attempt a swap-in in a new tab. The session storage doesn't match, so it
// should not swap.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPageNewTab) {
  PrerenderTestURL("/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING, 1);

  // Open a new tab to navigate in.
  ui_test_utils::NavigateToURLWithDisposition(
      current_browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Now navigate in the new tab.
  NavigateToDestURLWithDisposition(WindowOpenDisposition::CURRENT_TAB, false);
}

// Tests interaction between prerender and POST (i.e. POST request should still
// be made and POST data should not be dropped when the POST target is the same
// as a prerender link).
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, HttpPost) {
  // Expect that the prerendered contents won't get used (i.e. the prerendered
  // content should be destroyed when the test closes the browser under test).
  std::unique_ptr<TestPrerender> prerender =
      ExpectPrerender(FINAL_STATUS_APP_TERMINATING);

  // Navigate to a page containing a form that targets a prerendered link.
  GURL main_url(embedded_test_server()->GetURL(
      "/prerender/form_that_posts_to_prerendered_echoall.html"));
  ui_test_utils::NavigateToURL(current_browser(), main_url);

  // Wait for the prerender to be ready.
  prerender->WaitForStart();
  prerender->WaitForLoads(1);
  EXPECT_THAT(prerender->contents()->prerender_url().spec(),
              ::testing::MatchesRegex("^http://127.0.0.1.*:\\d+/echoall$"));

  // Submit the form.
  content::WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  TestNavigationObserver form_post_observer(web_contents, 1);
  EXPECT_TRUE(
      ExecuteScript(web_contents, "document.getElementById('form').submit();"));
  form_post_observer.Wait();

  // Verify that we arrived at the expected location.
  GURL target_url(embedded_test_server()->GetURL("/echoall"));
  EXPECT_EQ(target_url, web_contents->GetLastCommittedURL());

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page (i.e. verify that we haven't used the prerendered
  // page that doesn't contain the POST body).
  std::string body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send("
      "document.getElementsByTagName('pre')[0].innerText);",
      &body));
  EXPECT_EQ("text=value\n", body);
}

class PrerenderIncognitoBrowserTest : public PrerenderBrowserTest {
 public:
  void SetUpOnMainThread() override {
    Profile* normal_profile = current_browser()->profile();
    set_browser(OpenURLOffTheRecord(normal_profile, GURL("about:blank")));
    PrerenderBrowserTest::SetUpOnMainThread();
    current_browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(content_settings::CookieControlsMode::kOff));
  }
};

// Checks that prerendering works in incognito mode.
IN_PROC_BROWSER_TEST_F(PrerenderIncognitoBrowserTest, PrerenderIncognito) {
  PrerenderTestURL("/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that prerenders are aborted when an incognito profile is closed.
// ToDo(crbug.com/994068): The test is crashing on multiple platforms.
IN_PROC_BROWSER_TEST_F(PrerenderIncognitoBrowserTest,
                       DISABLED_PrerenderIncognitoClosed) {
  std::unique_ptr<TestPrerender> prerender = PrerenderTestURL(
      "/prerender/prerender_page.html", FINAL_STATUS_PROFILE_DESTROYED, 1);
  current_browser()->window()->Close();
  prerender->WaitForStop();
}

class PrerenderOmniboxBrowserTest : public PrerenderBrowserTest {
 public:
  LocationBar* GetLocationBar() {
    return current_browser()->window()->GetLocationBar();
  }

  OmniboxView* GetOmniboxView() { return GetLocationBar()->GetOmniboxView(); }

  predictors::AutocompleteActionPredictor* GetAutocompleteActionPredictor() {
    Profile* profile = current_browser()->profile();
    return predictors::AutocompleteActionPredictorFactory::GetForProfile(
        profile);
  }

  std::unique_ptr<TestPrerender> StartOmniboxPrerender(
      const GURL& url,
      FinalStatus expected_final_status) {
    std::unique_ptr<TestPrerender> prerender =
        ExpectPrerender(expected_final_status);
    WebContents* web_contents = GetActiveWebContents();
    GetAutocompleteActionPredictor()->StartPrerendering(
        url, web_contents->GetController().GetDefaultSessionStorageNamespace(),
        gfx::Size(50, 50));
    prerender->WaitForStart();
    return prerender;
  }
};

// Checks that closing the omnibox popup cancels an omnibox prerender.
// http://crbug.com/395152
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxBrowserTest,
                       DISABLED_PrerenderOmniboxCancel) {
  // Fake an omnibox prerender.
  std::unique_ptr<TestPrerender> prerender = StartOmniboxPrerender(
      embedded_test_server()->GetURL("/empty.html"), FINAL_STATUS_CANCELLED);

  // Revert the location bar. This should cancel the prerender.
  GetLocationBar()->Revert();
  prerender->WaitForStop();
}

// Checks that accepting omnibox input abandons an omnibox prerender.
// http://crbug.com/394592
IN_PROC_BROWSER_TEST_F(PrerenderOmniboxBrowserTest,
                       DISABLED_PrerenderOmniboxAbandon) {
  // Set the abandon timeout to something high so it does not introduce
  // flakiness if the prerender times out before the test completes.
  GetPrerenderManager()->mutable_config().abandon_time_to_live =
      base::TimeDelta::FromDays(999);

  // Enter a URL into the Omnibox.
  OmniboxView* omnibox_view = GetOmniboxView();
  omnibox_view->OnBeforePossibleChange();
  omnibox_view->SetUserText(base::UTF8ToUTF16(
      embedded_test_server()->GetURL("/empty.html?1").spec()));
  omnibox_view->OnAfterPossibleChange(true);
  ui_test_utils::WaitForAutocompleteDone(current_browser());

  // Fake an omnibox prerender for a different URL.
  std::unique_ptr<TestPrerender> prerender =
      StartOmniboxPrerender(embedded_test_server()->GetURL("/empty.html?2"),
                            FINAL_STATUS_APP_TERMINATING);

  // The final status may be either FINAL_STATUS_APP_TERMINATING or
  // FINAL_STATUS_CANCELLED. Although closing the omnibox will not cancel an
  // abandoned prerender, the AutocompleteActionPredictor will cancel the
  // predictor on destruction.
  prerender->contents()->set_skip_final_checks(true);

  // Navigate to the URL entered.
  omnibox_view->model()->AcceptInput(WindowOpenDisposition::CURRENT_TAB);

  // Prerender should be running, but abandoned.
  EXPECT_TRUE(
      GetAutocompleteActionPredictor()->IsPrerenderAbandonedForTesting());
}

// Can't run tests with NaCl plugins if built without ENABLE_NACL.
#if BUILDFLAG(ENABLE_NACL)
class PrerenderBrowserTestWithNaCl : public PrerenderBrowserTest {
 public:
  PrerenderBrowserTestWithNaCl() {}
  ~PrerenderBrowserTestWithNaCl() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableNaCl);
  }
};

// PrerenderNaClPluginEnabled crashes on ARM: http://crbug.com/585251
#if defined(ARCH_CPU_ARM_FAMILY)
#define MAYBE_PrerenderNaClPluginEnabled DISABLED_PrerenderNaClPluginEnabled
#else
#define MAYBE_PrerenderNaClPluginEnabled PrerenderNaClPluginEnabled
#endif

// Check that NaCl plugins work when enabled, with prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithNaCl,
                       MAYBE_PrerenderNaClPluginEnabled) {
  PrerenderTestURL("/prerender/prerender_plugin_nacl_enabled.html",
                   FINAL_STATUS_USED, 1);
  NavigateToDestURL();

  // To avoid any chance of a race, we have to let the script send its response
  // asynchronously.
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool display_test_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents, "DidDisplayReallyPass()", &display_test_result));
  ASSERT_TRUE(display_test_result);
}
#endif  // BUILDFLAG(ENABLE_NACL)

}  // namespace prerender

#endif  // !defined(OS_MACOSX) || !defined(ADDRESS_SANITIZER)
