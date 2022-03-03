// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/blocked_content/popup_opener_tab_helper.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/tick_clock.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "ui/base/scoped_visibility_tracker.h"

namespace blocked_content {

PopupOpenerTabHelper::~PopupOpenerTabHelper() {
  DCHECK(visibility_tracker_);
  base::TimeDelta total_visible_time =
      visibility_tracker_->GetForegroundDuration();
  if (did_tab_under()) {
    UMA_HISTOGRAM_LONG_TIMES(
        "Tab.TabUnder.VisibleTime",
        total_visible_time - visible_time_before_tab_under_.value());
    UMA_HISTOGRAM_LONG_TIMES("Tab.TabUnder.VisibleTimeBefore",
                             visible_time_before_tab_under_.value());
  }
  UMA_HISTOGRAM_LONG_TIMES("Tab.VisibleTime", total_visible_time);
}

void PopupOpenerTabHelper::OnOpenedPopup(PopupTracker* popup_tracker) {
  has_opened_popup_since_last_user_gesture_ = true;
  MaybeLogPagePopupContentSettings();

  last_popup_open_time_ = tick_clock_->NowTicks();
}

void PopupOpenerTabHelper::OnDidTabUnder() {
  // The tab already did a tab-under.
  if (did_tab_under())
    return;

  // Tab-under requires a popup, so this better not be null.
  DCHECK(!last_popup_open_time_.is_null());
  UMA_HISTOGRAM_LONG_TIMES("Tab.TabUnder.PopupToTabUnderTime",
                           tick_clock_->NowTicks() - last_popup_open_time_);

  visible_time_before_tab_under_ = visibility_tracker_->GetForegroundDuration();
}

PopupOpenerTabHelper::PopupOpenerTabHelper(content::WebContents* web_contents,
                                           const base::TickClock* tick_clock,
                                           HostContentSettingsMap* settings_map)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<PopupOpenerTabHelper>(*web_contents),
      tick_clock_(tick_clock),
      settings_map_(settings_map) {
  visibility_tracker_ = std::make_unique<ui::ScopedVisibilityTracker>(
      tick_clock_,
      web_contents->GetVisibility() != content::Visibility::HIDDEN);
}

void PopupOpenerTabHelper::OnVisibilityChanged(content::Visibility visibility) {
  // TODO(csharrison): Consider handling OCCLUDED tabs the same way as HIDDEN
  // tabs.
  if (visibility == content::Visibility::HIDDEN)
    visibility_tracker_->OnHidden();
  else
    visibility_tracker_->OnShown();
}

void PopupOpenerTabHelper::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  has_opened_popup_since_last_user_gesture_ = false;
}

void PopupOpenerTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Treat browser-initiated navigations as user interactions.
  if (!navigation_handle->IsRendererInitiated())
    has_opened_popup_since_last_user_gesture_ = false;
}

void PopupOpenerTabHelper::MaybeLogPagePopupContentSettings() {
  // If the user has opened a popup, record the page popup settings ukm.
  const GURL& url = web_contents()->GetLastCommittedURL();
  if (!url.is_valid())
    return;

  const ukm::SourceId source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents());

  // Do not record duplicate Popup.Page events for popups opened in succession
  // from the same opener.
  if (source_id != last_opener_source_id_) {
    bool user_allows_popups =
        settings_map_->GetContentSetting(
            url, url, ContentSettingsType::POPUPS) == CONTENT_SETTING_ALLOW;
    ukm::builders::Popup_Page(source_id)
        .SetAllowed(user_allows_popups)
        .Record(ukm::UkmRecorder::Get());
    last_opener_source_id_ = source_id;
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PopupOpenerTabHelper);

}  // namespace blocked_content
