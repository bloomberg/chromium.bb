// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/navigation_id.h"

#include <string>
#include <tuple>

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace predictors {

NavigationID::NavigationID() : tab_id(SessionID::InvalidValue()) {}

NavigationID::NavigationID(const NavigationID& other)
    : tab_id(other.tab_id),
      ukm_source_id(other.ukm_source_id),
      main_frame_url(other.main_frame_url),
      creation_time(other.creation_time) {}

// TODO(crbug.com/1061899): The code assumes that it was called on behalf of
// the active frame, which might not be true. Pass RenderFrameHost reference
// hereto avoid confusion.
NavigationID::NavigationID(content::WebContents* web_contents)
    : tab_id(sessions::SessionTabHelper::IdForTab(web_contents)),
      ukm_source_id(web_contents->GetMainFrame()->GetPageUkmSourceId()),
      main_frame_url(web_contents->GetLastCommittedURL()),
      creation_time(base::TimeTicks::Now()) {}

NavigationID::NavigationID(content::WebContents* web_contents,
                           ukm::SourceId ukm_source_id,
                           const GURL& main_frame_url,
                           const base::TimeTicks& creation_time)
    : tab_id(sessions::SessionTabHelper::IdForTab(web_contents)),
      ukm_source_id(ukm_source_id),
      main_frame_url(main_frame_url),
      creation_time(creation_time) {}

bool NavigationID::is_valid() const {
  return tab_id.is_valid() && !main_frame_url.is_empty();
}

bool NavigationID::operator<(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  return std::tie(tab_id, main_frame_url) <
         std::tie(rhs.tab_id, rhs.main_frame_url);
}

bool NavigationID::operator==(const NavigationID& rhs) const {
  DCHECK(is_valid() && rhs.is_valid());
  return tab_id == rhs.tab_id && ukm_source_id == rhs.ukm_source_id &&
         main_frame_url == rhs.main_frame_url;
}

bool NavigationID::operator!=(const NavigationID& rhs) const {
  return !(*this == rhs);
}

}  // namespace predictors
