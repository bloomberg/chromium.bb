// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_TRACKER_H_
#define CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_TRACKER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/base/scoped_visibility_tracker.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}

// This class tracks new popups, and is used to log metrics on the visibility
// time of the first document in the popup.
// TODO(csharrison): Consider adding more metrics like total visibility for the
// lifetime of the WebContents.
class PopupTracker : public content::WebContentsObserver,
                     public content::WebContentsUserData<PopupTracker>,
                     public subresource_filter::SubresourceFilterObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PopupSafeBrowsingStatus {
    kNoValue = 0,
    kSafe = 1,
    kUnsafe = 2,
    kMaxValue = kUnsafe,
  };

  static PopupTracker* CreateForWebContents(content::WebContents* contents,
                                            content::WebContents* opener,
                                            WindowOpenDisposition disposition);
  ~PopupTracker() override;

  void set_is_trusted(bool is_trusted) { is_trusted_ = is_trusted; }

 private:
  friend class content::WebContentsUserData<PopupTracker>;

  PopupTracker(content::WebContents* contents,
               content::WebContents* opener,
               WindowOpenDisposition disposition);

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override;

  // subresource_filter::SubresourceFilterObserver:
  void OnSafeBrowsingChecksComplete(
      content::NavigationHandle* navigation_handle,
      const subresource_filter::SubresourceFilterSafeBrowsingClient::
          CheckResult& result) override;
  void OnSubresourceFilterGoingAway() override;

  ScopedObserver<subresource_filter::SubresourceFilterObserverManager,
                 subresource_filter::SubresourceFilterObserver>
      scoped_observer_;

  // Will be unset until the first navigation commits. Will be set to the total
  // time the contents was visible at commit time.
  base::Optional<base::TimeDelta> first_load_visible_time_start_;
  // Will be unset until the second navigation commits. Is the total time the
  // contents is visible while the first document is loading (after commit).
  base::Optional<base::TimeDelta> first_load_visible_time_;

  ui::ScopedVisibilityTracker visibility_tracker_;

  // The number of user interactions occurring in this popup tab.
  int num_interactions_ = 0;
  // The number of user interacitons in a popup tab broken down into
  // user activation and gesture scroll begin events.
  int num_activation_events_ = 0;
  int num_gesture_scroll_begin_events_ = 0;

  // The id of the web contents that created the popup at the time of creation.
  // SourceIds are permanent so it's okay to use at any point so long as it's
  // not invalid.
  const ukm::SourceId opener_source_id_;

  bool is_trusted_ = false;

  // Whether the pop-up navigated to a site on the safe browsing list. Set when
  // the safe browsing checks complete.
  PopupSafeBrowsingStatus safe_browsing_status_ =
      PopupSafeBrowsingStatus::kNoValue;

  // The window open disposition used when creating the popup.
  const WindowOpenDisposition window_open_disposition_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PopupTracker);
};

#endif  // CHROME_BROWSER_UI_BLOCKED_CONTENT_POPUP_TRACKER_H_
