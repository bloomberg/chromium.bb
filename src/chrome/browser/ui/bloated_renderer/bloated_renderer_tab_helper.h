// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOATED_RENDERER_BLOATED_RENDERER_TAB_HELPER_H_
#define CHROME_BROWSER_UI_BLOATED_RENDERER_BLOATED_RENDERER_TAB_HELPER_H_

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/resource_coordinator/page_signal_receiver.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class InfoBarService;

// This tab helper observes the OnRendererIsBloated event for its
// WebContents. Upon receiving the event it reloads the bloated tab if
// possible and activates the logic to show an infobar on the
// subsequent DidFinishNavigation event.
//
// Note that we need to show the infobar after NavigationEntryCommitted
// because the infobar service removes existing infobars there.
class BloatedRendererTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<BloatedRendererTabHelper>,
      public resource_coordinator::PageSignalObserver {
 public:
  ~BloatedRendererTabHelper() override = default;

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  // resource_coordinator::PageSignalObserver:
  void OnRendererIsBloated(content::WebContents* web_contents,
                           const resource_coordinator::PageNavigationIdentity&
                               page_navigation_id) override;

  static void ShowInfoBar(InfoBarService* infobar_service);

 private:
  friend class content::WebContentsUserData<BloatedRendererTabHelper>;
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest, DetectReload);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest,
                           IgnoreUnrelatedNavigation);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest, CanReloadBloatedTab);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest,
                           CannotReloadBloatedTabCrashed);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest,
                           CannotReloadBloatedTabInvalidURL);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest,
                           CannotReloadBloatedTabPendingUserInteraction);
  FRIEND_TEST_ALL_PREFIXES(BloatedRendererTabHelperTest,
                           CannotReloadBloatedTabWithPostData);

  enum class State { kInactive, kRequestingReload, kStartedNavigation };

  explicit BloatedRendererTabHelper(content::WebContents* contents);

  bool CanReloadBloatedTab();

  // The state transitions as follows:
  // - kInactive is the initial state.
  //
  // - any state => kRequestingReload transition happens in
  //   OnRendererIsBloated before invoking NavigationController::Reload.
  //
  // - kRequestingReload => kStartedNavigation transition happens in
  //   NavigationController::Reload when it invokes DidStartNavigation.
  //
  // - kStartedNavigation => kInactive transitions happens in
  //   DidFinishNavigation.
  State state_ = State::kInactive;

  // The navigation id is saved on DidStartNavigation event when the state is
  // kRequestingReload. The infobar is shown on the subsequent
  // DidFinishNavigation only if its navigation id matches the saved id. This
  // ensures that the infobar is shown only for the reload that was requested
  // in OnRendererIsBloated event.
  int64_t saved_navigation_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BloatedRendererTabHelper);
};

#endif  // CHROME_BROWSER_UI_BLOATED_RENDERER_BLOATED_RENDERER_TAB_HELPER_H_
