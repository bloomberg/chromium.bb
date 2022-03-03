// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_
#define CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_

#include <vector>

#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "url/gurl.h"

class IntentPickerAutoDisplayService;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace apps {

void MaybeShowIntentPickerBubble(content::NavigationHandle* navigation_handle,
                                 std::vector<IntentPickerAppInfo> apps);

bool ContainsOnlyPwasAndMacApps(
    const std::vector<apps::IntentPickerAppInfo>& apps);

// These enums are used to define the intent picker show state, whether the
// picker is popped out or just displayed as a clickable omnibox icon.
enum class PickerShowState {
  kOmnibox = 1,  // Only show the intent icon in the omnibox
  kPopOut = 2,   // show the intent picker icon and pop out bubble
};

void OnIntentPickerClosedChromeOs(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    PickerShowState show_state,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_INTENT_HELPER_CHROMEOS_INTENT_PICKER_HELPERS_H_
