// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
#define CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/public/browser/navigation_type.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_errors.h"
#include "url/gurl.h"

namespace content {
class NavigationHandle;
class WebContents;

// For browser_tests, which run on the UI thread, run a second
// MessageLoop and quit when the navigation completes loading.
class TestNavigationObserver {
 public:
  enum class WaitEvent {
    kLoadStopped,
    kNavigationFinished,
  };

  // Create and register a new TestNavigationObserver against the
  // |web_contents|.
  TestNavigationObserver(WebContents* web_contents,
                         int number_of_navigations,
                         MessageLoopRunner::QuitMode quit_mode =
                             MessageLoopRunner::QuitMode::IMMEDIATE);
  // Like above but waits for one navigation.
  explicit TestNavigationObserver(WebContents* web_contents,
                                  MessageLoopRunner::QuitMode quit_mode =
                                      MessageLoopRunner::QuitMode::IMMEDIATE);
  // Create and register a new TestNavigationObserver that will wait for
  // |target_url| to complete loading or for a committed navigation to
  // |target_url|.
  explicit TestNavigationObserver(const GURL& target_url,
                                  MessageLoopRunner::QuitMode quit_mode =
                                      MessageLoopRunner::QuitMode::IMMEDIATE);

  // Create and register a new TestNavigationObserver that will wait for
  // a navigation with |target_error|.
  explicit TestNavigationObserver(WebContents* web_contents,
                                  net::Error target_error,
                                  MessageLoopRunner::QuitMode quit_mode =
                                      MessageLoopRunner::QuitMode::IMMEDIATE);

  virtual ~TestNavigationObserver();

  void set_wait_event(WaitEvent event) { wait_event_ = event; }

  // Runs a nested run loop and blocks until the expected number of navigations
  // stop loading or |target_url| has loaded.
  void Wait();

  // Runs a nested run loop and blocks until the expected number of navigations
  // finished or a navigation to |target_url| has finished.
  void WaitForNavigationFinished();

  // Start/stop watching newly created WebContents.
  void StartWatchingNewWebContents();
  void StopWatchingNewWebContents();

  // Makes this TestNavigationObserver an observer of all previously created
  // WebContents.
  void WatchExistingWebContents();

  // The URL of the last finished navigation (that matched URL / net error
  // filters, if set).
  const GURL& last_navigation_url() const { return last_navigation_url_; }

  // Returns true if the last finished navigation (that matched URL / net error
  // filters, if set) succeeded.
  bool last_navigation_succeeded() const { return last_navigation_succeeded_; }

  // Returns the initiator origin of the last finished navigation (that matched
  // URL / net error filters, if set).
  const base::Optional<url::Origin>& last_initiator_origin() const {
    return last_navigation_initiator_origin_;
  }

  const GlobalFrameRoutingId& last_initiator_routing_id() const {
    return last_initiator_routing_id_;
  }

  // Returns the net::Error origin of the last finished navigation (that matched
  // URL / net error filters, if set).
  net::Error last_net_error_code() const { return last_net_error_code_; }

  // Returns the NavigationType  of the last finished navigation (that matched
  // URL / net error filters, if set).
  NavigationType last_navigation_type() const { return last_navigation_type_; }

 protected:
  // Register this TestNavigationObserver as an observer of the |web_contents|.
  void RegisterAsObserver(WebContents* web_contents);

  // Protected so that subclasses can retrieve extra information from the
  // |navigation_handle|.
  virtual void OnDidFinishNavigation(NavigationHandle* navigation_handle);

 private:
  class TestWebContentsObserver;

  // State of a WebContents* known to this TestNavigationObserver.
  // Move-only.
  struct WebContentsState {
    WebContentsState();

    WebContentsState(const WebContentsState& other) = delete;
    WebContentsState& operator=(const WebContentsState& other) = delete;
    WebContentsState(WebContentsState&& other);
    WebContentsState& operator=(WebContentsState&& other);

    ~WebContentsState();

    // Observes the WebContents this state has been created for and relays
    // events to the TestNavigationObserver.
    std::unique_ptr<TestWebContentsObserver> observer;

    // If true, a navigation is in progress in the WebContents.
    bool navigation_started = false;
    // If true, the last navigation that finished in this WebContents matched
    // the filter criteria (|target_url_| or |target_error_|).
    // Only relevant if a filter is configured.
    bool last_navigation_matches_filter = false;
  };

  TestNavigationObserver(WebContents* web_contents,
                         int number_of_navigations,
                         const base::Optional<GURL>& target_url,
                         base::Optional<net::Error> target_error,
                         MessageLoopRunner::QuitMode quit_mode =
                             MessageLoopRunner::QuitMode::IMMEDIATE);

  // Callbacks for WebContents-related events.
  void OnWebContentsCreated(WebContents* web_contents);
  void OnWebContentsDestroyed(TestWebContentsObserver* observer,
                              WebContents* web_contents);
  void OnNavigationEntryCommitted(
      TestWebContentsObserver* observer,
      WebContents* web_contents,
      const LoadCommittedDetails& load_details);
  void OnDidAttachInterstitialPage(WebContents* web_contents);
  void OnDidStartLoading(WebContents* web_contents);
  void OnDidStopLoading(WebContents* web_contents);
  void OnDidStartNavigation(NavigationHandle* navigation_handle);
  void EventTriggered(WebContentsState* web_contents_state);

  // Returns true if |target_url_| or |target_error_| is configured.
  bool HasFilter();

  // Returns the WebContentsState for |web_contents|.
  WebContentsState* GetWebContentsState(WebContents* web_contents);

  // The event that once triggered will quit the run loop.
  WaitEvent wait_event_;

  // Tracks WebContents and their loading/navigation state.
  std::map<WebContents*, WebContentsState> web_contents_state_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  // If |target_url_| and/or |target_error_| are set, only navigations that
  // match those criteria will count towards this.
  int number_of_navigations_;

  // The URL to wait for.
  // If this is nullopt, any URL counts.
  const base::Optional<GURL> target_url_;

  // The net error of the finished navigation to wait for.
  // If this is nullopt, any net::Error counts.
  const base::Optional<net::Error> target_error_;

  // The url of the navigation that last committed.
  GURL last_navigation_url_;

  // True if the last navigation succeeded.
  bool last_navigation_succeeded_;

  // The initiator origin of the last navigation.
  base::Optional<url::Origin> last_navigation_initiator_origin_;

  // The routing id of the initiator frame for the last observed navigation.
  GlobalFrameRoutingId last_initiator_routing_id_;

  // The net error code of the last navigation.
  net::Error last_net_error_code_;

  // The NavigationType of the last navigation.
  NavigationType last_navigation_type_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // Callback invoked on WebContents creation.
  base::RepeatingCallback<void(WebContents*)> web_contents_created_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationObserver);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_NAVIGATION_OBSERVER_H_
