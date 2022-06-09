// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
#define CHROME_BROWSER_UI_TABS_TAB_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ui/tabs/tab_enums.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/skia/include/core/SkColor.h"

class TabStripModel;

namespace content {
class WebContents;
}  // namespace content

// TODO(elainechien): Move this to the tab_enums.h file and include tab_enums.h
// where TabMutedReason is used.
enum class TabMutedReason {
  NONE,                    // The tab has never been muted or unmuted.
  EXTENSION,               // Mute state changed via extension API.
  CONTENT_SETTING,         // The sound content setting was set to BLOCK.
  CONTENT_SETTING_CHROME,  // Mute toggled on chrome:// URL.
};

struct LastMuteMetadata
    : public content::WebContentsUserData<LastMuteMetadata> {
  TabMutedReason reason = TabMutedReason::NONE;
  std::string extension_id;  // Only valid when |reason| is EXTENSION.

 private:
  explicit LastMuteMetadata(content::WebContents* contents);
  friend class content::WebContentsUserData<LastMuteMetadata>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

namespace chrome {

// Returns the alert states to be shown by the tab's alert indicator.
// The returned list is in descending order of importance to user
// privacy, i.e. if only one is to be shown, it should be the first.
// TabAlertState::NONE will never be present in the list; an empty list
// is returned instead.
std::vector<TabAlertState> GetTabAlertStatesForContents(
    content::WebContents* contents);

// Returns a localized string describing the |alert_state|.
std::u16string GetTabAlertStateText(const TabAlertState alert_state);

// Sets whether all audio output from |contents| is muted, along with the
// |reason| it is to be muted/unmuted (via UI or extension API).  When |reason|
// is TAB_MUTED_REASON_EXTENSION, |extension_id| must be provided; otherwise, it
// is ignored.  Returns whether the tab was actually muted.
bool SetTabAudioMuted(content::WebContents* contents,
                      bool mute,
                      TabMutedReason reason,
                      const std::string& extension_id);

// Returns the last reason a tab's mute state was changed.
TabMutedReason GetTabAudioMutedReason(content::WebContents* contents);

// Returns true if the site at |index| in |tab_strip| is muted.
bool IsSiteMuted(const TabStripModel& tab_strip, const int index);

// Returns true if the sites at the |indices| in |tab_strip| are all muted.
bool AreAllSitesMuted(const TabStripModel& tab_strip,
                      const std::vector<int>& indices);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_TABS_TAB_UTILS_H_
