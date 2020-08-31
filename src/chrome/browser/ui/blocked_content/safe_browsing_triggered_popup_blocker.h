// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_SAFE_BROWSING_TRIGGERED_POPUP_BLOCKER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_SAFE_BROWSING_TRIGGERED_POPUP_BLOCKER_H_

#include <memory>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

extern const base::Feature kAbusiveExperienceEnforce;

constexpr char kAbusiveEnforceMessage[] =
    "Chrome prevented this site from opening a new tab or window. Learn more "
    "at https://www.chromestatus.com/feature/5243055179300864";
constexpr char kAbusiveWarnMessage[] =
    "Chrome might start preventing this site from opening new tabs or "
    "windows in the future. Learn more at "
    "https://www.chromestatus.com/feature/5243055179300864";

// This class observes main frame navigation checks incoming from safe browsing
// (currently implemented by the subresource_filter component). For navigations
// which match the ABUSIVE safe browsing list, this class will help the popup
// tab helper in applying a stronger policy for blocked popups.
class SafeBrowsingTriggeredPopupBlocker
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SafeBrowsingTriggeredPopupBlocker>,
      public subresource_filter::SubresourceFilterObserver {
 public:
  // This enum backs a histogram. Please append new entries to the end, and
  // update enums.xml when making changes.
  enum class Action : int {
    // User committed a navigation to a non-error page.
    kNavigation,

    // Safe Browsing considered this page abusive and the page should be warned.
    // Logged at navigation commit.
    kWarningSite,

    // Safe Browsing considered this page abusive and the page should be be
    // blocked against. Logged at navigation commit.
    kEnforcedSite,

    // The popup blocker called into this object to ask if the strong blocking
    // should be applied.
    kConsidered,

    // This object responded to the popup blocker in the affirmative, and the
    // popup was blocked.
    kBlocked,

    // Add new entries before this one
    kCount
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Creates a SafeBrowsingTriggeredPopupBlocker and attaches it (via UserData)
  // to |web_contents|.
  static void MaybeCreate(content::WebContents* web_contents);
  ~SafeBrowsingTriggeredPopupBlocker() override;

  bool ShouldApplyAbusivePopupBlocker();

 private:
  friend class content::WebContentsUserData<SafeBrowsingTriggeredPopupBlocker>;
  // The |web_contents| and |observer_manager| are expected to be
  // non-nullptr.
  SafeBrowsingTriggeredPopupBlocker(
      content::WebContents* web_contents,
      subresource_filter::SubresourceFilterObserverManager* observer_manager);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // subresource_filter::SubresourceFilterObserver:
  void OnSafeBrowsingChecksComplete(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::SubresourceFilterSafeBrowsingClient::
          CheckResult& result) override;
  void OnSubresourceFilterGoingAway() override;

  // Enabled state is governed by both a feature flag and a pref (which can be
  // controlled by enterprise policy).
  static bool IsEnabled(content::WebContents* web_contents);

  // Data scoped to a single page. Will be reset at navigation commit.
  class PageData {
   public:
    PageData();

    // Logs UMA in the destructor based on the number of popups blocked.
    ~PageData();

    void inc_num_popups_blocked() { ++num_popups_blocked_; }

    void set_is_triggered(bool is_triggered) { is_triggered_ = is_triggered; }
    bool is_triggered() const { return is_triggered_; }

   private:
    // How many popups are blocked in this page.
    int num_popups_blocked_ = 0;

    // Whether the current committed page load should trigger the stronger popup
    // blocker.
    bool is_triggered_ = false;

    DISALLOW_COPY_AND_ASSIGN(PageData);
  };

  ScopedObserver<subresource_filter::SubresourceFilterObserverManager,
                 subresource_filter::SubresourceFilterObserver>
      scoped_observer_;

  // Whether the next main frame navigation that commits should trigger the
  // stronger popup blocker in enforce or warn mode.
  base::Optional<safe_browsing::SubresourceFilterLevel>
      level_for_next_committed_navigation_;

  // Should never be nullptr.
  std::unique_ptr<PageData> current_page_data_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTriggeredPopupBlocker);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_SAFE_BROWSING_TRIGGERED_POPUP_BLOCKER_H_
